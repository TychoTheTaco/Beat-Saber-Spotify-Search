#pragma once

#include <string>

namespace spotify {

struct Album {
    std::string url;

    auto operator<=>(const Album&) const = default;
};

}// namespace spotify
