#include "core_beat.h"
#include <algorithm>

Time TempoMapAccelerationStructure::ConvertBeatToTimeUsingLookupTableIndexing(Beat beat) const
{
	const i32 beatTickToTimesCount = static_cast<i32>(BeatTickToTimes.size());
	const i32 totalBeatTicks = beat.Ticks;

	if (totalBeatTicks < 0) // NOTE: Negative tick (tempo changes are assumed to only be positive)
	{
		// NOTE: Calculate the duration of a Beat at the first tempo
		const Time firstTickDuration = Time::FromSec((60.0 / abs(FirstTempoBPM)) / Beat::TicksPerBeat);

		// NOTE: Then scale by the negative tick
		return firstTickDuration * totalBeatTicks;
	}
	else if (totalBeatTicks >= beatTickToTimesCount) // NOTE: Tick is outside the defined tempo map
	{
		// NOTE: Take the last calculated time
		const Time lastTime = GetLastCalculatedTime();

		// NOTE: Calculate the duration of a Beat at the last used tempo
		const Time lastTickDuration = Time::FromSec((60.0 / abs(LastTempoBPM)) / Beat::TicksPerBeat);

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
	return ConvertTimeToBeatUsingLookupTableBinarySearch(time, false);
}

Beat TempoMapAccelerationStructure::ConvertTimeToBeatUsingLookupTableBinarySearch(Time time, bool truncTo0) const
{
	const i32 beatTickToTimesCount = static_cast<i32>(BeatTickToTimes.size());
	const Time lastTime = GetLastCalculatedTime();

	if (time < Time::FromSec(0.0)) // NOTE: Negative time
	{
		// NOTE: Calculate the duration of a Beat at the first tempo
		const Time firstTickDuration = Time::FromSec((60.0 / abs(FirstTempoBPM)) / Beat::TicksPerBeat);

		// NOTE: Then the time by the negative tick, this is assuming all tempo changes happen on positive ticks
		return Beat(static_cast<i32>(time / firstTickDuration));
	}
	else if (time >= lastTime) // NOTE: Tick is outside the defined tempo map
	{
		const Time timePastLast = (time - lastTime);

		// NOTE: Each tick past the end has a duration of this value
		const Time lastTickDuration = Time::FromSec((60.0 / abs(LastTempoBPM)) / Beat::TicksPerBeat);

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

		// left > right
		return Beat::FromTicks((truncTo0) ? right
			: (BeatTickToTimes[left] - time) < (time - BeatTickToTimes[right]) ? left : right);
	}
}

// find the integer HBScroll beat tick by `beat`, and then interpolate or extrapolate to `time`
// allow over-extrapolating for reproducing TaikoJiro "time offset over tempo change" behavior
f64 TempoMapAccelerationStructure::ConvertBeatAndTimeToHBScrollBeatTickUsingLookupTableIndexing(Beat beat, Time time) const
{
	const i32 beatTickToTimesCount = static_cast<i32>(BeatTickToTimes.size());
	const i32 totalBeatTicks = beat.Ticks;

	if (totalBeatTicks < 0) // NOTE: Negative tick (tempo changes are assumed to only be positive)
	{
		// NOTE: Calculate the duration of a Beat at the first tempo
		const Time firstTickDuration = Time::FromSec((60.0 / FirstTempoBPM) / Beat::TicksPerBeat);

		// NOTE: Then the time by the negative tick, this is assuming all tempo changes happen on positive ticks
		return time / firstTickDuration;
	}
	else if (totalBeatTicks + 1 >= beatTickToTimesCount) // NOTE: Next tick is outside the defined tempo map
	{
		const f64 lastHBScrollBeatTick = GetLastCalculatedHBScrollBeatTick();
		const Time lastTime = GetLastCalculatedTime();
		const Time timePastLast = (time - lastTime);

		// NOTE: Each tick past the end has a duration of this value
		const Time lastTickDuration = Time::FromSec((60.0 / LastTempoBPM) / Beat::TicksPerBeat);

		// NOTE: So we just have to divide the remaining ticks by the duration
		const f64 ticks = (timePastLast / lastTickDuration);

		// NOTE: And add it to the last tick
		return (lastHBScrollBeatTick + ticks);
	}
	else // NOTE: Use the pre calculated lookup table directly
	{
		// find the integer HBScroll beat tick using `beat`, and then interpolate by `time`
		const i32 totalBeatTicks = beat.Ticks;
		const f64 timeLeft = BeatTickToTimes.at(totalBeatTicks).Seconds;
		const f64 timeRight = BeatTickToTimes.at(totalBeatTicks + 1).Seconds;
		const f64 ticksLeft = BeatTickToHBScrollBeatTicks.at(totalBeatTicks);
		const f64 ticksRight = BeatTickToHBScrollBeatTicks.at(totalBeatTicks + 1);

		return ConvertRange(timeLeft, timeRight, ticksLeft, ticksRight, time.Seconds);
	}
}

Time TempoMapAccelerationStructure::GetLastCalculatedTime() const
{
	return BeatTickToTimes.empty() ? Time::Zero() : BeatTickToTimes.back();
}

f64 TempoMapAccelerationStructure::GetLastCalculatedHBScrollBeatTick() const
{
	return BeatTickToHBScrollBeatTicks.empty() ? 0.0 : BeatTickToHBScrollBeatTicks.back();
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

	size_t nTickValues = (tempoCount > 0) ? tempoChanges[tempoCount - 1].Beat.Ticks + 1 : 0;
	BeatTickToTimes.resize(nTickValues);
	BeatTickToHBScrollBeatTicks.resize(nTickValues);

	f64 lastEndTime = 0.0;
	i32 lastEndHBScrollBeatTick = 0;
	for (size_t tempoChangeIndex = 0; tempoChangeIndex < tempoCount; tempoChangeIndex++)
	{
		const TempoChange& tempoChange = tempoChanges[tempoChangeIndex];

		const f64 bpm = SafetyCheckTempo(tempoChange.Tempo).BPM;
		const f64 beatDuration = (60.0 / bpm);
		const f64 tickDuration = abs(beatDuration / Beat::TicksPerBeat);
		const f64 tickSign = Sign(beatDuration);

		const b8 isSingleOrLastTempo = (tempoCount == 1) || (tempoChangeIndex == (tempoCount - 1));
		const size_t timesCount = isSingleOrLastTempo ? BeatTickToTimes.size() : (tempoChanges[tempoChangeIndex + 1].Beat.Ticks);

		for (size_t i = 0, t = tempoChange.Beat.Ticks; t < timesCount; i++, t++)
		{
			BeatTickToTimes[t] = Time::FromSec((tickDuration * i) + lastEndTime);
			BeatTickToHBScrollBeatTicks[t] = ((tickSign * i) + lastEndHBScrollBeatTick);
		}

		if (tempoCount > 1)
		{
			lastEndTime = BeatTickToTimes[timesCount - 1].ToSec() + tickDuration;
			lastEndHBScrollBeatTick = BeatTickToHBScrollBeatTicks[timesCount - 1] + tickSign;
		}

		FirstTempoBPM = (tempoChangeIndex == 0) ? bpm : FirstTempoBPM;
		LastTempoBPM = bpm;
	}

	if (!TempoBuffer.empty())
		TempoBuffer.clear();
}
