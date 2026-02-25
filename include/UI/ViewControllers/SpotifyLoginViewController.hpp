#pragma once

#include "HMUI/ImageView.hpp"
#include "bsml/shared/BSML/Components/CustomListTableData.hpp"
#include "custom-types/shared/macros.hpp"
#include "song-details/shared/SongDetails.hpp"
#include <HMUI/InputFieldView.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

#include "Spotify/SpotifyClient.hpp"
#include "UI/ModalView.hpp"

#if HOT_RELOAD
#include "bsml/shared/BSML/ViewControllers/HotReloadViewController.hpp"
using BaseViewController = BSML::HotReloadViewController;
#else
#include "HMUI/ViewController.hpp"
using BaseViewController = HMUI::ViewController;
#endif

DECLARE_CLASS_CODEGEN_INTERFACES(SpotifySearch::UI::ViewControllers, SpotifyLoginViewController, BaseViewController) {

    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool isFirstActivation, bool addedToHierarchy, bool screenSystemDisabling);

    DECLARE_INSTANCE_METHOD(void, PostParse);

    // Client ID
    DECLARE_INSTANCE_FIELD(UnityW<HMUI::InputFieldView>, clientIdTextField_);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, clientIdPasteButton_);
    DECLARE_INSTANCE_METHOD(void, onPasteClientIdButtonClicked);

    // Client secrets
    DECLARE_INSTANCE_FIELD(UnityW<HMUI::InputFieldView>, clientSecretTextField_);
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, clientSecretPasteButton_);
    DECLARE_INSTANCE_METHOD(void, onPasteClientSecretButtonClicked);

    // Redirect URI
    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, redirectUriTextField_);

    // PIN
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::VerticalLayoutGroup>, pinInputContainer_);
    DECLARE_INSTANCE_FIELD(UnityW<HMUI::InputFieldView>, pinTextField_);

    DECLARE_INSTANCE_FIELD(UnityW<TMPro::TextMeshProUGUI>, waitingOnBrowserTextView_);

    // Login button
    DECLARE_INSTANCE_FIELD(UnityW<UnityEngine::UI::Button>, loginButton_);
    DECLARE_INSTANCE_METHOD(void, onLoginButtonClicked);

    // Modal
    DECLARE_INSTANCE_FIELD(UnityW<ModalView>, modalView_);

    private:
    std::unique_ptr<httplib::SSLServer> server_;
    std::atomic_bool isServerStarted_ = false;

    std::string clientId_;
    std::string clientSecret_;
    std::string pin_;

    void onAuthorizationCodeReceived(const std::string& authorizationCode);
};
