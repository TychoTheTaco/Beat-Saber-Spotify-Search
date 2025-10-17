#pragma once

#include <string>

namespace spotify {

struct User {
    std::string id;
    std::string displayName;
    std::string imageUrl;
};

}// namespace spotify
