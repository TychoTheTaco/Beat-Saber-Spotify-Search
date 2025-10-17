#pragma once

#include "HMUI/FlowCoordinator.hpp"
#include "HMUI/ViewController.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "custom-types/shared/macros.hpp"

#include "UI/ViewControllers/DownloadHistoryViewController.hpp"
#include "UI/ViewControllers/FilterOptionsViewController.hpp"
#include "UI/ViewControllers/MainViewController.hpp"
#include "UI/ViewControllers/SpotifyLoginViewController.hpp"

DECLARE_CLASS_CODEGEN(SpotifySearch::UI::FlowCoordinators, SpotifySearchFlowCoordinator, HMUI::FlowCoordinator) {

    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::FlowCoordinator::DidActivate, bool isFirstActivation, bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_OVERRIDE_METHOD_MATCH(void, BackButtonWasPressed, &HMUI::FlowCoordinator::BackButtonWasPressed, HMUI::ViewController* topViewController);

    // View Controllers
    DECLARE_INSTANCE_FIELD(UnityW<ViewControllers::MainViewController>, mainViewController_);
    DECLARE_INSTANCE_FIELD(UnityW<ViewControllers::FilterOptionsViewController>, filterOptionsViewController_);
    DECLARE_INSTANCE_FIELD(UnityW<ViewControllers::DownloadHistoryViewController>, downloadHistoryViewController_);
    DECLARE_INSTANCE_FIELD(UnityW<ViewControllers::SpotifyLoginViewController>, spotifyLoginViewController_);
};
