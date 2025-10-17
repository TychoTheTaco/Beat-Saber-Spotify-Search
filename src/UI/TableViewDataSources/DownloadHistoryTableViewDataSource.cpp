#include "UI/TableViewDataSources/DownloadHistoryTableViewDataSource.hpp"
#include "Assets.hpp"
#include "Log.hpp"
#include "UI/TableViewCells/DownloadHistoryTableViewCell.hpp"

#include "HMUI/Touchable.hpp"

DEFINE_TYPE(SpotifySearch::UI, DownloadHistoryTableViewDataSource);

using namespace SpotifySearch::UI;

HMUI::TableCell* DownloadHistoryTableViewDataSource::CellForIdx(HMUI::TableView* tableView, int idx) {
    auto tcd = tableView->DequeueReusableCellForIdentifier("DownloadHistoryTableViewDataSource");
    DownloadHistoryTableViewCell* spotifyCell;
    if (!tcd) {
        auto tableCell = UnityEngine::GameObject::New_ctor();
        spotifyCell = tableCell->AddComponent<DownloadHistoryTableViewCell*>();
        spotifyCell->set_interactable(true);

        spotifyCell->set_reuseIdentifier("DownloadHistoryTableViewDataSource");
        BSML::parse_and_construct(Assets::DownloadHistoryTableViewCell_bsml, spotifyCell->get_transform(), spotifyCell);
    } else {
        spotifyCell = tcd->GetComponent<DownloadHistoryTableViewCell*>();
    }

    const std::shared_ptr<DownloadHistoryItem> downloadHistoryItem = downloadHistoryItems_.at(idx);

    spotifyCell->setDownloadHistoryItem(downloadHistoryItem);

    return spotifyCell;
}

int DownloadHistoryTableViewDataSource::NumberOfCells() {
    return downloadHistoryItems_.size();
}

float DownloadHistoryTableViewDataSource::CellSize() {
    return 8.0f;
}
