#pragma once
#include "core_types.h"
#include <vector>
#include <algorithm>
#include <functional>
#include <numeric>

struct Beat
{
	// NOTE: Meaning a 4/4 bar can be meaningfully subdivided into 192 parts, as is a common convention for rhythm games
	static constexpr i32 TicksPerBeat = (20160 / 4);//(192 / 4);

	i32 Ticks;

	Beat() = default;
	constexpr explicit Beat(i32 ticks) : Ticks(ticks) {}

	constexpr f64 BeatsFraction() const { return static_cast<f64>(Ticks) / static_cast<f64>(TicksPerBeat); }

	static constexpr Beat Zero() { return Beat(0); }
	static constexpr Beat FromTicks(i32 ticks) { return Beat(ticks); }
	static constexpr Beat FromBeats(i32 beats) { return Beat(TicksPerBeat * beats); }
	static constexpr Beat FromBars(i32 bars, i32 beatsPerBar = 4) { return FromBeats(bars * beatsPerBar); }
	static constexpr Beat FromBeatsFraction(f64 fraction) { return FromTicks(static_cast<i32>(Round(fraction * static_cast<f64>(TicksPerBeat)))); }

	constexpr b8 operator==(const Beat& other) const { return Ticks == other.Ticks; }
	constexpr b8 operator!=(const Beat& other) const { return Ticks != other.Ticks; }
	constexpr b8 operator<=(const Beat& other) const { return Ticks <= other.Ticks; }
	constexpr b8 operator>=(const Beat& other) const { return Ticks >= other.Ticks; }
	constexpr b8 operator<(const Beat& other) const { return Ticks < other.Ticks; }
	constexpr b8 operator>(const Beat& other) const { return Ticks > other.Ticks; }

	constexpr Beat operator+(const Beat& other) const { return Beat(Ticks + other.Ticks); }
	constexpr Beat operator-(const Beat& other) const { return Beat(Ticks - other.Ticks); }
	constexpr Beat operator*(const i32 ticks) const { return Beat(Ticks * ticks); }
	constexpr Beat operator/(const i32 ticks) const { return Beat(Ticks / ticks); }
	constexpr Beat operator%(const Beat& other) const { return Beat(Ticks % other.Ticks); }
	constexpr i32 operator/(const Beat& other) const { return Ticks / other.Ticks; }

	constexpr Beat& operator+=(const Beat& other) { (Ticks += other.Ticks); return *this; }
	constexpr Beat& operator-=(const Beat& other) { (Ticks -= other.Ticks); return *this; }
	constexpr Beat& operator*=(const i32 ticks) { (Ticks *= ticks); return *this; }
	constexpr Beat& operator/=(const i32 ticks) { (Ticks *= ticks); return *this; }
	constexpr Beat& operator%=(const Beat& other) { (Ticks %= other.Ticks); return *this; }

	constexpr Beat operator+() const { return Beat(+Ticks); }
	constexpr Beat operator-() const { return Beat(-Ticks); }
};

constexpr Beat abs(Beat beat) { return Beat(abs(beat.Ticks)); }
template <typename T, std::enable_if_t<!expect_type_v<T, Beat>, bool> = true>
constexpr auto operator*(T&& v, Beat beat) { return beat * v; }
inline Beat FloorBeatToGrid(Beat beat, Beat grid) { return Beat::FromTicks(static_cast<i32>(Floor(static_cast<f64>(beat.Ticks) / static_cast<f64>(grid.Ticks))) * grid.Ticks); }
inline Beat RoundBeatToGrid(Beat beat, Beat grid) { return Beat::FromTicks(static_cast<i32>(Round(static_cast<f64>(beat.Ticks) / static_cast<f64>(grid.Ticks))) * grid.Ticks); }
inline Beat CeilBeatToGrid(Beat beat, Beat grid) { return Beat::FromTicks(static_cast<i32>(Ceil(static_cast<f64>(beat.Ticks) / static_cast<f64>(grid.Ticks))) * grid.Ticks); }
constexpr Beat GetGridBeatSnap(i32 gridBarDivision) { return Beat::FromBars(1) / gridBarDivision; }
constexpr b8 IsTupletBarDivision(i32 gridBarDivision) { return (gridBarDivision % 3 == 0); }
constexpr b8 IsQuintupletBarDivision(i32 gridBarDivision) { return (gridBarDivision % 5 == 0); }
constexpr b8 IsSeptupletBarDivision(i32 gridBarDivision) { return (gridBarDivision % 7 == 0); }
constexpr b8 IsNonupletBarDivision(i32 gridBarDivision) { return (gridBarDivision % 9 == 0); }

struct Tempo
{
	f32 BPM = 0.0f;

	constexpr Tempo() = default;
	constexpr explicit Tempo(f32 bpm) : BPM(bpm) {}
};

constexpr f32 MinAbsSafeBPM = 60.0f / I32Max;
constexpr Tempo SafetyCheckTempo(Tempo v) { return (v.BPM < 0) ? Tempo(ClampTop(v.BPM, -MinAbsSafeBPM)) : Tempo(ClampBot(v.BPM, MinAbsSafeBPM)); }

struct TimeSignature
{
	i32 Numerator = 4;
	i32 Denominator = 4;

	constexpr TimeSignature() = default;
	constexpr TimeSignature(i32 numerator, i32 denominator) : Numerator(numerator), Denominator(denominator) {}

	constexpr i32 GetBeatsPerBar() const { return Numerator; }
	constexpr Beat GetDurationPerBeat() const { return Beat::FromBars(1) / Denominator; }
	constexpr Beat GetDurationPerBar() const { const auto sim = GetSimplified(); return Beat::FromBars(1) * sim.Numerator / sim.Denominator; }

	constexpr size_t size() const { return 2; }
	constexpr i32* data() { return &Numerator; }
	constexpr const i32* data() const { return &Numerator; }
	constexpr b8 operator==(const TimeSignature& other) const { return (Numerator == other.Numerator) && (Denominator == other.Denominator); }
	constexpr b8 operator!=(const TimeSignature& other) const { return (Numerator != other.Numerator) || (Denominator != other.Denominator); }
	constexpr i32 operator[](size_t index) const { return (&Numerator)[index]; }
	constexpr i32& operator[](size_t index) { return (&Numerator)[index]; }

	constexpr TimeSignature GetSimplified(i32 denomTarget = 0) const& { return TimeSignature(*this).GetSimplified(denomTarget); }
	constexpr TimeSignature GetSimplified(i32 denomTarget = 0) && { Simplify(denomTarget); return *this; }
	constexpr void Simplify(i32 denomTarget = 0)
	{
		if (Denominator == 0)
			return;
		auto gcd = std::gcd(Numerator, Denominator);
		Numerator /= gcd;
		Denominator /= gcd;
		if ((denomTarget != 0) && (denomTarget % Denominator == 0) && (abs(denomTarget / Denominator) < abs(I32Max / Numerator))) { // use target denominator
			Numerator *= denomTarget / Denominator;
			Denominator = denomTarget;
		} else if ((Denominator < 0) != (denomTarget < 0)) { // use non-negative denominator
			Numerator *= -1;
			Denominator *= -1;
		}
	}

	constexpr TimeSignature operator+(const TimeSignature& other) const
	{
		auto thisSim = GetSimplified(), otherSim = other.GetSimplified();
		i32 denom = std::lcm(thisSim.Denominator, otherSim.Denominator);
		i32 factorThis = denom / thisSim.Denominator;
		i32 factorOther = denom / otherSim.Denominator;
		return TimeSignature(factorThis * thisSim.Numerator + factorOther * otherSim.Numerator, denom)
			.GetSimplified(std::min(abs(Denominator), abs(other.Denominator)));
	}
	constexpr TimeSignature operator-(const TimeSignature& other) const { return *this + -other; }

	constexpr TimeSignature operator*(const TimeSignature& other) const
	{
		auto thisSim = GetSimplified(), otherSim = other.GetSimplified();
		return TimeSignature(thisSim.Numerator * otherSim.Numerator, thisSim.Denominator * otherSim.Denominator)
			.GetSimplified(std::min(abs(Denominator), abs(other.Denominator)));
	}
	constexpr TimeSignature operator/(const TimeSignature& other) const { return *this * TimeSignature(other.Denominator, other.Numerator); }
	constexpr TimeSignature operator*(const i32 rate) const { return *this * TimeSignature(rate, 1); }
	constexpr TimeSignature operator/(const i32 rate) const { return *this * TimeSignature(1, rate); }

	constexpr TimeSignature operator+() const { return *this; }
	constexpr TimeSignature operator-() const { return { -Numerator, Denominator }; }
};

constexpr i32 Sign(TimeSignature value) { return Sign(value.Numerator) * ((value.Denominator < 0) ? -1 : 1); }
template <typename T, std::enable_if_t<!expect_type_v<T, TimeSignature>, bool> = true>
constexpr auto operator*(T&& v, TimeSignature signature) { return signature * v; }
constexpr b8 IsTimeSignatureSupported(TimeSignature v) { return (v.Numerator != 0 && v.Denominator > 0) && (Beat::FromBars(1).Ticks % v.Denominator) == 0; }

// NOTE: Defined within PeepoDrumKit for making the Get/SetBeat() and other access functions accessible
namespace PeepoDrumKit {
	struct TempoChange
	{
		constexpr TempoChange() = default;
		constexpr TempoChange(Beat beat, Tempo tempo) : Beat(beat), Tempo(tempo) {}

		Beat Beat = {};
		Tempo Tempo = {};
		b8 IsSelected = false;
	};

	struct TimeSignatureChange
	{
		constexpr TimeSignatureChange() = default;
		constexpr TimeSignatureChange(Beat beat, TimeSignature signature) : Beat(beat), Signature(signature) {}

		Beat Beat = {};
		TimeSignature Signature = {};
		b8 IsSelected = false;
	};
}

using PeepoDrumKit::TempoChange;
using PeepoDrumKit::TimeSignatureChange;

template <typename T>
struct BeatSortedForwardIterator
{
	size_t LastIndex = 0;

	const T* Next(const std::vector<T>& sortedList, Beat nextBeat);
	inline T* Next(std::vector<T>& sortedList, Beat nextBeat) { return const_cast<T*>(Next(std::as_const(sortedList), nextBeat)); }
};

template <typename T>
struct BeatSortedList
{
	using value_type = T;

	std::vector<T> Sorted;

public:
	T* TryFindLastAtBeat(Beat beat);
	T* TryFindExactAtBeat(Beat beat);
	const T* TryFindLastAtBeat(Beat beat) const;
	const T* TryFindExactAtBeat(Beat beat) const;

	// NOTE: Inclusive check to be used for long notes (which must have spacing after the tail piece) and single point intersection tests (such as with a cursor)
	T* TryFindOverlappingBeat(Beat beatStart, Beat beatEnd, b8 inclusiveBeatCheck = true);
	const T* TryFindOverlappingBeat(Beat beatStart, Beat beatEnd, b8 inclusiveBeatCheck = true) const;
	// bypass sanity check, for untrusted inputs
	T* TryFindOverlappingBeatUntrusted(Beat beatStart, Beat beatEnd, b8 inclusiveBeatCheck = true);
	const T* TryFindOverlappingBeatUntrusted(Beat beatStart, Beat beatEnd, b8 inclusiveBeatCheck = true) const;

	// return the to-insert index
	template <typename Func> size_t InsertOrFunc(const T& valueToInsert, Func funcExist);
	// return { the to-insert index, is inserted }
	std::pair<size_t, b8> InsertOrIgnore(const T& valueToInsert);
	// return the insertion or update index
	size_t InsertOrUpdate(const T& valueToInsertOrUpdate);

	void RemoveAtBeat(Beat beatToFindAndRemove);
	void RemoveAtIndex(size_t indexToRemove);

	int CountIf(std::function<bool(const T&)> predicate) const { return std::count_if(Sorted.begin(), Sorted.end(), predicate); }
	std::vector<T> Filter(std::function<bool(const T&)> predicate) const {
		std::vector<T> result;
		std::copy_if(Sorted.begin(), Sorted.end(), std::back_inserter(result), predicate);
		return result;
	}

	inline b8 empty() const { return Sorted.empty(); }
	inline auto begin() { return Sorted.begin(); }
	inline auto end() { return Sorted.end(); }
	inline auto begin() const { return Sorted.begin(); }
	inline auto end() const { return Sorted.end(); }
	inline T* data() { return Sorted.data(); }
	inline const T* data() const { return Sorted.data(); }
	inline size_t size() const { return Sorted.size(); }
	inline T& operator[](size_t index) { return Sorted[index]; }
	inline const T& operator[](size_t index) const { return Sorted[index]; }
};

struct TempoMapAccelerationStructure
{
	// NOTE: Pre calculated beat times up to the last tempo change
	std::vector<Time> BeatTickToTimes;
	std::vector<i32> BeatTickToHBScrollBeatTicks;
	std::vector<TempoChange> TempoBuffer;
	f64 FirstTempoBPM = 0.0, LastTempoBPM = 0.0;

	Time ConvertBeatToTimeUsingLookupTableIndexing(Beat beat) const;
	Beat ConvertTimeToBeatUsingLookupTableBinarySearch(Time time) const;
	Beat ConvertTimeToBeatUsingLookupTableBinarySearch(Time time, bool truncTo0) const;
	f64 ConvertBeatAndTimeToHBScrollBeatTickUsingLookupTableIndexing(Beat beat, Time time) const;

	Time GetLastCalculatedTime() const;
	f64 GetLastCalculatedHBScrollBeatTick() const;
	void Rebuild(const TempoChange* inTempoChanges, size_t inTempoCount);
};

// NOTE: Used when no other tempo / time signature change is defined (empty list or pre-first beat)
constexpr Tempo FallbackTempo = Tempo(120.0f);
constexpr TimeSignature FallbackTimeSignature = TimeSignature(4, 4);

using SortedTempoChangesList = BeatSortedList<TempoChange>;
using SortedSignatureChangesList = BeatSortedList<TimeSignatureChange>;

struct SortedTempoMap
{
	// NOTE: These must always remain sorted and only have changes with (Beat.Ticks >= 0)
	SortedTempoChangesList Tempo;
	SortedSignatureChangesList Signature;
	TempoMapAccelerationStructure AccelerationStructure;

public:
	inline SortedTempoMap() { RebuildAccelerationStructure(); }

	// NOTE: Must manually be called every time a TempoChange has been edited otherwise Beat <-> Time conversions will be incorrect
	inline void RebuildAccelerationStructure() { AccelerationStructure.Rebuild(Tempo.data(), Tempo.size()); }
	inline Time BeatToTime(Beat beat) const { return AccelerationStructure.ConvertBeatToTimeUsingLookupTableIndexing(beat); }
	inline Beat TimeToBeat(Time time) const { return TimeToBeat(time, false); }
	inline Beat TimeToBeat(Time time, bool truncTo0) const { return AccelerationStructure.ConvertTimeToBeatUsingLookupTableBinarySearch(time, truncTo0); }
	inline f64 BeatAndTimeToHBScrollBeatTick(Beat beat, Time time) const { return AccelerationStructure.ConvertBeatAndTimeToHBScrollBeatTickUsingLookupTableIndexing(beat, time); }

	struct ForEachBeatBarData { TimeSignature Signature; Beat Beat; i32 BarIndex; b8 IsBar; };
	template <typename Func>
	inline void ForEachBeatBar(Func perBeatBarFunc) const
	{
		BeatSortedForwardIterator<TimeSignatureChange> signatureChangeIt {};
		Beat beatIt = {};

		for (i32 barIndex = 0; /*barIndex < MAX_BAR_COUNT*/; barIndex++)
		{
			const TimeSignatureChange* thisChange = signatureChangeIt.Next(Signature.Sorted, beatIt);
			TimeSignature thisSignature = (thisChange == nullptr) ? FallbackTimeSignature : thisChange->Signature;
			b8 isSignatureNegative = (thisSignature.Numerator < 0) != (thisSignature.Denominator < 0);
			thisSignature.Numerator = (isSignatureNegative ? -1 : 1) * ClampBot(abs(thisSignature.Numerator), 1);
			thisSignature.Denominator = ClampBot(abs(thisSignature.Denominator), 1);

			const Beat durationPerBar = std::max(abs(thisSignature.GetDurationPerBar()), Beat::FromTicks(1));
			if (auto flow = perBeatBarFunc(ForEachBeatBarData{ thisSignature, beatIt, barIndex, true }); flow == ControlFlow::Break) {
				return;
			} else if (flow == ControlFlow::Continue) {
				beatIt += durationPerBar;
				continue;
			}

			const i32 beatsPerBar = abs(thisSignature.GetBeatsPerBar());
			const Beat durationPerBeat = abs(thisSignature.GetDurationPerBeat());
			Beat beatWithinBar = beatIt;
			for (i32 beatIndexWithinBar = 1; beatIndexWithinBar < beatsPerBar; beatIndexWithinBar++)
			{
				beatWithinBar += durationPerBeat;
				if (perBeatBarFunc(ForEachBeatBarData { thisSignature, beatWithinBar, barIndex, false }) == ControlFlow::Break)
					return;
			}
			beatIt += durationPerBar;
		}
	}
};

template <typename T>
inline const T* BeatSortedForwardIterator<T>::Next(const std::vector<T>& sortedList, Beat nextBeat)
{
	const T* next = nullptr;
	for (size_t i = LastIndex; i < sortedList.size(); i++)
	{
		if (Beat beat = GetBeat(sortedList[i]); beat <= nextBeat)
			next = &sortedList[i];
		else if (beat > nextBeat)
			break;
	}
	if (next != nullptr)
		LastIndex = ArrayItToIndex(next, &sortedList[0]);
	return next;
}

template <typename T>
T* BeatSortedList<T>::TryFindLastAtBeat(Beat beat)
{
	return const_cast<T*>(static_cast<const BeatSortedList<T>*>(this)->TryFindLastAtBeat(beat));
}

template <typename T>
T* BeatSortedList<T>::TryFindExactAtBeat(Beat beat)
{
	return const_cast<T*>(static_cast<const BeatSortedList<T>*>(this)->TryFindExactAtBeat(beat));
}

template <typename T>
const T* BeatSortedList<T>::TryFindLastAtBeat(Beat beat) const
{
	// TODO: Optimize using binary search
	if (Sorted.size() == 0)
		return nullptr;
	if (Sorted.size() == 1)
		return (GetBeat(Sorted[0]) <= beat) ? &Sorted[0] : nullptr;
	if (beat < GetBeat(Sorted[0]))
		return nullptr;
	for (size_t i = 0; i < Sorted.size() - 1; i++)
	{
		if (GetBeat(Sorted[i]) <= beat && GetBeat(Sorted[i + 1]) > beat)
			return &Sorted[i];
	}
	return &Sorted.back();
}

template <typename T>
const T* BeatSortedList<T>::TryFindExactAtBeat(Beat beat) const
{
	// TODO: Optimize using binary search
	for (const T& v : Sorted)
	{
		if (GetBeat(v) == beat)
			return &v;
	}
	return nullptr;
}

template <typename T>
T* BeatSortedList<T>::TryFindOverlappingBeat(Beat beatStart, Beat beatEnd, b8 inclusiveBeatCheck)
{
	return const_cast<T*>(static_cast<const BeatSortedList<T>*>(this)->TryFindOverlappingBeat(beatStart, beatEnd, inclusiveBeatCheck));
}

template <typename T>
const T* BeatSortedList<T>::TryFindOverlappingBeat(Beat beatStart, Beat beatEnd, b8 inclusiveBeatCheck) const
{
	assert(beatEnd >= beatStart && "Don't accidentally mix up BeatEnd with BeatDuration");
	return this->TryFindOverlappingBeatUntrusted(beatStart, beatEnd, inclusiveBeatCheck);
}

template <typename T>
T* BeatSortedList<T>::TryFindOverlappingBeatUntrusted(Beat beatStart, Beat beatEnd, b8 inclusiveBeatCheck)
{
	return const_cast<T*>(static_cast<const BeatSortedList<T>*>(this)->TryFindOverlappingBeatUntrusted(beatStart, beatEnd, inclusiveBeatCheck));
}

template <typename T>
const T* BeatSortedList<T>::TryFindOverlappingBeatUntrusted(Beat beatStart, Beat beatEnd, b8 inclusiveBeatCheck) const
{
	// TODO: Optimize using binary search
	const T* found = nullptr;
	if (inclusiveBeatCheck)
	{
		for (const T& v : Sorted)
		{
			// NOTE: Only break after a large beat has been found to correctly handle long notes with other notes "inside"
			//		 (even if they should't be placable in the first place)
			if (GetBeat(v) <= beatEnd && beatStart <= (GetBeat(v) + GetBeatDuration(v)))
				found = &v;
			else if ((GetBeat(v) + GetBeatDuration(v)) > beatEnd)
				break;
		}
	}
	else
	{
		for (const T& v : Sorted)
		{
			if (GetBeat(v) < beatEnd && beatStart < (GetBeat(v) + GetBeatDuration(v)))
				found = &v;
			else if ((GetBeat(v) + GetBeatDuration(v)) > beatEnd)
				break;
		}
	}
	return found;
}

template <typename T>
inline size_t LinearlySearchForInsertionIndex(const BeatSortedList<T>& sortedList, Beat beat)
{
	// TODO: Optimize using binary search
	for (size_t i = 0; i < sortedList.size(); i++)
		if (beat <= GetBeat(sortedList[i])) return i;
	return sortedList.size();
}

template <typename T>
inline b8 ValidateIsSortedByBeat(const BeatSortedList<T>& sortedList)
{
	return std::is_sorted(sortedList.begin(), sortedList.end(), [](const T& a, const T& b) { return GetBeat(a) < GetBeat(b); });
}

template <typename T> template <typename Func>
size_t BeatSortedList<T>::InsertOrFunc(const T& valueToInsert, Func funcExist)
{
	const size_t insertionIndex = LinearlySearchForInsertionIndex(*this, GetBeat(valueToInsert));
	if (InBounds(insertionIndex, Sorted))
	{
		if (T& existing = Sorted[insertionIndex]; GetBeat(existing) == GetBeat(valueToInsert))
			funcExist(existing, valueToInsert);
		else
			Sorted.insert(Sorted.begin() + insertionIndex, valueToInsert);
	}
	else
	{
		Sorted.push_back(valueToInsert);
	}

#if PEEPO_DEBUG
	assert(GetBeat(valueToInsert).Ticks >= 0);
	assert(ValidateIsSortedByBeat(*this));
#endif

	return insertionIndex;
}

template <typename T>
std::pair<size_t, b8> BeatSortedList<T>::InsertOrIgnore(const T& valueToInsert)
{
	b8 isInserted = true;
	return { InsertOrFunc(valueToInsert, [&](...) { isInserted = false; }), isInserted };
}

template <typename T>
size_t BeatSortedList<T>::InsertOrUpdate(const T& valueToInsertOrUpdate)
{
	return InsertOrFunc(valueToInsertOrUpdate, [&](T& existing, ...) { existing = valueToInsertOrUpdate; });
}

template <typename T>
void BeatSortedList<T>::RemoveAtBeat(Beat beatToFindAndRemove)
{
	if (const T* foundAtBeat = TryFindExactAtBeat(beatToFindAndRemove); foundAtBeat != nullptr)
		RemoveAtIndex(ArrayItToIndex(foundAtBeat, &Sorted[0]));
}

template <typename T>
void BeatSortedList<T>::RemoveAtIndex(size_t indexToRemove)
{
	if (InBounds(indexToRemove, Sorted))
		Sorted.erase(Sorted.begin() + indexToRemove);
}
