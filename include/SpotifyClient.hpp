#pragma once

#include "web-utils/shared/WebUtils.hpp"

#include "Log.hpp"
#include "Spotify/Track.hpp"
#include "Spotify/User.hpp"
#include "Spotify/Playlist.hpp"
#include "Spotify/Image.hpp"
#include <string>

namespace spotify {

class Client {

    public:
    std::vector<Track> getLikedSongs(size_t offset = 0, size_t limit = MAX_LIMIT);

    std::vector<Track> getPlaylistTracks(std::string_view playlistId, size_t offset = 0, size_t limit = MAX_LIMIT);

    std::vector<Playlist> getPlaylists();
    Playlist getLikedSongsPlaylist();

    bool isAuthenticated();

    bool login(const std::string& clientId, const std::string& clientSecret, const std::string& redirectUri, const std::string& authorizationCode);
    void login(const std::filesystem::path& path);

    void logout();

    User getUser();

    static constexpr size_t MAX_LIMIT = 50;

    private:
    // Base URL for API requests
    static constexpr std::string_view BASE_API_URL{"https://api.spotify.com/v1/"};

    // Credentials needed for authentication
    std::string encodedClientIdAndClientSecret_;
    std::string accessToken_;
    std::string refreshToken_;

    void refreshAccessToken2();

    void getAuth();

    Track getTrackFromJson(const rapidjson::Value& json);
    Playlist getPlaylistFromJson(const rapidjson::Value& json);
    std::vector<Image> getImagesFromJson(const rapidjson::Value& json);
    User getUserFromJson(const rapidjson::Value& json);

    Image getSmallestImage(const std::vector<Image>& images);

    void saveAuthTokensToFile(const std::filesystem::path& path);
    void loadAuthTokensFromFile(const std::filesystem::path& path);
};

}// namespace spotify
