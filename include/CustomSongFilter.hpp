#pragma once

#include "Utils.hpp"
#include "Configuration.hpp"
#include "Log.hpp"

namespace SpotifySearch {

struct CustomSongFilter {

    CustomSongFilter() {
        try {
            // TODO: Better handling for "Any" difficulty
            const std::string difficultyFilterString = getConfig().config["filter"]["difficulty"].GetString();
            difficulties_.push_back(Utils::getMapDifficultyFromString(difficultyFilterString));
        } catch (const std::exception& exception) {
            SpotifySearch::Log.error("CustomSongFilter constructor exception: {}", exception.what());
        }
    }

    std::vector<SongDetailsCache::MapDifficulty> difficulties_;
    bool includeDownloadedSongs_ = true;
};

}
