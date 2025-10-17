#include <string>

#include "HMUI/Touchable.hpp"
#include "bsml/shared/BSML.hpp"

#include "Assets.hpp"
#include "Log.hpp"
#include "UI/TableViewDataSources/DownloadHistoryTableViewDataSource.hpp"
#include "UI/ViewControllers/DownloadHistoryViewController.hpp"

DEFINE_TYPE(SpotifySearch::UI::ViewControllers, DownloadHistoryViewController);

using namespace SpotifySearch::UI;
using namespace SpotifySearch::UI::ViewControllers;

void DownloadHistoryViewController::DidActivate(const bool isFirstActivation, const bool addedToHierarchy, const bool screenSystemDisabling) {
    SpotifySearch::Log.info("DownloadHistoryViewController::DidActivate {} {} {}", isFirstActivation, addedToHierarchy, screenSystemDisabling);
    if (isFirstActivation) {
        BSML::parse_and_construct(Assets::DownloadHistoryViewController_bsml, this->get_transform(), this);

#if HOT_RELOAD
        fileWatcher->filePath = "/sdcard/DownloadHistoryViewController.bsml";
        fileWatcher->checkInterval = 1.0f;
#endif
    }
}

void DownloadHistoryViewController::PostParse() {
    // Set up the download history list
    auto* downloadHistoryTableViewDataSource = gameObject->GetComponent<DownloadHistoryTableViewDataSource*>();
    if (!downloadHistoryTableViewDataSource) {
        downloadHistoryTableViewDataSource = gameObject->AddComponent<DownloadHistoryTableViewDataSource*>();
    }
    customSongsList_->tableView->SetDataSource(reinterpret_cast<HMUI::TableView::IDataSource*>(downloadHistoryTableViewDataSource), true);
}

void DownloadHistoryViewController::onCustomSongSelected(UnityW<HMUI::TableView> table, int id) {

}
