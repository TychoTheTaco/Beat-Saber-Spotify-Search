#pragma once

#include <vector>

#include "Artist.hpp"
#include "Album.hpp"

namespace spotify {

struct Track {
    std::string id;
    std::string name;
    std::vector<Artist> artists;
    Album album;

    auto operator<=>(const Track&) const = default;
};

std::string to_string(const Track& track);

}// namespace spotify
