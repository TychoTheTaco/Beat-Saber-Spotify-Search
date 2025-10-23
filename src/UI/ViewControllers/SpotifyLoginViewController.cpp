#include <string>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

#include <jni.h>
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/typedefs-string.hpp"
#include <bsml/shared/BSML/Components/ButtonIconImage.hpp>
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/Helpers/getters.hpp"

#include "Assets.hpp"
#include "Log.hpp"
#include "main.hpp"
#include "UI/ViewControllers/SpotifyLoginViewController.hpp"
#include "SpriteCache.hpp"

DEFINE_TYPE(SpotifySearch::UI::ViewControllers, SpotifyLoginViewController);

using namespace SpotifySearch::UI;
using namespace SpotifySearch::UI::ViewControllers;

static constexpr const char* const REDIRECT_URI_HOST = "127.0.0.1";
static constexpr const int REDIRECT_URI_PORT = 9999;

void SpotifyLoginViewController::DidActivate(const bool isFirstActivation, const bool addedToHierarchy, const bool screenSystemDisabling) {
    if (isFirstActivation) {
        BSML::parse_and_construct(Assets::SpotifyLoginViewController_bsml, this->get_transform(), this);

#if HOT_RELOAD
        fileWatcher->filePath = "/sdcard/SpotifyLoginViewController.bsml";
        fileWatcher->checkInterval = 1.0f;
#endif
    }
}

void SpotifyLoginViewController::PostParse() {
    // Set the clipboard icon
    static constexpr std::string_view KEY_CLIPBOARD_ICON = "clipboard-icon";
    UnityW<UnityEngine::Sprite> sprite = SpriteCache::getInstance().get(KEY_CLIPBOARD_ICON);
    if (!sprite) {
        sprite = BSML::Lite::ArrayToSprite(Assets::_binary_clipboard_icon_png_start);
        SpriteCache::getInstance().add(KEY_CLIPBOARD_ICON, sprite);
    }
    const std::vector<UnityW<UnityEngine::UI::Button>> pasteButtons = {
        clientIdPasteButton_,
        clientSecretPasteButton_
    };
    for (UnityW<UnityEngine::UI::Button> button : pasteButtons) {
        button->GetComponent<BSML::ButtonIconImage*>()->SetIcon(sprite);
        static constexpr float scale = 1.5f;
        button->get_transform()->Find("Content/Icon")->set_localScale({scale, scale, scale});
    }

    // Set the redirect URI text field
    // Using bolded "Small Colon" because the normal one doesn't render correctly
    redirectUriTextField_->set_text(std::format("https<b>\uFE55</b> //{}<b>\uFE55</b> {}", REDIRECT_URI_HOST, REDIRECT_URI_PORT));
}

bool generateSelfSignedCert(const std::string& certFile, const std::string& keyFile) {
    // Generate RSA key pair
    EVP_PKEY* pkey = EVP_PKEY_new();
    RSA* rsa = RSA_new();
    BIGNUM* bne = BN_new();

    BN_set_word(bne, RSA_F4);
    RSA_generate_key_ex(rsa, 2048, bne, nullptr);
    EVP_PKEY_assign_RSA(pkey, rsa);

    // Create X509 certificate
    X509* x509 = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 31536000L); // 1 year

    X509_set_pubkey(x509, pkey);

    // Set subject and issuer
    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC,
                               (unsigned char*)"US", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               (unsigned char*)"localhost", -1, -1, 0);
    X509_set_issuer_name(x509, name);

    // Sign certificate
    X509_sign(x509, pkey, EVP_sha256());

    // Write certificate to file
    FILE* certFp = fopen(certFile.c_str(), "wb");
    PEM_write_X509(certFp, x509);
    fclose(certFp);

    // Write private key to file
    FILE* keyFp = fopen(keyFile.c_str(), "wb");
    PEM_write_PrivateKey(keyFp, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(keyFp);

    // Cleanup
    X509_free(x509);
    EVP_PKEY_free(pkey);
    BN_free(bne);

    return true;
}

void SpotifyLoginViewController::onAuthorizationCodeReceived(const std::string& authorizationCode) {
    SpotifySearch::Log.info("Authorization code received!");

    // Stop the server
    server_->stop();

    // Authenticate
    try {
        SpotifySearch::spotifyClient->login(clientId_, clientSecret_, std::format("https://{}:{}", REDIRECT_URI_HOST, REDIRECT_URI_PORT), authorizationCode);
        waitingOnBrowserTextView_->set_text("Finished");

        // Re-launch the flow coordinator
        UnityW<HMUI::FlowCoordinator> parentFlowCoordinator = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        auto spotifySearchFlowCoordinator = parentFlowCoordinator.cast<SpotifySearch::UI::FlowCoordinators::SpotifySearchFlowCoordinator>();
        spotifySearchFlowCoordinator->_parentFlowCoordinator->DismissFlowCoordinator(spotifySearchFlowCoordinator, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, true);
        SpotifySearch::spotifySearchFlowCoordinator_ = nullptr;
        SpotifySearch::openSpotifySearchFlowCoordinator();

        return;
    } catch (const std::exception& exception) {
        waitingOnBrowserTextView_->set_fontSize(3);
        waitingOnBrowserTextView_->set_text(std::format("Authentication Failure\n{}", exception.what()));
    }

    loginButton_->set_interactable(true);
    clientIdTextField_->set_interactable(true);
    clientIdPasteButton_->set_interactable(true);
    clientSecretTextField_->set_interactable(true);
    clientSecretPasteButton_->set_interactable(true);
}

void SpotifyLoginViewController::onPasteClientIdButtonClicked() {
    static auto UnityEngine_GUIUtility_get_systemCopyBuffer = il2cpp_utils::resolve_icall<StringW>("UnityEngine.GUIUtility::get_systemCopyBuffer");
    const std::string text = UnityEngine_GUIUtility_get_systemCopyBuffer();
    clientIdTextField_->set_text(text);
}

void SpotifyLoginViewController::onPasteClientSecretButtonClicked() {
    static auto UnityEngine_GUIUtility_get_systemCopyBuffer = il2cpp_utils::resolve_icall<StringW>("UnityEngine.GUIUtility::get_systemCopyBuffer");
    const std::string text = UnityEngine_GUIUtility_get_systemCopyBuffer();
    clientSecretTextField_->set_text(text);
}

void SpotifyLoginViewController::showErrorMessageModal(const std::string& message) {
    errorMessageTextView_->set_text(message);
    errorMessageModal_->Show(true, true, nullptr);
}

void SpotifyLoginViewController::hideErrorMessageModal() {
    errorMessageModal_->Hide(true, nullptr);
}

void SpotifyLoginViewController::onDismissModalButtonClicked() {
    hideErrorMessageModal();
}

void SpotifyLoginViewController::onLoginButtonClicked() {
    // Validate user input
    const std::string clientIdInput = clientIdTextField_->get_text();
    if (clientIdInput.empty()) {
        showErrorMessageModal("Invalid Client ID!");
        return;
    }
    const std::string clientSecretInput = clientSecretTextField_->get_text();
    if (clientSecretInput.empty()) {
        showErrorMessageModal("Invalid Client Secret!");
        return;
    }

    clientId_ = clientIdInput;
    clientSecret_ = clientSecretInput;

    // Disable the button
    loginButton_->set_interactable(false);
    clientIdTextField_->set_interactable(false);
    clientIdPasteButton_->set_interactable(false);
    clientSecretTextField_->set_interactable(false);
    clientSecretPasteButton_->set_interactable(false);
    waitingOnBrowserTextView_->get_gameObject()->set_active(true);
    waitingOnBrowserTextView_->set_fontSize(4);

    // Make sure the server is not already running
    if (isServerStarted_) {
        return;
    }

    // Start HTTPS server
    std::promise<void> isServerReadyPromise;
    std::future<void> isServerReadyFuture = isServerReadyPromise.get_future();
    isServerStarted_ = true;
    std::thread([this, promise = std::move(isServerReadyPromise)]() mutable {
        SpotifySearch::Log.info("Starting HTTPS server");

        // Generate a self-signed TLS certificate
        const std::filesystem::path certPath = SpotifySearch::dataDir_ / "cert.pem";
        const std::filesystem::path keyPath = SpotifySearch::dataDir_ / "key.pem";

        if (!generateSelfSignedCert(certPath, keyPath)) {
            SpotifySearch::Log.error("Failed generating TLS certificate!");
            isServerStarted_ = false;
            return;
        }

        // Create server
        server_ = std::make_unique<httplib::SSLServer>(certPath.c_str(), keyPath.c_str());
        if (!server_->is_valid()) {
            SpotifySearch::Log.error("Invalid server!");
            isServerStarted_ = false;
            return;
        }

        server_->Get("/", [this](const httplib::Request &request, httplib::Response &response) {
            SpotifySearch::Log.debug("Received request: {} params: {}", request.path, request.params.size());

            // Check we got the authorization code
            std::string authorizationCode;
            for (const auto& item : request.params) {
                SpotifySearch::Log.debug("PARAM: {}, {}", item.first, item.second);
                if (item.first == "code") {
                    authorizationCode = item.second;
                    break;
                }
            }
            if (!authorizationCode.empty()) {
                response.status = 200;
                response.set_content("Login successful. You can now close this window.", "text/plain");
                BSML::MainThreadScheduler::Schedule([this, authorizationCode](){
                    onAuthorizationCodeReceived(authorizationCode);
                });
                return;
            }

            response.status = 404;
        });

        // Start listening for requests
        SpotifySearch::Log.info("HTTPS server is listening on {}:{}", REDIRECT_URI_HOST, REDIRECT_URI_PORT);
        promise.set_value();
        server_->listen(REDIRECT_URI_HOST, REDIRECT_URI_PORT);
        SpotifySearch::Log.info("HTTPS server stopped");
        isServerStarted_ = false;
    }).detach();

    // Wait for the HTTPS server to start, the browser seems to freeze if we launch the URL before the server is ready.
    isServerReadyFuture.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Build the query parameters for the request
    const std::string redirectUri = std::format("https://{}:{}", REDIRECT_URI_HOST, REDIRECT_URI_PORT);
    httplib::Params queryParams;
    queryParams.insert({"client_id", clientIdInput});
    queryParams.insert({"response_type", "code"});
    queryParams.insert({"redirect_uri", redirectUri});
    queryParams.insert({"scope", "user-library-read playlist-read-private playlist-read-collaborative"});

    // Open web browser
    const std::string url = httplib::append_query_params("https://accounts.spotify.com/authorize", queryParams);
    try {
        static auto UnityEngine_Application_OpenURL = il2cpp_utils::resolve_icall<void, StringW>("UnityEngine.Application::OpenURL");
        UnityEngine_Application_OpenURL(url);
    } catch (const std::exception& exception) {
        SpotifySearch::Log.error("Failed to open web browser: {}", exception.what());
    }
}
