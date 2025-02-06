#include "core_beat.h"
#include <algorithm>

Time TempoMapAccelerationStructure::ConvertBeatToTimeUsingLookupTableIndexing(Beat beat) const
{
	const i32 beatTickToTimesCount = static_cast<i32>(BeatTickToTimes.size());
	const i32 totalBeatTicks = beat.Ticks;

	if (totalBeatTicks < 0) // NOTE: Negative tick (tempo changes are assumed to only be positive)
	{
		// NOTE: Calculate the duration of a Beat at the first tempo
		const Time firstTickDuration = Time::FromSec((60.0 / FirstTempoBPM) / Beat::TicksPerBeat);

		// NOTE: Then scale by the negative tick
		return firstTickDuration * totalBeatTicks;
	}
	else if (totalBeatTicks >= beatTickToTimesCount) // NOTE: Tick is outside the defined tempo map
	{
		// NOTE: Take the last calculated time
		const Time lastTime = GetLastCalculatedTime();

		// NOTE: Calculate the duration of a Beat at the last used tempo
		const Time lastTickDuration = Time::FromSec((60.0 / LastTempoBPM) / Beat::TicksPerBeat);

		// NOTE: Then scale by the remaining ticks
		const i32 remainingTicks = (totalBeatTicks - beatTickToTimesCount) + 1;
		return lastTime + (lastTickDuration * remainingTicks);
	}
	else // NOTE: Use the pre calculated lookup table directly
	{
		return BeatTickToTimes.at(totalBeatTicks);
	}
}

Beat TempoMapAccelerationStructure::ConvertTimeToBeatUsingLookupTableBinarySearch(Time time) const
{
	const i32 beatTickToTimesCount = static_cast<i32>(BeatTickToTimes.size());
	const Time lastTime = GetLastCalculatedTime();

	if (time < Time::FromSec(0.0)) // NOTE: Negative time
	{
		// NOTE: Calculate the duration of a Beat at the first tempo
		const Time firstTickDuration = Time::FromSec((60.0 / FirstTempoBPM) / Beat::TicksPerBeat);

		// NOTE: Then the time by the negative tick, this is assuming all tempo changes happen on positive ticks
		return Beat(static_cast<i32>(time / firstTickDuration));
	}
	else if (time >= lastTime) // NOTE: Tick is outside the defined tempo map
	{
		const Time timePastLast = (time - lastTime);

		// NOTE: Each tick past the end has a duration of this value
		const Time lastTickDuration = Time::FromSec((60.0 / LastTempoBPM) / Beat::TicksPerBeat);

		// NOTE: So we just have to divide the remaining ticks by the duration
		const f64 ticks = (timePastLast / lastTickDuration);

		// NOTE: And add it to the last tick
		return Beat(static_cast<i32>(beatTickToTimesCount + ticks - 1));
	}
	else // NOTE: Perform a binary search
	{
		i32 left = 0, right = beatTickToTimesCount - 1;

		while (left <= right)
		{
			const i32 mid = (left + right) / 2;

			if (time < BeatTickToTimes[mid])
				right = mid - 1;
			else if (time > BeatTickToTimes[mid])
				left = mid + 1;
			else
				return Beat::FromTicks(mid);
		}

		return Beat::FromTicks((BeatTickToTimes[left] - time) < (time - BeatTickToTimes[right]) ? left : right);
	}
}

Time TempoMapAccelerationStructure::GetLastCalculatedTime() const
{
	return BeatTickToTimes.empty() ? Time::Zero() : BeatTickToTimes.back();
}

Time TempoMapAccelerationStructure::GetHBSCROLLApproachTime(f32 scrollSpeed, Time cursorTime, Time noteTime, const std::vector<TempoChange>& tempos) const
{
	f32 approachTimeDelta = (noteTime - cursorTime).ToSec_F32();
	if (approachTimeDelta == 0) return Time(0);

	f32 hbscrollTime = 0;
	f32 playBackTime = cursorTime.ToSec_F32();
	f32 noteApparitionTimeStamp = noteTime.ToSec_F32();

	f32 bpmAtCursorTime = (tempos.size() > 0) ? tempos[0].Tempo.BPM : 120;
	i32 idxAtStart = 0;

	// Normal scroll before the beginning of the song
	if (cursorTime.ToSec_F32() < 0) return Time(((bpmAtCursorTime * scrollSpeed) / 60.0f) * approachTimeDelta);
	
	// Search Cursor
	for (size_t i = 0; i < tempos.size(); i++) {
		TempoChange currTempo = tempos[i];
		f32 bpmChangeTimeStamp1 = ConvertBeatToTimeUsingLookupTableIndexing(currTempo.Beat).ToSec_F32();
		f32 bpmValue1 = currTempo.Tempo.BPM;

		if (bpmChangeTimeStamp1 > playBackTime) break; 
		bpmAtCursorTime = bpmValue1;
		idxAtStart = i;
		if (i == tempos.size() - 1) break;
	}

	// Quadrants
	if (approachTimeDelta > 0) {
		for (size_t i = idxAtStart; i < tempos.size(); i++) {
			TempoChange currTempo = tempos[i];
			TempoChange nextTempo = tempos[i + 1];

			f32 bpmChangeTimeStamp1 = ConvertBeatToTimeUsingLookupTableIndexing(currTempo.Beat).ToSec_F32();
			f32 bpmValue1 = currTempo.Tempo.BPM;
			f32 deltaTime = 0;

			if (bpmChangeTimeStamp1 > noteApparitionTimeStamp) break; // Done

			if (i == tempos.size() - 1) {
				// Process the last BPM change
				deltaTime = noteApparitionTimeStamp - Max(playBackTime, bpmChangeTimeStamp1);
			}
			else {
				f32 bpmChangeTimeStamp2 = ConvertBeatToTimeUsingLookupTableIndexing(nextTempo.Beat).ToSec_F32();
				deltaTime = Min(noteApparitionTimeStamp, bpmChangeTimeStamp2) - Max(playBackTime, bpmChangeTimeStamp1);
			}

			hbscrollTime += ((bpmValue1 * scrollSpeed) / 60.0f) * deltaTime;
		}
	}
	else {
		// Negative quadrant to fix
		return Time(((bpmAtCursorTime * scrollSpeed) / 60.0f) * approachTimeDelta);


		for (size_t i = idxAtStart; i > 0; i--) {
			TempoChange currTempo = tempos[i];
			TempoChange nextTempo = tempos[i - 1];

			f32 bpmChangeTimeStamp1 = ConvertBeatToTimeUsingLookupTableIndexing(currTempo.Beat).ToSec_F32();
			f32 bpmValue1 = currTempo.Tempo.BPM;
			f32 deltaTime = 0;

			if (bpmChangeTimeStamp1 < noteApparitionTimeStamp) break; // Done

			if (i == idxAtStart) {
				// Process the first BPM change
				deltaTime = playBackTime - Max(noteApparitionTimeStamp, bpmChangeTimeStamp1);
			}
			f32 bpmChangeTimeStamp2 = ConvertBeatToTimeUsingLookupTableIndexing(nextTempo.Beat).ToSec_F32();
			deltaTime = Min(playBackTime, bpmChangeTimeStamp1) - Max(noteApparitionTimeStamp, bpmChangeTimeStamp2);

			hbscrollTime -= ((bpmValue1 * scrollSpeed) / 60.0f) * deltaTime;
		}

	}
	

	return Time(hbscrollTime);
	
}

void TempoMapAccelerationStructure::Rebuild(const TempoChange* inTempoChanges, size_t inTempoCount)
{
	const TempoChange* tempoChanges = inTempoChanges;
	size_t tempoCount = inTempoCount;

	// HACK: Handle this special case here by creating an adjusted copy instead of changing the algorithm itself to not risk accidentally messing anything up
	if (inTempoCount < 1 || inTempoChanges[0].Beat > Beat::Zero())
	{
		TempoBuffer.resize(inTempoCount + 1);
		TempoBuffer[0] = TempoChange(Beat::Zero(), FallbackTempo);
		memcpy(TempoBuffer.data() + 1, inTempoChanges, sizeof(TempoChange) * inTempoCount);

		tempoChanges = TempoBuffer.data();
		tempoCount = TempoBuffer.size();
	}

	BeatTickToTimes.resize((tempoCount > 0) ? tempoChanges[tempoCount - 1].Beat.Ticks + 1 : 0);

	f64 lastEndTime = 0.0;
	for (size_t tempoChangeIndex = 0; tempoChangeIndex < tempoCount; tempoChangeIndex++)
	{
		const TempoChange& tempoChange = tempoChanges[tempoChangeIndex];

		const f64 bpm = SafetyCheckTempo(tempoChange.Tempo).BPM;
		const f64 beatDuration = (60.0 / bpm);
		const f64 tickDuration = (beatDuration / Beat::TicksPerBeat);

		const b8 isSingleOrLastTempo = (tempoCount == 1) || (tempoChangeIndex == (tempoCount - 1));
		const size_t timesCount = isSingleOrLastTempo ? BeatTickToTimes.size() : (tempoChanges[tempoChangeIndex + 1].Beat.Ticks);

		for (size_t i = 0, t = tempoChange.Beat.Ticks; t < timesCount; t++)
			BeatTickToTimes[t] = Time::FromSec((tickDuration * i++) + lastEndTime);

		if (tempoCount > 1)
			lastEndTime = BeatTickToTimes[timesCount - 1].ToSec() + tickDuration;

		FirstTempoBPM = (tempoChangeIndex == 0) ? bpm : FirstTempoBPM;
		LastTempoBPM = bpm;
	}

	if (!TempoBuffer.empty())
		TempoBuffer.clear();
}
