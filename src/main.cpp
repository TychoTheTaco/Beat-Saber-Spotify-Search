#include "HMUI/FlowCoordinator.hpp"
#include "System/Linq/Enumerable.hpp"
#include "UnityEngine/Resources.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/ViewControllers/HotReloadViewController.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "scotland2/shared/modloader.h"
#include <bsml/shared/Helpers/creation.hpp>

#include "UI/FlowCoordinators/SpotifySearchFlowCoordinator.hpp"
#include "main.hpp"
#include "BeatSaverUtils.hpp"
#include "Log.hpp"
#include "SpotifyClient.hpp"
#include "UI/ViewControllers/SettingsViewController.hpp"
#include "Configuration.hpp"

using SpotifySearch::UI::FlowCoordinators::SpotifySearchFlowCoordinator;
using namespace SpotifySearch;

// Called at the early stages of game loading
MOD_EXTERN_FUNC void setup(CModInfo* info) noexcept {
    *info = modInfo.to_c();
    getConfig().Load();

    try {
        std::filesystem::rename("/sdcard/ModData/com.beatgames.beatsaber/logs2/spotify-search.log", "/sdcard/ModData/com.beatgames.beatsaber/logs2/spotify-search.1.log");
    } catch (const std::filesystem::filesystem_error &error) {
        SpotifySearch::Log.info("Failed to rotate log: {}", error.what());
    }

    // Initialize logging. This enables saving logs to disk.
    Paper::Logger::RegisterFileContextId("spotify-search");

    SpotifySearch::Log.info("Version: {}", info->version);

    // Capture fatal logs from logcat to get crash stacks
    std::thread([](){
        SpotifySearch::Log.info("Logcat thread started");

        const char* command = "logcat *:F";

        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
        if (!pipe) {
            SpotifySearch::Log.info("Logcat thread failed!");
            return;
        }

        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            const std::string line(buffer);
            SpotifySearch::Log.info("[LOGCAT] {}", line);
        }
        SpotifySearch::Log.info("Logcat thread stopped!");
    }).detach();

    if (!getConfig().config.HasMember("filter")) {
        rapidjson::Value filterJson;
        filterJson.SetObject();
        getConfig().config.AddMember("filter", filterJson, getConfig().config.GetAllocator());
    }
    if (!getConfig().config["filter"].HasMember("difficulty")) {
        getConfig().config["filter"].AddMember("difficulty", "Normal", getConfig().config.GetAllocator());
    }
    getConfig().Write();
}

MAKE_HOOK_MATCH(
    onDismissFlowCoordinator,
    &HMUI::FlowCoordinator::DismissFlowCoordinator,
    void,
    HMUI::FlowCoordinator* self,
    HMUI::FlowCoordinator* flowCoordinator,
    HMUI::ViewController::AnimationDirection animationDirection,
    System::Action* finishedCallback,
    bool immediately) {
    SpotifySearch::Log.info(
        "onDismissFlowController: return = {} self = {} ({}) fc = {} ({})",
        SpotifySearch::returnToSpotifySearch,
        static_cast<void*>(self),
        self->get_gameObject()->get_name(),
        static_cast<void*>(flowCoordinator),
        flowCoordinator->get_gameObject()->get_name()
    );

    // Call the original function
    onDismissFlowCoordinator(self, flowCoordinator, animationDirection, finishedCallback, true);

    // Return to the Spotify Search flow coordinator
    if (SpotifySearch::returnToSpotifySearch) {
        auto currentFlowCoordinator = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        UnityW<SpotifySearch::UI::FlowCoordinators::SpotifySearchFlowCoordinator> spotifySearchFlowCoordinator = UnityEngine::Resources::FindObjectsOfTypeAll<SpotifySearch::UI::FlowCoordinators::SpotifySearchFlowCoordinator*>()->FirstOrDefault();
        if (spotifySearchFlowCoordinator) {
            currentFlowCoordinator->PresentFlowCoordinator(spotifySearchFlowCoordinator, nullptr, HMUI::ViewController::AnimationDirection::Horizontal, true, false);
        }
    }
};

void SpotifySearch::openSpotifySearchFlowCoordinator() {
    // Create the main flow coordinator
    if (!SpotifySearch::spotifySearchFlowCoordinator_) {
        SpotifySearch::spotifySearchFlowCoordinator_ = BSML::Helpers::CreateFlowCoordinator<SpotifySearch::UI::FlowCoordinators::SpotifySearchFlowCoordinator*>();
    }

    HMUI::FlowCoordinator* parentFlow = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();

    if (parentFlow) {
        parentFlow->PresentFlowCoordinator(SpotifySearch::spotifySearchFlowCoordinator_.ptr(), nullptr, HMUI::ViewController::AnimationDirection::Horizontal, false, false);
    }
}

// Called later on in the game loading - a good time to install function hooks
MOD_EXTERN_FUNC void late_load() noexcept {
    il2cpp_functions::Init();

    // make sure this is after il2cpp_functions::Init()
    BSML::Init();

    // Register custom types
    custom_types::Register::AutoRegister();

    // Register main menu button
    BSML::Register::RegisterMenuButton("Spotify Search", "Search your Spotify songs", []() {
        SpotifySearch::Log.info("Menu button clicked");
        SpotifySearch::openSpotifySearchFlowCoordinator();
    });

    // Register settings page
    BSML::Register::RegisterSettingsMenu<SpotifySearch::UI::ViewControllers::SettingsViewController*>("Spotify Search", false);

    // Install hooks
    INSTALL_HOOK(SpotifySearch::Log, onDismissFlowCoordinator);

    // Set up the data directory
    const std::string dataDirectoryString = getDataDir(modInfo);
    const std::filesystem::path dataDirectory {dataDirectoryString};
    if (!std::filesystem::exists(dataDirectory)) {
        std::filesystem::create_directory(dataDirectory);
    }
    SpotifySearch::dataDir_ = dataDirectory;

    // Initialize the Spotify client
    SpotifySearch::spotifyClient = std::make_shared<spotify::Client>();
    try {
        SpotifySearch::spotifyClient->login(SpotifySearch::dataDir_ / "spotifyAuthToken.json");
    } catch (const std::exception& exception) {
        SpotifySearch::Log.warn("Failed to authenticate: {}", exception.what());
    }

    // Initialize BeatSaver utils
    SpotifySearch::BeatSaverUtils::getInstance().init();
}
