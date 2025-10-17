#pragma once

#include "custom-types/shared/macros.hpp"
#include "bsml/shared/BSML.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/UI/Button.hpp"

#if HOT_RELOAD
#include "bsml/shared/BSML/ViewControllers/HotReloadViewController.hpp"
using BaseViewController = BSML::HotReloadViewController;
#else
#include "HMUI/ViewController.hpp"
using BaseViewController = HMUI::ViewController;
#endif

DECLARE_CLASS_CODEGEN_INTERFACES(SpotifySearch::UI::ViewControllers, SettingsViewController, BaseViewController) {

    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool isFirstActivation, bool addedToHierarchy, bool screenSystemDisabling);

    DECLARE_INSTANCE_METHOD(void, PostParse);

    // Spotify Account
    // DECLARE_INSTANCE_FIELD(UnityW<HMUI::ImageView>, profileImageView_);
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, profileTextView_);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, loginOrLogoutButton_);
    DECLARE_INSTANCE_METHOD(void, onLoginOrLogoutButtonClicked);

    // Image Cache
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, cacheSizeTextView_);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, clearCacheButton_);
    DECLARE_INSTANCE_METHOD(void, onClearCacheButtonClicked);

    private:
    void refreshSpotifyAccountStatus();
    void refreshCacheSizeStatus();

    uintmax_t getDirectorySizeInBytes(const std::filesystem::path& path);
    std::string getHumanReadableSize(uintmax_t bytes);

    std::atomic_bool isClearingCache_;
};
