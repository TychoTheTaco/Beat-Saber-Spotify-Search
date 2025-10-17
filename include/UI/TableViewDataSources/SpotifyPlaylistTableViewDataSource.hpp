#pragma once

#include "bsml/shared/BSML/Components/CustomListTableData.hpp"
#include "custom-types/shared/macros.hpp"
#include "song-details/shared/SongDetails.hpp"

#include "Spotify/Track.hpp"

DECLARE_CLASS_CODEGEN_INTERFACES(SpotifySearch::UI, SpotifyPlaylistTableViewDataSource, UnityEngine::MonoBehaviour, HMUI::TableView::IDataSource*) {

    DECLARE_OVERRIDE_METHOD_MATCH(HMUI::TableCell*, CellForIdx, &HMUI::TableView::IDataSource::CellForIdx, HMUI::TableView * tableView, int idx);
    DECLARE_OVERRIDE_METHOD_MATCH(float, CellSize, &HMUI::TableView::IDataSource::CellSize);
    DECLARE_OVERRIDE_METHOD_MATCH(int, NumberOfCells, &HMUI::TableView::IDataSource::NumberOfCells);

    private:
    std::function<void(size_t index)> onLoadItemCallback_;

    public:
    std::vector<spotify::Playlist> playlists_;
    void setOnLoadItemCallback(std::function<void(size_t index)> callback);
};
