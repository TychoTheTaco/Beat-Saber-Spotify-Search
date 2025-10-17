#include "Spotify/Playlist.hpp"

namespace spotify {

std::string spotify::to_string(const Playlist& playlist) {
    std::string result = "{ id: \"" + playlist.id + "\", name: \"" + playlist.name + "\" }";
    return result;
}

}// namespace spotify
