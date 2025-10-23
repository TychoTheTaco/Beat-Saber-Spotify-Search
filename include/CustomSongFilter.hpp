#pragma once

#include "Utils.hpp"
#include "Configuration.hpp"

namespace SpotifySearch {

struct CustomSongFilter {

    CustomSongFilter() {
        difficulties_.push_back(Utils::getMapDifficultyFromString(getConfig().config["filter"]["difficulty"].GetString()));
    }

    std::vector<SongDetailsCache::MapDifficulty> difficulties_;
    bool includeDownloadedSongs_ = true;
};

}
