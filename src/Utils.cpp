#include "GlobalNamespace/LevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/SelectLevelCategoryViewController.hpp"
#include "HMUI/NoTransitionsButton.hpp"
#include "System/IO/File.hpp"
#include "System/IO/Path.hpp"
#include "UnityEngine/AudioType.hpp"
#include "UnityEngine/Networking/DownloadHandlerAudioClip.hpp"
#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/Networking/UnityWebRequestMultimedia.hpp"
#include "UnityEngine/Resources.hpp"
#include "bsml/shared/BSML-Lite/Creation/Image.hpp"
#include "bsml/shared/BSML/SharedCoroutineStarter.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "songcore/shared/SongCore.hpp"
#include "web-utils/shared/WebUtils.hpp"

#include "Assets.hpp"
#include "Log.hpp"
#include "SpriteCache.hpp"
#include "UI/FlowCoordinators/SpotifySearchFlowCoordinator.hpp"
#include "Utils.hpp"
#include "main.hpp"
#include "BeatSaverUtils.hpp"

using ::GlobalNamespace::LevelSelectionFlowCoordinator;
using ::GlobalNamespace::SelectLevelCategoryViewController;
using ::GlobalNamespace::SoloFreePlayFlowCoordinator;

namespace SpotifySearch::Utils::json {

std::string getString(const rapidjson::Value& json, const std::string& key) {
    if (!json.HasMember(key)) {
        throw std::runtime_error(std::format("Missing key: {}", key));
    }
    const auto& jsonValue = json[key];
    if (!jsonValue.IsString()) {
        throw std::runtime_error(std::format("Unexpected type for key: {}", key));
    }
    const std::string value = jsonValue.GetString();
    if (value.empty()) {
        throw std::runtime_error(std::format("Value for key was empty: {}", key));
    }
    return value;
}

}

std::string SpotifySearch::Utils::encodeBase64(const std::string& input) {
    static const char b64_table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    unsigned int val = 0;
    int valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            output.push_back(b64_table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        output.push_back(b64_table[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (output.size() % 4) {
        output.push_back('=');
    }
    return output;
}

std::span<const uint8_t> SpotifySearch::Utils::toSpan(const std::string& text) {
    return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

std::string SpotifySearch::Utils::toLowerCase(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) {
                       return std::tolower(c);
                   });
    return lower;
}

std::string SpotifySearch::Utils::toLowerCase(std::string_view text) {
    std::string lower;
    lower.resize(text.size());
    std::transform(
        text.begin(),
        text.end(),
        lower.begin(),
        [](unsigned char c) {
            return std::tolower(c);
        });
    return lower;
}

void SpotifySearch::Utils::getImageAsSprite(std::string url, std::function<void(const UnityW<UnityEngine::Sprite> sprite)> onLoadComplete) {
    // Check the cache
    const UnityW<UnityEngine::Sprite> sprite = SpriteCache::getInstance().get(url);
    if (sprite) {
        onLoadComplete(sprite);
        return;
    }

    std::thread([url, onLoadComplete] {
        // Send request
        const auto response = WebUtils::Get<WebUtils::DataResponse>(WebUtils::URLOptions(url));
        const std::optional<std::vector<uint8_t>> data = response.responseData;
        if (!response.IsSuccessful()) {
            SpotifySearch::Log.error("Request failed: code = {} url = {}", response.httpCode, url);
            if (data) {
                SpotifySearch::Log.error("DATA SIZE {}", data->size());
                SpotifySearch::Log.error("DATA TXT {}", std::string(data->begin(), data->end()));
            }
            BSML::MainThreadScheduler::Schedule([onLoadComplete] {
                onLoadComplete(nullptr);
            });
            return;
        }

        // Get response data
        if (!data) {
            SpotifySearch::Log.error("Response had no data: url = {}", url);
            BSML::MainThreadScheduler::Schedule([onLoadComplete] {
                onLoadComplete(nullptr);
            });
            return;
        }

        // Save to file
        SpriteCache::getInstance().addToDiskCache(url, *data);

        // Create sprite on the main game thread
        BSML::MainThreadScheduler::Schedule([data, onLoadComplete, url] {
            const UnityW<UnityEngine::Sprite> sprite = BSML::Lite::ArrayToSprite(ArrayW<uint8_t>(*data));
            if (!sprite) {
                SpotifySearch::Log.error("Failed to create sprite from response data! url = {} data length = {}", url, data->size());
                onLoadComplete(nullptr);
                return;
            }
            SpriteCache::getInstance().add(url, sprite);
            onLoadComplete(sprite);
        });
    }).detach();
}

void SpotifySearch::Utils::getCoverImageSprite(const std::string& songHash, std::function<void(const UnityW<UnityEngine::Sprite> sprite)> onLoadComplete) {
    // Check if we have this beatmap loaded locally
    const SongCore::SongLoader::CustomBeatmapLevel* beatmap = SongCore::API::Loading::GetLevelByHash(songHash);
    if (!beatmap) {
        // Download the cover image
        const std::string coverImageUrl = std::format("https://cdn.beatsaver.com/{}.jpg", toLowerCase(songHash));
        getImageAsSprite(coverImageUrl, onLoadComplete);
        return;
    }

    // Get the cover image file path
    const std::string coverImageFilePath = getCoverImageFilePath(*beatmap);
    if (!System::IO::File::Exists(coverImageFilePath)) {
        SpotifySearch::Log.warn("Cover image page does not exist: {}", coverImageFilePath);
        return onLoadComplete(nullptr);
    }

    // Create sprite
    // TODO: Add to sprite cache
    return onLoadComplete(BSML::Lite::FileToSprite(coverImageFilePath));
}

std::string SpotifySearch::Utils::getCoverImageFilePath(const SongCore::SongLoader::CustomBeatmapLevel& beatmap) {
    std::string coverImageFileName;

    // Try V2/V3
    if (std::optional<SongCore::CustomJSONData::CustomLevelInfoSaveDataV2*> saveDataV2 = const_cast<SongCore::SongLoader::CustomBeatmapLevel&>(beatmap).get_standardLevelInfoSaveDataV2()) {
        coverImageFileName = static_cast<std::string>(saveDataV2.value()->get_coverImageFilename());
    }

    // Try V4
    if (coverImageFileName.empty()) {
        if (std::optional<SongCore::CustomJSONData::CustomBeatmapLevelSaveDataV4*> saveDataV4 = const_cast<SongCore::SongLoader::CustomBeatmapLevel&>(beatmap).get_beatmapLevelSaveDataV4()) {
            coverImageFileName = static_cast<std::string>(saveDataV4.value()->__cordl_internal_get_coverImageFilename());
        }
    }

    return System::IO::Path::Combine(beatmap.get_customLevelPath(), coverImageFileName);
}

UnityW<UnityEngine::Sprite> SpotifySearch::Utils::createSimpleSprite() {
    auto tex = UnityEngine::Texture2D::New_ctor(1, 1);
    tex->SetPixel(0, 0, UnityEngine::Color::get_white());
    tex->Apply();

    UnityEngine::Rect rect(0, 0, 1, 1);
    UnityEngine::Vector2 pivot(0.5f, 0.5f);
    return UnityEngine::Sprite::Create(tex, rect, pivot);
}

UnityW<UnityEngine::Sprite> SpotifySearch::Utils::getPlaylistPlaceholderSprite() {
    static constexpr std::string KEY_PLAYLIST_PLACEHOLDER = "playlist-placeholder";
    UnityW<UnityEngine::Sprite> sprite = SpriteCache::getInstance().get(KEY_PLAYLIST_PLACEHOLDER);
    if (!sprite) {
        sprite = BSML::Lite::ArrayToSprite(Assets::_binary_playlist_art_placeholder_png_start);
        SpriteCache::getInstance().add(KEY_PLAYLIST_PLACEHOLDER, sprite);
    }
    return sprite;
}

UnityW<UnityEngine::Sprite> SpotifySearch::Utils::getAlbumPlaceholderSprite() {
    static constexpr std::string KEY_ALBUM_PLACEHOLDER = "album-placeholder";
    UnityW<UnityEngine::Sprite> sprite = SpriteCache::getInstance().get(KEY_ALBUM_PLACEHOLDER);
    if (!sprite) {
        sprite = BSML::Lite::ArrayToSprite(Assets::_binary_album_art_placeholder_png_start);
        SpriteCache::getInstance().add(KEY_ALBUM_PLACEHOLDER, sprite);
    }
    return sprite;
}

custom_types::Helpers::Coroutine SpotifySearch::Utils::getAudioClipFromUrl(const std::string_view url, const std::function<void(UnityW<UnityEngine::AudioClip> audioClip)> onLoadComplete) {
    UnityEngine::Networking::UnityWebRequest* const request = UnityEngine::Networking::UnityWebRequestMultimedia::GetAudioClip(url, UnityEngine::AudioType::MPEG);
    co_yield reinterpret_cast<System::Collections::IEnumerator*>(CRASH_UNLESS(request->SendWebRequest()));
    if (request->GetError() != UnityEngine::Networking::UnityWebRequest::UnityWebRequestError::OK) {
        SpotifySearch::Log.warn("Web request error");
        request->Dispose();
        onLoadComplete(nullptr);
        co_return;
    }

    const UnityW<UnityEngine::AudioClip> audioClip = UnityEngine::Networking::DownloadHandlerAudioClip::GetContent(request);
    request->Dispose();
    onLoadComplete(audioClip);
    co_return;
}

void SpotifySearch::Utils::getAudioClipForSongHash(const std::string_view songHash, const std::function<void(UnityW<UnityEngine::AudioClip> audioClip)> onLoadComplete) {
    const std::string url = BeatSaverUtils::getInstance().getMP3PreviewDownloadUrl(toLowerCase(songHash));
    BSML::SharedCoroutineStarter::get_instance()->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(getAudioClipFromUrl(url, onLoadComplete)));
}

void SpotifySearch::Utils::reloadDataKeepingPosition(UnityW<HMUI::TableView> tableView) {
    // We could use ReloadDataKeepingPosition(), but it snaps to the nearest cell. Setting the scroll
    // position manually, we can maintain our position in between cells.
    const float scrollPosition = tableView->contentTransform->anchoredPosition.y;
    tableView->ReloadData();
    tableView->ScrollToPosition(scrollPosition, false);
}

std::string SpotifySearch::Utils::jsonDocumentToString(const rapidjson::Value& value) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

namespace SpotifySearch::Utils {

SongDetailsCache::MapDifficulty getMapDifficultyFromString(const std::string& text) {
    if (text == "Easy") {
        return SongDetailsCache::MapDifficulty::Easy;
    } else if (text == "Normal") {
        return SongDetailsCache::MapDifficulty::Normal;
    } else if (text == "Hard") {
        return SongDetailsCache::MapDifficulty::Hard;
    } else if (text == "Expert") {
        return SongDetailsCache::MapDifficulty::Expert;
    } else if (text == "Expert+") {
        return SongDetailsCache::MapDifficulty::ExpertPlus;
    }
    throw std::runtime_error(std::format("Unknown map difficulty: {}", text));
}

void removeRaycastFromButtonIcon(UnityW<UnityEngine::UI::Button> button) {
    auto icon = button->get_transform()->Find("Content/Icon");
    auto component = icon->get_gameObject()->GetComponent<HMUI::ImageView*>();
    component->set_raycastTarget(false);
}

}

void SpotifySearch::Utils::goToLevelSelect(const std::string& songHash) {
    auto level = SongCore::API::Loading::GetLevelByHash(songHash);
    BSML::MainThreadScheduler::Schedule([level] {
        UnityW<HMUI::FlowCoordinator> parentFlowCoordinator = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        auto spotifySearchFlowCoordinator = parentFlowCoordinator.cast<SpotifySearch::UI::FlowCoordinators::SpotifySearchFlowCoordinator>();
        spotifySearchFlowCoordinator->_parentFlowCoordinator->DismissFlowCoordinator(spotifySearchFlowCoordinator, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, true);

        auto customLevelsPack = SongCore::API::Loading::GetCustomLevelPack();

        auto category = SelectLevelCategoryViewController::LevelCategory(SelectLevelCategoryViewController::LevelCategory::All);

        // static_assert(sizeof (System::Nullable_1<SelectLevelCategoryViewController::LevelCategory>) == 0x8)
        auto levelCategory = System::Nullable_1<SelectLevelCategoryViewController::LevelCategory>();
        levelCategory.value = category;
        levelCategory.hasValue = true;

        auto state = LevelSelectionFlowCoordinator::State::New_ctor(customLevelsPack, static_cast<GlobalNamespace::BeatmapLevel*>(level));
        state->___levelCategory = levelCategory;

        auto soloFreePlayFlowCoordinator = BSML::Helpers::GetDiContainer()->Resolve<SoloFreePlayFlowCoordinator*>();
        soloFreePlayFlowCoordinator->Setup(state);
        SafePtrUnity<UnityEngine::GameObject> songSelectButton = UnityEngine::GameObject::Find("SoloButton").unsafePtr();
        if (!songSelectButton) {
            songSelectButton = UnityEngine::GameObject::Find("Wrapper/BeatmapWithModifiers/BeatmapSelection/EditButton");
        }
        if (!songSelectButton) {
            SpotifySearch::Log.error("Can't find song select button!");
            return;
        }
        songSelectButton->GetComponent<HMUI::NoTransitionsButton*>()->Press();
        SpotifySearch::Log.info("go to level search, set return = true");
        SpotifySearch::returnToSpotifySearch = true;
    });

}

#include "System/Type.hpp"
#include "UnityEngine/Object.hpp"

namespace SpotifySearch::Debug {

void dumpViewHierarchy(UnityW<UnityEngine::Transform> root, int depth) {
    if (!root) return;

    // Indent by depth
    std::string indent(depth * 2, ' ');

    // Print this GameObject
    auto go = root->get_gameObject();
    SpotifySearch::Log.info("{}- {} ({})", indent.c_str(), go->get_name(), static_cast<void*>(go.ptr()));

    // Print attached components
    auto comps = go->GetComponents<UnityEngine::Component*>();
    for (int i = 0; i < comps->get_Length(); i++) {
        auto comp = comps->_values[i];
        if (!comp) continue;
        auto type = comp->GetType();
        SpotifySearch::Log.info("{}  [Component] {} ({})", indent.c_str(), type->get_FullName(), static_cast<void*>(comp));
    }

    // Recurse into children
    for (int i = 0; i < root->get_childCount(); i++) {
        dumpViewHierarchy(root->GetChild(i), depth + 1);
    }
}

void dumpAllSprites() {
    auto sprites = UnityEngine::Resources::FindObjectsOfTypeAll<UnityEngine::Sprite*>();
    SpotifySearch::Log.info("SPRITE LEN: {}", sprites->get_Length());
    for (int i = 0; i < sprites->get_Length(); i++) {
        auto s = sprites->_values[i];
        if (s && !s->get_name()->IsNullOrEmpty(s->get_name())) {
            SpotifySearch::Log.info("SPRITE NAME: {}", s->get_name());
        }
    }
}

}// namespace SpotifySearch::Debug