#include <string>

#include "bsml/shared/BSML.hpp"
#include "UnityEngine/UI/ContentSizeFitter.hpp"
#include "HMUI/CurvedCanvasSettings.hpp"
#include "bsml/shared/BSML/Components/Backgroundable.hpp"
#include "UnityEngine/Resources.hpp"
#include "bsml/shared/Helpers/getters.hpp"

#include "Assets.hpp"
#include "UI/ViewControllers/FilterOptionsViewController.hpp"
#include "Log.hpp"
#include "Utils.hpp"
#include "UI/FlowCoordinators/SpotifySearchFlowCoordinator.hpp"
#include "UI/ViewControllers/MainViewController.hpp"
#include "main.hpp"

DEFINE_TYPE(SpotifySearch::UI::ViewControllers, FilterOptionsViewController);

using namespace SpotifySearch::UI::ViewControllers;

void FilterOptionsViewController::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemDisabling) {

    if (firstActivation) {
        BSML::parse_and_construct(Assets::FilterOptionsViewController_bsml, this->get_transform(), this);

#if HOT_RELOAD
        fileWatcher->filePath = "/sdcard/FilterOptionsViewController.bsml";
        fileWatcher->checkInterval = 1.0f;
#endif
    }

}

void FilterOptionsViewController::PostParse() {
    // Allow the dropdown to show all the difficulties at once without needing to scroll
    difficultyDropdown_->dropdown->_numberOfVisibleCells = get_dropdownOptionsDifficulties_().size();
    difficultyDropdown_->dropdown->ReloadData();

    // Check the saved filter
    selectedDifficultyString_ = getConfig().config["filter"]["difficulty"].GetString();
}

void FilterOptionsViewController::onFilterChanged() {
    // There seems to be a bug that causes this method to get called before the member variables are actually updated
    // with the new values. They seem to get set on the next frame, so this is an attempt to delay reading them until
    // then.
    std::thread([this](){
        BSML::MainThreadScheduler::Schedule([this](){
            // Save to config
            getConfig().config["filter"]["difficulty"].SetString(selectedDifficultyString_, getConfig().config.GetAllocator());
            getConfig().Write();

            // Difficulty
            customSongFilter_.difficulties_.clear();
            if (selectedDifficultyString_ != "Any") {
                customSongFilter_.difficulties_.push_back(Utils::getMapDifficultyFromString(selectedDifficultyString_));
            }

            // Notify the main view controller
            UnityW<HMUI::FlowCoordinator> parentFlow = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
            auto flow = parentFlow.cast<SpotifySearch::UI::FlowCoordinators::SpotifySearchFlowCoordinator>();
            auto mainViewController = flow->mainViewController_;
            mainViewController->setFilter(customSongFilter_);
        });
    }).detach();
}
