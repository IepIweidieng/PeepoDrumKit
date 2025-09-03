#pragma once
#include "core_types.h"
#include "core_string.h"
#include "chart.h"
#include "chart_editor_timeline.h"
#include "chart_editor_context.h"
#include "chart_editor_theme.h"
#include "imgui/imgui_include.h"

namespace PeepoDrumKit
{
	struct LoadingTextAnimation
	{
		b8 WasLoadingLastFrame = false;
		u8 RingIndex = 0;
		f32 AccumulatedTimeSec = 0.0f;

		cstr UpdateFrameAndGetText(b8 isLoadingThisFrame, f32 deltaTimeSec);
	};

	struct TempoTapCalculator
	{
		i32 TapCount = 0;
		Tempo LastTempo = Tempo(0.0f);
		Tempo LastTempoMin = Tempo(0.0f), LastTempoMax = Tempo(0.0f);
		CPUStopwatch FirstTap = CPUStopwatch::StartNew();
		CPUStopwatch LastTap = CPUStopwatch::StartNew();
		Time ResetThreshold = Time::FromSec(2.0);

		inline b8 HasTimedOut() const { return ResetThreshold > Time::Zero() && LastTap.GetElapsed() >= ResetThreshold; }
		inline void Reset() { FirstTap.Stop(); TapCount = 0; LastTempo = LastTempoMin = LastTempoMax = Tempo(0.0f); }
		inline void Tap()
		{
			if (ResetThreshold > Time::Zero() && LastTap.Restart() >= ResetThreshold)
				Reset();
			FirstTap.Start();
			LastTempo = CalculateTempo(TapCount++, FirstTap.GetElapsed());
			LastTempoMin.BPM = (TapCount <= 2) ? LastTempo.BPM : Min(LastTempo.BPM, LastTempoMin.BPM);
			LastTempoMax.BPM = (TapCount <= 2) ? LastTempo.BPM : Max(LastTempo.BPM, LastTempoMax.BPM);
		}

		static constexpr Tempo CalculateTempo(i32 tapCount, Time elapsed) { return Tempo((tapCount <= 0) ? 0.0f : static_cast<f32>(60.0 * tapCount / elapsed.ToSec())); }
	};

	struct ChartHelpWindow
	{
		void DrawGui(ChartContext& context);
	};

	struct ChartUpdateNotesWindow
	{
		void DrawGui(ChartContext& context);
	};

	struct ChartChartStatsWindow
	{
		void DrawGui(ChartContext& context);
	};

	struct ChartUndoHistoryWindow
	{
		void DrawGui(ChartContext& context);
	};

	struct TempoCalculatorWindow
	{
		TempoTapCalculator Calculator = {};
		void DrawGui(ChartContext& context);
	};

	struct ChartInspectorWindow
	{
		// NOTE: Temp buffer only valid within the scope of the function
		struct TempChartItem { 
			GenericList List; 
			size_t Index; 
			GenericMemberFlags AvailableMembers; 
			AllGenericMembersUnionArray MemberValues; 
			Tempo BaseScrollTempo; 

			TempChartItem() {

			}
			TempChartItem(const TempChartItem& other) {
				List = other.List;
				Index = other.Index;
				AvailableMembers = other.AvailableMembers;
				MemberValues = other.MemberValues;
				BaseScrollTempo = other.BaseScrollTempo;
			}
		};
		std::vector<TempChartItem> SelectedItems;

		void DrawGui(ChartContext& context);
	};

	struct ChartPropertiesWindowIn 
	{ 
		b8 IsSongAsyncLoading; 
		b8 IsJacketAsyncLoading; 
	};
	struct ChartPropertiesWindowOut 
	{ 
		b8 BrowseOpenSong; 
		b8 LoadNewSong; 
		std::string NewSongFilePath; 
		b8 BrowseOpenJacket;
		b8 LoadNewJacket;
		std::string NewJacketFilePath;
	};
	struct ChartPropertiesWindow
	{
		std::string SongFileNameInputBuffer;
		LoadingTextAnimation SongLoadingTextAnimation {};
		b8 DifficultySliderStarsFitOnScreenLastFrame = false;
		b8 DifficultySliderStarsWasHoveredLastFrame = false;
		std::string JacketFileNameInputBuffer;
		LoadingTextAnimation JacketLoadingTextAnimation{};

		void DrawGui(ChartContext& context, const ChartPropertiesWindowIn& in, ChartPropertiesWindowOut& out);
	};

	struct ChartTempoWindow
	{
		void DrawGui(ChartContext& context, ChartTimeline& timeline);
	};

	struct ChartLyricsWindow
	{
		std::string LyricInputBuffer;
		std::string AllLyricsBuffer, AllLyricsCopyOnMadeActive;
		b8 IsLyricInputActiveThisFrame, IsLyricInputActiveLastFrame;
		b8 IsAllLyricsInputActiveThisFrame, IsAllLyricsInputActiveLastFrame;

		void DrawGui(ChartContext& context, ChartTimeline& timeline);
	};

	struct GameCamera
	{
		Rect ScreenSpaceViewportRect {};
		f32 WorldToScreenScaleFactor = 1.0f;
		vec2 WorldSpaceSize {};
		Rect LaneRect {};

		constexpr f32 LaneWidth() const { return LaneRect.GetWidth(); }
		constexpr f32 ExtendedLaneWidthFactor() const { return LaneRect.GetWidth() / GameLaneStandardWidth; }

		constexpr f32 WorldToScreenScale(f32 worldScale) const { return worldScale * WorldToScreenScaleFactor; }
		constexpr vec2 WorldToScreenScale(vec2 worldScale) const { return worldScale * WorldToScreenScaleFactor; }
		constexpr vec2 WorldToScreenSpace(vec2 worldSpace) const { return ScreenSpaceViewportRect.TL + (worldSpace * WorldToScreenScaleFactor); }

		constexpr vec2 JPOSScrollToLaneSpace(const vec2& jPosCoord) const
		{
			constexpr f32 jposMoveCoordHeight = 720.0f;
			f32 coordRatio = GameWorldStandardHeight / jposMoveCoordHeight;
			return jPosCoord * coordRatio;
		}

		vec2 GetHitCircleCoordinatesJPOSScroll(const SortedJPOSScrollChangesList& jposScrollChanges, Time timeStamp, const TempoMapAccelerationStructure& accelerationStructure)
		{
			if (jposScrollChanges.empty())
				return { 0, 0 };

			f32 x = 0;
			f32 y = 0;
			Time jposTimeStamp = accelerationStructure.ConvertBeatToTimeUsingLookupTableIndexing(jposScrollChanges[0].BeatTime);
			Time nextJposTimeStamp;
			for (size_t i = 0; i < jposScrollChanges.size() && timeStamp >= jposTimeStamp; i++) {
				JPOSScrollChange jposChange = jposScrollChanges[i];
				nextJposTimeStamp = !(i + 1 < jposScrollChanges.size()) ? Time::FromSec(F32Max)
					: accelerationStructure.ConvertBeatToTimeUsingLookupTableIndexing(jposScrollChanges[i + 1].BeatTime);

				Complex jposMove = jposChange.Move;
				Time jposDuration = Time::FromSec(jposChange.Duration);
				Time jposDurationMax = nextJposTimeStamp - jposTimeStamp;

				f32 xMove = jposMove.GetRealPart();
				f32 yMove = jposMove.GetImaginaryPart();

				Time timeSinceJpos = timeStamp - jposTimeStamp;
				f32 timeRatio = (jposDuration <= Time::Zero()) ? 1.f
					: (std::min({ jposDurationMax, jposDuration, timeSinceJpos }) / jposDuration);
				x += xMove * timeRatio;
				y += yMove * timeRatio;
				jposTimeStamp = nextJposTimeStamp;
			}

			return vec2(x, y);
		}

		vec2 GetHitCircleCoordinatesLane(const SortedJPOSScrollChangesList& jposScrollChanges, Time timeStamp, const TempoMapAccelerationStructure& accelerationStructure)
		{
			return JPOSScrollToLaneSpace(GetHitCircleCoordinatesJPOSScroll(jposScrollChanges, timeStamp, accelerationStructure));
		}

		vec2 GetNoteCoordinatesLane(
			vec2 originLane,
			Time cursorTime,
			f64 cursorHBScrollBeatTick,
			Time noteTime,
			Beat noteBeat,
			Tempo tempo,
			Complex scrollSpeed,
			ScrollMethod scrollType,
			const TempoMapAccelerationStructure& accelerationStructure,
			const SortedJPOSScrollChangesList& jposScrollChanges
		)
		{
			Complex readaptedScrollSpeed = (scrollType == ScrollMethod::BMSCROLL) ? Complex(1.f, 0.f) : scrollSpeed;

			return vec2(
				originLane.x + TimeToLaneSpace(cursorTime, cursorHBScrollBeatTick, noteTime, noteBeat, tempo, readaptedScrollSpeed.GetRealPart(), scrollType, accelerationStructure),
				originLane.y + TimeToLaneSpace(cursorTime, cursorHBScrollBeatTick, noteTime, noteBeat, tempo, readaptedScrollSpeed.GetImaginaryPart(), scrollType, accelerationStructure)
			);
		}

		vec2 GetHitCircleCoordinatesScreen(const SortedJPOSScrollChangesList& jposScrollChanges, Time timeStamp, const TempoMapAccelerationStructure& accelerationStructure)
		{
			return LaneToScreenSpace(GetHitCircleCoordinatesLane(jposScrollChanges, timeStamp, accelerationStructure));
		}

		// NOTE: Same scale as world space but with (0,0) starting at the hit-circle center point
		constexpr f32 TimeToLaneSpace(
			Time cursorTime, 
			f64 cursorHBScrollBeatTick, 
			Time noteTime, 
			Beat noteBeat, 
			Tempo tempo, 
			f32 scrollSpeed, 
			ScrollMethod scrollType, 
			const TempoMapAccelerationStructure& accelerationStructure
		) const
		{
			switch (scrollType) {
				case (ScrollMethod::HBSCROLL):
				case (ScrollMethod::BMSCROLL):
				{
					f64 noteHBScrollBeatTick = accelerationStructure.ConvertBeatAndTimeToHBScrollBeatTickUsingLookupTableIndexing(noteBeat, noteTime);
					return scrollSpeed * ((noteHBScrollBeatTick - cursorHBScrollBeatTick) / Beat::TicksPerBeat) * GameWorldSpaceDistancePerLaneBeat;
				}
				case (ScrollMethod::NMSCROLL):
				default:
				{
					return ((tempo.BPM * scrollSpeed) / 60.0f) * (noteTime - cursorTime).ToSec_F32() * GameWorldSpaceDistancePerLaneBeat;
				}
			}
		}
		constexpr vec2 LaneXToWorldSpace(f32 laneX) const { return (LaneRect.TL + GameHitCircle.Center + vec2(laneX, 0.0f)); }
		constexpr vec2 LaneToWorldSpace(f32 laneX, f32 laneY) const { return (LaneRect.TL + GameHitCircle.Center + vec2(laneX, laneY)); }
		constexpr vec2 LaneToScreenSpace(const vec2& laneCoord) const { return WorldToScreenSpace(LaneToWorldSpace(laneCoord.x, laneCoord.y)); }

		constexpr b8 IsPointVisibleOnLane(f32 laneX, f32 threshold = 280.0f) const { return (laneX >= -threshold) && (laneX <= (LaneWidth() + threshold)); }
		constexpr b8 IsRangeVisibleOnLane(f32 laneHeadX, f32 laneTailX, f32 threshold = 280.0f) const { return (laneTailX >= -threshold) && (laneHeadX <= (LaneWidth() + threshold)); }
	};

	struct ChartGamePreview
	{
		GameCamera Camera = {};

		struct DeferredNoteDrawData { f32 LaneHeadX, LaneTailX, LaneHeadY, LaneTailY; Tempo Tempo; Complex ScrollSpeed; const Note* OriginalNote; Time NoteStartTime, NoteEndTime; };
		std::vector<DeferredNoteDrawData> ReverseNoteDrawBuffer;

		void DrawGui(ChartContext& context, Time animatedCursorTime);
	};

	// Other GUI helpers
	b8 GuiInputFraction(cstr label, ivec2* inOutValue, std::optional<ivec2> valueRange,
		i32 step = 0, i32 stepFast = 0,
		const u32* textColorOverride = nullptr, std::string_view divisionText = " / ");
}
