#include "GlobalNamespace/SongPreviewPlayer.hpp"
#include "UnityEngine/ScriptableObject.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include <bsml/shared/Helpers/creation.hpp>

#include "Log.hpp"
#include "UI/FlowCoordinators/SpotifySearchFlowCoordinator.hpp"
#include "main.hpp"

DEFINE_TYPE(SpotifySearch::UI::FlowCoordinators, SpotifySearchFlowCoordinator);

using namespace SpotifySearch::UI::FlowCoordinators;

void SpotifySearchFlowCoordinator::DidActivate(bool isFirstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (!isFirstActivation) {
        return;
    }

    // Set title
    SetTitle("Spotify Search", HMUI::ViewController::AnimationType::In);

    // Make sure the back button is visible
    showBackButton = true;

    // Show view controllers
    if (SpotifySearch::spotifyClient->isAuthenticated()) {
        // Initialize view controllers
        mainViewController_ = BSML::Helpers::CreateViewController<ViewControllers::MainViewController*>();
        filterOptionsViewController_ = BSML::Helpers::CreateViewController<ViewControllers::FilterOptionsViewController*>();
        downloadHistoryViewController_ = BSML::Helpers::CreateViewController<ViewControllers::DownloadHistoryViewController*>();

        ProvideInitialViewControllers(mainViewController_, filterOptionsViewController_, downloadHistoryViewController_, nullptr, nullptr);
    } else {
        // Initialize view controllers
        spotifyLoginViewController_ = BSML::Helpers::CreateViewController<ViewControllers::SpotifyLoginViewController*>();

        ProvideInitialViewControllers(spotifyLoginViewController_, nullptr, nullptr, nullptr, nullptr);
    }
}

void SpotifySearchFlowCoordinator::BackButtonWasPressed(HMUI::ViewController* topViewController) {
    SpotifySearch::Log.info("back button pressed, set return = false");
    SpotifySearch::returnToSpotifySearch = false;

    // Stop the song preview if it's playing
    UnityW<GlobalNamespace::SongPreviewPlayer> songPreviewPlayer = BSML::Helpers::GetDiContainer()->Resolve<GlobalNamespace::SongPreviewPlayer*>();
    if (songPreviewPlayer) {
        songPreviewPlayer->CrossfadeToDefault();
    }

    // Dismiss this flow coordinator
    this->_parentFlowCoordinator->DismissFlowCoordinator(this, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
}
