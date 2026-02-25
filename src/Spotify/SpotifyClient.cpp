#include "Spotify/SpotifyClient.hpp"

#include <chrono>

#include <web-utils/shared/WebUtils.hpp>

#include "Configuration.hpp"
#include "Encryption.hpp"
#include "Log.hpp"
#include "Spotify/Json.hpp"
#include "Spotify/Track.hpp"
#include "ThreadPool.hpp"
#include "Utils.hpp"

using namespace spotify::json;

namespace spotify {

class Profiler {
    public:
    Profiler(std::string_view message) : startTime_(std::chrono::steady_clock::now()), message_(message) {
    }

    ~Profiler() {
        const std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
        const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime_);
        SpotifySearch::Log.info("{} {} ms.", message_, elapsedTime.count());
    }

    private:
    const std::chrono::steady_clock::time_point startTime_;
    const std::string message_;
};

std::filesystem::path Client::getAuthTokenPath() {
    return SpotifySearch::getDataDirectory() / "spotifyAuthToken.json";
}

bool Client::isAuthenticated() const {
    return !encodedClientIdAndClientSecret_.empty() && !accessToken_.empty() && !refreshToken_.empty();
}

void Client::saveAuthTokensToFile(const std::filesystem::path& path, const std::string_view password) {
    // Create JSON document
    rapidjson::Document document;
    document.SetObject();

    // Get allocator
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    // Add data
    document.AddMember("client_id_and_client_secret", encodedClientIdAndClientSecret_, allocator);
    document.AddMember("access_token", accessToken_, allocator);
    document.AddMember("refresh_token", refreshToken_, allocator);

    // Convert JSON document to bytes
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);
    std::vector<uint8_t> data(buffer.GetString(), buffer.GetString() + buffer.GetSize());

    // Encrypt the data if a password is provided
    if (!password.empty()) {
        data = SpotifySearch::encrypt(password, data);
    }

    // Save to file
    std::ofstream outputFileStream(path, std::ios::binary);
    if (!outputFileStream) {
        throw std::runtime_error("Failed to open file for writing!");
    }
    outputFileStream.write(reinterpret_cast<const char*>(data.data()), data.size());
    outputFileStream.close();
}

void spotify::Client::getCredentialsFromJson(const rapidjson::Value& json) {
    encodedClientIdAndClientSecret_ = getString(json, "client_id_and_client_secret");
    accessToken_ = getString(json, "access_token");
    refreshToken_ = getString(json, "refresh_token");
}

void spotify::Client::loadAuthTokensFromFile(const std::filesystem::path& path) {
    // Read the file into a string
    std::ifstream inputFileStream(path);
    if (!inputFileStream) {
        throw std::runtime_error("Failed to open file for reading!");
    }
    std::stringstream buffer;
    buffer << inputFileStream.rdbuf();
    const std::string jsonStr = buffer.str();

    // Parse the string into a Document
    rapidjson::Document document;
    if (document.Parse(jsonStr.c_str()).HasParseError()) {
        throw std::runtime_error("Failed to parse JSON!");
    }

    getCredentialsFromJson(document);
}

void spotify::Client::login(const std::filesystem::path& path) {
    loadAuthTokensFromFile(path);
}

void spotify::Client::loginWithPassword(const std::string_view password) {
    std::ifstream stream(Client::getAuthTokenPath(), std::ios::binary);
    const std::vector<uint8_t> data((std::istreambuf_iterator<char>(stream)), {});
    const std::vector<uint8_t> decryptedData = SpotifySearch::decrypt(password, data);

    // Parse the string into a Document
    rapidjson::Document document;
    if (document.Parse(reinterpret_cast<const char*>(decryptedData.data()), decryptedData.size()).HasParseError()) {
        throw std::runtime_error("Failed to parse JSON!");
    }

    getCredentialsFromJson(document);
}

const rapidjson::Document& getJsonDocumentFromResponse(const WebUtils::JsonResponse& response) {
    const std::optional<rapidjson::Document>& data = response.responseData;
    if (!response.IsSuccessful()) {
        if (data) {
            throw std::runtime_error(std::format("API ERROR: code = {} data = {}", response.httpCode, toString(*data)));
        }
        throw std::runtime_error(std::format("API ERROR: code = {}", response.httpCode));
    }
    if (!data) {
        throw std::runtime_error(std::format("API ERROR: Request successful but no data! code = {}", response.httpCode));
    }
    return *response.responseData;
}

bool spotify::Client::login(const std::string& clientId, const std::string& clientSecret, const std::string& redirectUri, const std::string& authorizationCode) {
    encodedClientIdAndClientSecret_ = SpotifySearch::Utils::encodeBase64(std::format("{}:{}", clientId, clientSecret));

    const auto response = WebUtils::Post<WebUtils::JsonResponse>(
        WebUtils::URLOptions(
            "https://accounts.spotify.com/api/token",
            WebUtils::URLOptions::QueryMap{
                {"grant_type", "authorization_code"},
                {"code", authorizationCode},
                {"redirect_uri", redirectUri},
            },
            WebUtils::URLOptions::HeaderMap{
                {"Content-Type", "application/x-www-form-urlencoded"},
                {"Authorization", std::format("Basic {}", encodedClientIdAndClientSecret_)}}),
        SpotifySearch::Utils::toSpan(""));

    const rapidjson::Document& document = getJsonDocumentFromResponse(response);
    accessToken_ = getString(document, "access_token");
    refreshToken_ = getString(document, "refresh_token");

    return true;
}

void spotify::Client::refreshAccessToken2() {
    SpotifySearch::Log.debug("");

    const std::string data = std::format("grant_type=refresh_token&refresh_token={}", refreshToken_);

    const auto response = WebUtils::Post<WebUtils::JsonResponse>(
        WebUtils::URLOptions(
            "https://accounts.spotify.com/api/token",
            WebUtils::URLOptions::QueryMap{},
            WebUtils::URLOptions::HeaderMap{
                {"Content-Type", "application/x-www-form-urlencoded"},
                {"Authorization", std::format("Basic {}", encodedClientIdAndClientSecret_)}}),
        SpotifySearch::Utils::toSpan(data));
    const rapidjson::Document& document = getJsonDocumentFromResponse(response);
    accessToken_ = document["access_token"].GetString();
}

void spotify::Client::logout() {
    encodedClientIdAndClientSecret_ = "";
    accessToken_ = "";
    refreshToken_ = "";
    std::filesystem::remove(getAuthTokenPath());
}

User spotify::Client::getUser() {
    refreshAccessToken2();

    const auto response = WebUtils::Get<WebUtils::JsonResponse>(
        WebUtils::URLOptions(
            getUrl("me"),
            WebUtils::URLOptions::QueryMap{},
            WebUtils::URLOptions::HeaderMap{
                {"Authorization", std::format("Bearer {}", accessToken_)}}));

    const rapidjson::Document& document = getJsonDocumentFromResponse(response);
    return getUserFromJson(document);
}

std::vector<PlaylistTrack> Client::getPlaylistTracks(const std::string_view playlistId) {
    const Profiler profiler("[getPlaylistTracks] Request completed in");

    // Check cache
    static std::unordered_map<std::string, std::unique_ptr<std::vector<PlaylistTrack>>> cachedValue;
    auto iterator = cachedValue.find(std::string(playlistId));
    if (iterator != cachedValue.end() && iterator->second) {
        SpotifySearch::Log.info("Returning cached value!");
        return *(iterator->second);
    }

    // TODO: Only do this when necessary
    refreshAccessToken2();

    std::vector<PlaylistTrack> tracks = getAllItems<PlaylistTrack>([this, playlistId](const size_t offset, const size_t limit) {
        return apiGetPlaylistTracks(playlistId, offset, limit);
    }, getPlaylistTrackFromJson);

    // Update cached value
    cachedValue[std::string(playlistId)] = std::make_unique<std::vector<PlaylistTrack>>(tracks);

    return tracks;
}

std::vector<Playlist> Client::getPlaylists() {

    static std::unique_ptr<std::vector<Playlist>> cachedValue;
    if (cachedValue) {
        SpotifySearch::Log.info("Returning cached value!");
        return *cachedValue;
    }

    refreshAccessToken2();

    std::vector<Playlist> playlists = getAllItems<Playlist>([this](const size_t offset, const size_t limit) {
        return apiGetPlaylists(offset, limit);
    }, getPlaylistFromJson);

    // Update cached value
    cachedValue = std::make_unique<std::vector<Playlist>>(playlists);

    return playlists;
}

Playlist Client::getLikedSongsPlaylist() {
    refreshAccessToken2();

    Playlist playlist;

    const auto response = WebUtils::Get<WebUtils::JsonResponse>(
        WebUtils::URLOptions(
            getUrl("me/tracks"),
            WebUtils::URLOptions::QueryMap{
                {"offset", std::to_string(0)},
                {"limit", std::to_string(1)}},
            WebUtils::URLOptions::HeaderMap{
                {"Authorization", std::format("Bearer {}", accessToken_)}}));
    const rapidjson::Document& document = getJsonDocumentFromResponse(response);

    playlist.id = PLAYLIST_ID_LIKED_SONGS;
    playlist.name = "Liked Songs";
    playlist.totalItemCount = document["total"].GetUint64();

    return playlist;
}

rapidjson::Document Client::apiGetPlaylists(const size_t offset, const size_t limit) {
    const auto response = WebUtils::Get<WebUtils::JsonResponse>(
        WebUtils::URLOptions(
            getUrl("me/playlists"),
            WebUtils::URLOptions::QueryMap{
                {"offset", std::to_string(offset)},
                {"limit", std::to_string(limit)}},
            WebUtils::URLOptions::HeaderMap{
                {"Authorization", std::format("Bearer {}", accessToken_)}}));
    rapidjson::Document document;
    document.CopyFrom(getJsonDocumentFromResponse(response), document.GetAllocator());
    return document;
}

rapidjson::Document Client::apiGetLikedSongs(const size_t offset, const size_t limit) {
    const auto response = WebUtils::Get<WebUtils::JsonResponse>(
        WebUtils::URLOptions(
            getUrl("me/tracks"),
            WebUtils::URLOptions::QueryMap{
                {"offset", std::to_string(offset)},
                {"limit", std::to_string(limit)}},
            WebUtils::URLOptions::HeaderMap{
                {"Authorization", std::format("Bearer {}", accessToken_)}}));
    rapidjson::Document document;
    document.CopyFrom(getJsonDocumentFromResponse(response), document.GetAllocator());
    return document;
}

rapidjson::Document Client::apiGetPlaylistTracks(const std::string_view playlistId, const size_t offset, const size_t limit) {
    const auto response = WebUtils::Get<WebUtils::JsonResponse>(
        WebUtils::URLOptions(
            getUrl(std::format("playlists/{}/items", playlistId)),
            WebUtils::URLOptions::QueryMap{
                {"offset", std::to_string(offset)},
                {"limit", std::to_string(limit)},
                {"fields", "total,items(added_at,item(id,name,artists,album(images))"},
            },
            WebUtils::URLOptions::HeaderMap{
                {"Authorization", std::format("Bearer {}", accessToken_)}}));
    rapidjson::Document document;
    document.CopyFrom(getJsonDocumentFromResponse(response), document.GetAllocator());
    return document;
}

std::vector<PlaylistTrack> Client::getLikedSongs() {
    const Profiler profiler("[getLikedSongs] Request completed in");

    // Check cache
    static std::unique_ptr<std::vector<PlaylistTrack>> cachedValue;
    if (cachedValue) {
        SpotifySearch::Log.info("Returning cached value!");
        return *cachedValue;
    }

    refreshAccessToken2();

    std::vector<spotify::PlaylistTrack> tracks = getAllItems<PlaylistTrack>([this](const size_t offset, const size_t limit) {
        return apiGetLikedSongs(offset, limit);
    }, getPlaylistTrackFromJson);

    // Update cached value
    cachedValue = std::make_unique<std::vector<PlaylistTrack>>(tracks);

    return tracks;
}

} // namespace spotify
