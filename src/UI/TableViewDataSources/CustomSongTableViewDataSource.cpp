#include "UI/TableViewDataSources/CustomSongTableViewDataSource.hpp"
#include "Assets.hpp"
#include "Log.hpp"
#include "UI/TableViewCells/CustomSongTableViewCell.hpp"

#include "HMUI/Touchable.hpp"

DEFINE_TYPE(SpotifySearch::UI, CustomSongTableViewDataSource);

using namespace SpotifySearch::UI;

HMUI::TableCell* CustomSongTableViewDataSource::CellForIdx(HMUI::TableView* tableView, int idx) {
    auto tcd = tableView->DequeueReusableCellForIdentifier("CustomSongListTableCellReuseIdentifier");
    CustomSongTableViewCell* spotifyCell;
    if (!tcd) {
        auto tableCell = UnityEngine::GameObject::New_ctor("SpotifySongListTableCell");
        spotifyCell = tableCell->AddComponent<CustomSongTableViewCell*>();
        spotifyCell->set_interactable(true);

        spotifyCell->set_reuseIdentifier("CustomSongListTableCellReuseIdentifier");
        BSML::parse_and_construct(Assets::CustomSongTableViewCell_bsml, spotifyCell->get_transform(), spotifyCell);

        // Weird hack cause HMUI touchable is not there for some reason, thanks RedBrumbler
        spotifyCell->get_gameObject()->AddComponent<HMUI::Touchable*>();
    } else {
        spotifyCell = tcd->GetComponent<CustomSongTableViewCell*>();
    }

    const SongDetailsCache::Song* const song = customSongs_.at(idx);
    spotifyCell->setSong(song);

    return spotifyCell;
}

int CustomSongTableViewDataSource::NumberOfCells() {
    return customSongs_.size();
}

float CustomSongTableViewDataSource::CellSize() {
    return 12.0f;
}

void CustomSongTableViewDataSource::setSource(const std::vector<const SongDetailsCache::Song*>& source) {
    customSongs_ = source;
}
