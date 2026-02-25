#pragma once

#include <string>

namespace spotify {

constexpr const char* const PLAYLIST_ID_LIKED_SONGS = "liked-songs";

struct Playlist {
    std::string id;
    std::string name;
    std::string tracksUrl;
    std::string imageUrl;
    size_t totalItemCount;
};

std::string to_string(const Playlist& playlist);

}// namespace spotify
