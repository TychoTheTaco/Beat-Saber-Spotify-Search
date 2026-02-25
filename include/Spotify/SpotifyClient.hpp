#pragma once

#include <string>

#include "Log.hpp"
#include "Spotify/Playlist.hpp"
#include "Spotify/Track.hpp"
#include "Spotify/User.hpp"
#include "Spotify/Utils.hpp"
#include "Spotify/Json.hpp"
#include "ThreadPool.hpp"

namespace spotify {

class Client {

    public:
    static constexpr size_t MAX_CHUNK_SIZE = 50;
    static constexpr size_t LIMIT_ALL = 0;

    std::vector<PlaylistTrack> getLikedSongs();

    std::vector<PlaylistTrack> getPlaylistTracks(std::string_view playlistId);

    std::vector<Playlist> getPlaylists();
    Playlist getLikedSongsPlaylist();

    bool isAuthenticated() const;

    bool login(const std::string& clientId, const std::string& clientSecret, const std::string& redirectUri, const std::string& authorizationCode);
    void login(const std::filesystem::path& path);
    void loginWithPassword(std::string_view password);

    void logout();

    User getUser();

    static std::filesystem::path getAuthTokenPath();

    void saveAuthTokensToFile(const std::filesystem::path& path, std::string_view password = "");

    private:
    // Base URL for API requests
    static constexpr std::string_view BASE_API_URL{"https://api.spotify.com/v1"};

    static constexpr std::string getUrl(std::string_view path) {
        return std::format("{}/{}", BASE_API_URL, path);
    }

    // Credentials needed for authentication
    std::string encodedClientIdAndClientSecret_;
    std::string accessToken_;
    std::string refreshToken_;

    void refreshAccessToken2();

    void getCredentialsFromJson(const rapidjson::Value& json);

    void loadAuthTokensFromFile(const std::filesystem::path& path);

    rapidjson::Document apiGetPlaylists(size_t offset, size_t limit);
    rapidjson::Document apiGetLikedSongs(size_t offset, size_t limit);
    rapidjson::Document apiGetPlaylistTracks(std::string_view playlistId, size_t offset, size_t limit);

    template <typename T>
    std::vector<T> getAllItems(
        const std::function<rapidjson::Document(size_t offset, size_t limit)>& producer,
        const std::function<T(const rapidjson::Value& item)>& parser
    ) {
        // Initial request to determine the total number of items
        const rapidjson::Document& document = producer(0, MAX_CHUNK_SIZE);

        // Update the limit and chunk count according to the total number of tracks.
        const size_t total = document["total"].GetUint64();
        const size_t chunkCount = getChunkCount(total, MAX_CHUNK_SIZE);
        SpotifySearch::Log.info("Total = {} Chunks = {}", total, chunkCount);

        std::vector<T> items = json::getArray<T>(document, "items", parser);

        // We probably won't have enough CPU cores to run each thread truly parallelly, but most of the time is
        // spent waiting on the network request, so creating more threads is still faster overall.
        std::vector<std::future<std::vector<T>>> futures;
        for (size_t i = 1; i < chunkCount; ++i) {
            std::promise<std::vector<T>> promise;
            futures.push_back(promise.get_future());
            std::thread([i, &producer, &parser, promise = std::move(promise)]() mutable {
                try {
                    const rapidjson::Document& document = producer(MAX_CHUNK_SIZE * i, MAX_CHUNK_SIZE);
                    const std::vector<T> localTracks = json::getArray<T>(document, "items", parser);
                    promise.set_value(localTracks);
                } catch (const std::exception& exception) {
                    SpotifySearch::Log.error("Exception in async task: {}", exception.what());
                    promise.set_exception(std::current_exception());
                }
            }).detach();
        }

        // Wait for all the async tasks to finish. We need to ensure this loop finishes before throwing any exceptions.
        std::exception_ptr exceptionPointer = nullptr;
        for (std::future<std::vector<T>>& future : futures) {
            try {
                const std::vector<T> localItems = future.get();
                std::ranges::move(localItems, std::back_inserter(items));
            } catch (const std::exception& exception) {
                exceptionPointer = std::current_exception();
            }
        }

        // All threads have exited now, so it's safe to throw the exception.
        if (exceptionPointer) {
            std::rethrow_exception(exceptionPointer);
        }

        return items;
    }
};

} // namespace spotify
