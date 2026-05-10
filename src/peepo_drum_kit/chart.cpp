#include "chart.h"
#include "core_build_info.h"
#include <algorithm>
#include <array>

namespace PeepoDrumKit
{
	void DebugCompareCharts(const ChartProject& chartA, const ChartProject& chartB, DebugCompareChartsOnMessageFunc onMessageFunc)
	{
		using ASCII::ToString;
		auto logf = [&](b8 isError, cstr fmt, auto&&... args)
		{
			char buffer[512];
			onMessageFunc(std::string_view(buffer, sprintf_s(buffer, ArrayCount(buffer), fmt, std::forward<decltype(args)>(args)...)), isError);
		};
		auto logErr = [&](cstr fmt, auto&&... args) { logf(true, fmt, std::forward<decltype(args)>(args)...); };
		auto logInfo = [&](cstr fmt, auto&&... args) { logf(false, fmt, std::forward<decltype(args)>(args)...); };

		if (chartA.Courses.size() != chartB.Courses.size()) { logErr("Course count mismatch (%zu != %zu)", chartA.Courses.size(), chartB.Courses.size()); return; }

		for (size_t i = 0; i < chartA.Courses.size(); i++)
		{
			const ChartCourse& courseA = *chartA.Courses[i];
			const ChartCourse& courseB = *chartB.Courses[i];
			if (i > 0)
				logInfo(""); // empty line
			logInfo("Course #%d: %.*s vs %.*s", i, FmtStrViewArgs(courseA.ToString()), FmtStrViewArgs(courseB.ToString()));

			for (GenericList list = {}; list < GenericList::Count; IncrementEnum(list))
			{
				const size_t countA = GetGenericListCount(courseA, list);
				const size_t countB = GetGenericListCount(courseB, list);
				const auto listName = ToString(list);
				if (countA != countB) { logErr("%.*s count mismatch (%zu != %zu)", FmtStrViewArgs(listName), countA, countB); continue; }

				for (size_t itemIndex = 0; itemIndex < countA; itemIndex++)
				{
					for (GenericMember member = {}; member < GenericMember::Count; IncrementEnum(member))
					{
						GenericMemberUnion valueA {}, valueB {};
						const b8 hasValueA = TryGet(courseA, list, itemIndex, member, valueA);
						const b8 hasValueB = TryGet(courseB, list, itemIndex, member, valueB);
						assert(hasValueA == hasValueB);
						if (!hasValueA || member == GenericMember::B8_IsSelected)
							continue;

						const auto memberName = ToString(member);
						TryDo([&](auto&& typedValueA, auto&& typedValueB)
						{
							auto checkMatch = [&](b8 match, auto&& a, auto&& b)
							{
								if (!match) {
									auto strA = ToString(a);
									auto strB = ToString(b);
									logErr("%.*s[%zu].%.*s value mismatch: %.*s vs. %.*s", FmtStrViewArgs(listName), itemIndex, FmtStrViewArgs(memberName), FmtStrViewArgs(strA), FmtStrViewArgs(strB));
								}
							};
							using T = decltype(typedValueA);
							if constexpr (expect_type_v<T, cstr>) {
								checkMatch((!typedValueA && !typedValueB) || (typedValueA && typedValueB && strcmp(typedValueA, typedValueB) == 0), typedValueA, typedValueB);
							} else if constexpr (expect_type_v<T, Complex>) {
								checkMatch(ApproxmiatelySame(typedValueA, typedValueB), typedValueA, typedValueB);
							} else if constexpr (expect_type_v<T, Time>) {
								checkMatch(ApproxmiatelySame(typedValueA.Seconds, typedValueB.Seconds), typedValueA.Seconds, typedValueB.Seconds);
							} else if constexpr (expect_type_v<T, Tempo>) {
								checkMatch(ApproxmiatelySame(typedValueA.BPM, typedValueB.BPM), typedValueA.BPM, typedValueB.BPM);
							} else if constexpr (expect_type_v<T, Beat>) {
								checkMatch(typedValueA == typedValueB, typedValueA.Ticks, typedValueB.Ticks);
							} else {
								checkMatch(typedValueA == typedValueB, typedValueA, typedValueB);
							}
						}, valueA, member, valueB);
					}
				}
			}
		}
	}

	struct TempTimedDelayCommand { Beat Beat; Time Delay; };

	template <>
	struct IsNonListChartEventTrait<TempTimedDelayCommand> : std::true_type { };

	template <GenericMember Member, typename TempTimedDelayCommandT, expect_type_t<TempTimedDelayCommandT, TempTimedDelayCommand> = true>
	constexpr decltype(auto) get(TempTimedDelayCommandT&& event)
	{
		if constexpr (Member == GenericMember::Beat_Start) return (std::forward<TempTimedDelayCommandT>(event).Beat);
	}

	static constexpr NoteType ConvertTJANoteType(TJA::NoteType tjaNoteType)
	{
		switch (tjaNoteType)
		{
		case TJA::NoteType::None: return NoteType::Count;
		case TJA::NoteType::Don: return NoteType::Don;
		case TJA::NoteType::Ka: return NoteType::Ka;
		case TJA::NoteType::DonBig: return NoteType::DonBig;
		case TJA::NoteType::KaBig: return NoteType::KaBig;
		case TJA::NoteType::Start_Drumroll: return NoteType::Drumroll;
		case TJA::NoteType::Start_DrumrollBig: return NoteType::DrumrollBig;
		case TJA::NoteType::Start_Balloon: return NoteType::Balloon;
		case TJA::NoteType::End_BalloonOrDrumroll: return NoteType::Count;
		case TJA::NoteType::Start_BaloonSpecial: return NoteType::BalloonSpecial;
		case TJA::NoteType::DonBigBoth: return NoteType::DonBigHand;
		case TJA::NoteType::KaBigBoth: return NoteType::KaBigHand;
		case TJA::NoteType::Hidden: return NoteType::Adlib;
		case TJA::NoteType::Bomb: return NoteType::Bomb;
		case TJA::NoteType::KaDon: return NoteType::KaDon;
		case TJA::NoteType::Fuse: return NoteType::Fuse;
		default: return NoteType::Count;
		}
	}

	static constexpr TJA::NoteType ConvertTJANoteType(NoteType noteType)
	{
		switch (noteType)
		{
		case NoteType::Don: return TJA::NoteType::Don;
		case NoteType::DonBig: return TJA::NoteType::DonBig;
		case NoteType::Ka: return TJA::NoteType::Ka;
		case NoteType::KaBig: return TJA::NoteType::KaBig;
		case NoteType::Drumroll: return TJA::NoteType::Start_Drumroll;
		case NoteType::DrumrollBig: return TJA::NoteType::Start_DrumrollBig;
		case NoteType::Balloon: return TJA::NoteType::Start_Balloon;
		case NoteType::BalloonSpecial: return TJA::NoteType::Start_BaloonSpecial;
		case NoteType::DonBigHand: return TJA::NoteType::DonBigBoth;
		case NoteType::KaBigHand: return TJA::NoteType::KaBigBoth;
		case NoteType::KaDon: return TJA::NoteType::KaDon;
		case NoteType::Adlib: return TJA::NoteType::Hidden;
		case NoteType::Fuse: return TJA::NoteType::Fuse;
		case NoteType::Bomb: return TJA::NoteType::Bomb;
		default: return TJA::NoteType::None;
		}
	}

	std::string ChartCourse::ToString(OmitLevel omitLevel) const
	{
		constexpr cstr fmts[] = { u8"%s ★%.0f%s %s", u8"%.0s★%.0f%s %s", u8"%.0s★%.0f%s%.0s" };
		cstr fmt = fmts[std::array{ 0, 1, 1, 2 } [EnumToIndex(omitLevel)] ];
		f64 levelRound = Round(Level, std::pow(10, -LevelDecimalPlaces));
		f64 levelWhole, levelFrac = std::modf(levelRound, &levelWhole);

		std::string buffer;
		buffer.resize(96);
		int len = sprintf_s(buffer.data(), buffer.size(), fmt,
			UI_StrRuntime(DifficultyTypeNames[EnumToIndex(Type)]),
			levelWhole,
			(LevelDecimalPlaces == 0) ? "" : (10 * levelFrac >= DifficultyLevelDecimal::PlusThreshold) ? "+" : "",
			GetStyleName(Style, PlayerSide, omitLevel >= ChartCourse::OmitLevel::PlayerCount).data());
		buffer.resize(std::max(0, len)); // set to empty if -1
		return buffer;
	}

	Beat FindCourseMaxUsedBeat(const ChartCourse& course)
	{
		// NOTE: Technically only need to look at the last item of each sorted list **but just to be sure**, in case there is something wonky going on with out-of-order durations or something
		Beat maxBeat = Beat::Zero();
		ApplyForEachGenericList([&](GenericList list, const auto& typedList)
		{
			for (const auto& v : typedList)
				maxBeat = Max(maxBeat, GetBeat(v) + Max(Beat::Zero(), GetBeatDuration(v)));
		}, course);
		return maxBeat;
	}

	Beat FindCourseMaxUsedBeatFast(const ChartCourse& course)
	{
		// NOTE: Fast version by only look at the last item of each sorted list
		Beat maxBeat = Beat::Zero();
		ApplyForEachGenericList([&](GenericList list, const auto& typedList)
		{
			if (!typedList.empty()) {
				const auto& last = typedList[std::size(typedList) - 1];
				maxBeat = Max(maxBeat, GetBeat(last) + Max(Beat::Zero(), GetBeatDuration(last)));
			}
		}, course);
		return maxBeat;
	}

	b8 CreateChartProjectFromTJA(const TJA::ParsedTJA& inTJA, ChartProject& out)
	{
		out.ChartDuration = Time::Zero();
		out.ChartTitle = inTJA.Metadata.TITLE;
		out.ChartTitleLocalized = inTJA.Metadata.TITLE_localized;
		out.ChartSubtitle = inTJA.Metadata.SUBTITLE;
		out.ChartSubtitleLocalized = inTJA.Metadata.SUBTITLE_localized;
		out.ChartCreator = inTJA.Metadata.MAKER;
		out.ChartGenre = inTJA.Metadata.GENRE;
		out.ChartLyricsFileName = inTJA.Metadata.LYRICS;
		out.SongOffset = inTJA.Metadata.OFFSET;
		out.SongDemoStartTime = inTJA.Metadata.DEMOSTART;
		out.SongFileName = inTJA.Metadata.WAVE;
		out.SongJacket = inTJA.Metadata.PREIMAGE;
		out.SongVolume = inTJA.Metadata.SONGVOL;
		out.SoundEffectVolume = inTJA.Metadata.SEVOL;
		out.BackgroundImageFileName = inTJA.Metadata.BGIMAGE;
		out.BackgroundMovieFileName = inTJA.Metadata.BGMOVIE;
		out.MovieOffset = inTJA.Metadata.MOVIEOFFSET;
		out.OtherMetadata = inTJA.Metadata.Others;
		for (size_t i = 0; i < inTJA.Courses.size(); i++)
		{
			if (!inTJA.Courses[i].HasChart) // metadata-only TJA section
				continue;

			const TJA::ConvertedCourse& inCourse = TJA::ConvertParsedToConvertedCourse(inTJA, inTJA.Courses[i]);
			ChartCourse& outCourse = *out.Courses.emplace_back(std::make_unique<ChartCourse>());

			// HACK: Write proper enum conversion functions
			outCourse.Type = Clamp(static_cast<DifficultyType>(inCourse.CourseMetadata.COURSE), DifficultyType {}, DifficultyType::Count);
			outCourse.Level = inCourse.CourseMetadata.LEVEL;
			outCourse.LevelDecimalPlaces = inCourse.CourseMetadata.LEVEL_DECIMALPLACES;
			outCourse.Style = std::max(inCourse.CourseMetadata.STYLE, 1);
			outCourse.PlayerSide = std::clamp(inCourse.CourseMetadata.START_PLAYERSIDE, 1, outCourse.Style);

			outCourse.CourseCreator = inCourse.CourseMetadata.NOTESDESIGNER;

			outCourse.Life = Clamp(static_cast<TowerLives>(inCourse.CourseMetadata.LIFE), TowerLives::Min, TowerLives::Max);
			outCourse.Side = Clamp(static_cast<Side>(inCourse.CourseMetadata.SIDE), Side{}, Side::Count);

			outCourse.TempoMap.Tempo.Sorted = { TempoChange(Beat::Zero(), inTJA.Metadata.BPM) };
			outCourse.TempoMap.Signature.Sorted = { TimeSignatureChange(Beat::Zero(), TimeSignature(4, 4)) };
			TimeSignature lastSignature = TimeSignature(4, 4);

			i32 currentBalloonIndex = 0;

			BeatSortedList<TempTimedDelayCommand> tempSortedDelayCommands;
			BeatSortedForwardIterator<TempTimedDelayCommand> tempDelayCommandsIt;
			for (const TJA::ConvertedMeasure& inMeasure : inCourse.Measures)
			{
				for (const TJA::ConvertedDelayChange& inDelayChange : inMeasure.DelayChanges)
					tempSortedDelayCommands.InsertOrUpdate(TempTimedDelayCommand { inMeasure.StartTime + inDelayChange.TimeWithinMeasure, inDelayChange.Delay });
			}

			for (const TJA::ConvertedMeasure& inMeasure : inCourse.Measures)
			{
				for (const TJA::ConvertedNote& inNote : inMeasure.Notes)
				{
					if (inNote.Type == TJA::NoteType::End_BalloonOrDrumroll)
					{
						// TODO: Proper handling
						if (!outCourse.Notes_Normal.Sorted.empty())
							outCourse.Notes_Normal.Sorted.back().BeatDuration = (inMeasure.StartTime + inNote.TimeWithinMeasure) - outCourse.Notes_Normal.Sorted.back().BeatTime;
						continue;
					}

					const NoteType outNoteType = ConvertTJANoteType(inNote.Type);
					if (outNoteType == NoteType::Count)
						continue;

					Note& outNote = outCourse.Notes_Normal.Sorted.emplace_back();
					outNote.BeatTime = (inMeasure.StartTime + inNote.TimeWithinMeasure);
					outNote.Type = outNoteType;

					const TempTimedDelayCommand* delayCommandForThisNote = tempDelayCommandsIt.Next(tempSortedDelayCommands.Sorted, outNote.BeatTime);
					outNote.TimeOffset = (delayCommandForThisNote != nullptr) ? delayCommandForThisNote->Delay : Time::Zero();

					if (inNote.Type == TJA::NoteType::Start_Balloon || inNote.Type == TJA::NoteType::Start_BaloonSpecial || inNote.Type == TJA::NoteType::Fuse)
					{
						// TODO: Implement properly with correct branch handling
						if (InBounds(currentBalloonIndex, inCourse.CourseMetadata.BALLOON))
							outNote.BalloonPopCount = inCourse.CourseMetadata.BALLOON[currentBalloonIndex];
						currentBalloonIndex++;
					}
				}

				if (inMeasure.TimeSignature != lastSignature)
				{
					outCourse.TempoMap.Signature.InsertOrUpdate(TimeSignatureChange(inMeasure.StartTime, inMeasure.TimeSignature));
					lastSignature = inMeasure.TimeSignature;
				}

				for (const TJA::ConvertedTempoChange& inTempoChange : inMeasure.TempoChanges)
					outCourse.TempoMap.Tempo.InsertOrUpdate(TempoChange(inMeasure.StartTime + inTempoChange.TimeWithinMeasure, inTempoChange.Tempo));

				for (const TJA::ConvertedScrollChange& inScrollChange : inMeasure.ScrollChanges)
					outCourse.ScrollChanges.Sorted.push_back(ScrollChange { (inMeasure.StartTime + inScrollChange.TimeWithinMeasure), inScrollChange.ScrollSpeed });

				for (const TJA::ConvertedScrollType& inScrollType : inMeasure.ScrollTypes)
					outCourse.ScrollTypes.Sorted.push_back(ScrollType{ (inMeasure.StartTime + inScrollType.TimeWithinMeasure),  static_cast<ScrollMethod>(inScrollType.Method) });

				for (const TJA::ConvertedSudden& inSuddenChange : inMeasure.SuddenChanges)
					outCourse.SuddenChanges.Sorted.push_back(SuddenChange{ (inMeasure.StartTime + inSuddenChange.TimeWithinMeasure), inSuddenChange.AppearanceOffset, inSuddenChange.MovementOffset, inSuddenChange.HideRoll });

				for (const TJA::ConvertedJPOSScroll& inJPOSScrollChange : inMeasure.JPOSScrollChanges)
					outCourse.JPOSScrollChanges.Sorted.push_back(JPOSScrollChange{ (inMeasure.StartTime + inJPOSScrollChange.TimeWithinMeasure), inJPOSScrollChange.Move, inJPOSScrollChange.Duration });


				for (const TJA::ConvertedBarLineChange& barLineChange : inMeasure.BarLineChanges)
					outCourse.BarLineChanges.Sorted.push_back(BarLineChange { (inMeasure.StartTime + barLineChange.TimeWithinMeasure), barLineChange.Visibile });

				for (const TJA::ConvertedLyricChange& lyricChange : inMeasure.LyricChanges)
					outCourse.Lyrics.Sorted.push_back(LyricChange { (inMeasure.StartTime + lyricChange.TimeWithinMeasure), lyricChange.Lyric });
			}

			for (const TJA::ConvertedGoGoRange& inGoGoRange : inCourse.GoGoRanges)
				outCourse.GoGoRanges.Sorted.push_back(GoGoRange { inGoGoRange.StartTime, (inGoGoRange.EndTime - inGoGoRange.StartTime) });

			//outCourse.TempoMap.SetTempoChange(TempoChange());
			//outCourse.TempoMap = inCourse.GoGoRanges;

			outCourse.ScoreInit = inCourse.CourseMetadata.SCOREINIT;
			outCourse.ScoreDiff = inCourse.CourseMetadata.SCOREDIFF;

			outCourse.OtherMetadata = inCourse.CourseMetadata.Others;

			outCourse.TempoMap.RebuildAccelerationStructure();
			outCourse.RecalculateNoteStates();

			// NOTE: use the non-0 shortest duration to prevent extra measures (editor need to display until max used beat in each difficulty)
			if (!inCourse.Measures.empty()) {
				Time courseDuration = outCourse.TempoMap.BeatToTime(inCourse.Measures.back().StartTime); // last measure end time
				if (out.ChartDuration <= Time::Zero())
					out.ChartDuration = courseDuration;
				else if (courseDuration <= Time::Zero())
					/* keep out.ChartDuration unchanged */;
				else
					out.ChartDuration = Min(out.ChartDuration, courseDuration);
			}
		}

		return true;
	}

	b8 ConvertChartProjectToTJA(const ChartProject& in, TJA::ParsedTJA& out, b8 includePeepoDrumKitComment)
	{
		static constexpr cstr FallbackTJAChartTitle = "Untitled Chart";
		out.Metadata.TITLE = !in.ChartTitle.empty() ? in.ChartTitle : FallbackTJAChartTitle;
		out.Metadata.TITLE_localized = in.ChartTitleLocalized;
		out.Metadata.SUBTITLE = in.ChartSubtitle;
		out.Metadata.SUBTITLE_localized = in.ChartSubtitleLocalized;
		out.Metadata.MAKER = in.ChartCreator;
		out.Metadata.GENRE = in.ChartGenre;
		out.Metadata.LYRICS = in.ChartLyricsFileName;
		out.Metadata.OFFSET = in.SongOffset;
		out.Metadata.DEMOSTART = in.SongDemoStartTime;
		out.Metadata.WAVE = in.SongFileName;
		out.Metadata.PREIMAGE = in.SongJacket;
		out.Metadata.SONGVOL = in.SongVolume;
		out.Metadata.SEVOL = in.SoundEffectVolume;
		out.Metadata.BGIMAGE = in.BackgroundImageFileName;
		out.Metadata.BGMOVIE = in.BackgroundMovieFileName;
		out.Metadata.MOVIEOFFSET = in.MovieOffset;
		out.Metadata.Others = in.OtherMetadata;

		if (includePeepoDrumKitComment)
		{
			out.HasPeepoDrumKitComment = true;
			out.PeepoDrumKitCommentDate = BuildInfo::CompilationDateParsed;
		}

		if (!in.Courses.empty())
		{
			if (!in.Courses[0]->TempoMap.Tempo.empty())
			{
				const TempoChange* initialTempo = in.Courses[0]->TempoMap.Tempo.TryFindLastAtBeat(Beat::Zero());
				out.Metadata.BPM = (initialTempo != nullptr) ? initialTempo->Tempo : FallbackTempo;
			}
		}

		out.Courses.reserve(in.Courses.size());
		for (const std::unique_ptr<ChartCourse>& inCourseIt : in.Courses)
		{
			const ChartCourse& inCourse = *inCourseIt;
			TJA::ParsedCourse& outCourse = out.Courses.emplace_back();

			// HACK: Write proper enum conversion functions
			outCourse.Metadata.COURSE = static_cast<TJA::DifficultyType>(inCourse.Type);
			outCourse.Metadata.LEVEL = inCourse.Level;
			outCourse.Metadata.LEVEL_DECIMALPLACES = inCourse.LevelDecimalPlaces;
			outCourse.Metadata.STYLE = inCourse.Style;
			outCourse.Metadata.START_PLAYERSIDE = inCourse.PlayerSide;
			outCourse.Metadata.NOTESDESIGNER = inCourse.CourseCreator;
			for (const Note& inNote : inCourse.Notes_Normal) if (IsBalloonNote(inNote.Type)) { outCourse.Metadata.BALLOON.push_back(inNote.BalloonPopCount); }
			outCourse.Metadata.SCOREINIT = inCourse.ScoreInit;
			outCourse.Metadata.SCOREDIFF = inCourse.ScoreDiff;

			outCourse.Metadata.LIFE = static_cast<i32>(inCourse.Life);
			outCourse.Metadata.SIDE = static_cast<TJA::SongSelectSide>(inCourse.Side);

			outCourse.Metadata.Others = inCourse.OtherMetadata;

			// TODO: Is this implemented correctly..? Need to have enough measures to cover every note/command and pad with empty measures up to the chart duration
			// BUG: NOPE! "07 ゲームミュージック/003D. MagiCatz/MagiCatz.tja" for example still gets rounded up and then increased by a measure each time it gets saved
			// ... and even so does "Heat Haze Shadow 2.tja" without any weird time signatures..??
			// 1. rounded beat of time can have 1 extra tick which would become a whole measure -> used truncated beat
			// 2. use minimum length of difficulties in case the timing differ slightly
			const Beat inChartMaxUsedBeat = FindCourseMaxUsedBeat(inCourse);
			const Beat inChartBeatDuration = inCourse.TempoMap.TimeToBeat(in.GetDuration(), true);
			std::vector<TJA::ConvertedMeasure> outConvertedMeasures;

			inCourse.TempoMap.ForEachBeatBar([&](const SortedTempoMap::ForEachBeatBarData& it)
			{
				if (it.Beat > inChartMaxUsedBeat && it.Beat >= inChartBeatDuration) // ensure max used beat is converted
					return ControlFlow::Break;
				if (it.IsBar)
				{
					TJA::ConvertedMeasure& outConvertedMeasure = outConvertedMeasures.emplace_back();
					outConvertedMeasure.StartTime = it.Beat;
					outConvertedMeasure.TimeSignature = it.Signature;
				}
				// if max used beat is converted, can stop it now
				return (it.Beat >= inChartMaxUsedBeat && it.Beat >= inChartBeatDuration) ? ControlFlow::Break : ControlFlow::Fallthrough;
			});

			if (outConvertedMeasures.empty())
				outConvertedMeasures.push_back(TJA::ConvertedMeasure { Beat::Zero(), TimeSignature(4, 4) });

			static constexpr auto tryFindMeasureForBeat = [](std::vector<TJA::ConvertedMeasure>& measures, Beat beatToFind) -> TJA::ConvertedMeasure*
			{
				static constexpr auto isMoreBeat = [](const TJA::ConvertedMeasure& lhs, const TJA::ConvertedMeasure& rhs)
				{
					return lhs.StartTime > rhs.StartTime;
				};
				// Binary search in descending (ascending but reversed) list
				// if found: `it` is the last element such that `beatToFind >= it->StartTime`
				auto it = std::lower_bound(measures.rbegin(), measures.rend(), TJA::ConvertedMeasure { beatToFind }, isMoreBeat);
				return (it == measures.rend()) ? nullptr : &*it;
			};

			for (const TempoChange& inTempoChange : inCourse.TempoMap.Tempo)
			{
				if (!(&inTempoChange == &inCourse.TempoMap.Tempo[0] && inTempoChange.Tempo.BPM == out.Metadata.BPM.BPM))
				{
					TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inTempoChange.Beat);
					if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
						outConvertedMeasure->TempoChanges.push_back(TJA::ConvertedTempoChange { (inTempoChange.Beat - outConvertedMeasure->StartTime), inTempoChange.Tempo });
				}
			}

			Time lastNoteTimeOffset = Time::Zero();
			for (const Note& inNote : inCourse.Notes_Normal)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inNote.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->Notes.push_back(TJA::ConvertedNote { (inNote.BeatTime - outConvertedMeasure->StartTime), ConvertTJANoteType(inNote.Type) });

				if (inNote.BeatDuration > Beat::Zero())
				{
					TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inNote.BeatTime + inNote.BeatDuration);
					if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
						outConvertedMeasure->Notes.push_back(TJA::ConvertedNote { ((inNote.BeatTime + inNote.BeatDuration) - outConvertedMeasure->StartTime), TJA::NoteType::End_BalloonOrDrumroll });
				}

				const Time thisNoteTimeOffset = ApproxmiatelySame(inNote.TimeOffset.Seconds, 0.0) ? Time::Zero() : inNote.TimeOffset;
				if (thisNoteTimeOffset != lastNoteTimeOffset)
				{
					outConvertedMeasure->DelayChanges.push_back(TJA::ConvertedDelayChange { (inNote.BeatTime - outConvertedMeasure->StartTime), thisNoteTimeOffset });
					lastNoteTimeOffset = thisNoteTimeOffset;
				}
			}

			for (const ScrollChange& inScroll : inCourse.ScrollChanges)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inScroll.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->ScrollChanges.push_back(TJA::ConvertedScrollChange { (inScroll.BeatTime - outConvertedMeasure->StartTime), inScroll.ScrollSpeed });
			}

			for (const ScrollType& inScrollType : inCourse.ScrollTypes)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inScrollType.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->ScrollTypes.push_back(TJA::ConvertedScrollType { (inScrollType.BeatTime - outConvertedMeasure->StartTime), static_cast<i8>(inScrollType.Method) });
			}

			for (const SuddenChange& inSudden : inCourse.SuddenChanges)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inSudden.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->SuddenChanges.push_back(TJA::ConvertedSudden{ (inSudden.BeatTime - outConvertedMeasure->StartTime), inSudden.AppearanceOffset, inSudden.MovementOffset, inSudden.HideRoll });
			}

			for (const JPOSScrollChange& JPOSScroll : inCourse.JPOSScrollChanges)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, JPOSScroll.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->JPOSScrollChanges.push_back(TJA::ConvertedJPOSScroll { (JPOSScroll.BeatTime - outConvertedMeasure->StartTime), JPOSScroll.Move, JPOSScroll.Duration });
			}

			for (const BarLineChange& barLineChange : inCourse.BarLineChanges)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, barLineChange.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->BarLineChanges.push_back(TJA::ConvertedBarLineChange { (barLineChange.BeatTime - outConvertedMeasure->StartTime), barLineChange.IsVisible });
			}

			for (const LyricChange& inLyric : inCourse.Lyrics)
			{
				TJA::ConvertedMeasure* outConvertedMeasure = tryFindMeasureForBeat(outConvertedMeasures, inLyric.BeatTime);
				if (assert(outConvertedMeasure != nullptr); outConvertedMeasure != nullptr)
					outConvertedMeasure->LyricChanges.push_back(TJA::ConvertedLyricChange { (inLyric.BeatTime - outConvertedMeasure->StartTime), inLyric.Lyric });
			}

			// For go-go time events, convert each range to a pair of start & end changes
			for (const GoGoRange& gogo : inCourse.GoGoRanges)
			{
				// start
				TJA::ConvertedMeasure* outConvertedMeasureStart = tryFindMeasureForBeat(outConvertedMeasures, gogo.BeatTime);
				if (assert(outConvertedMeasureStart != nullptr); outConvertedMeasureStart != nullptr)
					outConvertedMeasureStart->GoGoChanges.push_back(TJA::ConvertedGoGoChange{ (gogo.BeatTime - outConvertedMeasureStart->StartTime), true });
				// end
				const Beat endTime = gogo.BeatTime + Max(Beat::Zero(), gogo.BeatDuration);
				TJA::ConvertedMeasure* outConvertedMeasureEnd = tryFindMeasureForBeat(outConvertedMeasures, endTime);
				if (assert(outConvertedMeasureEnd != nullptr); outConvertedMeasureEnd != nullptr)
					outConvertedMeasureEnd->GoGoChanges.push_back(TJA::ConvertedGoGoChange{ (endTime - outConvertedMeasureEnd->StartTime), false });
			}

			TJA::ConvertConvertedMeasuresToParsedCommands(outConvertedMeasures, outCourse.ChartCommands);
		}

		return true;
	}
}
