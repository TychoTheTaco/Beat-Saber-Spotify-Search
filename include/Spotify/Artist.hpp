#pragma once

#include <string>

namespace spotify {

struct Artist {
    std::string id;
    std::string name;

    auto operator<=>(const Artist&) const = default;
};

}// namespace spotify
