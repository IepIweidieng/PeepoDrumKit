#include "chart_editor_settings.h"
#include "core_build_info.h"
#include "core_string.h"
#include "core_io.h"
#include <algorithm>

namespace PeepoDrumKit
{
	void RecentFilesList::Add(std::string newFilePath, b8 addToEnd)
	{
		Path::NormalizeInPlace(newFilePath);
		erase_remove_if(SortedPaths, [&](const auto& path) { return ASCII::MatchesInsensitive(path, newFilePath); });
		SortedPaths.reserve(MaxCount);
		if (addToEnd)
			SortedPaths.push_back(std::move(newFilePath));
		else
			SortedPaths.emplace(SortedPaths.begin(), std::move(newFilePath));
		if (SortedPaths.size() > MaxCount)
			SortedPaths.resize(MaxCount);
	}

	void RecentFilesList::Remove(std::string filePathToRemove)
	{
		Path::NormalizeInPlace(filePathToRemove);
		erase_remove_if(SortedPaths, [&](const auto& path) { return ASCII::MatchesInsensitive(path, filePathToRemove); });
		if (SortedPaths.size() > MaxCount)
			SortedPaths.resize(MaxCount);
	}

	static cstr BoolToString(b8 in)
	{
		return in ? "true" : "false";
	}

	static cstr BoolToString(Optional_B8 in)
	{
		return in.IsTrue() ? "true" : in.IsFalse() ? "false" : "";
	}

	static b8 BoolFromString(std::string_view in, b8& out)
	{
		if (in == "") { return true; }
		if (in == "true") { out = true; return true; }
		if (in == "false") { out = false; return true; }
		if (i32 v = 0; ASCII::TryParse(in, v)) { out = (v != 0); return true; }
		return false;
	}

	static b8 BoolFromString(std::string_view in, Optional_B8& out)
	{
		if (in == "") { out.Reset(); return true; }
		if (in == "true") { out.SetValue(true); return true; }
		if (in == "false") { out.SetValue(false); return true; }
		if (i32 v = 0; ASCII::TryParse(in, v)) { out.SetValue((v != 0)); return true; }
		return false;
	}

	static i32 RectToTLSizeString(char* buffer, size_t bufferSize, const Rect& in)
	{
		return sprintf_s(buffer, bufferSize, "%g, %g, %g, %g", in.TL.x, in.TL.y, in.GetWidth(), in.GetHeight());
	}

	static b8 RectFromTLSizeString(std::string_view in, Rect& out)
	{
		b8 outSuccess = true; vec2 outTL {}, outSize {};
		ASCII::ForEachInCommaSeparatedList(in, [&, index = 0](std::string_view value) mutable
		{
			value = ASCII::Trim(value);
			if (index == 0) { outSuccess &= ASCII::TryParse(value, outTL.x); }
			if (index == 1) { outSuccess &= ASCII::TryParse(value, outTL.y); }
			if (index == 2) { outSuccess &= ASCII::TryParse(value, outSize.x); }
			if (index == 3) { outSuccess &= ASCII::TryParse(value, outSize.y); }
			index++;
		});
		out = Rect::FromTLSize(outTL, outSize); return outSuccess;
	}

	namespace Ini
	{
		constexpr IniMemberParseResult MemberParseError(cstr errorMessage) { return IniMemberParseResult { true, errorMessage }; }

		static IniMemberParseResult FromString(std::string_view stringToParse, i32& out)
		{
			return ASCII::TryParse(stringToParse, out) ? IniMemberParseResult {} : MemberParseError("Invalid int");
		}

		static void ToString(const i32& in, std::string& stringToAppendTo)
		{
			char b[32]; stringToAppendTo += std::string_view(b, sprintf_s(b, "%d", in));
		}

		static IniMemberParseResult FromString(std::string_view stringToParse, f32& out)
		{
			return ASCII::TryParse(stringToParse, out) ? IniMemberParseResult {} : MemberParseError("Invalid float");
		}

		static void ToString(const f32& in, std::string& stringToAppendTo)
		{
			char b[64]; stringToAppendTo += std::string_view(b, sprintf_s(b, "%g", in));
		}

		static IniMemberParseResult FromString(std::string_view stringToParse, b8& out)
		{
			return BoolFromString(stringToParse, out) ? IniMemberParseResult {} : MemberParseError("Invalid bool");
		}

		static void ToString(const b8& in, std::string& stringToAppendTo)
		{
			stringToAppendTo += BoolToString(in);
		}

		static IniMemberParseResult FromString(std::string_view stringToParse, std::string& out)
		{
			out = stringToParse; return {};
		}

		static void ToString(const std::string& in, std::string& stringToAppendTo)
		{
			stringToAppendTo += in;
		}

		static IniMemberParseResult FromString(std::string_view stringToParse, Beat& out)
		{
			return ASCII::TryParse(stringToParse, out.Ticks) ? IniMemberParseResult {} : MemberParseError("Invalid int");
		}

		static void ToString(const Beat& in, std::string& stringToAppendTo)
		{
			char b[32]; stringToAppendTo += std::string_view(b, sprintf_s(b, "%d", in.Ticks));
		}

		static IniMemberParseResult FromString(std::string_view stringToParse, CustomSelectionPattern& out)
		{
			CopyStringViewIntoFixedBuffer(out.Data, stringToParse);
			return {};
		}

		static void ToString(const CustomSelectionPattern& in, std::string& stringToAppendTo)
		{
			stringToAppendTo += FixedBufferStringView(in.Data);
		}

		static IniMemberParseResult FromString(std::string_view stringToParse, CustomScaleRatio& out)
		{
			IniMemberParseResult res = {};
			ASCII::ForEachInCharSeparatedList(stringToParse, ':', [&, i = 0](std::string_view part) mutable
			{
				auto resI = (i >= 2) ? MemberParseError("More than 2 ratio components")
					: FromString(ASCII::Trim(part), out.TimeRatio[i++]);
				if (resI.HasError)
					res = resI;
			});
			return res;
		}

		static void ToString(const CustomScaleRatio& in, std::string& stringToAppendTo)
		{
			ToString(in.TimeRatio[0], stringToAppendTo);
			stringToAppendTo += ":";
			ToString(in.TimeRatio[1], stringToAppendTo);
		}

		template <typename TOption, typename TIn = TOption, typename FnConvert = void>
		static IniMemberParseResult FromString(std::string_view stringToParse, std::vector<TOption>& out, FnConvert fnConvert)
		{
			b8 hasAnyError = false;
			cstr lastErrorMessage = nullptr;
			size_t expectedCount = 0;
			ASCII::ForEachInCommaSeparatedList(stringToParse, [&](std::string_view) { expectedCount++; });
			out.clear();
			out.reserve(expectedCount);
			ASCII::ForEachInCommaSeparatedList(stringToParse, [&](std::string_view commaSeparatedValue)
			{
				TIn v = {};
				IniMemberParseResult res = FromString(ASCII::Trim(commaSeparatedValue), v);
				if (res.HasError) {
					hasAnyError = true;
					lastErrorMessage = res.ErrorMessage;
				}
				else {
					out.push_back(fnConvert(v));
				}
			});
			return hasAnyError ? MemberParseError(lastErrorMessage) : IniMemberParseResult{};
		}

		template <typename TOption, typename FnConvert>
		static void ToString(const std::vector<TOption>& in, std::string& stringToAppendTo, FnConvert fnConvert)
		{
			for (size_t i = 0; i < in.size(); i++) {
				if (i > 0)
					stringToAppendTo += ", ";
				ToString(fnConvert(in[i]), stringToAppendTo);
			}
		}

		template <typename TOption>
		static IniMemberParseResult FromString(std::string_view stringToParse, std::vector<TOption>& out)
		{
			return FromString(stringToParse, out, [](auto&& x) { return x; });
		}

		template <typename TOption>
		static void ToString(const std::vector<TOption>& in, std::string& stringToAppendTo)
		{
			ToString(in, stringToAppendTo, [](auto&& x) { return x; });
		}

		static IniMemberParseResult FromString(std::string_view stringToParse, PlaybackSpeedStepList& out)
		{
			return FromString(stringToParse, out.V, FromPercent);
		}

		static void ToString(const PlaybackSpeedStepList& in, std::string& stringToAppendTo)
		{
			ToString(in.V, stringToAppendTo, ToPercent);
		}

		static IniMemberParseResult FromString(std::string_view stringToParse, CustomSelectionPatternList& out)
		{
			return FromString(stringToParse, out.V);
		}

		static void ToString(const CustomSelectionPatternList& in, std::string& stringToAppendTo)
		{
			ToString(in.V, stringToAppendTo);
		}

		static IniMemberParseResult FromString(std::string_view stringToParse, MultiInputBinding& out)
		{
			return InputBindingFromStorageString(stringToParse, out) ? IniMemberParseResult {} : MemberParseError("Invalid input binding");
		}

		static void ToString(const MultiInputBinding& in, std::string& stringToAppendTo)
		{
			InputBindingToStorageString(in, stringToAppendTo);
		}

		// NOTE: Rely entirely on overload resolution for these
		template<typename T> static IniMemberParseResult VoidPtrToTypeWrapper(std::string_view stringToParse, IniMemberVoidPtr out)
		{
			assert(out.Address != nullptr && out.ByteSize == sizeof(T));
			return FromString(stringToParse, *static_cast<T*>(out.Address));
		}
		template<typename T> static void VoidPtrToTypeWrapper(IniMemberVoidPtr in, std::string& stringToAppendTo)
		{
			assert(in.Address != nullptr && in.ByteSize == sizeof(T));
			ToString(*static_cast<const T*>(in.Address), stringToAppendTo);
		}

		struct IniWriter
		{
			std::string& Out;
			char Buffer[4096];

			inline void Line() { Out += '\n'; }
			inline void Line(std::string_view line) { Out += line; Out += '\n'; }
			inline void LineSection(std::string_view section) { Out += '['; Out += section; Out += "]\n"; }
			inline void LineKeyValue_Str(std::string_view key, std::string_view value) { Out += key; Out += " = "; Out += value; Out += '\n'; }
			inline void LineKeyValue_I32(std::string_view key, i32 value) { LineKeyValue_Str(key, std::string_view(Buffer, sprintf_s(Buffer, "%d", value))); }
			inline void LineKeyValue_F32(std::string_view key, f32 value) { LineKeyValue_Str(key, std::string_view(Buffer, sprintf_s(Buffer, "%g", value))); }
			inline void LineKeyValue_F64(std::string_view key, f64 value) { LineKeyValue_Str(key, std::string_view(Buffer, sprintf_s(Buffer, "%g", value))); }

			inline void Comment() { Out += "; "; }
			inline void LineComment(std::string_view comment) { Out += "; "; Out += comment; Out += '\n'; }
		};
	}

	constexpr size_t SizeOfPersistentAppData = sizeof(PersistentAppData);
	static_assert(PEEPO_RELEASE || SizeOfPersistentAppData == 136, "TODO: Add missing ini file handling for newly added PersistentAppData fields");

	SettingsParseResult ParseSettingsIni(std::string_view fileContent, PersistentAppData& out)
	{
		Ini::IniParser parser = {};

		parser.ForEachIniKeyValueLine(fileContent,
			[&](const Ini::IniParser::SectionIt& it)
		{
			return;
		}, [&](const Ini::IniParser::KeyValueIt& it)
		{
			const std::string_view in = it.Value;
			if (parser.CurrentSection == "last_session")
			{
				if (it.Key == "")
				{
					if (in == "file_version")
					{
						// TODO: ...
					}
				}
				else if (it.Key == "gui_scale") { if (f32 v = out.LastSession.GuiScale; ASCII::TryParse(in, v)) out.LastSession.GuiScale = FromPercent(v); else return parser.Error_InvalidFloat(); }
				else if (it.Key == "gui_language") { out.LastSession.GuiLanguage = in; }
				else if (it.Key == "os_window_swap_interval") { if (!ASCII::TryParse(in, out.LastSession.OSWindow_SwapInterval)) return parser.Error_InvalidInt(); }
				else if (it.Key == "os_window_region") { if (!RectFromTLSizeString(in, out.LastSession.OSWindow_Region)) return parser.Error_InvalidRect(); }
				else if (it.Key == "os_window_region_restore") { if (!RectFromTLSizeString(in, out.LastSession.OSWindow_RegionRestore)) return parser.Error_InvalidRect(); }
				else if (it.Key == "os_window_is_fullscreen") { if (!BoolFromString(in, out.LastSession.OSWindow_IsFullscreen)) return parser.Error_InvalidBool(); }
				else if (it.Key == "os_window_is_maximized") { if (!BoolFromString(in, out.LastSession.OSWindow_IsMaximized)) return parser.Error_InvalidBool(); }
				else if (it.Key == "show_window_test_menu") { if (!BoolFromString(in, out.LastSession.ShowWindow_TestMenu)) return parser.Error_InvalidBool(); }
				else if (it.Key == "show_window_help") { if (!BoolFromString(in, out.LastSession.ShowWindow_Help)) return parser.Error_InvalidBool(); }
				else if (it.Key == "show_window_update_notes") { if (!BoolFromString(in, out.LastSession.ShowWindow_UpdateNotes)) return parser.Error_InvalidBool(); }
				else if (it.Key == "show_window_chart_stats") { if (!BoolFromString(in, out.LastSession.ShowWindow_ChartStats)) return parser.Error_InvalidBool(); }
				else if (it.Key == "show_window_settings") { if (!BoolFromString(in, out.LastSession.ShowWindow_Settings)) return parser.Error_InvalidBool(); }
				else if (it.Key == "show_window_audio_test") { if (!BoolFromString(in, out.LastSession.ShowWindow_AudioTest)) return parser.Error_InvalidBool(); }
				else if (it.Key == "show_window_tja_import_test") { if (!BoolFromString(in, out.LastSession.ShowWindow_TJAImportTest)) return parser.Error_InvalidBool(); }
				else if (it.Key == "show_window_tja_export_test") { if (!BoolFromString(in, out.LastSession.ShowWindow_TJAExportTest)) return parser.Error_InvalidBool(); }
				else if (it.Key == "show_window_imgui_demo") { if (!BoolFromString(in, out.LastSession.ShowWindow_ImGuiDemo)) return parser.Error_InvalidBool(); }
				else if (it.Key == "show_window_imgui_style_editor") { if (!BoolFromString(in, out.LastSession.ShowWindow_ImGuiStyleEditor)) return parser.Error_InvalidBool(); }
			}
			else if (parser.CurrentSection == "recent_files")
			{
				const std::string_view indexSuffix = ASCII::TrimPrefix(it.Key, "file_");
				if (i32 v = 0; ASCII::TryParse(indexSuffix, v))
					out.RecentFiles.Add(std::string { in }, true);
				else
					return parser.Error("Invalid file index");
			}
		});

		return parser.Result;
	}

	void SettingsToIni(const PersistentAppData& in, std::string& out)
	{
		Ini::IniWriter writer { out };
		char b[512], keyBuffer[64];

		writer.LineComment(std::string_view(b, sprintf_s(b, "PeepoDrumKit %s", BuildInfo::CompilationDateParsed.ToString().Data)));
		writer.LineKeyValue_Str("file_version", std::string_view(b, sprintf_s(b, "%d.%d.%d", 1, 0, 0)));
		writer.Line();

		writer.LineSection("last_session");
		writer.LineKeyValue_F32("gui_scale", ToPercent(in.LastSession.GuiScale));
		writer.LineKeyValue_Str("gui_language", in.LastSession.GuiLanguage);
		writer.LineKeyValue_I32("os_window_swap_interval", in.LastSession.OSWindow_SwapInterval);
		writer.LineKeyValue_Str("os_window_region", std::string_view(b, RectToTLSizeString(b, sizeof(b), in.LastSession.OSWindow_Region)));
		writer.LineKeyValue_Str("os_window_region_restore", std::string_view(b, RectToTLSizeString(b, sizeof(b), in.LastSession.OSWindow_RegionRestore)));
		writer.LineKeyValue_Str("os_window_is_fullscreen", BoolToString(in.LastSession.OSWindow_IsFullscreen));
		writer.LineKeyValue_Str("os_window_is_maximized", BoolToString(in.LastSession.OSWindow_IsMaximized));
		writer.LineKeyValue_Str("show_window_test_menu", BoolToString(in.LastSession.ShowWindow_TestMenu));
		writer.LineKeyValue_Str("show_window_help", BoolToString(in.LastSession.ShowWindow_Help));
		writer.LineKeyValue_Str("show_window_update_notes", BoolToString(in.LastSession.ShowWindow_UpdateNotes));
		writer.LineKeyValue_Str("show_window_chart_stats", BoolToString(in.LastSession.ShowWindow_ChartStats));
		writer.LineKeyValue_Str("show_window_settings", BoolToString(in.LastSession.ShowWindow_Settings));
		writer.LineKeyValue_Str("show_window_audio_test", BoolToString(in.LastSession.ShowWindow_AudioTest));
		writer.LineKeyValue_Str("show_window_tja_import_test", BoolToString(in.LastSession.ShowWindow_TJAImportTest));
		writer.LineKeyValue_Str("show_window_tja_export_test", BoolToString(in.LastSession.ShowWindow_TJAExportTest));
		writer.LineKeyValue_Str("show_window_imgui_demo", BoolToString(in.LastSession.ShowWindow_ImGuiDemo));
		writer.LineKeyValue_Str("show_window_imgui_style_editor", BoolToString(in.LastSession.ShowWindow_ImGuiStyleEditor));
		writer.Line();

		writer.LineSection("recent_files");
		if (!in.RecentFiles.SortedPaths.empty())
			for (size_t i = 0; i < in.RecentFiles.SortedPaths.size(); i++)
				writer.LineKeyValue_Str(std::string_view(keyBuffer, sprintf_s(keyBuffer, "file_%02zu", i)), in.RecentFiles.SortedPaths[i]);
		else
			writer.LineComment("...");
	}

	SettingsParseResult ParseSettingsIni(std::string_view fileContent, UserSettingsData& out)
	{
		Ini::IniParser parser = {};

		parser.ForEachIniKeyValueLine(fileContent,
			[&](const Ini::IniParser::SectionIt& it)
		{
			return;
		}, [&](const Ini::IniParser::KeyValueIt& it)
		{
			if (parser.CurrentSection == "" && it.Value == "file_version")
			{
				// TODO: ...
			}

			for (size_t i = 0; i < AppSettingsReflectionMap.MemberCount; i++)
			{
				const auto& member = AppSettingsReflectionMap.MemberSlots[i];
				if (parser.CurrentSection == member.SerializedSection && it.Key == member.SerializedName)
				{
					const IniMemberParseResult memberParseResult = member.FromStringFunc(it.Value, { reinterpret_cast<u8*>(&out) + member.ByteOffsetValue, member.ByteSizeValue });
					if (!memberParseResult.HasError)
						*(reinterpret_cast<b8*>(reinterpret_cast<u8*>(&out) + member.ByteOffsetHasValueB8)) = true;
					else
						return parser.Error(memberParseResult.ErrorMessage);
					break;
				}
			}
		});

		out.General.DrumrollAutoHitBarDivision.Value = Clamp(out.General.DrumrollAutoHitBarDivision.Value, 1, Beat::TicksPerBeat);

		return parser.Result;
	}

	void SettingsToIni(const UserSettingsData& in, std::string& out)
	{
		Ini::IniWriter writer { out };
		char b[512];
		std::string strBuffer; strBuffer.reserve(256);

		writer.LineComment(std::string_view(b, sprintf_s(b, "PeepoDrumKit %s", BuildInfo::CompilationDateParsed.ToString().Data)));
		writer.LineKeyValue_Str("file_version", std::string_view(b, sprintf_s(b, "%d.%d.%d", 1, 0, 0)));
		cstr lastSection = nullptr;

		for (size_t i = 0; i < AppSettingsReflectionMap.MemberCount; i++)
		{
			const auto& member = AppSettingsReflectionMap.MemberSlots[i];
			if (member.SerializedSection != lastSection) { writer.Line(); writer.LineSection(member.SerializedSection); lastSection = member.SerializedSection; }

			if (!*(reinterpret_cast<const b8*>(reinterpret_cast<const u8*>(&in) + member.ByteOffsetHasValueB8)))
			{
				writer.Comment();
				member.ToStringFunc({ const_cast<u8*>(reinterpret_cast<const u8*>(&in)) + member.ByteOffsetDefault, member.ByteSizeValue }, strBuffer);
			}
			else
			{
				member.ToStringFunc({ const_cast<u8*>(reinterpret_cast<const u8*>(&in)) + member.ByteOffsetValue, member.ByteSizeValue }, strBuffer);
			}

			writer.LineKeyValue_Str(member.SerializedName, strBuffer);
			strBuffer.clear();
		}
	}

	constexpr size_t SizeOfUserSettingsData = sizeof(UserSettingsData);
	//static_assert(PEEPO_RELEASE || SizeOfUserSettingsData == 6120, "TODO: Add missing reflection entries for newly added UserSettingsData fields");

	SettingsReflectionMap StaticallyInitializeAppSettingsReflectionMap()
	{
		// TODO: Static assert size to not forget about adding new members
		SettingsReflectionMap outMap {};
		cstr currentSection = "";

#define SECTION(serializedSection) do { currentSection = serializedSection; } while (false)
#define X(member, serializedName) do																			\
		{																										\
			SettingsReflectionMember& out = outMap.MemberSlots[outMap.MemberCount++];							\
			out.ByteSizeValue = static_cast<u16>(sizeof(UserSettingsData::member.Value));						\
			out.ByteOffsetDefault = static_cast<u16>(offsetof(UserSettingsData, member.Default));				\
			out.ByteOffsetValue = static_cast<u16>(offsetof(UserSettingsData, member.Value));					\
			out.ByteOffsetHasValueB8 = static_cast<u16>(offsetof(UserSettingsData, member.HasValue));			\
			out.SourceCodeName = #member;																		\
			out.SerializedSection = currentSection;																\
			out.SerializedName = serializedName;																\
			out.FromStringFunc = Ini::VoidPtrToTypeWrapper<decltype(UserSettingsData::member)::UnderlyingType>;	\
			out.ToStringFunc = Ini::VoidPtrToTypeWrapper<decltype(UserSettingsData::member)::UnderlyingType>;	\
		} while (false)
		{
			SECTION("general");
			X(General.DefaultCreatorName, "default_creator_name");
			X(General.DrumrollAutoHitBarDivision, "drumroll_auto_hit_bar_division");
			X(General.DisplayTimeInSongSpace, "display_time_in_song_space");
			X(General.TimelineScrollInvertMouseWheel, "timeline_scroll_invert_mouse_wheel");
			X(General.TimelineScrollDistancePerMouseWheelTick, "timeline_scroll_distance_per_mouse_wheel_tick");
			X(General.TimelineScrollDistancePerMouseWheelTickFast, "timeline_scroll_distance_per_mouse_wheel_tick_fast");
			X(General.TimelineZoomFactorPerMouseWheelTick, "timeline_zoom_factor_per_mouse_wheel_tick");
			X(General.TimelineScrubAutoScrollEnableClamp, "timeline_scrub_auto_scroll_enable_clamp");
			X(General.TimelineScrubAutoScrollPixelThreshold, "timeline_scrub_auto_scroll_pixel_threshold");
			X(General.TimelineScrubAutoScrollSpeedMin, "timeline_scrub_auto_scroll_speed_min");
			X(General.TimelineScrubAutoScrollSpeedMax, "timeline_scrub_auto_scroll_speed_max");
			X(General.PlaybackSpeedSteps, "playback_speed_steps");
			X(General.PlaybackSpeedStepsRough, "playback_speed_steps_rough");
			X(General.PlaybackSpeedStepsPrecise, "playback_speed_steps_precise");
			X(General.DisableTempoWindowWidgetsIfHasSelection, "disable_tempo_window_widgets_if_has_selection");
			X(General.ConvertSelectionToScrollChanges_UnselectOld, "convert_selection_to_scroll_changes_unselect_old");
			X(General.ConvertSelectionToScrollChanges_SelectNew, "convert_selection_to_scroll_changes_select_new");
			X(General.CustomSelectionPatterns, "custom_selection_patterns");
			X(General.CustomScaleRatios, "custom_scale_ratios");
			X(General.TransformScale_ByTempo, "transform_scale_by_tempo");
			X(General.TransformScale_KeepTimePosition, "transform_scale_keep_time_position");
			X(General.TransformScale_KeepTimeSignature, "transform_scale_keep_time_signature");
			X(General.TransformScale_KeepItemDuration, "transform_scale_keep_item_duration");

			SECTION("audio");
			X(Audio.OpenDeviceOnStartup, "open_device_on_startup");
			X(Audio.CloseDeviceOnIdleFocusLoss, "close_device_on_idle_focus_loss");
			X(Audio.RequestExclusiveDeviceAccess, "request_exclusive_device_access");

			SECTION("animation");
			X(Animation.EnableGuiScaleAnimation, "enable_gui_scale_animation");
			X(Animation.TimelineSmoothScrollSpeed, "timeline_smooth_scroll_speed");
			X(Animation.TimelineWaveformFadeSpeed, "timeline_waveform_fade_speed");
			X(Animation.TimelineRangeSelectionExpansionSpeed, "timeline_range_selection_expansion_speed");
			X(Animation.TimelineWorldSpaceCursorXSpeed, "timeline_world_space_cursor_x_speed");
			X(Animation.TimelineGridSnapLineSpeed, "timeline_grid_snap_line_speed");
			X(Animation.TimelineGoGoRangeExpansionSpeed, "timeline_gogo_range_expansion_speed");

			SECTION("input");
			X(Input.Dialog_YesOrOk, "dialog_yes_or_ok");
			X(Input.Dialog_No, "dialog_no");
			X(Input.Dialog_Cancel, "dialog_cancel");
			X(Input.Dialog_SelectNextTab, "dialog_select_next_tab");
			X(Input.Dialog_SelectPreviousTab, "dialog_select_previous_tab");
			X(Input.Editor_ToggleFullscreen, "editor_toggle_fullscreen");
			X(Input.Editor_ToggleVSync, "editor_toggle_vsync");
			X(Input.Editor_IncreaseGuiScale, "editor_increase_gui_scale");
			X(Input.Editor_DecreaseGuiScale, "editor_decrease_gui_scale");
			X(Input.Editor_ResetGuiScale, "editor_reset_gui_scale");
			X(Input.Editor_Undo, "editor_undo");
			X(Input.Editor_Redo, "editor_redo");
			X(Input.Editor_OpenHelp, "editor_open_help");
			X(Input.Editor_OpenUpdateNotes, "editor_open_update_notes");
			X(Input.Editor_OpenChartStats, "editor_open_chart_stats");
			X(Input.Editor_OpenSettings, "editor_open_settings");
			X(Input.Editor_ChartNew, "editor_chart_new");
			X(Input.Editor_ChartOpen, "editor_chart_open");
			X(Input.Editor_ChartOpenDirectory, "editor_chart_open_directory");
			X(Input.Editor_ChartSave, "editor_chart_save");
			X(Input.Editor_ChartSaveAs, "editor_chart_save_as");
			X(Input.Timeline_PlaceNoteDon, "timeline_place_note_don");
			X(Input.Timeline_PlaceNoteKa, "timeline_place_note_ka");
			X(Input.Timeline_PlaceNoteBalloon, "timeline_place_note_balloon");
			X(Input.Timeline_PlaceNoteDrumroll, "timeline_place_note_drumroll");
			X(Input.Timeline_Cut, "timeline_cut");
			X(Input.Timeline_Copy, "timeline_copy");
			X(Input.Timeline_Paste, "timeline_paste");
			X(Input.Timeline_DeleteSelection, "timeline_delete_selection");
			X(Input.Timeline_StartEndRangeSelection, "timeline_start_end_range_selection");
			X(Input.Timeline_SelectAll, "timeline_select_all");
			X(Input.Timeline_ClearSelection, "timeline_clear_selection");
			X(Input.Timeline_InvertSelection, "timeline_invert_selection");
			X(Input.Timeline_SelectToChartEnd, "timeline_select_to_chart_end");
			X(Input.Timeline_SelectAllWithinRangeSelection, "timeline_select_all_within_range_selection");
			X(Input.Timeline_ShiftSelectionLeft, "timeline_shift_selection_left");
			X(Input.Timeline_ShiftSelectionRight, "timeline_shift_selection_right");
			X(Input.Timeline_SelectItemPattern_xo, "timeline_select_item_pattern_xo");
			X(Input.Timeline_SelectItemPattern_xoo, "timeline_select_item_pattern_xoo");
			X(Input.Timeline_SelectItemPattern_xooo, "timeline_select_item_pattern_xooo");
			X(Input.Timeline_SelectItemPattern_xxoo, "timeline_select_item_pattern_xxoo");
			X(Input.Timeline_SelectItemPattern_CustomA, "timeline_select_item_pattern_custom_a");
			X(Input.Timeline_SelectItemPattern_CustomB, "timeline_select_item_pattern_custom_b");
			X(Input.Timeline_SelectItemPattern_CustomC, "timeline_select_item_pattern_custom_c");
			X(Input.Timeline_SelectItemPattern_CustomD, "timeline_select_item_pattern_custom_d");
			X(Input.Timeline_SelectItemPattern_CustomE, "timeline_select_item_pattern_custom_e");
			X(Input.Timeline_SelectItemPattern_CustomF, "timeline_select_item_pattern_custom_f");
			X(Input.Timeline_ConvertSelectionToScrollChanges, "timeline_convert_selection_to_scroll_changes");
			X(Input.Timeline_FlipNoteType, "timeline_flip_note_type");
			X(Input.Timeline_ToggleNoteSize, "timeline_toggle_note_size");
			X(Input.Timeline_ExpandItemTime_2To1, "timeline_expand_item_time_2_to_1");
			X(Input.Timeline_ExpandItemTime_3To2, "timeline_expand_item_time_3_to_2");
			X(Input.Timeline_ExpandItemTime_4To3, "timeline_expand_item_time_4_to_3");
			X(Input.Timeline_CompressItemTime_1To2, "timeline_compress_item_time_1_to_2");
			X(Input.Timeline_CompressItemTime_2To3, "timeline_compress_item_time_2_to_3");
			X(Input.Timeline_CompressItemTime_3To4, "timeline_compress_item_time_3_to_4");
			X(Input.Timeline_CompressItemTime_0To1, "timeline_compress_item_time_0_to_1");
			X(Input.Timeline_ReverseItemTime_N1To1, "timeline_reverse_item_time_n1_to_1");
			X(Input.Timeline_ScaleItemTime_CustomA, "timeline_scale_item_time_custom_a");
			X(Input.Timeline_ScaleItemTime_CustomB, "timeline_scale_item_time_custom_b");
			X(Input.Timeline_ScaleItemTime_CustomC, "timeline_scale_item_time_custom_c");
			X(Input.Timeline_ScaleItemTime_CustomD, "timeline_scale_item_time_custom_d");
			X(Input.Timeline_ScaleItemTime_CustomE, "timeline_scale_item_time_custom_e");
			X(Input.Timeline_ScaleItemTime_CustomF, "timeline_scale_item_time_custom_f");
			X(Input.Timeline_StepCursorLeft, "timeline_step_cursor_left");
			X(Input.Timeline_StepCursorRight, "timeline_step_cursor_right");
			X(Input.Timeline_JumpToTimelineStart, "timeline_jump_to_timeline_start");
			X(Input.Timeline_JumpToTimelineEnd, "timeline_jump_to_timeline_end");
			X(Input.Timeline_IncreaseGridDivision, "timeline_increase_grid_division");
			X(Input.Timeline_DecreaseGridDivision, "timeline_decrease_grid_division");
			X(Input.Timeline_SetGridDivision_1_4, "timeline_set_grid_division_1_4");
			X(Input.Timeline_SetGridDivision_1_5, "timeline_set_grid_division_1_5");
			X(Input.Timeline_SetGridDivision_1_6, "timeline_set_grid_division_1_6");
			X(Input.Timeline_SetGridDivision_1_7, "timeline_set_grid_division_1_7");
			X(Input.Timeline_SetGridDivision_1_8, "timeline_set_grid_division_1_8");
			X(Input.Timeline_SetGridDivision_1_9, "timeline_set_grid_division_1_9");
			X(Input.Timeline_SetGridDivision_1_10, "timeline_set_grid_division_1_10");
			X(Input.Timeline_SetGridDivision_1_12, "timeline_set_grid_division_1_12");
			X(Input.Timeline_SetGridDivision_1_14, "timeline_set_grid_division_1_14");
			X(Input.Timeline_SetGridDivision_1_16, "timeline_set_grid_division_1_16");
			X(Input.Timeline_SetGridDivision_1_18, "timeline_set_grid_division_1_18");
			X(Input.Timeline_SetGridDivision_1_20, "timeline_set_grid_division_1_20");
			X(Input.Timeline_SetGridDivision_1_24, "timeline_set_grid_division_1_24");
			X(Input.Timeline_SetGridDivision_1_28, "timeline_set_grid_division_1_28");
			X(Input.Timeline_SetGridDivision_1_32, "timeline_set_grid_division_1_32");
			X(Input.Timeline_SetGridDivision_1_36, "timeline_set_grid_division_1_36");
			X(Input.Timeline_SetGridDivision_1_48, "timeline_set_grid_division_1_40");
			X(Input.Timeline_SetGridDivision_1_48, "timeline_set_grid_division_1_48");
			X(Input.Timeline_SetGridDivision_1_64, "timeline_set_grid_division_1_64");
			X(Input.Timeline_SetGridDivision_1_96, "timeline_set_grid_division_1_96");
			X(Input.Timeline_SetGridDivision_1_192, "timeline_set_grid_division_1_192");
			X(Input.Timeline_IncreasePlaybackSpeed, "timeline_increase_playback_speed");
			X(Input.Timeline_DecreasePlaybackSpeed, "timeline_decrease_playback_speed");
			X(Input.Timeline_SetPlaybackSpeed_100, "timeline_set_playback_speed_100");
			X(Input.Timeline_SetPlaybackSpeed_75, "timeline_set_playback_speed_75");
			X(Input.Timeline_SetPlaybackSpeed_50, "timeline_set_playback_speed_50");
			X(Input.Timeline_SetPlaybackSpeed_25, "timeline_set_playback_speed_25");
			X(Input.Timeline_TogglePlayback, "timeline_toggle_playback");
			X(Input.Timeline_ToggleMetronome, "timeline_toggle_metronome");
			X(Input.TempoCalculator_Tap, "tempo_calculator_tap");
			X(Input.TempoCalculator_Reset, "tempo_calculator_reset");
		}
#undef X
#undef SECTION
		assert(outMap.MemberCount < ArrayCount(outMap.MemberSlots));
		return outMap;
	}
}
