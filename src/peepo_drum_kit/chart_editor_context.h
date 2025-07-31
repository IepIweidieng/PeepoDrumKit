#pragma once
#include "core_types.h"
#include "core_undo.h"
#include "chart.h"
#include "chart_editor_sound.h"
#include "chart_editor_graphics.h"
#include "audio/audio_engine.h"
#include "audio/audio_waveform.h"
#include <set>
#include <unordered_map>

namespace PeepoDrumKit
{
	struct BeatAndTime { Beat Beat; Time Time; };

	// NOTE: Essentially the shared data between all the chart editor components, split into a separate file away from the chart editor to avoid circular #includes
	struct ChartContext : NonCopyable
	{
		ChartProject Chart;
		std::string ChartFilePath;

		// NOTE: Must **ALWAYS** point to a valid course. If "Chart.Courses.empty()" then a new one should automatically be added at the start of the frame before anything else is updated

		ChartCourse* ChartSelectedCourse = nullptr;
		BranchType ChartSelectedBranch = BranchType::Normal;

		b8 CompareMode = false;
		std::unordered_map<const ChartCourse*, std::set<BranchType>> ChartsCompared; // should always include ChartSelected

		// NOTE: Specifically for accurate playback preview sound future-offset calculations
		Time CursorNonSmoothTimeThisFrame, CursorNonSmoothTimeLastFrame;
		// NOTE: Store cursor time as Beat while paused to avoid any floating point precision issues and make sure "SetCursorBeat(x); assert(GetCursorBeat() == x)"
		Beat CursorBeatWhilePaused = Beat::Zero();
		// NOTE: Specifically to skip hit animations for notes before this time point
		Time CursorTimeOnPlaybackStart = Time::Zero();

		// NOTE: Specifically for animations, accumulated program delta time, unrelated to GetCursorTime() etc.
		Time ElapsedProgramTimeSincePlaybackStarted = Time::Zero();
		Time ElapsedProgramTimeSincePlaybackStopped = Time::Zero();

		Audio::Voice SongVoice = Audio::VoiceHandle::Invalid;
		Audio::SourceHandle SongSource = Audio::SourceHandle::Invalid;
		SoundEffectsVoicePool SfxVoicePool;
		ChartGraphicsResources Gfx;

		std::string SongSourceFilePath;
		std::string SongJacketFilePath;
		Audio::WaveformMipChain SongWaveformL;
		Audio::WaveformMipChain SongWaveformR;
		f32 SongWaveformFadeAnimationCurrent = 0.0f;
		f32 SongWaveformFadeAnimationTarget = 0.0f;

		Undo::UndoHistory Undo;

	public:
		inline Time BeatToTime(Beat beat) const { return ChartSelectedCourse->TempoMap.BeatToTime(beat); }
		inline Beat TimeToBeat(Time time) const { return ChartSelectedCourse->TempoMap.TimeToBeat(time); }
		inline Beat TimeToBeat(Time time, bool truncTo0) const { return ChartSelectedCourse->TempoMap.TimeToBeat(time, truncTo0); }
		inline f64 BeatAndTimeToHBScrollBeatTick(Beat beat, Time time) const { return ChartSelectedCourse->TempoMap.BeatAndTimeToHBScrollBeatTick(beat, time); }

		void ResetChartsCompared() { ChartsCompared = { { ChartSelectedCourse, { ChartSelectedBranch } } }; CompareMode = false; }
		b8 IsChartCompared(const ChartCourse* course, BranchType branch) const
		{
			auto it = ChartsCompared.find(course);
			return (it != cend(ChartsCompared)) && (it->second.find(branch) != cend(it->second));
		}

		void SetSelectedChart(ChartCourse* course, BranchType branch)
		{
			ChartSelectedCourse = course;
			ChartSelectedBranch = branch;
			if (CompareMode)
				ChartsCompared[course].insert(branch);
			else if (!IsChartCompared(course, branch))
				ResetChartsCompared();
		}

		inline f32 GetPlaybackSpeed() { return SongVoice.GetPlaybackSpeed(); }
		inline void SetPlaybackSpeed(f32 newSpeed) { if (!ApproxmiatelySame(SongVoice.GetPlaybackSpeed(), newSpeed)) SongVoice.SetPlaybackSpeed(newSpeed); }

		inline b8 GetIsPlayback() const
		{
			return SongVoice.GetIsPlaying();
		}

		inline void SetIsPlayback(b8 newIsPlaying)
		{
			if (SongVoice.GetIsPlaying() == newIsPlaying)
				return;

			if (newIsPlaying)
			{
				CursorTimeOnPlaybackStart = (SongVoice.GetPosition() + Chart.SongOffset);
				SongVoice.SetIsPlaying(true);
			}
			else
			{
				SongVoice.SetIsPlaying(false);
				CursorBeatWhilePaused = ChartSelectedCourse->TempoMap.TimeToBeat((SongVoice.GetPosition() + Chart.SongOffset));
				SfxVoicePool.PauseAllFutureVoices();
			}
		}

		inline Time GetCursorTime() const
		{
			if (SongVoice.GetIsPlaying())
				return (SongVoice.GetPositionSmooth() + Chart.SongOffset);
			else
				return ChartSelectedCourse->TempoMap.BeatToTime(CursorBeatWhilePaused);
		}

		inline Beat GetCursorBeat() const
		{
			return GetCursorBeat(false);
		}

		inline Beat GetCursorBeat(bool truncTo0) const
		{
			if (SongVoice.GetIsPlaying())
				return ChartSelectedCourse->TempoMap.TimeToBeat((SongVoice.GetPositionSmooth() + Chart.SongOffset), truncTo0);
			else
				return CursorBeatWhilePaused;
		}

		// NOTE: Specifically to make sure the cursor time and beat are both in sync (as the song voice position is updated asynchronously)
		inline BeatAndTime GetCursorBeatAndTime() const
		{
			return GetCursorBeatAndTime(false);
		}

		inline BeatAndTime GetCursorBeatAndTime(bool truncTo0) const
		{
			if (SongVoice.GetIsPlaying()) { const Time t = (SongVoice.GetPositionSmooth() + Chart.SongOffset); return { ChartSelectedCourse->TempoMap.TimeToBeat(t, truncTo0), t }; }
			else { const Beat b = CursorBeatWhilePaused; return { b, ChartSelectedCourse->TempoMap.BeatToTime(b) }; }
		}

		inline void SetCursorTime(Time newTime)
		{
			SongVoice.SetPosition(newTime - Chart.SongOffset);
			CursorNonSmoothTimeThisFrame = CursorNonSmoothTimeLastFrame = newTime;
			CursorBeatWhilePaused = ChartSelectedCourse->TempoMap.TimeToBeat(newTime);
			CursorTimeOnPlaybackStart = newTime;
		}

		inline void SetCursorBeat(Beat newBeat)
		{
			const Time newTime = ChartSelectedCourse->TempoMap.BeatToTime(newBeat);
			SongVoice.SetPosition(newTime - Chart.SongOffset);
			CursorNonSmoothTimeThisFrame = CursorNonSmoothTimeLastFrame = newTime;
			CursorBeatWhilePaused = newBeat;
			CursorTimeOnPlaybackStart = newTime;
		}
	};
}
