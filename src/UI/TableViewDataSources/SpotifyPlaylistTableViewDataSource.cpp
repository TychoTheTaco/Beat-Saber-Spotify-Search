#include "HMUI/Touchable.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"

#include "Assets.hpp"
#include "Log.hpp"
#include "UI/TableViewCells/SpotifyPlaylistTableViewCell.hpp"
#include "UI/TableViewDataSources/SpotifyPlaylistTableViewDataSource.hpp"
#include "main.hpp"

DEFINE_TYPE(SpotifySearch::UI, SpotifyPlaylistTableViewDataSource);

using namespace SpotifySearch::UI;

HMUI::TableCell* SpotifyPlaylistTableViewDataSource::CellForIdx(HMUI::TableView* tableView, int idx) {
    if (onLoadItemCallback_) {
        onLoadItemCallback_(idx);
    }
    auto tcd = tableView->DequeueReusableCellForIdentifier(SpotifyPlaylistTableViewCell::CELL_REUSE_ID);
    SpotifyPlaylistTableViewCell* spotifyCell;
    if (!tcd) {
        auto tableCell = UnityEngine::GameObject::New_ctor("SpotifyPlaylistTableViewCell");
        spotifyCell = tableCell->AddComponent<SpotifyPlaylistTableViewCell*>();
        spotifyCell->set_interactable(true);

        spotifyCell->set_reuseIdentifier(SpotifyPlaylistTableViewCell::CELL_REUSE_ID);
        BSML::parse_and_construct(Assets::SpotifyPlaylistTableViewCell_bsml, spotifyCell->get_transform(), spotifyCell);

        // Weird hack cause HMUI touchable is not there for some reason, thanks RedBrumbler
        spotifyCell->get_gameObject()->AddComponent<HMUI::Touchable*>();
    } else {
        spotifyCell = tcd->GetComponent<SpotifyPlaylistTableViewCell*>();
    }

    const spotify::Playlist& playlist = playlists_.at(idx);
    spotifyCell->setPlaylist(playlist);

    return spotifyCell;
}

int SpotifyPlaylistTableViewDataSource::NumberOfCells() {
    return playlists_.size();
}

float SpotifyPlaylistTableViewDataSource::CellSize() {
    return 8.0f;
}

void SpotifyPlaylistTableViewDataSource::setOnLoadItemCallback(std::function<void(size_t index)> callback) {
    onLoadItemCallback_ = callback;
}
