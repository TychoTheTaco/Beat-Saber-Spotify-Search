#include <random>
#include <string>

#include "GlobalNamespace/LevelCollectionViewController.hpp"
#include "GlobalNamespace/SongPreviewPlayer.hpp"
#include "HMUI/CurvedCanvasSettings.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Sprite.hpp"
#include "beatsaverplusplus/shared/BeatSaver.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/Components/Backgroundable.hpp"
#include "bsml/shared/BSML/Components/HotReloadFileWatcher.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "song-details/shared/SongDetails.hpp"
#include "songcore/shared/SongCore.hpp"
#include "web-utils/shared/WebUtils.hpp"
#include <bsml/shared/BSML/Components/ButtonIconImage.hpp>
#include <bsml/shared/BSML/Animations/AnimationStateUpdater.hpp>

#include "Assets.hpp"
#include "CustomSongFilter.hpp"
#include "HMUI/Touchable.hpp"
#include "Log.hpp"
#include "SpriteCache.hpp"
#include "UI/FlowCoordinators/SpotifySearchFlowCoordinator.hpp"
#include "UI/TableViewDataSources/CustomSongTableViewDataSource.hpp"
#include "UI/TableViewDataSources/DownloadHistoryTableViewDataSource.hpp"
#include "UI/TableViewDataSources/SpotifyPlaylistTableViewDataSource.hpp"
#include "UI/TableViewDataSources/SpotifyTrackTableViewDataSource.hpp"
#include "UI/ViewControllers/MainViewController.hpp"
#include "Utils.hpp"
#include "main.hpp"

DEFINE_TYPE(SpotifySearch::UI::ViewControllers, MainViewController);

using namespace SpotifySearch::UI::ViewControllers;

namespace SpotifySearch::Filter {

static std::vector<std::string> getWords(const std::string& text) {
    std::vector<std::string> words;

    // Keep only alphanumeric characters and space
    std::string cleanText;
    std::copy_if(text.begin(), text.end(),
                 std::back_inserter(cleanText),
                 [](unsigned char c) {
                     return std::isalnum(c) || c == ' ';
                 });

    // Split into words
    for (auto&& word : std::views::split(cleanText, ' ')) {
        if (!word.empty()) {// Ignore empty parts from multiple spaces
            words.emplace_back(word.begin(), word.end());
        }
    }

    return words;
}

SongFilterFunction DEFAULT_SONG_FILTER_FUNCTION = [](const SongDetailsCache::Song* const song, const spotify::Track& track) {
    // Remove songs that don't have at least one word from the Spotify track in the name
    const std::vector<std::string> wordsInTrackName = getWords(track.name);
    const std::vector<std::string> wordsInSongName = getWords(song->songName());

    bool didMatchAtLeastOneWordFromSongName = false;
    for (const std::string& word : wordsInTrackName) {
        if (std::ranges::find(wordsInSongName, word) != wordsInSongName.end()) {
            didMatchAtLeastOneWordFromSongName = true;
        }
    }

    return didMatchAtLeastOneWordFromSongName;
};

SongScoreFunction DEFAULT_SONG_SCORE_FUNCTION = [](const spotify::Track& track, const SongDetailsCache::Song& song) {
    int score = 0;

    // Song name
    if (std::ranges::equal(
            song.songName(), track.name,
            [](unsigned char c1, unsigned char c2) {
                return std::tolower(c1) == std::tolower(c2);
            })) {
        score += 1000;
    } else {
        const std::vector<std::string> wordsInTrackName = getWords(track.name);
        const std::vector<std::string> wordsInSongName = getWords(song.songName());

        // One point for every word in the song name
        for (const std::string& word : wordsInTrackName) {
            if (std::ranges::find(wordsInSongName, word) != wordsInSongName.end()) {
                score += (100.0f * ((float) word.size() / (float) 10));
            }
        }
    }

    // Song artists
    const std::string songArtistLowercase = SpotifySearch::Utils::toLowerCase(song.songAuthorName());
    std::vector<std::string> trackArtists;
    for (const spotify::Artist& artist : track.artists) {
        const std::string artistNameLowercase = SpotifySearch::Utils::toLowerCase(artist.name);
        if (songArtistLowercase.contains(artistNameLowercase)) {
            score += 100;
        }
    }

    // Increase score based on upvotes
    score += std::min(80, static_cast<int>((float) song.upvotes / (float) 10));

    // Decrease score based on downvotes
    score -= std::min(80, static_cast<int>((float) song.downvotes / 5));

    return score;
};

}// namespace SpotifySearch::Filter

void MainViewController::DidActivate(const bool isFirstActivation, const bool addedToHierarchy, const bool screenSystemDisabling) {
    if (isFirstActivation) {
        BSML::parse_and_construct(Assets::MainViewController_bsml, this->get_transform(), this);

#if HOT_RELOAD
        fileWatcher->filePath = "/sdcard/MainViewController.bsml";
        fileWatcher->checkInterval = 1.0f;
#endif
    }

    SpotifySearch::Log.info("main activate, set return = false");
    SpotifySearch::returnToSpotifySearch = false;
}

void MainViewController::onTrackLoadError(const std::string& message) {
    spotifyTrackListLoadingIndicatorContainer_->get_gameObject()->set_active(false);
    spotifyListViewErrorContainer_->get_gameObject()->set_active(true);
    spotifyTrackListErrorMessageTextView_->set_text(message);
    spotifyTrackListView_->get_gameObject()->set_active(false);
    randomTrackButton_->get_gameObject()->set_active(false);
}

void MainViewController::resetListError() {
    spotifyTrackListLoadingIndicatorContainer_->get_gameObject()->set_active(true);
    spotifyListViewStatusContainer_->get_gameObject()->set_active(false);
    spotifyListViewErrorContainer_->get_gameObject()->set_active(false);
}

void MainViewController::reloadSpotifyTrackListView() {
    auto* spotifyTrackTableViewDataSource = gameObject->GetComponent<SpotifyTrackTableViewDataSource*>();
    resetListError();
    spotifyTrackListView_->get_gameObject()->set_active(true);
    randomTrackButton_->get_gameObject()->set_active(false);
    const auto loadMoreSpotifyTracks = [this, spotifyTrackTableViewDataSource](const size_t index) {
        if (isLoadingMoreSpotifyTracks_ || allTracksLoaded_) {
            return;
        }
        isLoadingMoreSpotifyTracks_ = true;
        std::thread([this, spotifyTrackTableViewDataSource]() {
            // Make sure the Spotify client is still valid
            if (!SpotifySearch::spotifyClient) {
                return;
            }

            // Load more tracks
            const size_t offset = spotifyTrackTableViewDataSource->tracks_.size();
            std::vector<spotify::Track> tracks;
            try {
                if (selectedPlaylist_->id == "liked-songs") {
                    tracks = spotifyClient->getLikedSongs(offset, 300);
                } else {
                    tracks = spotifyClient->getPlaylistTracks(selectedPlaylist_->id, offset, 50);
                }
            } catch (const std::exception& exception) {
                const std::string message = exception.what();
                BSML::MainThreadScheduler::Schedule([this, message]() {
                    onTrackLoadError(message);
                });
                isLoadingMoreSpotifyTracks_ = false;
                return;
            }
            if (tracks.size() < 50) {
                allTracksLoaded_ = true;
            }
            BSML::MainThreadScheduler::Schedule([this, spotifyTrackTableViewDataSource, tracks]() {
                spotifyTrackTableViewDataSource->tracks_.insert(spotifyTrackTableViewDataSource->tracks_.end(), tracks.begin(), tracks.end());
                if (spotifyTrackTableViewDataSource->tracks_.empty()) {
                    spotifyListViewStatusContainer_->get_gameObject()->set_active(true);

                    //spotifyTrackListView_->get_gameObject()->set_active(false);
                    spotifyTrackListStatusTextView_->set_text("No tracks");
                }
                Utils::reloadDataKeepingPosition(spotifyTrackListView_->tableView);

                isLoadingMoreSpotifyTracks_ = false;
                spotifyTrackListLoadingIndicatorContainer_->get_gameObject()->set_active(false);
                randomTrackButton_->get_gameObject()->set_active(true);
            });
        }).detach();
    };
    spotifyTrackTableViewDataSource->setOnLoadItemCallback([spotifyTrackTableViewDataSource, loadMoreSpotifyTracks](const size_t index) {
        if (index > spotifyTrackTableViewDataSource->tracks_.size() - 10) {
            loadMoreSpotifyTracks(index);
        }
    });
    loadMoreSpotifyTracks(spotifyTrackTableViewDataSource->NumberOfCells());
}

void MainViewController::PostParse() {
    randomTrackButton_->get_gameObject()->set_active(false);

    if (isLoadingMoreSpotifyPlaylists_ || isLoadingMoreSpotifyTracks_) {
        spotifyTrackListLoadingIndicatorContainer_->get_gameObject()->set_active(true);
    } else {
        spotifyTrackListLoadingIndicatorContainer_->get_gameObject()->set_active(false);
    }

    // todo: if active error
    spotifyListViewErrorContainer_->get_gameObject()->set_active(false);
    spotifyListViewStatusContainer_->get_gameObject()->set_active(false);

    searchResultsListViewErrorContainer_->get_gameObject()->set_active(false);
    if (isSearchInProgress_) {
        searchResultsListLoadingIndicatorContainer_->get_gameObject()->set_active(true);
    } else {
        searchResultsListLoadingIndicatorContainer_->get_gameObject()->set_active(false);
    }

    // Set up the Spotify playlist list
    auto* spotifyPlaylistTableViewDataSource = gameObject->GetComponent<SpotifyPlaylistTableViewDataSource*>();
    if (!spotifyPlaylistTableViewDataSource) {
        spotifyPlaylistTableViewDataSource = gameObject->AddComponent<SpotifyPlaylistTableViewDataSource*>();
        reloadSpotifyPlaylistListView();
    }
    spotifyPlaylistListView_->tableView->SetDataSource(reinterpret_cast<HMUI::TableView::IDataSource*>(spotifyPlaylistTableViewDataSource), true);

    // Set up the Spotify track list
    auto* spotifyTrackTableViewDataSource = gameObject->GetComponent<SpotifyTrackTableViewDataSource*>();
    if (!spotifyTrackTableViewDataSource) {
        spotifyTrackTableViewDataSource = gameObject->AddComponent<SpotifyTrackTableViewDataSource*>();
        if (selectedPlaylist_) {
            reloadSpotifyTrackListView();
        }
    }
    spotifyTrackListView_->tableView->SetDataSource(reinterpret_cast<HMUI::TableView::IDataSource*>(spotifyTrackTableViewDataSource), true);

    // Set up the search results list
    auto* customSongTableViewDataSource = gameObject->GetComponent<CustomSongTableViewDataSource*>();
    if (!customSongTableViewDataSource) {
        customSongTableViewDataSource = gameObject->AddComponent<CustomSongTableViewDataSource*>();
    }
    searchResultsList_->tableView->SetDataSource(reinterpret_cast<HMUI::TableView::IDataSource*>(customSongTableViewDataSource), true);

    if (selectedPlaylist_) {
        // Set title
        spotifyColumnTitleTextView_->set_text(selectedPlaylist_->name);

        // Enable button
        playlistsMenuButton_->get_gameObject()->set_active(true);

        randomTrackButton_->get_gameObject()->set_active(true);

        // Set up the Spotify track list
        //spotifyTrackListView_->get_gameObject()->set_active(true);
        //spotifyPlaylistListView_->get_gameObject()->set_active(false);
    } else {
        // Set title
        spotifyColumnTitleTextView_->set_text("Select a Playlist");

        // Disable back button
        playlistsMenuButton_->get_gameObject()->set_active(false);

        // Set up the Spotify playlist list
        //spotifyTrackListView_->get_gameObject()->set_active(false);
        //spotifyPlaylistListView_->get_gameObject()->set_active(true);
    }

    // Set the icon
    static constexpr std::string_view KEY_DL_ICON = "show-downloaded-songs-icon";
    UnityW<UnityEngine::Sprite> sprite = SpriteCache::getInstance().get(KEY_DL_ICON);
    if (!sprite) {
        sprite = BSML::Lite::ArrayToSprite(Assets::_binary_show_downloaded_songs_png_start);
        SpriteCache::getInstance().add(KEY_DL_ICON, sprite);
    }
    hideDownloadedMapsButton_->GetComponent<BSML::ButtonIconImage*>()->SetIcon(sprite);
    static constexpr float scale = 1.5f;
    hideDownloadedMapsButton_->get_transform()->Find("Content/Icon")->set_localScale({scale, scale, scale});

    setSelectedSongUi(previewSong_);

    Utils::removeRaycastFromButtonIcon(playlistsMenuButton_);
    Utils::removeRaycastFromButtonIcon(randomTrackButton_);
    Utils::removeRaycastFromButtonIcon(showAllByArtistButton_);
    Utils::removeRaycastFromButtonIcon(hideDownloadedMapsButton_);
}

void MainViewController::onRandomTrackButtonClicked() {
    if (!selectedPlaylist_) {
        return;
    }

    auto* spotifyTrackTableViewDataSource = gameObject->GetComponent<SpotifyTrackTableViewDataSource*>();

    const size_t trackCount = spotifyTrackTableViewDataSource->tracks_.size();
    if (trackCount < 2) {
        return;
    }

    std::random_device randomDevice;
    std::mt19937 randomGenerator(randomDevice());

    std::uniform_int_distribution<> dist(0, trackCount - 1);

    static int lastRandomIndex = -1;

    int index = lastRandomIndex;
    while (index == lastRandomIndex) {
        index = dist(randomGenerator);
    }
    lastRandomIndex = index;

    spotifyTrackListView_->tableView->SelectCellWithIdx(index, true);
    spotifyTrackListView_->tableView->ScrollToCellWithIdx(index, HMUI::TableView_ScrollPositionType::Center, true);
}

void MainViewController::onSpotifyTrackListRetryButtonClicked() {
    if (selectedPlaylist_) {
        reloadSpotifyTrackListView();
    } else {
        reloadSpotifyPlaylistListView();
    }
}

void MainViewController::setFilter(const CustomSongFilter& customSongFilter) {
    customSongFilter_ = std::make_unique<CustomSongFilter>(customSongFilter);
    if (selectedTrack_) {
        doSongSearch(*selectedTrack_);
    }
}

void MainViewController::ctor() {
    previewSong_ = nullptr;
    selectedPlaylist_ = nullptr;
    isDownloadThreadRunning_ = false;
    isLoadingMoreSpotifyTracks_ = false;
    isLoadingMoreSpotifyPlaylists_ = false;
    allTracksLoaded_ = false;
    allPlaylistsLoaded_ = false;
    customSongFilter_ = std::make_unique<CustomSongFilter>();
    isShowingAllTracksByArtist_ = false;
    isShowingDownloadedMaps_ = true;
    currentSongFilter_ = SpotifySearch::Filter::DEFAULT_SONG_FILTER_FUNCTION;
    currentSongScore_ = SpotifySearch::Filter::DEFAULT_SONG_SCORE_FUNCTION;
}

void MainViewController::showSpotifyTrackLoadingIndicator() {
    // Hide the list views
    spotifyTrackListView_->get_gameObject()->set_active(false);
    spotifyPlaylistListView_->get_gameObject()->set_active(false);

    // Hide the error message container
    spotifyListViewErrorContainer_->get_gameObject()->set_active(false);

    // Show the loading indicator
    spotifyTrackListLoadingIndicatorContainer_->get_gameObject()->set_active(true);
}

void MainViewController::showSpotifyTrackListView() {
    // Hide the error message container
    spotifyListViewErrorContainer_->get_gameObject()->set_active(false);

    // Hide the loading indicator
    spotifyTrackListLoadingIndicatorContainer_->get_gameObject()->set_active(false);

    // Show the playlist list view
    spotifyTrackListView_->get_gameObject()->set_active(true);
    spotifyPlaylistListView_->get_gameObject()->set_active(false);
}

void MainViewController::showSpotifyPlaylistListView() {
    // Hide the error message container
    spotifyListViewErrorContainer_->get_gameObject()->set_active(false);

    // Hide the loading indicator
    spotifyTrackListLoadingIndicatorContainer_->get_gameObject()->set_active(false);

    // Show the playlist list view
    spotifyTrackListView_->get_gameObject()->set_active(false);
    spotifyPlaylistListView_->get_gameObject()->set_active(true);
}

void MainViewController::onSpotifyTrackLoadingError(const std::string& message) {
    // Hide the loading indicator
    spotifyTrackListLoadingIndicatorContainer_->get_gameObject()->set_active(false);

    // Hide the list views
    spotifyTrackListView_->get_gameObject()->set_active(false);
    spotifyPlaylistListView_->get_gameObject()->set_active(false);

    // Show the error message
    spotifyListViewErrorContainer_->get_gameObject()->set_active(true);
    spotifyTrackListStatusTextView_->set_text(message);
}

void MainViewController::reloadSpotifyPlaylistListView() {
    spotifyListViewStatusContainer_->get_gameObject()->set_active(false);

    showSpotifyTrackLoadingIndicator();
    auto* spotifyPlaylistTableViewDataSource = gameObject->GetComponent<SpotifyPlaylistTableViewDataSource*>();
    const auto loadSpotifyPlaylists = [this, spotifyPlaylistTableViewDataSource](const size_t index) {
        if (isLoadingMoreSpotifyPlaylists_ || allPlaylistsLoaded_) {
            showSpotifyPlaylistListView();
            return;
        }
        isLoadingMoreSpotifyPlaylists_ = true;
        std::thread([this, spotifyPlaylistTableViewDataSource]() {
            // Make sure the Spotify client is still valid
            if (!SpotifySearch::spotifyClient) {
                isLoadingMoreSpotifyPlaylists_ = false;
                // TODO: update vis
                return;
            }

            // Load playlists
            // TODO: Support paging
            std::vector<spotify::Playlist> playlists;
            try {
                playlists = spotifyClient->getPlaylists();
                playlists.insert(playlists.begin(), spotifyClient->getLikedSongsPlaylist());
            } catch (const std::exception& exception) {
                BSML::MainThreadScheduler::Schedule([this]() {
                    onSpotifyTrackLoadingError("Loading Error");
                });
                isLoadingMoreSpotifyPlaylists_ = false;
                return;
            }
            if (playlists.size() < 50) {
                allPlaylistsLoaded_ = true;
            }
            BSML::MainThreadScheduler::Schedule([this, spotifyPlaylistTableViewDataSource, playlists]() {
                showSpotifyPlaylistListView();
                spotifyPlaylistTableViewDataSource->playlists_.insert(spotifyPlaylistTableViewDataSource->playlists_.end(), playlists.begin(), playlists.end());
                Utils::reloadDataKeepingPosition(spotifyPlaylistListView_->tableView);
                isLoadingMoreSpotifyPlaylists_ = false;
            });
        }).detach();
    };
    spotifyPlaylistTableViewDataSource->setOnLoadItemCallback([spotifyPlaylistTableViewDataSource, loadSpotifyPlaylists](const size_t index) {
        if (index > spotifyPlaylistTableViewDataSource->playlists_.size() - 10) {
            loadSpotifyPlaylists(index);
        }
    });
    loadSpotifyPlaylists(spotifyPlaylistTableViewDataSource->NumberOfCells());
}

void MainViewController::onPlaylistsMenuButtonClicked() {
    selectedPlaylist_ = nullptr;

    // Hide tracks list
    spotifyTrackListView_->get_gameObject()->set_active(false);

    // Set title
    spotifyColumnTitleTextView_->set_text("Select a Playlist");

    // Disable button
    playlistsMenuButton_->get_gameObject()->set_active(false);

    // Load playlists
    spotifyPlaylistListView_->tableView->ClearSelection();
    spotifyPlaylistListView_->get_gameObject()->set_active(true);

    // Hide the search results
    searchResultsList_->get_gameObject()->set_active(false);
    searchResultsListLoadingIndicatorContainer_->get_gameObject()->set_active(false);
    searchResultsListViewErrorContainer_->get_gameObject()->set_active(false);

    setSelectedSongUi(nullptr);

    spotifyListViewStatusContainer_->get_gameObject()->set_active(false);

    showAllByArtistButton_->get_gameObject()->set_active(false);
    hideDownloadedMapsButton_->get_gameObject()->set_active(false);

    randomTrackButton_->get_gameObject()->set_active(false);

    spotifyListViewErrorContainer_->get_gameObject()->set_active(false);
}

void MainViewController::onPlaylistSelected(UnityW<HMUI::TableView> table, int id) {
    const auto* const spotifyPlaylistTableViewDataSource = gameObject->GetComponent<SpotifyPlaylistTableViewDataSource*>();
    selectedPlaylist_ = std::make_unique<spotify::Playlist>(spotifyPlaylistTableViewDataSource->playlists_.at(id));

    // Hide playlists list
    spotifyPlaylistListView_->get_gameObject()->set_active(false);

    // Enable button
    playlistsMenuButton_->get_gameObject()->set_active(true);

    // Set title
    spotifyColumnTitleTextView_->set_text(selectedPlaylist_->name);

    // Load tracks list
    allTracksLoaded_ = false;
    auto* spotifyTrackTableViewDataSource = gameObject->GetComponent<SpotifyTrackTableViewDataSource*>();
    if (spotifyTrackTableViewDataSource) {
        spotifyTrackTableViewDataSource->tracks_.clear();
        spotifyTrackListView_->tableView->ReloadData();
        spotifyTrackListView_->tableView->ClearSelection();
    }
    spotifyTrackListView_->get_gameObject()->set_active(true);
    reloadSpotifyTrackListView();
}

void MainViewController::setSelectedSongUi(const SongDetailsCache::Song* const song) {
    if (!song) {
        // Song name
        previewSongNameTextView_->set_text("-");

        // Song author
        previewSongAuthorTextView_->set_text("-");

        // Loading sprite
        const UnityW<UnityEngine::Sprite> placeholderSprite = Utils::getAlbumPlaceholderSprite();
        previewSongImage_->set_sprite(placeholderSprite);

        // Song uploader
        previewSongUploaderTextView_->set_text("-");

        // Song length
        previewSongLengthTextView_->set_text("-");

        // NPS and NJS
        previewSongNPSTextView_->set_text("-");
        previewSongNJSTextView_->set_text("-");

        // Download and Play buttons
        downloadButton_->get_gameObject()->set_active(false);
        playButton_->get_gameObject()->set_active(false);

        UnityW<GlobalNamespace::SongPreviewPlayer> songPreviewPlayer = BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::SongPreviewPlayer*>();
        if (songPreviewPlayer) {
            songPreviewPlayer->CrossfadeToDefault();
        }

        return;
    }

    // Try to load the beatmap. It will be null if it is not loaded locally.
    const std::string songHash = song->hash();
    SongCore::SongLoader::CustomBeatmapLevel* beatmap = SongCore::API::Loading::GetLevelByHash(songHash);

    // Song name
    previewSongNameTextView_->set_text(song->songName());

    // Song author
    previewSongAuthorTextView_->set_text(song->songAuthorName());

    // Loading sprite
    const UnityW<UnityEngine::Sprite> placeholderSprite = Utils::getAlbumPlaceholderSprite();
    previewSongImage_->set_sprite(placeholderSprite);

    // Load cover image
    Utils::getCoverImageSprite(songHash, [this, songHash](const UnityW<UnityEngine::Sprite> sprite) {
        // Check if the selected song has changed
        if (!previewSong_ || previewSong_->hash() != songHash) {
            SpotifySearch::Log.warn("Cancelled sprite update");
            return;
        }

        // Update UI
        if (sprite) {
            previewSongImage_->set_sprite(sprite);
        } else {
            SpotifySearch::Log.warn("Failed loading cover image for song with hash: {}", songHash);
        }
    });

    // Song uploader
    previewSongUploaderTextView_->set_text(previewSong_->uploaderName());

    // Song length
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(previewSong_->songDuration());
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(previewSong_->songDuration() - minutes);
    // Using bolded "Small Colon" because the normal one doesn't render correctly
    previewSongLengthTextView_->set_text(std::format("{:%M<b>\uFE55</b> %S}", previewSong_->songDuration()));

    // NPS and NJS
    uint32_t minNotes = std::numeric_limits<uint32_t>::max();
    uint32_t maxNotes = 0;
    float minNjs = std::numeric_limits<float>::max();
    float maxNjs = 0;
    for (auto& x : *previewSong_) {
        minNotes = std::min(minNotes, x.notes);
        maxNotes = std::max(maxNotes, x.notes);
        minNjs = std::min(minNjs, x.njs);
        maxNjs = std::max(maxNjs, x.njs);
    }
    const float minNotesPerSecond = static_cast<float>(minNotes) / static_cast<float>(previewSong_->songDurationSeconds);
    const float maxNotesPerSecond = static_cast<float>(maxNotes) / static_cast<float>(previewSong_->songDurationSeconds);
    if (std::fabs(maxNotesPerSecond - minNotesPerSecond) <= 1e-6) {
        previewSongNPSTextView_->set_text(std::format("{:.2f}", minNotesPerSecond));
    } else {
        previewSongNPSTextView_->set_text(std::format("{:.2f} - {:.2f}", minNotesPerSecond, maxNotesPerSecond));
    }
    if (fabs(maxNjs - minNjs) <= 1e-6) {
        previewSongNJSTextView_->set_text(std::format("{:.2f}", minNjs));
    } else {
        previewSongNJSTextView_->set_text(std::format("{:.2f} - {:.2f}", minNjs, maxNjs));
    }

    // Download and Play buttons
    if (beatmap) {
        downloadButton_->set_interactable(false);
        downloadButton_->get_gameObject()->set_active(false);
        playButton_->get_gameObject()->set_active(true);
    } else {
        // Check if this song is pending download
        bool isPendingDownload = false;
        HMUI::FlowCoordinator* parentFlow = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        auto flow = static_cast<SpotifySearch::UI::FlowCoordinators::SpotifySearchFlowCoordinator*>(parentFlow);
        auto* downloadHistoryTableViewDataSource = flow->downloadHistoryViewController_->GetComponent<DownloadHistoryTableViewDataSource*>();
        for (const std::shared_ptr<DownloadHistoryItem>& downloadHistoryItem : downloadHistoryTableViewDataSource->downloadHistoryItems_) {
            if (downloadHistoryItem->song == song) {
                isPendingDownload = true;
            }
        }

        if (isPendingDownload) {
            downloadButton_->set_interactable(false);
            playButton_->get_gameObject()->set_active(false);
        } else {
            downloadButton_->set_interactable(true);
            downloadButton_->get_gameObject()->set_active(true);
            playButton_->get_gameObject()->set_active(false);
        }
    }
}

void MainViewController::doSongSearch(const spotify::Track& track) {
    // Show loading indicator
    searchResultsListLoadingIndicatorContainer_->get_gameObject()->set_active(true);
    searchResultsList_->get_gameObject()->set_active(false);
    searchResultsListViewErrorContainer_->get_gameObject()->set_active(false);

    isSearchInProgress_ = true;
    std::thread([this, track]() {
        SongDetailsCache::SongDetails* songDetails = SongDetailsCache::SongDetails::Init().get();
        std::vector<const SongDetailsCache::Song*> songs = songDetails->FindSongs([this](const SongDetailsCache::SongDifficulty& songDifficulty) {
            // Difficulty
            const std::vector<SongDetailsCache::MapDifficulty>& filterMapDifficulties = customSongFilter_->difficulties_;
            if (!filterMapDifficulties.empty()) {
                const SongDetailsCache::MapDifficulty mapDifficulty = songDifficulty.difficulty;
                if (!std::ranges::contains(filterMapDifficulties, mapDifficulty)) {
                    return false;
                }
            }

            // Show downloaded songs
            if (!customSongFilter_->includeDownloadedSongs_) {
                if (SongCore::API::Loading::GetLevelByHash(songDifficulty.song().hash())) {
                    return false;
                }
            }

            return true;
        });

        // Filter songs
        songs.erase(
            std::remove_if(
                songs.begin(), songs.end(),
                [this, track](const SongDetailsCache::Song* song) {
                    return !currentSongFilter_(song, track);
                }),
            songs.end());

        // Sort songs
        std::stable_sort(songs.begin(), songs.end(), [&](const SongDetailsCache::Song* const a, const SongDetailsCache::Song* const b) {
            return currentSongScore_(track, *a) > currentSongScore_(track, *b);
        });

        /*for (int i = 0; i < std::min(10, (int) songs.size()); ++i) {
            SpotifySearch::Log.info("[{}] SCORE: {} SONG: {}", i, currentSongScore_(track, *songs.at(i)), songs.at(i)->songName());
        }*/

        BSML::MainThreadScheduler::Schedule([this, track, songs]() {
            // Check if the user canceled the selection
            if (selectedTrack_ == nullptr) {
                isSearchInProgress_ = false;
                searchResultsListLoadingIndicatorContainer_->get_gameObject()->set_active(false);
                searchResultsList_->get_gameObject()->set_active(true);
                return;
            }

            // Check if the user selected a different track
            if (*selectedTrack_ != track) {
                isSearchInProgress_ = false;
                return;
            }

            // Update the list view
            auto customSongTableViewDataSource = gameObject->GetComponent<CustomSongTableViewDataSource*>();
            customSongTableViewDataSource->setSource(songs);
            searchResultsList_->tableView->SetDataSource(reinterpret_cast<HMUI::TableView::IDataSource*>(customSongTableViewDataSource), true);
            searchResultsList_->tableView->ClearSelection();

            searchResultsListLoadingIndicatorContainer_->get_gameObject()->set_active(false);
            searchResultsList_->get_gameObject()->set_active(true);

            if (songs.empty()) {
                searchResultsListViewErrorContainer_->get_gameObject()->set_active(true);
                searchResultsList_->get_gameObject()->set_active(false);
                searchResultsListStatusTextView_->set_text("No Songs");
            }

            searchResultItems_ = songs;

            isSearchInProgress_ = false;

            // Automatically select the first search result
            if (!searchResultItems_.empty()) {
                searchResultsList_->tableView->SelectCellWithIdx(0, true);
                onSearchResultSelected(searchResultsList_->tableView, 0);
            }
        });
    }).detach();
}

void MainViewController::onTrackSelected(UnityW<HMUI::TableView> table, int id) {
    // Clear UI
    setSelectedSongUi(nullptr);

    // Get the selected track
    const SpotifyTrackTableViewDataSource* const spotifyTrackTableViewDataSource = gameObject->GetComponent<SpotifyTrackTableViewDataSource*>();
    const spotify::Track& track = spotifyTrackTableViewDataSource->tracks_.at(id);
    selectedTrack_ = std::make_unique<const spotify::Track>(track);

    // Start search
    doSongSearch(track);

    // Enable the artist search button
    showAllByArtistButton_->get_gameObject()->set_active(true);
    hideDownloadedMapsButton_->get_gameObject()->set_active(true);

    // Update the artist search button
    if (isShowingAllTracksByArtist_) {
        std::stringstream stringStream;
        stringStream << "Showing all songs by";
        for (const spotify::Artist& artist : selectedTrack_->artists) {
            stringStream << "\n<color=blue>" << artist.name << "</color>";
        }
        UnityW<HMUI::HoverHint> hoverHintComponent = showAllByArtistButton_->GetComponent<HMUI::HoverHint*>();
        hoverHintComponent->set_text(stringStream.str());

        UnityW<HMUI::ImageView> iconImageView = showAllByArtistButton_->get_transform()->Find("Content/Icon")->GetComponent<HMUI::ImageView*>();
        UnityW<HMUI::ImageView> underlineImageView = showAllByArtistButton_->get_transform()->Find("Underline")->GetComponent<HMUI::ImageView*>();
        const UnityEngine::Color color(0.0f, 0.8118f, 1.0f, 1.0f);
        iconImageView->set_color(color);
        underlineImageView->set_color(color);
    }
}

void MainViewController::onSearchResultSelected(UnityW<HMUI::TableView> table, int id) {
    previewSong_ = searchResultItems_.at(id);

    setSelectedSongUi(previewSong_);

    // Try to load the beatmap. It will be null if it is not loaded locally.
    const std::string songHash = previewSong_->hash();
    SongCore::SongLoader::CustomBeatmapLevel* beatmap = SongCore::API::Loading::GetLevelByHash(songHash);

    // Get audio preview
    if (beatmap) {
        auto* levelCollectionViewController = BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::LevelCollectionViewController*>();
        levelCollectionViewController->SongPlayerCrossfadeToLevelAsync(beatmap, System::Threading::CancellationToken::get_None());
    } else {
        Utils::getAudioClipForSongHash(songHash, [this, songHash](UnityW<UnityEngine::AudioClip> audioClip) {
            // Check if the selected song has changed
            if (!previewSong_ || previewSong_->hash() != songHash) {
                SpotifySearch::Log.warn("Cancelled audio update");
                UnityEngine::Object::Destroy(audioClip);
                return;
            }

            const std::function<void()> onFadeOutCallback = [audioClip]() {
                if (!audioClip) {
                    return;
                }
                try {
                    UnityEngine::Object::Destroy(audioClip);
                } catch (...) {
                    SpotifySearch::Log.error("Error destroying clip");
                }
            };

            auto* spp = BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::SongPreviewPlayer*>();
            spp->CrossfadeTo(audioClip, -5, 0, audioClip->length, BSML::MakeDelegate<System::Action*>(onFadeOutCallback));
        });
    }
}

void MainViewController::onPlayButtonClicked() {
    SpotifySearch::Utils::goToLevelSelect(previewSong_->hash());
}

void MainViewController::startDownloadThread() {
    if (isDownloadThreadRunning_) {
        return;
    }
    isDownloadThreadRunning_ = true;
    std::thread([this]() {
        // Process download queue
        while (true) {
            // Get next item in queue
            std::shared_ptr<DownloadHistoryItem> downloadHistoryItem = nullptr;
            {
                std::unique_lock lock(pendingDownloadsMutex_);
                if (pendingDownloads_.empty()) {
                    break;
                }
                downloadHistoryItem = pendingDownloads_.front();
                pendingDownloads_.pop();
            }

            const std::string songHash = downloadHistoryItem->song->hash();

            downloadHistoryItem->onDownloadStarted();

            // Download beatmap info
            auto response = WebUtils::Get<BeatSaver::API::BeatmapResponse>(BeatSaver::API::GetBeatmapByHashURLOptions(songHash));
            if (!response.IsSuccessful()) {
                SpotifySearch::Log.info("Failed to download beatmap info for song with hash {}", songHash);
                downloadHistoryItem->onDownloadStopped(false);
                continue;
            }

            // Get beatmap info
            const std::optional<BeatSaver::Models::Beatmap> beatmap = response.responseData;
            if (!beatmap) {
                SpotifySearch::Log.info("Empty response when downloading beatmap for song with hash {}", songHash);
                downloadHistoryItem->onDownloadStopped(false);
                continue;
            }

            // Create download progress callback
            const std::function<void(float)> onDownloadProgress = [downloadHistoryItem](const float progress) {
                downloadHistoryItem->onDownloadProgress(progress);
            };

            // Download beatmap
            const BeatSaver::API::BeatmapDownloadInfo beatmapDownloadInfo(*beatmap);
            const std::pair<WebUtils::URLOptions, BeatSaver::API::DownloadBeatmapResponse> urlOptionsAndResponse = BeatSaver::API::DownloadBeatmapURLOptionsAndResponse(beatmapDownloadInfo);
            BeatSaver::API::DownloadBeatmapResponse downloadBeatmapResponse = urlOptionsAndResponse.second;
            BeatSaver::API::GetBeatsaverDownloader().GetInto(urlOptionsAndResponse.first, &downloadBeatmapResponse, onDownloadProgress);
            const bool isSuccessful = downloadBeatmapResponse.IsSuccessful();

            downloadHistoryItem->onDownloadStopped(isSuccessful);

            if (!isSuccessful) {
                SpotifySearch::Log.info("Failed to download beatmap for song with hash {}", songHash);
                continue;
            }
        }

        // Refresh songs
        std::shared_future<void> future = SongCore::API::Loading::RefreshSongs(false);
        future.wait();
        isDownloadThreadRunning_ = false;

        // Refreshing songs can take a while, so if we've requested more downloads since then, start up the thread again
        {
            std::lock_guard lock(pendingDownloadsMutex_);
            if (!pendingDownloads_.empty()) {
                startDownloadThread();
            }
        }

        // Update UI
        BSML::MainThreadScheduler::Schedule([this]() {
            setSelectedSongUi(previewSong_);

            UnityW<HMUI::FlowCoordinator> parentFlowCoordinator = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
            auto flow = parentFlowCoordinator.cast<SpotifySearch::UI::FlowCoordinators::SpotifySearchFlowCoordinator>();
            Utils::reloadDataKeepingPosition(flow->downloadHistoryViewController_->customSongsList_->tableView);

            // Update the search results for green
            Utils::reloadDataKeepingPosition(searchResultsList_->tableView);
        });
    }).detach();
}

bool containsContiguous(const std::vector<std::string>& haystack, const std::vector<std::string>& needle) {
    if (needle.empty()) return true;
    if (needle.size() > haystack.size()) return false;

    for (size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (haystack[i + j] != needle[j]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

void MainViewController::onShowAllByArtistButtonClicked() {
    isShowingAllTracksByArtist_ = !isShowingAllTracksByArtist_;

    UnityW<HMUI::HoverHint> hoverHintComponent = showAllByArtistButton_->GetComponent<HMUI::HoverHint*>();
    UnityW<HMUI::ImageView> iconImageView = showAllByArtistButton_->get_transform()->Find("Content/Icon")->GetComponent<HMUI::ImageView*>();
    UnityW<HMUI::ImageView> underlineImageView = showAllByArtistButton_->get_transform()->Find("Underline")->GetComponent<HMUI::ImageView*>();

    if (isShowingAllTracksByArtist_) {

        // Update the artist search button
        std::stringstream stringStream;
        stringStream << "Showing all songs by";
        for (const spotify::Artist& artist : selectedTrack_->artists) {
            stringStream << "\n<color=blue>" << artist.name << "</color>";
        }
        hoverHintComponent->set_text(stringStream.str());
        const UnityEngine::Color color(0.0f, 0.8118f, 1.0f, 1.0f);
        iconImageView->set_color(color);
        underlineImageView->set_color(color);

        currentSongFilter_ = [](const SongDetailsCache::Song* const song, const spotify::Track& track) {
            // Remove songs that don't have at least one word from the Spotify track in the name
            bool didContainArtist = false;
            for (const spotify::Artist& artist : track.artists) {
                const std::vector<std::string> artistTokens = SpotifySearch::Filter::getWords(artist.name);
                if (containsContiguous(SpotifySearch::Filter::getWords(song->songAuthorName()), artistTokens)) {
                    didContainArtist = true;
                    break;
                }
                if (containsContiguous(SpotifySearch::Filter::getWords(song->levelAuthorName()), artistTokens)) {
                    didContainArtist = true;
                    break;
                }

                // todo: handle case where song name contains artist but not correct
                // todo: ex. artist = Luma; song name = "Luma Pools"
                //if (containsContiguous(SpotifySearch::Filter::getWords(song->songName()), artistTokens)) {
                //    didContainArtist = true;
                //    break;
                //}
            }
            return didContainArtist;
        };
        currentSongScore_ = [](const spotify::Track& track, const SongDetailsCache::Song& song) {
            int score = 0;

            // Increase score based on upvotes
            score += song.upvotes;

            // Decrease score based on downvotes
            score -= song.downvotes;

            score += song.upvotes + song.downvotes;

            return score;
        };
    } else {
        // Update the artist search button
        hoverHintComponent->set_text("Show all songs by this artist");
        iconImageView->set_color(UnityEngine::Color::get_white());
        underlineImageView->set_color(UnityEngine::Color::get_white());

        currentSongFilter_ = SpotifySearch::Filter::DEFAULT_SONG_FILTER_FUNCTION;
        currentSongScore_ = SpotifySearch::Filter::DEFAULT_SONG_SCORE_FUNCTION;
    }

    // Hide and show the hover hint to update the text
    auto* controller = UnityEngine::Object::FindObjectOfType<HMUI::HoverHintController*>();
    controller->HideHintInstant(hoverHintComponent);
    controller->SetupAndShowHintPanel(hoverHintComponent);

    doSongSearch(*selectedTrack_);
}

void MainViewController::onHideDownloadedMapsButtonClicked() {
    isShowingDownloadedMaps_ = !isShowingDownloadedMaps_;

    UnityW<HMUI::HoverHint> hoverHintComponent = hideDownloadedMapsButton_->GetComponent<HMUI::HoverHint*>();
    UnityW<HMUI::ImageView> iconImageView = hideDownloadedMapsButton_->get_transform()->Find("Content/Icon")->GetComponent<HMUI::ImageView*>();
    UnityW<HMUI::ImageView> underlineImageView = hideDownloadedMapsButton_->get_transform()->Find("Underline")->GetComponent<HMUI::ImageView*>();

    if (!isShowingDownloadedMaps_) {

        // Update the artist search button
        hoverHintComponent->set_text("Downloaded maps hidden");
        const UnityEngine::Color color(0.0f, 0.8118f, 1.0f, 1.0f);
        iconImageView->set_color(color);
        underlineImageView->set_color(color);
    } else {
        // Update the artist search button
        hoverHintComponent->set_text("Hide downloaded maps");
        iconImageView->set_color(UnityEngine::Color::get_white());
        underlineImageView->set_color(UnityEngine::Color::get_white());
    }

    // Hide and show the hover hint to update the text
    auto* controller = UnityEngine::Object::FindObjectOfType<HMUI::HoverHintController*>();
    controller->HideHintInstant(hoverHintComponent);
    controller->SetupAndShowHintPanel(hoverHintComponent);

    auto csf = std::move(customSongFilter_);
    csf->includeDownloadedSongs_ = isShowingDownloadedMaps_;
    setFilter(*csf);
}

void MainViewController::onDownloadButtonClicked() {
    // Add the selected song to the download queue
    const SongDetailsCache::Song* const song = previewSong_;
    const std::shared_ptr<DownloadHistoryItem> downloadHistoryItem = std::make_shared<DownloadHistoryItem>();
    downloadHistoryItem->song = song;
    {
        std::lock_guard lock(pendingDownloadsMutex_);
        pendingDownloads_.push(downloadHistoryItem);
    }

    // Start download thread if needed
    startDownloadThread();

    // Update UI
    UnityW<HMUI::FlowCoordinator> parentFlowCoordinator = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
    auto flow = parentFlowCoordinator.cast<SpotifySearch::UI::FlowCoordinators::SpotifySearchFlowCoordinator>();
    auto* downloadHistoryTableViewDataSource = flow->downloadHistoryViewController_->GetComponent<DownloadHistoryTableViewDataSource*>();
    downloadHistoryTableViewDataSource->downloadHistoryItems_.push_back(downloadHistoryItem);
    flow->downloadHistoryViewController_->customSongsList_->tableView->SetDataSource(reinterpret_cast<HMUI::TableView::IDataSource*>(downloadHistoryTableViewDataSource), true);

    downloadButton_->set_interactable(false);
}