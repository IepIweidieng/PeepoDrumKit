﻿#include "chart_editor.h"
#include "core_build_info.h"
#include "chart_editor_undo.h"
#include "chart_editor_widgets.h"
#include "audio/audio_file_formats.h"
#include "chart_editor_i18n.h"

namespace PeepoDrumKit
{
	static constexpr std::string_view UntitledChartFileName = "Untitled Chart.tja";

	static constexpr f32 PresetGuiScaleFactors[] = { 0.5, (2.0f / 3.0f), 0.75f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, };
	static constexpr f32 PresetGuiScaleFactorMin = PresetGuiScaleFactors[0];
	static constexpr f32 PresetGuiScaleFactorMax = PresetGuiScaleFactors[ArrayCount(PresetGuiScaleFactors) - 1];
	static constexpr f32 NextPresetGuiScaleFactor(f32 current, i32 direction)
	{
		i32 closest = 0;
		for (const f32& it : PresetGuiScaleFactors) if (it <= current || ApproxmiatelySame(it, current)) closest = ArrayItToIndexI32(&it, &PresetGuiScaleFactors[0]);
		return ClampRoundGuiScaleFactor(PresetGuiScaleFactors[Clamp(closest + direction, 0, ArrayCountI32(PresetGuiScaleFactors) - 1)]);
	}

	static b8 CanZoomInGuiScale() { return (GuiScaleFactorTarget < PresetGuiScaleFactorMax); }
	static b8 CanZoomOutGuiScale() { return (GuiScaleFactorTarget > PresetGuiScaleFactorMin); }
	static b8 CanZoomResetGuiScale() { return (GuiScaleFactorTarget != 1.0f); }
	static void ZoomInGuiScale() { GuiScaleFactorToSetNextFrame = NextPresetGuiScaleFactor(GuiScaleFactorTarget, +1); }
	static void ZoomOutGuiScale() { GuiScaleFactorToSetNextFrame = NextPresetGuiScaleFactor(GuiScaleFactorTarget, -1); }
	static void ZoomResetGuiScale() { GuiScaleFactorToSetNextFrame = 1.0f; }

	static b8 CanOpenChartDirectoryInFileExplorer(const ChartContext& context)
	{
		return !context.ChartFilePath.empty();
	}

	static void OpenChartDirectoryInFileExplorer(const ChartContext& context)
	{
		const std::string_view chartDirectory = Path::GetDirectoryName(context.ChartFilePath);
		if (!chartDirectory.empty() && Directory::Exists(chartDirectory))
			Shell::OpenInExplorer(chartDirectory);
	}

	static void SetChartDefaultSettingsAndCourses(ChartProject& outChart)
	{
		outChart.ChartCreator = *Settings.General.DefaultCreatorName;

		assert(!outChart.Courses.empty() && "Expected to have initialized the base course first");
		for (auto& course : outChart.Courses)
		{
			course->TempoMap.Tempo.Sorted = { TempoChange(Beat::Zero(), Tempo(160.0f)) };
			course->TempoMap.Signature.Sorted = { TimeSignatureChange(Beat::Zero(), TimeSignature(4, 4)) };
			course->TempoMap.RebuildAccelerationStructure();
		}
	}

	static b8 GlobalLastSetRequestExclusiveDeviceAccessAudioSetting = {};

	ChartEditor::ChartEditor()
	{
		context.Gfx.StartAsyncLoading();
		context.SongVoice = Audio::Engine.AddVoice(Audio::SourceHandle::Invalid, "ChartEditor SongVoice", false, 1.0f, true);
		context.SfxVoicePool.StartAsyncLoadingAndAddVoices();

		context.ResetChartsCompared();
		context.SetSelectedChart(context.Chart.Courses.emplace_back(std::make_unique<ChartCourse>()).get(), BranchType::Normal);
		SetChartDefaultSettingsAndCourses(context.Chart);

		GlobalLastSetRequestExclusiveDeviceAccessAudioSetting = *Settings.Audio.RequestExclusiveDeviceAccess;
		Audio::Engine.SetBackend(*Settings.Audio.RequestExclusiveDeviceAccess ? Audio::Backend::WASAPI_Exclusive : Audio::Backend::WASAPI_Shared);
		Audio::Engine.SetMasterVolume(0.75f);
		if (*Settings.Audio.OpenDeviceOnStartup)
			Audio::Engine.OpenStartStream();

#if 0 // DEBUG: TJA bug hunting
		tjaTestWindow.TabIndexToSelectThisFrame = 2;
		context.Chart.ChartDuration = Time::FromSec(2.0);
		for (auto& course : context.Chart.Courses)
		{
			course->Notes_Normal.Sorted = { Note { Beat::FromTicks(0) }, Note { Beat::FromTicks(48) }, };
			course->TempoMap.Tempo.Sorted = { TempoChange(Beat::Zero(), Tempo(240.0f)) };
			course->TempoMap.Signature.Sorted = { TimeSignatureChange(Beat::Zero(), TimeSignature(7, 8)) };
			course->TempoMap.RebuildAccelerationStructure();
		}
#endif
	}

	ChartEditor::~ChartEditor()
	{
		context.SfxVoicePool.UnloadAllSourcesAndVoices();
	}

	void ChartEditor::DrawFullscreenMenuBar()
	{
		if (Gui::BeginMenuBar())
		{
			std::string nextLanguageToSelect = SelectedGuiLanguage;
			defer {
				if (nextLanguageToSelect != SelectedGuiLanguage)
				{
					SelectedGuiLanguage = nextLanguageToSelect;
					i18n::ReloadLocaleFile(SelectedGuiLanguage.c_str());
				}
			};

			if (Gui::BeginMenu(UI_Str("MENU_FILE")))
			{
				if (Gui::MenuItem(UI_Str("ACT_FILE_NEW_CHART"), ToShortcutString(*Settings.Input.Editor_ChartNew).Data)) { CheckOpenSaveConfirmationPopupThenCall([&] { CreateNewChart(context); }); }
				if (Gui::MenuItem(UI_Str("ACT_FILE_OPEN"), ToShortcutString(*Settings.Input.Editor_ChartOpen).Data)) { CheckOpenSaveConfirmationPopupThenCall([&] { OpenLoadChartFileDialog(context); }); }

				if (Gui::BeginMenu(UI_Str("ACT_FILE_OPEN_RECENT"), !PersistentApp.RecentFiles.SortedPaths.empty()))
				{
					for (size_t i = 0; i < PersistentApp.RecentFiles.SortedPaths.size(); i++)
					{
						const auto& path = PersistentApp.RecentFiles.SortedPaths[i];
						const char shortcutDigit[2] = { (i <= ('9' - '1')) ? static_cast<char>('1' + i) : '\0', '\0' };

						if (Gui::MenuItem(path.c_str(), shortcutDigit))
						{
							if (File::Exists(path))
							{
								CheckOpenSaveConfirmationPopupThenCall([this, pathCopy = std::string(path)] { StartAsyncImportingChartFile(pathCopy); });
							}
							else
							{
								// TODO: Popup asking to remove from list
							}
						}
					}
					Gui::Separator();
					if (Gui::MenuItem(UI_Str("ACT_FILE_CLEAR_ITEMS")))
						PersistentApp.RecentFiles.SortedPaths.clear();
					Gui::EndMenu();
				}

				if (Gui::MenuItem(UI_Str("ACT_FILE_OPEN_CHART_DIRECTORY"), ToShortcutString(*Settings.Input.Editor_ChartOpenDirectory).Data, nullptr, CanOpenChartDirectoryInFileExplorer(context))) { OpenChartDirectoryInFileExplorer(context); }
				Gui::Separator();
				if (Gui::MenuItem(UI_Str("ACT_EDIT_SAVE"), ToShortcutString(*Settings.Input.Editor_ChartSave).Data)) { TrySaveChartOrOpenSaveAsDialog(context); }
				if (Gui::MenuItem(UI_Str("ACT_FILE_SAVE_AS"), ToShortcutString(*Settings.Input.Editor_ChartSaveAs).Data)) { OpenChartSaveAsDialog(context); }
				Gui::Separator();
				if (Gui::MenuItem(UI_Str("ACT_FILE_EXIT"), ToShortcutString(InputBinding(ImGuiKey_F4, ImGuiMod_Alt)).Data))
					tryToCloseApplicationOnNextFrame = true;
				Gui::EndMenu();
			}

			size_t selectedItemCount = 0, selectedNoteCount = 0;
			ForEachSelectedChartItem(*context.ChartSelectedCourse, [&](const ForEachChartItemData& it) { selectedItemCount++; selectedNoteCount += IsNotesList(it.List); });
			const b8 isAnyItemSelected = (selectedItemCount > 0);
			const b8 isAnyNoteSelected = (selectedNoteCount > 0);

			if (Gui::BeginMenu(UI_Str("MENU_EDIT")))
			{
				if (Gui::MenuItem(UI_Str("ACT_EDIT_UNDO"), ToShortcutString(*Settings.Input.Editor_Undo).Data, nullptr, context.Undo.CanUndo())) { context.Undo.Undo(); }
				if (Gui::MenuItem(UI_Str("ACT_EDIT_REDO"), ToShortcutString(*Settings.Input.Editor_Redo).Data, nullptr, context.Undo.CanRedo())) { context.Undo.Redo(); }
				Gui::Separator();
				if (Gui::MenuItem(UI_Str("ACT_EDIT_CUT"), ToShortcutString(*Settings.Input.Timeline_Cut).Data, nullptr, isAnyItemSelected)) { timeline.ExecuteClipboardAction(context, ClipboardAction::Cut); }
				if (Gui::MenuItem(UI_Str("ACT_EDIT_COPY"), ToShortcutString(*Settings.Input.Timeline_Copy).Data, nullptr, isAnyItemSelected)) { timeline.ExecuteClipboardAction(context, ClipboardAction::Copy); }
				if (Gui::MenuItem(UI_Str("ACT_EDIT_PASTE"), ToShortcutString(*Settings.Input.Timeline_Paste).Data, nullptr, true)) { timeline.ExecuteClipboardAction(context, ClipboardAction::Paste); }
				if (Gui::MenuItem(UI_Str("ACT_EDIT_DELETE"), ToShortcutString(*Settings.Input.Timeline_DeleteSelection).Data, nullptr, isAnyItemSelected)) { timeline.ExecuteClipboardAction(context, ClipboardAction::Delete); }
				Gui::Separator();
				if (Gui::MenuItem(UI_Str("TAB_SETTINGS"), ToShortcutString(*Settings.Input.Editor_OpenSettings).Data)) { PersistentApp.LastSession.ShowWindow_Settings = focusSettingsWindowNextFrame = true; }
				Gui::EndMenu();
			}

			if (Gui::BeginMenu(UI_Str("MENU_SELECTION")))
			{
				const b8 setRangeSelectionStartNext = (!timeline.RangeSelection.IsActive || timeline.RangeSelection.HasEnd);
				if (Gui::MenuItem(setRangeSelectionStartNext ? UI_Str("ACT_SELECTION_START_RANGE") : UI_Str("ACT_SELECTION_END_RANGE"), ToShortcutString(*Settings.Input.Timeline_StartEndRangeSelection).Data))
					timeline.StartEndRangeSelectionAtCursor(context);
				Gui::Separator();

				SelectionActionParam param {};
				if (Gui::MenuItem(UI_Str("ACT_SELECTION_SELECT_ALL"), ToShortcutString(*Settings.Input.Timeline_SelectAll).Data))
					timeline.ExecuteSelectionAction(context, SelectionAction::SelectAll, param);
				if (Gui::MenuItem(UI_Str("ACT_SELECTION_CLEAR"), ToShortcutString(*Settings.Input.Timeline_ClearSelection).Data, false, isAnyItemSelected))
					timeline.ExecuteSelectionAction(context, SelectionAction::UnselectAll, param);
				if (Gui::MenuItem(UI_Str("ACT_SELECTION_INVERT"), ToShortcutString(*Settings.Input.Timeline_InvertSelection).Data))
					timeline.ExecuteSelectionAction(context, SelectionAction::InvertAll, param);
				if (Gui::MenuItem(UI_Str("ACT_SELECTION_SELECT_TO_CHART_END"), ToShortcutString(*Settings.Input.Timeline_SelectToChartEnd).Data))
					timeline.ExecuteSelectionAction(context, SelectionAction::SelectToEnd, param.SetBeatCursor(context.GetCursorBeat()));
				if (Gui::MenuItem(UI_Str("ACT_SELECTION_FROM_RANGE"), ToShortcutString(*Settings.Input.Timeline_SelectAllWithinRangeSelection).Data, nullptr, timeline.RangeSelection.IsActiveAndHasEnd()))
					timeline.ExecuteSelectionAction(context, SelectionAction::SelectAllWithinRangeSelection, param);
				Gui::Separator();

				if (Gui::BeginMenu(UI_Str("ACT_SELECTION_REFINE")))
				{
					if (Gui::MenuItem(UI_Str("ACT_SELECTION_SHIFT_LEFT"), ToShortcutString(*Settings.Input.Timeline_ShiftSelectionLeft).Data, nullptr, isAnyItemSelected))
						timeline.ExecuteSelectionAction(context, SelectionAction::PerRowShiftSelected, param.SetShiftDelta(-1));
					if (Gui::MenuItem(UI_Str("ACT_SELECTION_SHIFT_RIGHT"), ToShortcutString(*Settings.Input.Timeline_ShiftSelectionRight).Data, nullptr, isAnyItemSelected))
						timeline.ExecuteSelectionAction(context, SelectionAction::PerRowShiftSelected, param.SetShiftDelta(+1));
					Gui::Separator();

					if (Gui::MenuItem(UI_Str("ACT_SELECTION_ITEM_PATTERN_XO"), ToShortcutString(*Settings.Input.Timeline_SelectItemPattern_xo).Data, nullptr, isAnyItemSelected))
						timeline.ExecuteSelectionAction(context, SelectionAction::PerRowSelectPattern, param.SetPattern("xo"));
					if (Gui::MenuItem(UI_Str("ACT_SELECTION_ITEM_PATTERN_XOO"), ToShortcutString(*Settings.Input.Timeline_SelectItemPattern_xoo).Data, nullptr, isAnyItemSelected))
						timeline.ExecuteSelectionAction(context, SelectionAction::PerRowSelectPattern, param.SetPattern("xoo"));
					if (Gui::MenuItem(UI_Str("ACT_SELECTION_ITEM_PATTERN_XOOO"), ToShortcutString(*Settings.Input.Timeline_SelectItemPattern_xooo).Data, nullptr, isAnyItemSelected))
						timeline.ExecuteSelectionAction(context, SelectionAction::PerRowSelectPattern, param.SetPattern("xooo"));
					if (Gui::MenuItem(UI_Str("ACT_SELECTION_ITEM_PATTERN_XXOO"), ToShortcutString(*Settings.Input.Timeline_SelectItemPattern_xxoo).Data, nullptr, isAnyItemSelected))
						timeline.ExecuteSelectionAction(context, SelectionAction::PerRowSelectPattern, param.SetPattern("xxoo"));
					Gui::Separator();

					WithDefault<CustomSelectionPatternList>& customPatterns = Settings_Mutable.General.CustomSelectionPatterns;
					WithDefault<MultiInputBinding>* customBindings[] =
					{
						&Settings_Mutable.Input.Timeline_SelectItemPattern_CustomA, &Settings_Mutable.Input.Timeline_SelectItemPattern_CustomB, &Settings_Mutable.Input.Timeline_SelectItemPattern_CustomC,
						&Settings_Mutable.Input.Timeline_SelectItemPattern_CustomD, &Settings_Mutable.Input.Timeline_SelectItemPattern_CustomE, &Settings_Mutable.Input.Timeline_SelectItemPattern_CustomF,
					};

					const b8 disableAddNew = (customPatterns->V.size() >= 6);
					if (Gui::Selectable(UI_Str("ACT_SELECTION_ADD_NEW_PATTERN"), false, ImGuiSelectableFlags_NoAutoClosePopups | (disableAddNew ? ImGuiSelectableFlags_Disabled : 0)))
					{
						static constexpr std::string_view defaultPattern = "xoooooo";
						const std::string_view newPattern = defaultPattern.substr(0, ClampTop<size_t>(customPatterns->V.size() + 2, defaultPattern.size()));

						CopyStringViewIntoFixedBuffer(customPatterns->V.emplace_back().Data, newPattern);
						customPatterns.SetHasValueIfNotDefault();
						Settings_Mutable.IsDirty = true;
					}

					for (size_t i = 0; i < customPatterns->V.size(); i++)
					{
						char label[64]; sprintf_s(label, "%s %c", UI_Str("ACT_SELECTION_CUSTOM_PATTERN"), static_cast<char>('A' + i));
						if (Gui::MenuItem(label, (i < ArrayCount(customBindings)) ? ToShortcutString(**customBindings[i]).Data : "", nullptr, isAnyItemSelected && customPatterns->V[i].Data[0] != '\0'))
							timeline.ExecuteSelectionAction(context, SelectionAction::PerRowSelectPattern, param.SetPattern(customPatterns->V[i].Data));
					}

					size_t indexToRemove = customPatterns->V.size();
					for (size_t i = 0; i < customPatterns->V.size(); i++)
					{
						Gui::PushID(&customPatterns->V[i]);
						Gui::PushStyleVar(ImGuiStyleVar_FramePadding, vec2(Gui::GetStyle().FramePadding.x, 0.0f));
						{
							char label[] = { static_cast<char>('A' + i), '\0' };
							Gui::TextUnformatted(label);
							Gui::SameLine();

							static constexpr auto textFilterSelectionPattern = [](ImGuiInputTextCallbackData* data) -> int { return (data->EventChar == 'x' || data->EventChar == 'o') ? 0 : 1; };
							Gui::SetNextItemWidth(Gui::GetContentRegionAvail().x);
							if (Gui::InputTextWithHint("##", UI_Str("PROMPT_SELECTION_CUSTOM_PATTERN_DELETE"), customPatterns->V[i].Data, sizeof(CustomSelectionPattern::Data), ImGuiInputTextFlags_CallbackCharFilter, textFilterSelectionPattern))
							{
								customPatterns.SetHasValueIfNotDefault();
								Settings_Mutable.IsDirty = true;
							}

							b8* inputActiveLastFrame = Gui::GetCurrentWindow()->StateStorage.GetBoolRef(Gui::GetID("CustomPatternInputActive"), false);
							if (!Gui::IsItemActive() && *inputActiveLastFrame && customPatterns->V[i].Data[0] == '\0')
								indexToRemove = i;
							*inputActiveLastFrame = Gui::IsItemActive();
						}
						Gui::PopStyleVar();
						Gui::PopID();
					}

					if (indexToRemove < customPatterns->V.size())
					{
						customPatterns->V.erase(customPatterns->V.begin() + indexToRemove);
						customPatterns.SetHasValueIfNotDefault();
						Settings_Mutable.IsDirty = true;
					}

					Gui::EndMenu();
				}
				Gui::EndMenu();
			}

			if (Gui::BeginMenu(UI_Str("MENU_TRANSFORM")))
			{
				TransformActionParam param {};
				if (Gui::MenuItem(UI_Str("ACT_TRANSFORM_FLIP_NOTE_TYPES"), ToShortcutString(*Settings.Input.Timeline_FlipNoteType).Data, nullptr, isAnyNoteSelected))
					timeline.ExecuteTransformAction(context, TransformAction::FlipNoteType, param);
				if (Gui::MenuItem(UI_Str("ACT_TRANSFORM_TOGGLE_NOTE_SIZES"), ToShortcutString(*Settings.Input.Timeline_ToggleNoteSize).Data, nullptr, isAnyNoteSelected))
					timeline.ExecuteTransformAction(context, TransformAction::ToggleNoteSize, param);

				auto scaleMenu = [&](TransformAction scaleAction, b8 enabled)
				{
					if (Gui::MenuItem(UI_Str("ACT_TRANSFORM_RATIO_2_1"), ToShortcutString(*Settings.Input.Timeline_ExpandItemTime_2To1).Data, nullptr, enabled))
						timeline.ExecuteTransformAction(context, scaleAction, param.SetTimeRatio(2, 1));
					if (Gui::MenuItem(UI_Str("ACT_TRANSFORM_RATIO_3_2"), ToShortcutString(*Settings.Input.Timeline_ExpandItemTime_3To2).Data, nullptr, enabled))
						timeline.ExecuteTransformAction(context, scaleAction, param.SetTimeRatio(3, 2));
					if (Gui::MenuItem(UI_Str("ACT_TRANSFORM_RATIO_4_3"), ToShortcutString(*Settings.Input.Timeline_ExpandItemTime_4To3).Data, nullptr, enabled))
						timeline.ExecuteTransformAction(context, scaleAction, param.SetTimeRatio(4, 3));
					Gui::Separator();

					if (Gui::MenuItem(UI_Str("ACT_TRANSFORM_RATIO_1_2"), ToShortcutString(*Settings.Input.Timeline_CompressItemTime_1To2).Data, nullptr, enabled))
						timeline.ExecuteTransformAction(context, scaleAction, param.SetTimeRatio(1, 2));
					if (Gui::MenuItem(UI_Str("ACT_TRANSFORM_RATIO_2_3"), ToShortcutString(*Settings.Input.Timeline_CompressItemTime_2To3).Data, nullptr, enabled))
						timeline.ExecuteTransformAction(context, scaleAction, param.SetTimeRatio(2, 3));
					if (Gui::MenuItem(UI_Str("ACT_TRANSFORM_RATIO_3_4"), ToShortcutString(*Settings.Input.Timeline_CompressItemTime_3To4).Data, nullptr, enabled))
						timeline.ExecuteTransformAction(context, scaleAction, param.SetTimeRatio(3, 4));
					Gui::Separator();

					b8 willTouchTempo = (*Settings.General.TransformScale_ByTempo || *Settings.General.TransformScale_KeepTimePosition);
					if (Gui::MenuItem(UI_Str("ACT_TRANSFORM_RATIO_0_1"), ToShortcutString(*Settings.Input.Timeline_CompressItemTime_0To1).Data, nullptr, enabled && !willTouchTempo))
						timeline.ExecuteTransformAction(context, scaleAction, param.SetTimeRatio(0, 1));
					if (Gui::MenuItem(willTouchTempo ? UI_Str("ACT_TRANSFORM_RATIO_N1_1_SCROLL") : UI_Str("ACT_TRANSFORM_RATIO_N1_1_TIME"), ToShortcutString(*Settings.Input.Timeline_ReverseItemTime_N1To1).Data, nullptr, enabled))
						timeline.ExecuteTransformAction(context, scaleAction, param.SetTimeRatio(-1, 1));
					Gui::Separator();

					WithDefault<CustomScaleRatioList>& customRatios = Settings_Mutable.General.CustomScaleRatios;
					WithDefault<MultiInputBinding>* customBindings[] =
					{
						&Settings_Mutable.Input.Timeline_ScaleItemTime_CustomA, &Settings_Mutable.Input.Timeline_ScaleItemTime_CustomB, &Settings_Mutable.Input.Timeline_ScaleItemTime_CustomC,
						&Settings_Mutable.Input.Timeline_ScaleItemTime_CustomD, &Settings_Mutable.Input.Timeline_ScaleItemTime_CustomE, &Settings_Mutable.Input.Timeline_ScaleItemTime_CustomF,
					};

					const b8 disableAddNew = (customRatios->size() >= 6);
					if (Gui::Selectable(UI_Str("ACT_TRANSFORM_ADD_NEW_RATIO"), false, ImGuiSelectableFlags_NoAutoClosePopups | (disableAddNew ? ImGuiSelectableFlags_Disabled : 0)))
					{
						static constexpr ivec2 defaultRatio = { 3, 1 };
						const ivec2 newRatio = { defaultRatio[0] + static_cast<i32>(customRatios->size()), defaultRatio[1] };

						customRatios->emplace_back(newRatio);
						customRatios.SetHasValueIfNotDefault();
						Settings_Mutable.IsDirty = true;
					}

					for (size_t i = 0; i < customRatios->size(); i++)
					{
						b8 canScale = (!willTouchTempo || ((*customRatios)[i].TimeRatio[0] != 0)) && ((*customRatios)[i].TimeRatio[1] != 0);
						char label[64]; sprintf_s(label, "%s %c", UI_Str("ACT_TRANSFORM_CUSTOM_RATIO"), static_cast<char>('A' + i));
						if (Gui::MenuItem(label, (i < ArrayCount(customBindings)) ? ToShortcutString(**customBindings[i]).Data : "", nullptr, enabled && canScale))
							timeline.ExecuteTransformAction(context, scaleAction, param.SetTimeRatio((*customRatios)[i].TimeRatio));
					}

					size_t indexToRemove = customRatios->size();
					for (size_t i = 0; i < customRatios->size(); i++)
					{
						auto& ratio = (*customRatios)[i];
						Gui::PushID(&ratio);
						Gui::PushStyleVar(ImGuiStyleVar_FramePadding, vec2(Gui::GetStyle().FramePadding.x, 0.0f));
						{
							char label[] = { static_cast<char>('A' + i), '\0' };
							Gui::TextUnformatted(label);
							Gui::SameLine();

							Gui::SetNextItemWidth(Gui::GetContentRegionAvail().x);

							Gui::BeginGroup();
							if (GuiInputFraction("##Ratio", &ratio.TimeRatio, std::nullopt, 1, 4,
								(*customRatios)[i].TimeRatio == ivec2{ 0, 0 } ? PtrArg(Gui::GetColorU32(ImGuiCol_TextDisabled))
									: !IsTimeSignatureSupported({ ratio.TimeRatio[0], ratio.TimeRatio[1] }) ? &InputTextWarningTextColor
									: nullptr,
								" : ")
								) {
								customRatios.SetHasValueIfNotDefault();
								Settings_Mutable.IsDirty = true;
							}
							Gui::EndGroup();
							if (!(Gui::IsItemActive() || Gui::IsItemFocused()) && (*customRatios)[i].TimeRatio == ivec2{ 0, 0 })
								indexToRemove = i;
						}
						Gui::PopStyleVar();
						Gui::PopID();
					}

					if (indexToRemove < customRatios->size()) {
						customRatios->erase(customRatios->begin() + indexToRemove);
						customRatios.SetHasValueIfNotDefault();
						Settings_Mutable.IsDirty = true;
					}
					if (!customRatios->empty())
						Gui::MenuItem(UI_Str("INFO_TRANSFORM_CUSTOM_RATIO_DELETE"), nullptr, false, false);

					for (const auto& [pSetting, label] : {
						std::tuple{ &UserSettingsData::GeneralData::TransformScale_ByTempo, UI_Str("ACT_TRANSFORM_SCALE_BY_TEMPO") },
						std::tuple{ &UserSettingsData::GeneralData::TransformScale_KeepTimePosition, UI_Str("ACT_TRANSFORM_SCALE_KEEP_TIME_POSITION") },
						std::tuple{ &UserSettingsData::GeneralData::TransformScale_KeepTimeSignature, UI_Str("ACT_TRANSFORM_SCALE_KEEP_TIME_SIGNATURE") },
						std::tuple{ &UserSettingsData::GeneralData::TransformScale_KeepItemDuration, UI_Str("ACT_TRANSFORM_SCALE_KEEP_ITEM_DURATION") },
						}) {
						if (b8 v = *(Settings.General.*pSetting); Gui::Checkbox(label, &v)) {
							(Settings_Mutable.General.*pSetting).Value = v;
							(Settings_Mutable.General.*pSetting).SetHasValueIfNotDefault();
							Settings_Mutable.IsDirty = true;
						}
					}
				};

				if (Gui::BeginMenu(UI_Str("ACT_TRANSFORM_SCALE_ITEMS"))) {
					scaleMenu(TransformAction::ScaleItemTime, isAnyItemSelected);
					Gui::EndMenu();
				}

				if (Gui::BeginMenu(UI_Str("ACT_TRANSFORM_SCALE_RANGE"))) {
					scaleMenu(TransformAction::ScaleRangeTime, timeline.RangeSelection.IsActiveAndHasEnd());
					Gui::EndMenu();
				}

				Gui::EndMenu();
			}

			if (Gui::BeginMenu(UI_Str("MENU_WINDOW")))
			{
				if (b8 v = (ApplicationHost::GlobalState.SwapInterval != 0); Gui::MenuItem(UI_Str("ACT_WINDOW_TOGGLE_VSYNC"), ToShortcutString(*Settings.Input.Editor_ToggleVSync).Data, &v))
					ApplicationHost::GlobalState.SwapInterval = v ? 1 : 0;

				if (Gui::MenuItem(UI_Str("ACT_WINDOW_TOGGLE_FULLSCREEN"), ToShortcutString(*Settings.Input.Editor_ToggleFullscreen).Data, ApplicationHost::GlobalState.IsBorderlessFullscreen))
					ApplicationHost::GlobalState.SetBorderlessFullscreenNextFrame = !ApplicationHost::GlobalState.IsBorderlessFullscreen;

				if (Gui::BeginMenu(UI_Str("ACT_WINDOW_SIZE")))
				{
					const b8 allowResize = !ApplicationHost::GlobalState.IsBorderlessFullscreen;
					const ivec2 currentResolution = ApplicationHost::GlobalState.WindowSize;

					struct NamedResolution { ivec2 Resolution; cstr Name; };
					static constexpr NamedResolution presetResolutions[] =
					{
						{ { 1280,  720 }, "HD" },
						{ { 1366,  768 }, "FWXGA" },
						{ { 1600,  900 }, "HD+" },
						{ { 1920, 1080 }, "FHD" },
						{ { 2560, 1440 }, "QHD" },
					};

					char labelBuffer[128];
					for (auto[resolution, name] : presetResolutions)
					{
						sprintf_s(labelBuffer, "%s %dx%d", UI_Str("ACT_WINDOW_RESIZE_TO"), resolution.x, resolution.y);
						if (Gui::MenuItem(labelBuffer, name, (resolution == currentResolution), allowResize))
							ApplicationHost::GlobalState.SetWindowSizeNextFrame = resolution;
					}

					Gui::Separator();
					sprintf_s(labelBuffer, "%s: %dx%d", UI_Str("INFO_WINDOW_CURRENT_SIZE"), currentResolution.x, currentResolution.y);
					Gui::MenuItem(labelBuffer, nullptr, nullptr, false);

					Gui::EndMenu();
				}

				if (Gui::BeginMenu(UI_Str("ACT_WINDOW_DPI_SCALE")))
				{
					const f32 guiScaleFactorToSetNextFrame = GuiScaleFactorToSetNextFrame;
					if (Gui::MenuItem(UI_Str("ACT_WINDOW_DPI_SCALE_ZOOM_IN"), ToShortcutString(*Settings.Input.Editor_IncreaseGuiScale).Data, nullptr, CanZoomInGuiScale())) ZoomInGuiScale();
					if (Gui::MenuItem(UI_Str("ACT_WINDOW_DPI_SCALE_ZOOM_OUT"), ToShortcutString(*Settings.Input.Editor_DecreaseGuiScale).Data, nullptr, CanZoomOutGuiScale())) ZoomOutGuiScale();
					if (Gui::MenuItem(UI_Str("ACT_WINDOW_DPI_SCALE_RESET_ZOOM"), ToShortcutString(*Settings.Input.Editor_ResetGuiScale).Data, nullptr, CanZoomResetGuiScale())) ZoomResetGuiScale();

					if (guiScaleFactorToSetNextFrame != GuiScaleFactorToSetNextFrame) { if (!zoomPopup.IsOpen) zoomPopup.Open(); zoomPopup.OnChange(); }

					Gui::Separator();
					char labelBuffer[128]; sprintf_s(labelBuffer, "%s: %g%%", UI_Str("INFO_WINDOW_DPI_SCALE_CURRENT"), ToPercent(GuiScaleFactorTarget));
					Gui::MenuItem(labelBuffer, nullptr, nullptr, false);

					Gui::EndMenu();
				}

				Gui::EndMenu();
			}

			if (Gui::BeginMenu(UI_Str("MENU_LANGUAGE")))
			{
				for (const auto& it : i18n::LocaleEntries)
				{
					// This should not be localized, just display as is
					std::string buffer = it.name;
					buffer += " (";
					buffer += it.id;
					buffer += ")";

					if (Gui::MenuItem(buffer.c_str(), 0, SelectedGuiLanguage == it.id))
						nextLanguageToSelect = it.id;
				}
				Gui::Separator();
				if (Gui::MenuItem("Export Builtin Locale Files"))
					i18n::ExportBuiltinLocaleFiles();
				Gui::EndMenu();
			}

			if ((PEEPO_DEBUG || PersistentApp.LastSession.ShowWindow_TestMenu) && Gui::BeginMenu(UI_Str("MENU_TEST")))
			{
				Gui::MenuItem(UI_Str("ACT_TEST_SHOW_AUDIO_TEST"), "(Debug)", &PersistentApp.LastSession.ShowWindow_AudioTest);
				Gui::MenuItem(UI_Str("ACT_TEST_SHOW_TJA_IMPORT_TEST"), "(Debug)", &PersistentApp.LastSession.ShowWindow_TJAImportTest);
				Gui::MenuItem(UI_Str("ACT_TEST_SHOW_TJA_EXPORT_VIEW"), "(Debug)", &PersistentApp.LastSession.ShowWindow_TJAExportTest);
#if !defined(IMGUI_DISABLE_DEMO_WINDOWS)
				Gui::Separator();
				Gui::MenuItem(UI_Str("ACT_TEST_SHOW_IMGUI_DEMO"), " ", &PersistentApp.LastSession.ShowWindow_ImGuiDemo);
				Gui::MenuItem(UI_Str("ACT_TEST_SHOW_IMGUI_STYLE_EDITOR"), " ", &PersistentApp.LastSession.ShowWindow_ImGuiStyleEditor);
				Gui::Separator();
				if (Gui::MenuItem(UI_Str("ACT_TEST_RESET_STYLE_COLORS"), " "))
					GuiStyleColorPeepoDrumKit();
#endif

#if PEEPO_DEBUG
				if (Gui::BeginMenu("Embedded Icons Test"))
				{
					for (size_t i = 0; i < GetEmbeddedIconsList().Count; i++)
					{
						if (auto icon = GetEmbeddedIconsList().V[i]; Gui::MenuItem(icon.Name, icon.UTF8))
							Gui::SetClipboardText(icon.UTF8);
					}
					Gui::EndMenu();
				}
#endif

				Gui::EndMenu();
			}

			if (Gui::BeginMenu(UI_Str("MENU_HELP")))
			{
				Gui::MenuItem(UI_Str("INFO_HELP_COPYRIGHT_YEAR"), "Samyuu", false, false);
				Gui::MenuItem(UI_Str("INFO_HELP_BUILD_TIME"), BuildInfo::CompilationTime(), false, false);
				Gui::MenuItem(UI_Str("INFO_HELP_BUILD_DATE"), BuildInfo::CompilationDate(), false, false);
				Gui::MenuItem(UI_Str("INFO_HELP_BUILD_CONFIGURATION"), UI_Str(BuildInfo::BuildConfiguration()), false, false);
				Gui::MenuItem(UI_Str("INFO_HELP_CURRENT_VERSION"), BuildInfo::CurrentVersion(), false, false);
				Gui::Separator();
				if (Gui::MenuItem(UI_Str("TAB_USAGE_GUIDE"), ToShortcutString(*Settings.Input.Editor_OpenHelp).Data)) { PersistentApp.LastSession.ShowWindow_Help = focusHelpWindowNextFrame = true; }
				if (Gui::MenuItem(UI_Str("TAB_UPDATE_NOTES"), ToShortcutString(*Settings.Input.Editor_OpenUpdateNotes).Data)) { PersistentApp.LastSession.ShowWindow_UpdateNotes = focusUpdateNotesWindowNextFrame = true; }
				if (Gui::MenuItem(UI_Str("TAB_CHART_STATS"), ToShortcutString(*Settings.Input.Editor_OpenChartStats).Data)) { PersistentApp.LastSession.ShowWindow_ChartStats = focusChartStatsWindowNextFrame = true; }
				Gui::EndMenu();
			}

			static constexpr Audio::Backend availableBackends[] = { Audio::Backend::WASAPI_Shared, Audio::Backend::WASAPI_Exclusive, };
			static constexpr auto backendToString = [](Audio::Backend backend) -> cstr
			{
				return (backend == Audio::Backend::WASAPI_Shared) ? "WASAPI Shared" : (backend == Audio::Backend::WASAPI_Exclusive) ? "WASAPI Exclusive" : "Invalid";
			};

			char performanceTextBuffer[64];
			sprintf_s(performanceTextBuffer, "[ %.3f ms (%.1f FPS) ]", (1000.0f / Gui::GetIO().Framerate), Gui::GetIO().Framerate);

			char audioTextBuffer[128];
			if (Audio::Engine.GetIsStreamOpenRunning())
			{
				sprintf_s(audioTextBuffer, "[ %gkHz %zubit %dch ~%.0fms %s ]",
					static_cast<f64>(Audio::Engine.OutputSampleRate) / 1000.0,
					sizeof(i16) * BitsPerByte,
					Audio::Engine.OutputChannelCount,
					Audio::FramesToTime(Audio::Engine.GetBufferFrameSize(), Audio::Engine.OutputSampleRate).ToMS(),
					backendToString(Audio::Engine.GetBackend()));
			}
			else
			{
				strcpy_s(audioTextBuffer, "[ Audio Device Closed ]");
			}

			const f32 perItemItemSpacing = (Gui::GetStyle().ItemSpacing.x * 2.0f);
			const f32 performanceMenuWidth = Gui::CalcTextSize(performanceTextBuffer).x + perItemItemSpacing;
			const f32 audioMenuWidth = Gui::CalcTextSize(audioTextBuffer).x + perItemItemSpacing;

			{
				Gui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
				if (Gui::BeginMenu(UI_Str("MENU_COURSES")))
				{
					if (Gui::BeginMenu(UI_Str("ACT_COURSES_ADD_NEW")))
					{
						const struct { DifficultyType dType; cstr CurrentName; } difficulties[] =
						{
							{ DifficultyType::Easy, UI_Str("DIFFICULTY_TYPE_EASY"), },
							{ DifficultyType::Normal, UI_Str("DIFFICULTY_TYPE_NORMAL"), },
							{ DifficultyType::Hard, UI_Str("DIFFICULTY_TYPE_HARD"), },
							{ DifficultyType::Oni, UI_Str("DIFFICULTY_TYPE_ONI"), },
							{ DifficultyType::OniUra, UI_Str("DIFFICULTY_TYPE_ONI_URA"), },
							{ DifficultyType::Tower, UI_Str("DIFFICULTY_TYPE_TOWER"), },
							{ DifficultyType::Dan, UI_Str("DIFFICULTY_TYPE_DAN"), },
						};
						static_assert(ArrayCount(difficulties) == EnumCount<DifficultyType>);

						for (const auto& dt : difficulties)
						{

							char labelBuffer[128];
							sprintf_s(labelBuffer, "%s", dt.CurrentName);

							b8 isDiffExist = std::any_of(begin(context.Chart.Courses), end(context.Chart.Courses), [&](const auto& obj) { return obj->Type == dt.dType; });
							if (Gui::MenuItem(labelBuffer, " ", false, !isDiffExist)) {
								CreateNewDifficulty(context, dt.dType);
								context.Undo.NotifyChangesWereMade();
							}
						}
						Gui::EndMenu();
					}

					if (Gui::BeginMenu(UI_Str("ACT_COURSES_COMPARE_GROUP")))
					{
						static constexpr auto addToCompared = [](ChartContext& context, auto&& filter)
						{
							for (const auto& uptrCourse : context.Chart.Courses) {
								if (const auto* course = uptrCourse.get(); filter(course))
									context.ChartsCompared[course].insert(BranchType::Normal);
							}
							context.CompareMode |= (size(context.ChartsCompared) > 1);
						};

						b8 isComparingNone = (size(context.ChartsCompared) <= 1);
						b8 isComparingAll = (size(context.ChartsCompared) == size(context.Chart.Courses));

						if (Gui::MenuItem(UI_Str("ACT_COURSES_COMPARE_NONE"), " ", nullptr, !isComparingNone))
							context.ResetChartsCompared();
						if (Gui::MenuItem(UI_Str("ACT_COURSES_COMPARE_ALL"), " ", nullptr, !isComparingAll))
							addToCompared(context, [](...) { return true; });

						Gui::Separator();

						static constexpr auto addToComparedNested = [&](ChartContext& context, auto&& filter)
						{
							for (const auto& [course, branch] : context.ChartsCompared)
								addToCompared(context, [&](const auto* courseI) { return filter(course, courseI); });
						};
						static constexpr auto getNDiffs = [](const ChartCourse* course, const ChartCourse* courseI)
						{
							return (course->Type != courseI->Type) + (course->Style != courseI->Style) + (course->PlayerSide != courseI->PlayerSide);
						};

						if (Gui::MenuItem(UI_Str("ACT_COURSES_COMPARE_ACROSS_DIFFICULTIES"), " ", nullptr, !isComparingAll))
							addToComparedNested(context, [](auto a, auto b) { return (getNDiffs(a, b) - (a->Type != b->Type)) == 0; });
						if (Gui::MenuItem(UI_Str("ACT_COURSES_COMPARE_ACROSS_PLAYERCOUNTS"), " ", nullptr, !isComparingAll))
							addToComparedNested(context, [](auto a, auto b) { return (getNDiffs(a, b) - (a->Style != b->Style)) == 0; });
						if (Gui::MenuItem(UI_Str("ACT_COURSES_COMPARE_ACROSS_PLAYERSIDES"), " ", nullptr, !isComparingAll))
							addToComparedNested(context, [](auto a, auto b) { return (getNDiffs(a, b) - (a->PlayerSide != b->PlayerSide)) == 0; });
						if (Gui::MenuItem(UI_Str("ACT_COURSES_COMPARE_ACROSS_BRANCHES"), "(TODO)", nullptr, false))
							/* TODO */;

						Gui::Separator();

						if (Gui::MenuItem(UI_Str("ACT_COURSES_COMPARE_MODE"), " ", &context.CompareMode) && !context.CompareMode)
							context.ResetChartsCompared();

						Gui::EndMenu();
					}

					Gui::MenuItem(UI_Str("ACT_COURSES_EDIT"), "(TODO)", nullptr, false);
					Gui::EndMenu();
				}

				if (Gui::BeginChild("MenuBarTabsChild", vec2(-(audioMenuWidth + performanceMenuWidth + Gui::GetStyle().ItemSpacing.x), 0.0f)))
				{
					auto tabBarFlags = (ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll);
					i32 nPushedStyleColor = 0;
					if (!context.CompareMode) {
						// NOTE: To essentially make these tab items look similar to regular menu items (the inverted Active <-> Hovered colors are not a mistake)
						Gui::PushStyleColor(ImGuiCol_TabHovered, Gui::GetStyleColorVec4(ImGuiCol_HeaderActive));
						Gui::PushStyleColor(ImGuiCol_TabSelected, Gui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
						nPushedStyleColor += 2;
					} else {
						tabBarFlags |= ImGuiTabBarFlags_DrawSelectedOverline;
					}

					if (Gui::BeginTabBar("MenuBarTabs", tabBarFlags))
					{
						// HACK: How to properly manage the imgui selected tab internal state..?
						static const ChartCourse* lastFrameSelectedCoursePtrID = nullptr;
						b8 isAnyCourseTabSelected = false;

						// Reorder courses by querying tab bar order
						if (ImGuiTabBar* tab_bar = Gui::GetCurrentTabBar(); tab_bar->ReorderRequestTabId != 0) {
							// save current order
							std::vector<ImGuiID> tabIdx = {};
							for (const auto& tab : tab_bar->Tabs)
								tabIdx.push_back(tab.ID);
							// force update order
							Gui::TabBarProcessReorder(tab_bar);
							tab_bar->ReorderRequestTabId = 0; // cancel further reorder
							// reorder courses into updated tab order
							auto& courses = context.Chart.Courses;
							std::vector<std::unique_ptr<ChartCourse>> reorderedCourses (size(courses));
							for (size_t i = 0; i < std::size(courses); ++i) {
								auto& course = courses[i];
								ImGuiTabItem* tab = Gui::TabBarFindTabByID(tab_bar, tabIdx[i]);
								assert(tab != nullptr && "failed to reorder courses; found a course without tab");
								i32 newOrder = Gui::TabBarGetTabOrder(tab_bar, tab);
								assert(newOrder >= 0 && newOrder < size(courses) && "failed to reorder courses; found a tab without course");
								reorderedCourses[newOrder] = std::move(course);
							}
							courses = std::move(reorderedCourses);
							context.Undo.NotifyChangesWereMade();
						}

						for (std::unique_ptr<ChartCourse>& course : context.Chart.Courses)
						{
							char buffer[64]; sprintf_s(buffer, "%s \xe2\x98\x85%d%s %s###Course_%p",
								UI_StrRuntime(DifficultyTypeNames[EnumToIndex(course->Type)]),
								static_cast<i32>(course->Level), 
								(course->Decimal == DifficultyLevelDecimal::None) ? "" : ((course->Decimal >= DifficultyLevelDecimal::PlusThreshold) ? "+" : ""),
								GetStyleName(course->Style, course->PlayerSide).data(),
								course.get());
							const b8 isSelected = (course.get() == context.ChartSelectedCourse);
							const b8 setSelectedThisFrame = (isSelected && course.get() != lastFrameSelectedCoursePtrID);

							Gui::PushID(course.get());

							i32 nPushedStyleColorI = 0;
							if ((i32)course->Level >= 11) {
								Gui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 122, 122, 255));
								++nPushedStyleColorI;
							}

							const b8 wasCompared = (context.CompareMode && context.IsChartCompared(course.get(), BranchType::Normal));
							if (wasCompared) { // display compared courses as hovered
								Gui::PushStyleColor(ImGuiCol_TabHovered, Gui::GetStyleColorVec4(ImGuiCol_HeaderActive));
								Gui::PushStyleColor(ImGuiCol_Tab, Gui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
								nPushedStyleColorI += 2;
							}

							if (Gui::BeginTabItem(buffer, nullptr, setSelectedThisFrame ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
							{
								// TODO: Selecting a course should also be an undo command so that there isn't ever any confusion (?)
								context.SetSelectedChart(course.get(), BranchType::Normal);
								lastFrameSelectedCoursePtrID = context.ChartSelectedCourse;
								isAnyCourseTabSelected = true;
								Gui::EndTabItem();
							}

							Gui::PopStyleColor(nPushedStyleColorI);

							if (Gui::BeginPopupContextItem()) {
								const b8 isOnlyCourse = isSelected && (size(context.ChartsCompared) <= 1);
								if (b8 isCompared = isSelected || wasCompared; Gui::MenuItem(UI_Str("ACT_COURSES_COMPARE_THIS"), " ", &isCompared, !isOnlyCourse)) {
									if (isCompared) { // add
										context.ChartsCompared[course.get()].insert(BranchType::Normal);
									} else if (!isSelected) { // remove
										if (auto& branches = context.ChartsCompared[course.get()]; branches.size() > 1)
											branches.erase(BranchType::Normal);
										else
											context.ChartsCompared.erase(course.get());
									} else { // remove & select next chart
										if (auto& branches = context.ChartsCompared[course.get()]; branches.size() > 1) {
											auto it = branches.find(BranchType::Normal);
											context.ChartSelectedBranch = (it != end(branches) && ++it != end(branches)) ? *it : *begin(branches);
											branches.erase(BranchType::Normal);
										} else {
											const auto itEnd = end(context.Chart.Courses);
											auto itL = itEnd, itNext = itEnd;
											auto* pit = &itL;
											for (auto it = begin(context.Chart.Courses); it != itEnd; ++it) {
												if (it->get() == course.get()) {
													pit = &itNext;
												} else if (*pit == itEnd && context.ChartsCompared.find(it->get()) != cend(context.ChartsCompared)) {
													*pit = it;
													if (pit == &itNext)
														break;
												}
											}
											if (itNext == itEnd)
												itNext = itL;
											context.SetSelectedChart(itNext->get(), *begin(context.ChartsCompared[itNext->get()]));
											context.ChartsCompared.erase(course.get());
											// give away tab focus
											ImGuiTabBar* tab_bar = Gui::GetCurrentTabBar();
											Gui::TabBarQueueFocus(tab_bar, Gui::TabBarFindTabByOrder(tab_bar, itNext - begin(context.Chart.Courses)));
										}
									}
								}
								if (Gui::MenuItem(UI_Str("ACT_COURSES_COMPARE_MODE"), " ", &context.CompareMode) && !context.CompareMode)
									context.ResetChartsCompared();

								Gui::EndPopup();
							}

							Gui::PopID();
						}

						if (!isAnyCourseTabSelected)
						{
							assert(!context.Chart.Courses.empty() && "Courses must never be empty so that the selected course always points to a valid object");
							context.SetSelectedChart(context.Chart.Courses.front().get(), BranchType::Normal);
						}

						Gui::EndTabBar();
					}
					Gui::PopStyleColor(nPushedStyleColor);
				}
				Gui::EndChild();
			}

			// NOTE Right-aligned peformance / audio display
			{
				// TODO: Redirect to an audio settings window instead similar to how it works in REAPER for example (?)
				Gui::SetCursorPos(vec2(Gui::GetWindowWidth() - performanceMenuWidth - audioMenuWidth, Gui::GetCursorPos().y));
				if (Gui::BeginMenu(audioTextBuffer))
				{
					const b8 deviceIsOpen = Audio::Engine.GetIsStreamOpenRunning();
					if (Gui::MenuItem(UI_Str("ACT_AUDIO_OPEN_DEVICE"), nullptr, false, !deviceIsOpen))
						Audio::Engine.OpenStartStream();
					if (Gui::MenuItem(UI_Str("ACT_AUDIO_CLOSE_DEVICE"), nullptr, false, deviceIsOpen))
						Audio::Engine.StopCloseStream();
					Gui::Separator();

					const Audio::Backend currentBackend = Audio::Engine.GetBackend();
					for (const Audio::Backend backendType : availableBackends)
					{
						char labelBuffer[128];
						sprintf_s(labelBuffer, UI_Str("ACT_AUDIO_USE_FMT_%s_DEVICE"), backendToString(backendType));
						if (Gui::MenuItem(labelBuffer, nullptr, (backendType == currentBackend), (backendType != currentBackend)))
							Audio::Engine.SetBackend(backendType);
					}
					Gui::EndMenu();
				}

				if (Gui::MenuItem(performanceTextBuffer))
					performance.ShowOverlay ^= true;

				if (performance.ShowOverlay)
				{
					const ImGuiViewport* mainViewport = Gui::GetMainViewport();
					Gui::SetNextWindowPos(vec2(mainViewport->Pos.x + mainViewport->Size.x, mainViewport->Pos.y + GuiScale(24.0f)), ImGuiCond_Always, vec2(1.0f, 0.0f));
					Gui::SetNextWindowViewport(mainViewport->ID);
					Gui::PushStyleColor(ImGuiCol_WindowBg, Gui::GetStyleColorVec4(ImGuiCol_PopupBg));
					Gui::PushStyleColor(ImGuiCol_FrameBg, Gui::GetColorU32(ImGuiCol_FrameBg, 0.0f));
					Gui::PushStyleColor(ImGuiCol_PlotLines, Gui::GetStyleColorVec4(ImGuiCol_TextDisabled));

					static constexpr ImGuiWindowFlags overlayWindowFlags =
						ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
						ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
						ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

					if (Gui::Begin("PerformanceOverlay", nullptr, overlayWindowFlags))
					{
						performance.FrameTimesMS[performance.FrameTimeIndex++] = (Gui::GetIO().DeltaTime * 1000.0f);
						if (performance.FrameTimeIndex >= ArrayCount(performance.FrameTimesMS)) performance.FrameTimeIndex = 0;
						performance.FrameTimeCount = Max(performance.FrameTimeIndex, performance.FrameTimeCount);

						f32 averageFrameTime = 0.0f, minFrameTime = F32Max, maxFrameTime = F32Min;
						for (size_t i = 0; i < performance.FrameTimeCount; i++) averageFrameTime += performance.FrameTimesMS[i]; averageFrameTime /= static_cast<f32>(performance.FrameTimeCount);
						for (size_t i = 0; i < performance.FrameTimeCount; i++) minFrameTime = Min(minFrameTime, performance.FrameTimesMS[i]);
						for (size_t i = 0; i < performance.FrameTimeCount; i++) maxFrameTime = Max(maxFrameTime, performance.FrameTimesMS[i]);

						static constexpr f32 plotLinesHeight = 120.0f;
						const f32 scaleMin = ClampBot((1000.0f / 30.0f), minFrameTime - 0.001f);
						const f32 scaleMax = ClampTop((1000.0f / 1000.0f), maxFrameTime + 0.001f);
						Gui::PlotLines("##PerformanceHistoryPlot", performance.FrameTimesMS, ArrayCountI32(performance.FrameTimesMS), 0,
							"", scaleMin, scaleMax, GuiScale(vec2(static_cast<f32>(ArrayCount(performance.FrameTimesMS)), plotLinesHeight)));
						const Rect plotLinesRect = Gui::GetItemRect();

						char overlayTextBuffer[96];
						const auto overlayText = std::string_view(overlayTextBuffer, sprintf_s(overlayTextBuffer,
							"%s%.5g ms\n"
							"%s%.5g ms\n"
							"%s%.5g ms",
							UI_Str("INFO_LATENCY_AVERAGE"), averageFrameTime,
							UI_Str("INFO_LATENCY_MIN"), minFrameTime,
							UI_Str("INFO_LATENCY_MAX"), maxFrameTime));

						const vec2 overlayTextSize = Gui::CalcTextSize(overlayText);
						const Rect overlayTextRect = Rect::FromTLSize(plotLinesRect.GetCenter() - (overlayTextSize * 0.5f) - vec2(0.0f, plotLinesRect.GetHeight() / 4.0f), overlayTextSize);
						Gui::GetWindowDrawList()->AddRectFilled(overlayTextRect.TL - vec2(2.0f), overlayTextRect.BR + vec2(2.0f), Gui::GetColorU32(ImGuiCol_WindowBg, 0.5f));
						Gui::AddTextWithDropShadow(Gui::GetWindowDrawList(), overlayTextRect.TL, Gui::GetColorU32(ImGuiCol_Text), overlayText, 0xFF111111);
					}
					Gui::End();

					Gui::PopStyleColor(3);
				}
			}

			Gui::EndMenuBar();
		}
	}

	void ChartEditor::DrawGui()
	{
		InternalUpdateAsyncLoading();

		if (tryToCloseApplicationOnNextFrame)
		{
			tryToCloseApplicationOnNextFrame = false;
			CheckOpenSaveConfirmationPopupThenCall([&]
			{
				if (loadSongFuture.valid()) loadSongFuture.get();
				if (loadJacketFuture.valid()) loadJacketFuture.get();
				if (importChartFuture.valid()) importChartFuture.get();
				context.Undo.ClearAll();
				ApplicationHost::GlobalState.RequestExitNextFrame = EXIT_SUCCESS;
			});
		}

		UpdateApplicationWindowTitle(context);

		// NOTE: Check for external changes made via the settings window
		if (GlobalLastSetRequestExclusiveDeviceAccessAudioSetting != *Settings.Audio.RequestExclusiveDeviceAccess)
		{
			Audio::Engine.SetBackend(*Settings.Audio.RequestExclusiveDeviceAccess ? Audio::Backend::WASAPI_Exclusive : Audio::Backend::WASAPI_Shared);
			GlobalLastSetRequestExclusiveDeviceAccessAudioSetting = *Settings.Audio.RequestExclusiveDeviceAccess;
		}
		EnableGuiScaleAnimation = *Settings.Animation.EnableGuiScaleAnimation;

		// NOTE: Window focus audio engine response
		{
			if (ApplicationHost::GlobalState.HasAllFocusBeenLostThisFrame)
			{
				wasAudioEngineRunningIdleOnFocusLost = (Audio::Engine.GetIsStreamOpenRunning() && Audio::Engine.GetAllVoicesAreIdle());
				if (wasAudioEngineRunningIdleOnFocusLost && *Settings.Audio.CloseDeviceOnIdleFocusLoss)
					Audio::Engine.StopCloseStream();
			}
			if (ApplicationHost::GlobalState.HasAnyFocusBeenGainedThisFrame)
			{
				if (wasAudioEngineRunningIdleOnFocusLost && *Settings.Audio.CloseDeviceOnIdleFocusLoss)
					Audio::Engine.OpenStartStream();
			}
		}

		// NOTE: Apply volume
		{
			context.SongVoice.SetVolume(context.Chart.SongVolume);
			context.SfxVoicePool.BaseVolumeSfx = context.Chart.SoundEffectVolume;
		}

		// NOTE: Drag and drop handling
		for (const std::string& droppedFilePath : ApplicationHost::GlobalState.FilePathsDroppedThisFrame)
		{
			if (Path::HasAnyExtension(droppedFilePath, TJA::Extension)) { CheckOpenSaveConfirmationPopupThenCall([this, pathCopy = droppedFilePath] { StartAsyncImportingChartFile(pathCopy); }); break; }
			if (Path::HasAnyExtension(droppedFilePath, Audio::SupportedFileFormatExtensionsPacked)) { SetAndStartLoadingChartSongFileName(droppedFilePath, context.Undo); break; }
			if (Path::HasAnyExtension(droppedFilePath, TJA::PreimageExtensions)) { SetAndStartLoadingSongJacketFileName(droppedFilePath, context.Undo); break; }
		}

		// NOTE: Global input bindings
		{
			const b8 noActiveID = (Gui::GetActiveID() == 0);
			const b8 noOpenPopup = (Gui::GetCurrentContext()->OpenPopupStack.Size <= 0);

			if (noActiveID)
			{
				const f32 guiScaleFactorToSetNextFrame = GuiScaleFactorToSetNextFrame;
				if (Gui::IsAnyPressed(*Settings.Input.Editor_IncreaseGuiScale, true)) ZoomInGuiScale();
				if (Gui::IsAnyPressed(*Settings.Input.Editor_DecreaseGuiScale, true)) ZoomOutGuiScale();
				if (Gui::IsAnyPressed(*Settings.Input.Editor_ResetGuiScale, false)) ZoomResetGuiScale();

				if (guiScaleFactorToSetNextFrame != GuiScaleFactorToSetNextFrame) { if (!zoomPopup.IsOpen) zoomPopup.Open(); zoomPopup.OnChange(); }

				if (Gui::IsAnyPressed(*Settings.Input.Editor_ToggleFullscreen, false))
					ApplicationHost::GlobalState.SetBorderlessFullscreenNextFrame = !ApplicationHost::GlobalState.IsBorderlessFullscreen;

				if (Gui::IsAnyPressed(*Settings.Input.Editor_ToggleVSync, false))
					ApplicationHost::GlobalState.SwapInterval = !ApplicationHost::GlobalState.SwapInterval;

				if (Gui::IsAnyPressed(*Settings.Input.Editor_OpenHelp, true)) PersistentApp.LastSession.ShowWindow_Help = focusHelpWindowNextFrame = true;
				if (Gui::IsAnyPressed(*Settings.Input.Editor_OpenUpdateNotes, true)) PersistentApp.LastSession.ShowWindow_UpdateNotes = focusUpdateNotesWindowNextFrame = true;
				if (Gui::IsAnyPressed(*Settings.Input.Editor_OpenChartStats, true)) PersistentApp.LastSession.ShowWindow_ChartStats = focusChartStatsWindowNextFrame = true;
				if (Gui::IsAnyPressed(*Settings.Input.Editor_OpenSettings, true)) PersistentApp.LastSession.ShowWindow_Settings = focusSettingsWindowNextFrame = true;
			}

			if (noActiveID && noOpenPopup)
			{
				if (Gui::IsAnyPressed(*Settings.Input.Editor_Undo, true))
					context.Undo.Undo();
				if (Gui::IsAnyPressed(*Settings.Input.Editor_Redo, true))
					context.Undo.Redo();

				if (Gui::IsAnyPressed(*Settings.Input.Editor_ChartNew, false))
					CheckOpenSaveConfirmationPopupThenCall([&] { CreateNewChart(context); });

				if (Gui::IsAnyPressed(*Settings.Input.Editor_ChartOpen, false))
					CheckOpenSaveConfirmationPopupThenCall([&] { OpenLoadChartFileDialog(context); });

				if (Gui::IsAnyPressed(*Settings.Input.Editor_ChartOpenDirectory, false) && CanOpenChartDirectoryInFileExplorer(context))
					OpenChartDirectoryInFileExplorer(context);

				if (Gui::IsAnyPressed(*Settings.Input.Editor_ChartSave, false))
					TrySaveChartOrOpenSaveAsDialog(context);

				if (Gui::IsAnyPressed(*Settings.Input.Editor_ChartSaveAs, false))
					OpenChartSaveAsDialog(context);
			}
		}

		if (PersistentApp.LastSession.ShowWindow_Help)
		{
			if (Gui::Begin(UI_WindowName("TAB_USAGE_GUIDE"), &PersistentApp.LastSession.ShowWindow_Help, ImGuiWindowFlags_None))
			{
				helpWindow.DrawGui(context);
			}
			if (focusHelpWindowNextFrame) { focusHelpWindowNextFrame = false; Gui::SetWindowFocus(); }
			Gui::End();
		}

		if (PersistentApp.LastSession.ShowWindow_UpdateNotes)
		{
			if (Gui::Begin(UI_WindowName("TAB_UPDATE_NOTES"), &PersistentApp.LastSession.ShowWindow_UpdateNotes, ImGuiWindowFlags_None))
			{
				updateNotesWindow.DrawGui(context);
			}
			if (focusUpdateNotesWindowNextFrame) { focusUpdateNotesWindowNextFrame = false; Gui::SetWindowFocus(); }
			Gui::End();
		}

		if (PersistentApp.LastSession.ShowWindow_ChartStats)
		{
			if (Gui::Begin(UI_WindowName("TAB_CHART_STATS"), &PersistentApp.LastSession.ShowWindow_ChartStats, ImGuiWindowFlags_None))
			{
				chartStatsWindow.DrawGui(context);
			}
			if (focusChartStatsWindowNextFrame) { focusChartStatsWindowNextFrame = false; Gui::SetWindowFocus(); }
			Gui::End();
		}

		if (PersistentApp.LastSession.ShowWindow_Settings)
		{
			if (Gui::Begin(UI_WindowName("TAB_SETTINGS"), &PersistentApp.LastSession.ShowWindow_Settings, ImGuiWindowFlags_None))
			{
				if (settingsWindow.DrawGui(context, Settings_Mutable))
					Settings_Mutable.IsDirty = true;
			}
			if (focusSettingsWindowNextFrame) { focusSettingsWindowNextFrame = false; Gui::SetWindowFocus(); }
			Gui::End();
		}

		if (Gui::Begin(UI_WindowName("TAB_INSPECTOR"), nullptr, ImGuiWindowFlags_None))
		{
			chartInspectorWindow.DrawGui(context);
		}
		Gui::End();

		if (Gui::Begin(UI_WindowName("TAB_UNDO_HISTORY"), nullptr, ImGuiWindowFlags_None))
		{
			undoHistoryWindow.DrawGui(context);
		}
		Gui::End();

		if (Gui::Begin(UI_WindowName("TAB_TEMPO_CALCULATOR"), nullptr, ImGuiWindowFlags_None))
		{
			tempoCalculatorWindow.DrawGui(context);
		}
		Gui::End();

		if (Gui::Begin(UI_WindowName("TAB_LYRICS"), nullptr, ImGuiWindowFlags_None))
		{
			lyricsWindow.DrawGui(context, timeline);
		}
		Gui::End();

		if (Gui::Begin(UI_WindowName("TAB_EVENTS"), nullptr, ImGuiWindowFlags_None))
		{
			tempoWindow.DrawGui(context, timeline);
		}
		Gui::End();

		if (Gui::Begin(UI_WindowName("TAB_CHART_PROPERTIES"), nullptr, ImGuiWindowFlags_None))
		{
			ChartPropertiesWindowIn in = {};
			in.IsSongAsyncLoading = loadSongFuture.valid();
			in.IsJacketAsyncLoading = loadJacketFuture.valid();
			ChartPropertiesWindowOut out = {};
			propertiesWindow.DrawGui(context, in, out);

			if (out.LoadNewSong)
				SetAndStartLoadingChartSongFileName(out.NewSongFilePath, context.Undo);
			else if (out.BrowseOpenSong)
				OpenLoadAudioFileDialog(context.Undo);

			if (out.LoadNewJacket)
				SetAndStartLoadingSongJacketFileName(out.NewJacketFilePath, context.Undo);
			else if (out.BrowseOpenJacket)
				OpenLoadJacketFileDialog(context.Undo);
		}
		Gui::End();

#if PEEPO_DEBUG // DEBUG: Manually submit debug window before the timeline window is drawn for better tab ordering
		if (Gui::Begin(UI_WindowName("TAB_TIMELINE_DEBUG"))) { /* ... */ } Gui::End();
#endif

		if (Gui::Begin(UI_WindowName("TAB_GAME_PREVIEW"), nullptr, ImGuiWindowFlags_None))
		{
			gamePreview.DrawGui(context, timeline.Camera.WorldSpaceXToTime(timeline.WorldSpaceCursorXAnimationCurrent));
		}
		Gui::End();

		// NOTE: Always update the timeline even if the window isn't visible so that child-windows can be docked properly and hit sounds can always be heard
		Gui::Begin(UI_WindowName("TAB_TIMELINE"), nullptr, ImGuiWindowFlags_None);
		timeline.DrawGui(context);
		Gui::End();

		// NOTE: Test stuff
		{
			if (PersistentApp.LastSession.ShowWindow_ImGuiDemo)
			{
				if (auto* window = Gui::FindWindowByName("Dear ImGui Demo"))
					Gui::UpdateSmoothScrollWindow(window);

				Gui::ShowDemoWindow(&PersistentApp.LastSession.ShowWindow_ImGuiDemo);
			}

			if (PersistentApp.LastSession.ShowWindow_ImGuiStyleEditor)
			{
				if (Gui::Begin("ImGui Style Editor", &PersistentApp.LastSession.ShowWindow_ImGuiStyleEditor))
				{
					Gui::UpdateSmoothScrollWindow();
					Gui::ShowStyleEditor();
				}
				Gui::End();
			}

			if (PersistentApp.LastSession.ShowWindow_TJAImportTest)
			{
				if (Gui::Begin(UI_WindowName("TAB_TJA_IMPORT_TEST"), &PersistentApp.LastSession.ShowWindow_TJAImportTest, ImGuiWindowFlags_None))
				{
					tjaTestWindow.DrawGui();
					if (tjaTestWindow.WasTJAEditedThisFrame)
					{
						CheckOpenSaveConfirmationPopupThenCall([&]
						{
							// HACK: Incredibly inefficient of course but just here for quick testing
							ChartProject convertedChart {};
							CreateChartProjectFromTJA(tjaTestWindow.LoadedTJAFile.Parsed, convertedChart);
							createBackupOfOriginalTJABeforeOverwriteSave = false;
							context.Chart = std::move(convertedChart);
							context.ChartFilePath = tjaTestWindow.LoadedTJAFile.FilePath;
							context.ResetChartsCompared();
							context.SetSelectedChart(
								context.Chart.Courses.empty() ?
									context.Chart.Courses.emplace_back(std::make_unique<ChartCourse>()).get()
									: context.Chart.Courses.front().get(),
								BranchType::Normal);
							context.Undo.ClearAll();
						});
					}
				}
				Gui::End();
			}

			if (PersistentApp.LastSession.ShowWindow_AudioTest)
			{
				if (Gui::Begin(UI_WindowName("TAB_AUDIO_TEST"), &PersistentApp.LastSession.ShowWindow_AudioTest, ImGuiWindowFlags_None))
				{
					audioTestWindow.DrawGui();
					Gui::End();
				}
			}

			// DEBUG: LIVE PREVIEW PagMan
			if (PersistentApp.LastSession.ShowWindow_TJAExportTest)
			{
				if (Gui::Begin(UI_WindowName("TAB_TJA_EXPORT_DEBUG_VIEW"), &PersistentApp.LastSession.ShowWindow_TJAExportTest, ImGuiWindowFlags_MenuBar))
				{
					static struct { b8 RoundTripCheck = false, Update = true; i32 Changes = -1, Undos = 0, Redos = 0; std::string Text, DebugLog; ChartProject DebugChart; ::TextEditor Editor = CreateImGuiColorTextEditWithNiceTheme(); } exportDebugViewData;

					if (Gui::BeginMenuBar())
					{
						Gui::MenuItem("Round-trip Conversion Check", nullptr, &exportDebugViewData.RoundTripCheck);
						Gui::EndMenuBar();
					}

					if (exportDebugViewData.RoundTripCheck)
					{
						Gui::PushStyleColor(ImGuiCol_Text, 0xFF42AEF7);
						Gui::InputTextMultilineWithHint("##DebugLog", "No errors detected " UTF8_PeepoHappy, &exportDebugViewData.DebugLog, vec2(-1.0f, Gui::GetFrameHeight() * 4.0f), ImGuiInputTextFlags_ReadOnly);
						Gui::PopStyleColor();
					}

					if (context.Undo.NumberOfChangesMade != exportDebugViewData.Changes) { exportDebugViewData.Update = true; exportDebugViewData.Changes = context.Undo.NumberOfChangesMade; }
					if (context.Undo.UndoStack.size() != exportDebugViewData.Undos) { exportDebugViewData.Update = true; exportDebugViewData.Undos = static_cast<i32>(context.Undo.UndoStack.size()); }
					if (context.Undo.RedoStack.size() != exportDebugViewData.Redos) { exportDebugViewData.Update = true; exportDebugViewData.Redos = static_cast<i32>(context.Undo.RedoStack.size()); }
					if (exportDebugViewData.Update)
					{
						exportDebugViewData.Update = false;
						TJA::ParsedTJA tja;
						ConvertChartProjectToTJA(context.Chart, tja);
						exportDebugViewData.Text.clear();
						TJA::ConvertParsedToText(tja, exportDebugViewData.Text, TJA::Encoding::Unknown);
						exportDebugViewData.Editor.SetText(exportDebugViewData.Text);

						// DEBUG: TJA bug hunting
						if (exportDebugViewData.RoundTripCheck)
						{
							std::vector<TJA::Token> tempTokens = TJA::TokenizeLines(TJA::SplitLines(exportDebugViewData.Text));
							TJA::ErrorList tempErrors;
							TJA::ParsedTJA tempTJA = TJA::ParseTokens(tempTokens, tempErrors);
							exportDebugViewData.DebugChart = {}; CreateChartProjectFromTJA(tempTJA, exportDebugViewData.DebugChart); exportDebugViewData.DebugLog.clear();
							DebugCompareCharts(context.Chart, exportDebugViewData.DebugChart, [](std::string_view message, void*) { exportDebugViewData.DebugLog += message; exportDebugViewData.DebugLog += '\n'; });
						}
					}
					const f32 buttonHeight = Gui::GetFrameHeight();
					exportDebugViewData.Editor.SetReadOnly(true);
					exportDebugViewData.Editor.Render("TJAExportTextEditor", vec2(Gui::GetContentRegionAvail().x, ClampBot(buttonHeight * 2.0f, Gui::GetContentRegionAvail().y - buttonHeight)), true);
					exportDebugViewData.Update |= Gui::Button("Force Update", vec2(Gui::GetContentRegionAvail().x, buttonHeight));
				}
				Gui::End();
			}
		}

		// NOTE: Zoom change popup
		if (zoomPopup.IsOpen)
		{
			static constexpr Time fadeInDuration = Time::FromFrames(10.0), fadeOutDuration = Time::FromFrames(14.0);
			static constexpr Time closeDuration = Time::FromSec(2.0);
			const f32 fadeIn = ConvertRangeClampInput(0.0f, fadeInDuration.ToSec_F32(), 0.0f, 1.0f, zoomPopup.TimeSinceOpen.ToSec_F32());
			const f32 fadeOut = ConvertRangeClampInput(0.0f, fadeOutDuration.ToSec_F32(), 0.0f, 1.0f, (closeDuration - zoomPopup.TimeSinceLastChange).ToSec_F32());

			b8 isWindowHovered = false;
			Gui::PushStyleVar(ImGuiStyleVar_Alpha, (fadeIn < 1.0f) ? (fadeIn * fadeIn) : (fadeOut * fadeOut));
			Gui::PushStyleVar(ImGuiStyleVar_WindowRounding, GuiScale(6.0f));
			Gui::PushStyleColor(ImGuiCol_WindowBg, Gui::ColorU32WithNewAlpha(Gui::GetColorU32(ImGuiCol_FrameBg), 1.0f));
			Gui::PushStyleColor(ImGuiCol_Button, 0x00000000);
			Gui::PushFont(FontMain, GuiScaleI32_AtTarget(FontBaseSizes::Medium));

			constexpr ImGuiWindowFlags popupFlags =
				ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;
			const ImGuiViewport* mainViewport = Gui::GetMainViewport();
			Gui::SetNextWindowPos(vec2(mainViewport->Pos.x + mainViewport->Size.x, mainViewport->Pos.y + GuiScale(24.0f)) + vec2(Gui::GetStyle().FramePadding) * vec2(-1.0f, +1.0f), ImGuiCond_Always, vec2(1.0f, 0.0f));
			Gui::Begin("ZoomPopup", nullptr, popupFlags);
			{
				isWindowHovered = Gui::IsWindowHovered();
				Gui::AlignTextToFramePadding();
				Gui::Text("%g%% ", ToPercent(GuiScaleFactorTarget));

				Gui::BeginDisabled(!CanZoomOutGuiScale());
				Gui::SameLine(0.0f, 0.0f);
				if (Gui::Button("-", vec2(Gui::GetFrameHeight(), 0.0f))) { ZoomOutGuiScale(); zoomPopup.OnChange(); }
				Gui::EndDisabled();

				Gui::BeginDisabled(!CanZoomInGuiScale());
				Gui::SameLine(0.0f, 0.0f);
				if (Gui::Button("+", vec2(Gui::GetFrameHeight(), 0.0f))) { ZoomInGuiScale(); zoomPopup.OnChange(); }
				Gui::EndDisabled();

				Gui::SameLine(0.0f, 0.0f);
				if (Gui::Button(UI_Str("ACT_ZOOM_POPUP_RESET_ZOOM"))) { ZoomResetGuiScale(); zoomPopup.OnChange(); }
			}
			Gui::End();

			Gui::PopFont();
			Gui::PopStyleColor(2);
			Gui::PopStyleVar(2);

			if (zoomPopup.TimeSinceLastChange >= closeDuration)
			{
				zoomPopup.IsOpen = false;
				zoomPopup.TimeSinceOpen = zoomPopup.TimeSinceLastChange = {};
			}
			else
			{
				// NOTE: Clamp so that the fade-in animation won't ever be fully skipped even with font rebuild lag
				zoomPopup.TimeSinceOpen += Time::FromSec(ClampTop(Gui::GetIO().DeltaTime, 1.0f / 30.0f));
				zoomPopup.TimeSinceLastChange = isWindowHovered ? (closeDuration * 0.5) : zoomPopup.TimeSinceLastChange + Time::FromSec(Gui::GetIO().DeltaTime);
			}
		}

		// NOTE: Save confirmation popup
		{
			static constexpr cstr saveConfirmationPopupID = "INFO_MSGBOX_UNSAVED";
			if (saveConfirmationPopup.OpenOnNextFrame) { Gui::OpenPopup(UI_WindowName(saveConfirmationPopupID)); saveConfirmationPopup.OpenOnNextFrame = false; }

			const ImGuiViewport* mainViewport = Gui::GetMainViewport();
			Gui::SetNextWindowPos(Rect::FromTLSize(mainViewport->Pos, mainViewport->Size).GetCenter(), ImGuiCond_Appearing, vec2(0.5f));

			Gui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { GuiScale(4.0f), Gui::GetStyle().ItemSpacing.y });
			Gui::PushStyleVar(ImGuiStyleVar_WindowPadding, { GuiScale(6.0f), GuiScale(6.0f) });
			b8 isPopupOpen = true;
			if (Gui::BeginPopupModal(UI_WindowName(saveConfirmationPopupID), &isPopupOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
			{
				const vec2 buttonSize = GuiScale(vec2(120.0f, 0.0f));
				Gui::PushFont(FontMain, GuiScaleI32_AtTarget(FontBaseSizes::Medium));
				{
					// NOTE: Manual child size calculation required for proper dynamic scaling
					Gui::BeginChild("TextChild", vec2((buttonSize.x * 3.0f) + Gui::GetStyle().ItemSpacing.x, Gui::GetFontSize() * 3.0f), true, ImGuiWindowFlags_NoBackground);
					Gui::AlignTextToFramePadding();
					Gui::TextUnformatted(UI_Str("PROMPT_MSGBOX_UNSAVED_SAVE_CHANGES"));
					Gui::EndChild();
				}
				Gui::PopFont();

				const b8 clickedYes = Gui::Button(UI_Str("ACT_MSGBOX_UNSAVED_SAVE_CHANGES"), buttonSize) | (Gui::IsWindowFocused() && Gui::IsAnyPressed(*Settings.Input.Dialog_YesOrOk, false));
				Gui::SameLine();
				const b8 clickedNo = Gui::Button(UI_Str("ACT_MSGBOX_UNSAVED_DISCARD_CHANGES"), buttonSize) | (Gui::IsWindowFocused() && Gui::IsAnyPressed(*Settings.Input.Dialog_No, false));
				Gui::SameLine();
				const b8 clickedCancel = Gui::Button(UI_Str("ACT_MSGBOX_CANCEL"), buttonSize) | (Gui::IsWindowFocused() && Gui::IsAnyPressed(*Settings.Input.Dialog_Cancel, false));

				if (clickedYes || clickedNo || clickedCancel)
				{
					const b8 saveDialogCanceled = clickedYes ? !TrySaveChartOrOpenSaveAsDialog(context) : false;
					UpdateApplicationWindowTitle(context);
					if (saveConfirmationPopup.OnSuccessFunction)
					{
						if (!clickedCancel && !saveDialogCanceled)
							saveConfirmationPopup.OnSuccessFunction();
						saveConfirmationPopup.OnSuccessFunction = {};
					}
					Gui::CloseCurrentPopup();
				}
				Gui::EndPopup();
			}
			Gui::PopStyleVar(2);
		}

		context.Undo.FlushAndExecuteEndOfFrameCommands();
	}

	void ChartEditor::RestoreDefaultDockSpaceLayout(ImGuiID dockSpaceID)
	{
		const ImGuiViewport* mainViewport = Gui::GetMainViewport();

		// HACK: This kinda sucks. Referencing windows by hardcoded ID strings and also not allow for incremental window additions without rebuilding the entire node tree...
		//		 This function can also only be called right before the fullscreen "Gui::DockSpace()" (meaining outside the chart editor update functions)
		Gui::DockBuilderRemoveNode(dockSpaceID);
		Gui::DockBuilderAddNode(dockSpaceID, ImGuiDockNodeFlags_DockSpace);
		Gui::DockBuilderSetNodeSize(dockSpaceID, mainViewport->WorkSize);
		{
			struct DockIDs { ImGuiID Top, Bot, TopLeft, TopCenter, TopRight, TopRightBot; } dock = {};
			Gui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Down, 0.55f, &dock.Bot, &dock.Top);
			Gui::DockBuilderSplitNode(dock.Top, ImGuiDir_Left, 0.3f, &dock.TopLeft, &dock.TopRight);
			Gui::DockBuilderSplitNode(dock.TopRight, ImGuiDir_Right, 0.4f, &dock.TopRight, &dock.TopCenter);
			Gui::DockBuilderSplitNode(dock.TopRight, ImGuiDir_Down, 0.4f, &dock.TopRightBot, &dock.TopRight);

			// HACK: Shitty dock tab order. Windows which are "Gui::Begin()"-added last (ones with higher focus priority) are always put on the right
			//		 and it doesn't look like there's an easy way to change this...
			Gui::DockBuilderDockWindow(UI_WindowName("TAB_TIMELINE_DEBUG"), dock.Bot);
			Gui::DockBuilderDockWindow(UI_WindowName("TAB_TIMELINE"), dock.Bot);

			Gui::DockBuilderDockWindow(UI_WindowName("TAB_TEMPO_CALCULATOR"), dock.TopLeft);
			Gui::DockBuilderDockWindow(UI_WindowName("TAB_LYRICS"), dock.TopLeft);
			Gui::DockBuilderDockWindow(UI_WindowName("TAB_EVENTS"), dock.TopLeft);
			Gui::DockBuilderDockWindow(UI_WindowName("TAB_TJA_EXPORT_DEBUG_VIEW"), dock.TopLeft);

			Gui::DockBuilderDockWindow(UI_WindowName("TAB_SETTINGS"), dock.TopCenter);
			Gui::DockBuilderDockWindow(UI_WindowName("TAB_USAGE_GUIDE"), dock.TopCenter);
			Gui::DockBuilderDockWindow(UI_WindowName("TAB_UPDATE_NOTES"), dock.TopCenter);
			Gui::DockBuilderDockWindow(UI_WindowName("TAB_GAME_PREVIEW"), dock.TopCenter);
			Gui::DockBuilderDockWindow(UI_WindowName("TAB_AUDIO_TEST"), dock.TopCenter);
			Gui::DockBuilderDockWindow(UI_WindowName("TAB_TJA_IMPORT_TEST"), dock.TopCenter);
			Gui::DockBuilderDockWindow("Dear ImGui Demo", dock.TopCenter);
			Gui::DockBuilderDockWindow("ImGui Style Editor", dock.TopCenter);

			Gui::DockBuilderDockWindow(UI_WindowName("TAB_UNDO_HISTORY"), dock.TopRight);
			Gui::DockBuilderDockWindow(UI_WindowName("TAB_CHART_PROPERTIES"), dock.TopRight);

			Gui::DockBuilderDockWindow(UI_WindowName("TAB_INSPECTOR"), dock.TopRightBot);
		}
		Gui::DockBuilderFinish(dockSpaceID);
	}

	ApplicationHost::CloseResponse ChartEditor::OnWindowCloseRequest()
	{
		if (context.Undo.HasPendingChanges)
		{
			tryToCloseApplicationOnNextFrame = true;
			return ApplicationHost::CloseResponse::SupressExit;
		}
		else
		{
			return ApplicationHost::CloseResponse::Exit;
		}
	}

	void ChartEditor::UpdateApplicationWindowTitle(const ChartContext& context)
	{
		ApplicationHost::GlobalState.SetWindowTitleNextFrame = PeepoDrumKitApplicationTitle;
		ApplicationHost::GlobalState.SetWindowTitleNextFrame += " - ";
		if (!context.ChartFilePath.empty())
			ApplicationHost::GlobalState.SetWindowTitleNextFrame += Path::GetFileName(context.ChartFilePath);
		else
			ApplicationHost::GlobalState.SetWindowTitleNextFrame += UntitledChartFileName;

		if (context.Undo.HasPendingChanges)
			ApplicationHost::GlobalState.SetWindowTitleNextFrame += "*";
	}

	void ChartEditor::CreateNewChart(ChartContext& context)
	{
		if (loadSongFuture.valid()) loadSongFuture.get();
		if (!context.SongSourceFilePath.empty()) StartAsyncLoadingSongAudioFile("");
		if (loadJacketFuture.valid()) loadJacketFuture.get();
		if (!context.SongJacketFilePath.empty()) StartAsyncLoadingSongJacketFile("");
		if (importChartFuture.valid()) importChartFuture.get();
		InternalUpdateAsyncLoading();

		createBackupOfOriginalTJABeforeOverwriteSave = false;
		context.Chart = {};
		context.ChartFilePath.clear();
		context.ResetChartsCompared();
		context.SetSelectedChart(
			context.Chart.Courses.empty() ?
				context.Chart.Courses.emplace_back(std::make_unique<ChartCourse>()).get()
				: context.Chart.Courses.front().get(),
			BranchType::Normal);
		SetChartDefaultSettingsAndCourses(context.Chart);

		context.SetIsPlayback(false);
		context.SetCursorBeat(Beat::Zero());
		context.Undo.ClearAll();

		timeline.Camera.PositionTarget.x = TimelineCameraBaseScrollX;
		timeline.Camera.ZoomTarget = vec2(1.0f);
	}
	
	void ChartEditor::CreateNewDifficulty(ChartContext& context, DifficultyType difficulty)
	{
		auto ccourse = std::make_unique<ChartCourse>();

		ccourse->Type = difficulty;

		for (auto& course : context.Chart.Courses)
		{
			ccourse->TempoMap.Tempo.Sorted = course->TempoMap.Tempo.Sorted;
			ccourse->TempoMap.Signature.Sorted = course->TempoMap.Signature.Sorted;
			ccourse->TempoMap.RebuildAccelerationStructure();
			break;
		}

		context.SetSelectedChart(context.Chart.Courses.emplace_back(std::move(ccourse)).get(), BranchType::Normal);
	}

	void ChartEditor::SaveChart(ChartContext& context, std::string_view filePath)
	{
		if (filePath.empty())
			filePath = context.ChartFilePath;

		assert(!filePath.empty());
		if (!filePath.empty())
		{
			TJA::ParsedTJA tja;
			ConvertChartProjectToTJA(context.Chart, tja);
			std::string tjaText;
			TJA::ConvertParsedToText(tja, tjaText, TJA::Encoding::UTF8);

			// TODO: Proper async file saving by copying in-memory
			if (createBackupOfOriginalTJABeforeOverwriteSave)
			{
				static constexpr b8 overwriteExisting = false;
				const std::string originalFileBackupPath { std::string(filePath).append(".bak") };

				File::Copy(filePath, originalFileBackupPath, overwriteExisting);
				createBackupOfOriginalTJABeforeOverwriteSave = false;
			}

			File::WriteAllBytes(filePath, tjaText);

			context.ChartFilePath = filePath;
			context.Undo.ClearChangesWereMade();

			PersistentApp.RecentFiles.Add(std::string { filePath });
		}
	}

	b8 ChartEditor::OpenChartSaveAsDialog(ChartContext& context)
	{
		static constexpr auto getChartFileNameWithoutExtensionOrDefault = [](const ChartContext& context) -> std::string_view
		{
			if (!context.ChartFilePath.empty()) return Path::GetFileName(context.ChartFilePath, false);
			if (!context.Chart.ChartTitle.Base().empty()) return context.Chart.ChartTitle.Base();
			return Path::TrimExtension(UntitledChartFileName);
		};

		Shell::FileDialog fileDialog {};
		fileDialog.InTitle = "Save Chart As";
		fileDialog.InFileName = getChartFileNameWithoutExtensionOrDefault(context);
		fileDialog.InDefaultExtension = TJA::Extension;
		fileDialog.InFilters = { { TJA::FilterName, TJA::FilterSpec }, { Shell::AllFilesFilterName, Shell::AllFilesFilterSpec }, };
		fileDialog.InParentWindowHandle = ApplicationHost::GlobalState.NativeWindowHandle;

		if (fileDialog.OpenSave() != Shell::FileDialogResult::OK)
			return false;

		SaveChart(context, fileDialog.OutFilePath);
		return true;
	}

	b8 ChartEditor::TrySaveChartOrOpenSaveAsDialog(ChartContext& context)
	{
		if (context.ChartFilePath.empty())
			return OpenChartSaveAsDialog(context);
		else
			SaveChart(context);
		return true;
	}

	void ChartEditor::StartAsyncImportingChartFile(std::string_view absoluteChartFilePath)
	{
		if (importChartFuture.valid())
			importChartFuture.get();

		PersistentApp.RecentFiles.Add(std::string { absoluteChartFilePath });
		importChartFuture = std::async(std::launch::async, [tempPathCopy = std::string(absoluteChartFilePath)]() mutable->AsyncImportChartResult
		{
			AsyncImportChartResult result {};
			result.ChartFilePath = std::move(tempPathCopy);

			auto[fileContent, fileSize] = File::ReadAllBytes(result.ChartFilePath);
			if (fileContent == nullptr || fileSize == 0)
			{
				printf("Failed to read file '%.*s'\n", FmtStrViewArgs(result.ChartFilePath));
				return result;
			}

			assert(Path::HasExtension(result.ChartFilePath, TJA::Extension));

			const std::string_view fileContentView = std::string_view(reinterpret_cast<const char*>(fileContent.get()), fileSize);
			if (UTF8::HasBOM(fileContentView))
				result.TJA.FileContentUTF8 = UTF8::TrimBOM(fileContentView);
			else
				result.TJA.FileContentUTF8 = UTF8::FromShiftJIS(fileContentView);

			result.TJA.Lines = TJA::SplitLines(result.TJA.FileContentUTF8);
			result.TJA.Tokens = TJA::TokenizeLines(result.TJA.Lines);
			result.TJA.Parsed = ParseTokens(result.TJA.Tokens, result.TJA.ParseErrors);

			if (!CreateChartProjectFromTJA(result.TJA.Parsed, result.Chart))
			{
				printf("Failed to create chart from TJA file '%.*s'\n", FmtStrViewArgs(result.ChartFilePath));
				return result;
			}

			return result;
		});
	}
	
	void ChartEditor::StartAsyncLoadingSongJacketFile(std::string_view absoluteJacketFilePath)
	{
		if (loadJacketFuture.valid())
			loadJacketFuture.get();

		loadJacketFuture = std::async(std::launch::async, [tempPathCopy = std::string(absoluteJacketFilePath)]() ->AsyncLoadJacketResult
			{
				AsyncLoadJacketResult result{};
				result.JacketFilePath = std::move(tempPathCopy);

				auto [fileContent, fileSize] = File::ReadAllBytes(result.JacketFilePath);
				if (fileContent == nullptr || fileSize == 0)
				{
					printf("Failed to read file '%.*s'\n", FmtStrViewArgs(result.JacketFilePath));
					return result;
				}

				return result;
			});
	}

	void ChartEditor::StartAsyncLoadingSongAudioFile(std::string_view absoluteAudioFilePath)
	{
		if (loadSongFuture.valid())
			loadSongFuture.get();

		context.SongWaveformFadeAnimationTarget = 0.0f;
		loadSongStopwatch.Restart();
		loadSongFuture = std::async(std::launch::async, [tempPathCopy = std::string(absoluteAudioFilePath)]()->AsyncLoadSongResult
		{
			AsyncLoadSongResult result {};
			result.SongFilePath = std::move(tempPathCopy);

			// TODO: Maybe handle this in a different way... but for now loading an empty file path works as an "unload"
			if (result.SongFilePath.empty())
				return result;

			auto[fileContent, fileSize] = File::ReadAllBytes(result.SongFilePath);
			if (fileContent == nullptr || fileSize == 0)
			{
				printf("Failed to read file '%.*s'\n", FmtStrViewArgs(result.SongFilePath));
				return result;
			}

			if (Audio::DecodeEntireFile(result.SongFilePath, fileContent.get(), fileSize, result.SampleBuffer) != Audio::DecodeFileResult::FeelsGoodMan)
			{
				printf("Failed to decode audio file '%.*s'\n", FmtStrViewArgs(result.SongFilePath));
				return result;
			}

			// HACK: ...
			if (result.SampleBuffer.SampleRate != Audio::Engine.OutputSampleRate)
				Audio::LinearlyResampleBuffer<i16>(result.SampleBuffer.InterleavedSamples, result.SampleBuffer.FrameCount, result.SampleBuffer.SampleRate, result.SampleBuffer.ChannelCount, Audio::Engine.OutputSampleRate);

			if (result.SampleBuffer.ChannelCount > 0) result.WaveformL.GenerateEntireMipChainFromSampleBuffer(result.SampleBuffer, 0);
#if !PEEPO_DEBUG // NOTE: Always ignore the second channel in debug builds for performance reasons!
			if (result.SampleBuffer.ChannelCount > 1) result.WaveformR.GenerateEntireMipChainFromSampleBuffer(result.SampleBuffer, 1);
#endif

			return result;
		});
	}

	void ChartEditor::SetAndStartLoadingChartSongFileName(std::string_view relativeOrAbsoluteAudioFilePath, Undo::UndoHistory& undo)
	{
		if (!relativeOrAbsoluteAudioFilePath.empty() && !Path::IsRelative(relativeOrAbsoluteAudioFilePath))
		{
			context.Chart.SongFileName = Path::TryMakeRelative(relativeOrAbsoluteAudioFilePath, context.ChartFilePath);
			if (context.Chart.SongFileName.empty())
				context.Chart.SongFileName = relativeOrAbsoluteAudioFilePath;
		}
		else
		{
			context.Chart.SongFileName = relativeOrAbsoluteAudioFilePath;
		}

		Path::NormalizeInPlace(context.Chart.SongFileName);
		undo.NotifyChangesWereMade();

		// NOTE: Construct a new absolute path even if the input path was already absolute
		//		 so that there won't ever be any discrepancy in case the relative path code is behaving unexpectedly
		StartAsyncLoadingSongAudioFile(Path::TryMakeAbsolute(context.Chart.SongFileName, context.ChartFilePath));
	}

	void ChartEditor::SetAndStartLoadingSongJacketFileName(std::string_view relativeOrAbsoluteAudioFilePath, Undo::UndoHistory& undo)
	{
		if (!relativeOrAbsoluteAudioFilePath.empty() && !Path::IsRelative(relativeOrAbsoluteAudioFilePath))
		{
			context.Chart.SongJacket = Path::TryMakeRelative(relativeOrAbsoluteAudioFilePath, context.ChartFilePath);
			if (context.Chart.SongJacket.empty())
				context.Chart.SongJacket = relativeOrAbsoluteAudioFilePath;
		}
		else
		{
			context.Chart.SongJacket = relativeOrAbsoluteAudioFilePath;
		}

		Path::NormalizeInPlace(context.Chart.SongJacket);
		undo.NotifyChangesWereMade();

		// NOTE: Construct a new absolute path even if the input path was already absolute
		//		 so that there won't ever be any discrepancy in case the relative path code is behaving unexpectedly
		StartAsyncLoadingSongJacketFile(Path::TryMakeAbsolute(context.Chart.SongJacket, context.ChartFilePath));
	}

	b8 ChartEditor::OpenLoadChartFileDialog(ChartContext& context)
	{
		Shell::FileDialog fileDialog {};
		fileDialog.InTitle = "Open Chart File";
		fileDialog.InFilters = { { TJA::FilterName, TJA::FilterSpec }, { Shell::AllFilesFilterName, Shell::AllFilesFilterSpec }, };
		fileDialog.InParentWindowHandle = ApplicationHost::GlobalState.NativeWindowHandle;

		if (fileDialog.OpenRead() != Shell::FileDialogResult::OK)
			return false;

		StartAsyncImportingChartFile(fileDialog.OutFilePath);
		return true;
	}

	b8 ChartEditor::OpenLoadAudioFileDialog(Undo::UndoHistory& undo)
	{
		Shell::FileDialog fileDialog {};
		fileDialog.InTitle = "Open Audio File";
		fileDialog.InFilters = { { "Audio Files", "*.flac;*.ogg;*.mp3;*.wav" }, { Shell::AllFilesFilterName, Shell::AllFilesFilterSpec }, };
		fileDialog.InParentWindowHandle = ApplicationHost::GlobalState.NativeWindowHandle;

		if (fileDialog.OpenRead() != Shell::FileDialogResult::OK)
			return false;

		SetAndStartLoadingChartSongFileName(fileDialog.OutFilePath, undo);
		return true;
	}

	b8 ChartEditor::OpenLoadJacketFileDialog(Undo::UndoHistory& undo)
	{
		Shell::FileDialog fileDialog {};
		fileDialog.InTitle = "Open Jacket File";
		fileDialog.InFilters = { { "Image Files", "*.jpg;*.jpeg;*.png" }, { Shell::AllFilesFilterName, Shell::AllFilesFilterSpec }, };
		fileDialog.InParentWindowHandle = ApplicationHost::GlobalState.NativeWindowHandle;

		if (fileDialog.OpenRead() != Shell::FileDialogResult::OK)
			return false;

		SetAndStartLoadingSongJacketFileName(fileDialog.OutFilePath, undo);
		return true;
	}

	void ChartEditor::CheckOpenSaveConfirmationPopupThenCall(std::function<void()> onSuccess)
	{
		if (context.Undo.HasPendingChanges)
		{
			saveConfirmationPopup.OpenOnNextFrame = true;
			saveConfirmationPopup.OnSuccessFunction = std::move(onSuccess);
		}
		else
		{
			onSuccess();
		}
	}

	void ChartEditor::InternalUpdateAsyncLoading()
	{
		context.Gfx.UpdateAsyncLoading();
		context.SfxVoicePool.UpdateAsyncLoading();

		if (importChartFuture.valid() && importChartFuture._Is_ready())
		{
			const Time previousChartSongOffset = context.Chart.SongOffset;

			AsyncImportChartResult loadResult = importChartFuture.get();

			// TODO: Maybe also do date version check (?)
			createBackupOfOriginalTJABeforeOverwriteSave = !loadResult.TJA.Parsed.HasPeepoDrumKitComment;

			context.Chart = std::move(loadResult.Chart);
			context.ChartFilePath = std::move(loadResult.ChartFilePath);
			context.ResetChartsCompared();
			context.SetSelectedChart(
				context.Chart.Courses.empty() ?
					context.Chart.Courses.emplace_back(std::make_unique<ChartCourse>()).get()
					: context.Chart.Courses.front().get(),
				BranchType::Normal);
			StartAsyncLoadingSongAudioFile(Path::TryMakeAbsolute(context.Chart.SongFileName, context.ChartFilePath));
			StartAsyncLoadingSongJacketFile(Path::TryMakeAbsolute(context.Chart.SongJacket, context.ChartFilePath));

			// NOTE: Prevent the cursor from changing screen position. Not needed if paused because a stable beat time is used instead
			if (context.GetIsPlayback())
				context.SetCursorTime(context.GetCursorTime() + (previousChartSongOffset - context.Chart.SongOffset));

			context.Undo.ClearAll();
		}

		// NOTE: Just in case there is something wrong with the animation, that could otherwise prevent the song from finishing to load
		static constexpr Time maxWaveformFadeOutDelaySafetyLimit = Time::FromSec(0.5);
		const b8 waveformHasFadedOut = (context.SongWaveformFadeAnimationCurrent <= 0.01f || loadSongStopwatch.GetElapsed() >= maxWaveformFadeOutDelaySafetyLimit);

		if (loadSongFuture.valid() && loadSongFuture._Is_ready() && waveformHasFadedOut)
		{
			loadSongStopwatch.Stop();
			AsyncLoadSongResult loadResult = loadSongFuture.get();
			context.SongSourceFilePath = std::move(loadResult.SongFilePath);
			context.SongWaveformL = std::move(loadResult.WaveformL);
			context.SongWaveformR = std::move(loadResult.WaveformR);
			context.SongWaveformFadeAnimationTarget = context.SongWaveformL.IsEmpty() ? 0.0f : 1.0f;

			// TODO: Maybe handle this differently...
			if (context.Chart.ChartTitle.Base().empty() && !context.SongSourceFilePath.empty())
				context.Chart.ChartTitle.Base() = Path::GetFileName(context.SongSourceFilePath, false);

			if (context.Chart.ChartDuration.Seconds <= 0.0 && loadResult.SampleBuffer.SampleRate > 0)
				context.Chart.ChartDuration = Audio::FramesToTime(loadResult.SampleBuffer.FrameCount, loadResult.SampleBuffer.SampleRate);

			if (context.SongSource != Audio::SourceHandle::Invalid)
				Audio::Engine.UnloadSource(context.SongSource);

			context.SongSource = Audio::Engine.LoadSourceFromBufferMove(Path::GetFileName(context.SongSourceFilePath), std::move(loadResult.SampleBuffer));
			context.SongVoice.SetSource(context.SongSource);

			Audio::Engine.EnsureStreamRunning();
		}

		if (loadJacketFuture.valid() && loadJacketFuture._Is_ready())
		{
			AsyncLoadJacketResult loadResult = loadJacketFuture.get();

			context.SongJacketFilePath = std::move(loadResult.JacketFilePath);
		}
	}
}
