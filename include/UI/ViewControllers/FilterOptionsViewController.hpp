#pragma once

#include "custom-types/shared/macros.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/Components/Settings/DropdownListSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/ListSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/SliderSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/StringSetting.hpp"

#if HOT_RELOAD
#include "bsml/shared/BSML/ViewControllers/HotReloadViewController.hpp"
using BaseViewController = BSML::HotReloadViewController;
#else
#include "HMUI/ViewController.hpp"
using BaseViewController = HMUI::ViewController;
#endif

#include "CustomSongFilter.hpp"
#include "UI/ViewControllers/MainViewController.hpp"

DECLARE_CLASS_CODEGEN_INTERFACES(SpotifySearch::UI::ViewControllers, FilterOptionsViewController, BaseViewController) {

    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemDisabling);

    DECLARE_INSTANCE_METHOD(void, PostParse);

    DECLARE_INSTANCE_FIELD(UnityW<BSML::DropdownListSetting>, difficultyDropdown_);
    BSML_OPTIONS_LIST_OBJECT(dropdownOptionsDifficulties_, "Any", "Easy", "Normal", "Hard", "Expert", "Expert+");
    DECLARE_INSTANCE_FIELD(StringW, selectedDifficultyString_);

    DECLARE_INSTANCE_METHOD(void, onFilterChanged);

    private:
    CustomSongFilter customSongFilter_;
};
