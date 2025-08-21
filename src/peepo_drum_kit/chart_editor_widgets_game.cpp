#include "chart_editor_widgets.h"

namespace PeepoDrumKit
{
	static constexpr f32 FrameToTime(f32 frame, f32 fps = 60.0f) { return (frame / fps); }
	static constexpr BezierKeyFrame2D GameNoteHitPath[] =
	{
		BezierKeyFrame2D::Linear(FrameToTime({  0 }), vec2 {  615, 386 }),
		BezierKeyFrame2D::Linear(FrameToTime({  1 }), vec2 {  639, 342 }),
		BezierKeyFrame2D::Linear(FrameToTime({  2 }), vec2 {  664, 300 }),
		BezierKeyFrame2D::Linear(FrameToTime({  3 }), vec2 {  693, 260 }),
		BezierKeyFrame2D::Linear(FrameToTime({  4 }), vec2 {  725, 222 }),
		BezierKeyFrame2D::Linear(FrameToTime({  5 }), vec2 {  758, 186 }),
		BezierKeyFrame2D::Linear(FrameToTime({  6 }), vec2 {  793, 153 }),
		BezierKeyFrame2D::Linear(FrameToTime({  7 }), vec2 {  830, 122 }),
		BezierKeyFrame2D::Linear(FrameToTime({  8 }), vec2 {  870, 93  }),
		BezierKeyFrame2D::Linear(FrameToTime({  9 }), vec2 {  912, 66  }),
		BezierKeyFrame2D::Linear(FrameToTime({ 10 }), vec2 {  954, 43  }),
		BezierKeyFrame2D::Linear(FrameToTime({ 11 }), vec2 { 1001, 27  }),
		BezierKeyFrame2D::Linear(FrameToTime({ 12 }), vec2 { 1046, 11  }),
		BezierKeyFrame2D::Linear(FrameToTime({ 13 }), vec2 { 1094, -2  }),
		BezierKeyFrame2D::Linear(FrameToTime({ 14 }), vec2 { 1142, -14 }),
		BezierKeyFrame2D::Linear(FrameToTime({ 15 }), vec2 { 1192, -18 }),
		BezierKeyFrame2D::Linear(FrameToTime({ 16 }), vec2 { 1240, -22 }),
		BezierKeyFrame2D::Linear(FrameToTime({ 17 }), vec2 { 1292, -23 }),
		BezierKeyFrame2D::Linear(FrameToTime({ 18 }), vec2 { 1336, -22 }),
		BezierKeyFrame2D::Linear(FrameToTime({ 19 }), vec2 { 1385, -16 }),
		BezierKeyFrame2D::Linear(FrameToTime({ 20 }), vec2 { 1435, -8  }),
		BezierKeyFrame2D::Linear(FrameToTime({ 21 }), vec2 { 1479, 3   }),
		BezierKeyFrame2D::Linear(FrameToTime({ 22 }), vec2 { 1526, 16  }),
		BezierKeyFrame2D::Linear(FrameToTime({ 23 }), vec2 { 1570, 36  }),
		BezierKeyFrame2D::Linear(FrameToTime({ 24 }), vec2 { 1612, 56  }),
		BezierKeyFrame2D::Linear(FrameToTime({ 25 }), vec2 { 1658, 83  }),
		BezierKeyFrame2D::Linear(FrameToTime({ 26 }), vec2 { 1696, 115 }),
		BezierKeyFrame2D::Linear(FrameToTime({ 27 }), vec2 { 1734, 144 }),
		BezierKeyFrame2D::Linear(FrameToTime({ 28 }), vec2 { 1770, 176 }),
		BezierKeyFrame2D::Linear(FrameToTime({ 29 }), vec2 { 1803, 210 }),
		BezierKeyFrame2D::Linear(FrameToTime({ 30 }), vec2 { 1836, 247 }),
	};

	static constexpr BezierKeyFrame1D GameNoteHitFadeIn[] =
	{
		{ FrameToTime(30.0f), 0.0f, 0.0f, 0.0f },
		{ FrameToTime(39.0f), 1.0f, 1.0f, 1.0f },
	};
	static constexpr BezierKeyFrame1D GameNoteHitFadeOut[] =
	{
		{ FrameToTime(39.0f), 1.0f, 1.0f, 1.0f },
		{ FrameToTime(46.0f), 0.0f, 0.0f, 0.0f },
	};
	static constexpr Time GameNoteHitAnimationDuration = Time::FromSec(GameNoteHitPath[ArrayCount(GameNoteHitPath) - 1].Time); // Time::FromFrames(46.0);

	struct NoteHitPathAnimationData
	{
		vec2 PositionOffset = {};
		f32 WhiteFadeIn = 0.0f;
		f32 AlphaFadeOut = 1.0f;
	};

	static NoteHitPathAnimationData GetNoteHitPathAnimation(Time timeSinceHit, f32 extendedLaneWidthFactor)
	{
		NoteHitPathAnimationData out {};
		if (timeSinceHit >= Time::Zero())
		{
			const f32 animationTime = timeSinceHit.ToSec_F32();
			out.PositionOffset = SampleBezierFCurve(GameNoteHitPath, animationTime) - GameNoteHitPath[0].Value;
			out.PositionOffset.x *= extendedLaneWidthFactor;
#if 0 // TODO: Just doesn't really look that great... maybe also needs the other hit effects too
			out.WhiteFadeIn = SampleBezierFCurve(NoteHitFadeIn, animationTime);
			out.AlphaFadeOut = SampleBezierFCurve(NoteHitFadeOut, animationTime);
#endif
		}
		return out;
	}

	static constexpr Time TimeSinceNoteHit(Time noteTime, Time cursorTime) { return (cursorTime - noteTime); }

	static u32 InterpolateDrumrollHitColor(NoteType noteType, f32 hitPercentage)
	{
		u32 hitNoteColor = 0xFFFFFFFF;
		if (hitPercentage > 0.0f)
			hitNoteColor = Gui::ColorConvertFloat4ToU32(ImLerp(Gui::ColorConvertU32ToFloat4(hitNoteColor), Gui::ColorConvertU32ToFloat4(NoteColorDrumrollHit), hitPercentage));
		return hitNoteColor;
	}

	constexpr static std::pair<Angle, bool> GetNoteFaceRotationMirror(Complex scrollSpeed, NoteType noteType)
	{
		switch (noteType) {
		default:
			return {
				Angle::FromRadians(arg((scrollSpeed.GetRealPart() >= 0) ? scrollSpeed.cpx : -scrollSpeed.cpx)),
				(scrollSpeed.GetRealPart() < 0),
			}; // prevent top-down rotation; mirror horizontally for negative scroll balloon
		case NoteType::Bomb:
			return { Angle::FromRadians(0), false }; // no rotation
		}
	}

	static void DrawGamePreviewNote(ChartGraphicsResources& gfx, const GameCamera& camera, ImDrawList* drawList, vec2 center, Complex scrollSpeed, NoteType noteType)
	{
		SprID spr = SprID::Count;
		switch (noteType)
		{
		case NoteType::Don: { spr = SprID::Game_Note_Don; } break;
		case NoteType::DonBig: { spr = SprID::Game_Note_DonBig; } break;
		case NoteType::Ka: { spr = SprID::Game_Note_Ka; } break;
		case NoteType::KaBig: { spr = SprID::Game_Note_KaBig; } break;
		case NoteType::Drumroll: { spr = SprID::Game_Note_Drumroll; } break;
		case NoteType::DrumrollBig: { spr = SprID::Game_Note_DrumrollBig; } break;
		case NoteType::Balloon: { spr = SprID::Game_Note_Balloon; } break;
		case NoteType::BalloonSpecial: { spr = SprID::Game_Note_BalloonSpecial; } break;
		case NoteType::KaDon: { spr = SprID::Game_Note_KaDon; } break;
		case NoteType::Adlib: { spr = SprID::Game_Note_Adlib; } break;
		case NoteType::Fuse: { spr = SprID::Game_Note_Fuse; } break;
		case NoteType::Bomb: { spr = SprID::Game_Note_Bomb; } break;
		}

		// TODO: Right part stretch animation
		if (noteType == NoteType::Balloon) { /* ... */ }

		const auto [angle, mirror] = GetNoteFaceRotationMirror(scrollSpeed, noteType);
		gfx.DrawSprite(drawList, spr, SprTransform::FromCenter(
			camera.WorldToScreenSpace(center),
			vec2(camera.WorldToScreenScale(mirror ? -1.0f : 1.0f), camera.WorldToScreenScale(1.0f)),
			angle));
	}

	static void DrawGamePreviewNoteDuration(ChartGraphicsResources& gfx, const GameCamera& camera, ImDrawList* drawList, vec2 centerHead, vec2 centerTail, NoteType noteType, u32 colorTint = 0xFFFFFFFF)
	{
		const SprID spr = IsFuseRoll(noteType)
			? SprID::Game_Note_FuseLong
			: (IsBigNote(noteType) 
				? SprID::Game_Note_DrumrollLongBig 
				: SprID::Game_Note_DrumrollLong);
		const SprInfo sprInfo = gfx.GetInfo(spr);

		const f32 midScaleX = (Distance(centerTail, centerHead) / (sprInfo.SourceSize.x)) * 3.0f;

		const SprStretchtOut split = StretchMultiPartSpr(gfx, spr,
			SprTransform::FromCenter(
				camera.WorldToScreenSpace((centerHead + centerTail) / 2.0f),
				vec2(camera.WorldToScreenScale(1.0f)),
				AngleBetween(centerTail, centerHead)),
				colorTint,
			SprStretchtParam { 1.0f, midScaleX, 1.0f }, 3);

		for (size_t i = 0; i < 3; i++)
			gfx.DrawSprite(drawList, split.Quads[i]);
	}

	static void DrawGamePreviewNoteSEText(ChartGraphicsResources& gfx, const GameCamera& camera, ImDrawList* drawList, vec2 centerHead, vec2 centerTail, Complex scrollSpeed, NoteSEType seType)
	{
		static constexpr f32 contentToFooterOffsetY = (GameLaneSlice.FooterCenterY() - GameLaneSlice.ContentCenterY());
		centerHead.y += contentToFooterOffsetY;
		centerTail.y += contentToFooterOffsetY;

		SprID spr = SprID::Count;
		switch (seType)
		{
		case NoteSEType::Do: { spr = SprID::Game_NoteTxt_Do; } break;
		case NoteSEType::Don: { spr = SprID::Game_NoteTxt_Don; } break;
		case NoteSEType::DonBig: { spr = SprID::Game_NoteTxt_DonBig; } break;
		case NoteSEType::Ka: { spr = SprID::Game_NoteTxt_Ka; } break;
		case NoteSEType::Katsu: { spr = SprID::Game_NoteTxt_Katsu; } break;
		case NoteSEType::KatsuBig: { spr = SprID::Game_NoteTxt_KatsuBig; } break;
		case NoteSEType::Drumroll: { spr = SprID::Game_NoteTxt_Drumroll; } break;
		case NoteSEType::DrumrollBig: { spr = SprID::Game_NoteTxt_DrumrollBig; } break;
		case NoteSEType::Balloon: { spr = SprID::Game_NoteTxt_Balloon; } break;
		case NoteSEType::BalloonSpecial: { spr = SprID::Game_NoteTxt_BalloonSpecial; } break;
		default: return;
		}

		if (seType == NoteSEType::Drumroll || seType == NoteSEType::DrumrollBig)
		{
			const SprInfo sprInfo = gfx.GetInfo(spr);

			const f32 midAlignmentOffset = (seType == NoteSEType::DrumrollBig) ? 136.0f : 68.0f;
			const f32 distance = Distance(centerTail, centerHead);
			const f32 midScaleX = (ClampBot(distance - midAlignmentOffset, 0.0f) / (sprInfo.SourceSize.x)) * 3.0f;

			const SprStretchtOut split = StretchMultiPartSpr(gfx, spr,
				SprTransform::FromCenter(
					camera.WorldToScreenSpace((centerHead + centerTail) / 2.0f),
					vec2(camera.WorldToScreenScale(1.0f)),
					(distance < 1) ? Angle::FromRadians(arg(-scrollSpeed.cpx)) : AngleBetween(centerTail, centerHead)), // TODO: only rotate the mid part?
				0xFFFFFFFF,
				SprStretchtParam { 1.0f, midScaleX, 1.0f }, 3);

			for (size_t i = 0; i < 3; i++)
				gfx.DrawSprite(drawList, split.Quads[i]);
		}
		else
		{
			gfx.DrawSprite(drawList, spr, SprTransform::FromCenter(camera.WorldToScreenSpace(centerHead), vec2(camera.WorldToScreenScale(1.0f))));
		}
	}

	static void DrawGamePreviewNumericText(ChartGraphicsResources& gfx, const GameCamera& camera, ImDrawList* drawList, SprTransform baseTransform, std::string_view text, u32 color = 0xFFFFFFFF)
	{
		// TODO: Make more generic by taking in an array of glyph rects as lookup table (?)
		static constexpr std::string_view sprFontNumericalCharSet = "0123456789+-./%";
		static constexpr f32 advanceX = 11.0f;
		const SprInfo sprInfo = gfx.GetInfo(SprID::Game_Font_Numerical);
		const vec2 perCharSprSize = vec2(sprInfo.SourceSize.x, sprInfo.SourceSize.y / static_cast<f32>(sprFontNumericalCharSet.size()));

		vec2 pivotOffset = vec2(0.0f);
		if (baseTransform.Pivot.x != 0.0f || baseTransform.Pivot.y != 0.0f)
		{
			vec2 totalSize = vec2(0.0f, sprInfo.SourceSize.y);
			for (const char c : text)
			{
				i32 charIndex = -1;
				for (i32 i = 0; i < static_cast<i32>(sprFontNumericalCharSet.size()); i++)
					if (c == sprFontNumericalCharSet[i])
						charIndex = i;

				if (charIndex < 0)
					continue;

				totalSize.x += advanceX;
			}

			pivotOffset = (totalSize * baseTransform.Pivot);
		}

		vec2 writeHead = vec2(0.0f);
		for (const char c : text)
		{
			i32 charIndex = -1;
			for (i32 i = 0; i < static_cast<i32>(sprFontNumericalCharSet.size()); i++)
				if (c == sprFontNumericalCharSet[i])
					charIndex = i;

			if (charIndex < 0)
				continue;

			SprTransform charTransform = SprTransform::FromTL(camera.WorldToScreenSpace(baseTransform.Position), vec2(camera.WorldToScreenScale(1.0f)), baseTransform.Rotation);
			SprUV charUV = SprUV::FromRect(
				vec2(0.0f, static_cast<f32>(charIndex + 0) * perCharSprSize.y / sprInfo.SourceSize.y),
				vec2(1.0f, static_cast<f32>(charIndex + 1) * perCharSprSize.y / sprInfo.SourceSize.y));

			charTransform.Pivot.x = (-writeHead.x) / sprInfo.SourceSize.x;
			charTransform.Pivot += (pivotOffset / sprInfo.SourceSize);

			charTransform.Scale *= baseTransform.Scale;
			charTransform.Scale.y /= static_cast<f32>(sprFontNumericalCharSet.size());

			gfx.DrawSprite(drawList, SprID::Game_Font_Numerical, charTransform, color, &charUV);

			writeHead.x += advanceX;
		}
	}

	struct ForEachBarLaneData
	{
		Beat Beat;
		Time Time;
		Tempo Tempo;
		Complex ScrollSpeed;
		ScrollMethod ScrollType;
		i32 BarIndex;
	};

	template <typename Func>
	static void ForEachBarOnNoteLane(ChartCourse& course, BranchType branch, Beat maxBeatDuration, Func perBarFunc)
	{
		BeatSortedForwardIterator<TempoChange> tempoChangeIt {};
		BeatSortedForwardIterator<ScrollChange> scrollChangeIt {};
		BeatSortedForwardIterator<BarLineChange> barLineChangeIt {};
		BeatSortedForwardIterator<ScrollType> scrollTypeIt {};
		BeatSortedForwardIterator<JPOSScrollChange> JPOSscrollChangeIt {};

		course.TempoMap.ForEachBeatBar([&](const SortedTempoMap::ForEachBeatBarData& it)
		{
			if (it.Beat > maxBeatDuration)
				return ControlFlow::Break;

			if (!it.IsBar)
				return ControlFlow::Continue;

			if (!VisibleOrDefault(barLineChangeIt.Next(course.BarLineChanges.Sorted, it.Beat)))
				return ControlFlow::Continue;

			const Time time = course.TempoMap.BeatToTime(it.Beat);
			perBarFunc(ForEachBarLaneData { it.Beat, time,
				TempoOrDefault(tempoChangeIt.Next(course.TempoMap.Tempo.Sorted, it.Beat)),
				ScrollOrDefault(scrollChangeIt.Next(course.ScrollChanges.Sorted, it.Beat)), 
				ScrollTypeOrDefault(scrollTypeIt.Next(course.ScrollTypes.Sorted, it.Beat)),
				it.BarIndex });

			return ControlFlow::Continue;
		});
	}

	struct ForEachNoteLaneData
	{
		const Note* OriginalNote;
		Beat Beat;
		Time Time;
		Tempo Tempo;
		Complex ScrollSpeed;
		ScrollMethod ScrollType;
		struct {
			struct Beat Beat;
			struct Time Time;
			struct Tempo Tempo;
			Complex ScrollSpeed;
			ScrollMethod ScrollType;
		} Tail;
	};

	template <typename Func>
	static void ForEachNoteOnNoteLane(ChartCourse& course, BranchType branch, Func perNoteFunc)
	{
		BeatSortedForwardIterator<TempoChange> tempoChangeIt {};
		BeatSortedForwardIterator<ScrollChange> scrollChangeIt {};
		BeatSortedForwardIterator<ScrollType> scrollTypeIt {};
		BeatSortedForwardIterator<JPOSScrollChange> JPOSscrollChangeIt {};

		for (const Note& note : course.GetNotes(branch))
		{
			const Beat beat = note.BeatTime;
			const Time head = (course.TempoMap.BeatToTime(beat) + note.TimeOffset);
			const Beat beatTail = (note.BeatDuration > Beat::Zero()) ? (beat + note.BeatDuration) : beat;
			const Time tail = (note.BeatDuration > Beat::Zero()) ? (course.TempoMap.BeatToTime(beatTail) + note.TimeOffset) : head;
			perNoteFunc(ForEachNoteLaneData { &note, beat, head,
				TempoOrDefault(tempoChangeIt.Next(course.TempoMap.Tempo.Sorted, beat)),
				ScrollOrDefault(scrollChangeIt.Next(course.ScrollChanges.Sorted, beat)),
				ScrollTypeOrDefault(scrollTypeIt.Next(course.ScrollTypes.Sorted, beat)),
				{
					beatTail, tail,
					TempoOrDefault(tempoChangeIt.Next(course.TempoMap.Tempo.Sorted, beatTail)),
					ScrollOrDefault(scrollChangeIt.Next(course.ScrollChanges.Sorted, beatTail)),
					ScrollTypeOrDefault(scrollTypeIt.Next(course.ScrollTypes.Sorted, beatTail)),
				},
			});
		}
	}

	void ChartGamePreview::DrawGui(ChartContext& context, Time animatedCursorTime)
	{
#if 1 // HACK: Note text detection
		{
			// TODO: Implement properly
			static constexpr auto isLastNoteInGroup = [](const SortedNotesList& notes, i32 index) -> b8
			{
				const Note& thisNote = notes[index];
				const Note* nextNote = IndexOrNull(index + 1, notes);
				if (nextNote == nullptr)
					return true;

				const Beat beatDistanceToNext = (nextNote->BeatTime - thisNote.BeatTime);
				if (beatDistanceToNext >= (Beat::FromBars(1) / 4))
					return true;
				return false;
			};

			SortedNotesList& notes = context.ChartSelectedCourse->GetNotes(context.ChartSelectedBranch);
			for (i32 i = 0; i < static_cast<i32>(notes.size()); i++)
			{
				switch (Note& it = notes[i]; it.Type)
				{
				case NoteType::Don: { it.TempSEType = isLastNoteInGroup(notes, i) ? NoteSEType::Don : NoteSEType::Do; } break;
				case NoteType::DonBig: { it.TempSEType = NoteSEType::DonBig; } break;
				case NoteType::Ka: { it.TempSEType = isLastNoteInGroup(notes, i) ? NoteSEType::Katsu : NoteSEType::Ka; } break;
				case NoteType::KaBig: { it.TempSEType = NoteSEType::KatsuBig; } break;
				case NoteType::Drumroll: { it.TempSEType = NoteSEType::Drumroll; } break;
				case NoteType::DrumrollBig: { it.TempSEType = NoteSEType::DrumrollBig; } break;
				case NoteType::Balloon: { it.TempSEType = NoteSEType::Balloon; } break;
				case NoteType::BalloonSpecial: { it.TempSEType = NoteSEType::BalloonSpecial; } break;
				default: { it.TempSEType = NoteSEType::Count; } break;
				}
			}
		}
#endif

		static constexpr vec2 buttonMargin = vec2(8.0f);
		static constexpr vec2 minContentRectSize = vec2(128.0f, 72.0f);
		Gui::BeginDisabled();
		Gui::PushStyleColor(ImGuiCol_Button, Gui::GetColorU32(ImGuiCol_Button, 0.25f));
		Gui::Button("##GamePreview", ClampBot(vec2(Gui::GetContentRegionAvail()), minContentRectSize));
		Gui::PopStyleColor();
		Gui::EndDisabled();
		Camera.ScreenSpaceViewportRect = Gui::GetItemRect();
		Camera.ScreenSpaceViewportRect.TL = Camera.ScreenSpaceViewportRect.TL + buttonMargin;
		Camera.ScreenSpaceViewportRect.BR = ClampBot(Camera.ScreenSpaceViewportRect.BR - buttonMargin, Camera.ScreenSpaceViewportRect.TL + (minContentRectSize - vec2(buttonMargin.x, 0.0f)));
		{
			f32 newAspectRatio = GetAspectRatio(Camera.ScreenSpaceViewportRect);
			if (const f32 min = GetAspectRatio(*Settings.General.GameViewportAspectRatioMin); min != 0.0f) newAspectRatio = ClampBot(newAspectRatio, min);
			if (const f32 max = GetAspectRatio(*Settings.General.GameViewportAspectRatioMax); max != 0.0f) newAspectRatio = ClampTop(newAspectRatio, max);

			if (newAspectRatio != 0.0f && newAspectRatio != GetAspectRatio(Camera.ScreenSpaceViewportRect))
				Camera.ScreenSpaceViewportRect = FitInsideFixedAspectRatio(Camera.ScreenSpaceViewportRect, newAspectRatio);
		}

		const vec2 standardSize = vec2(GameLaneStandardWidth + GameLanePaddingL + GameLanePaddingR, GameLaneSlice.TotalHeight() + GameLanePaddingTop + GameLanePaddingBot);
		const f32 standardAspectRatio = GetAspectRatio(standardSize);
		const f32 viewportAspectRatio = GetAspectRatio(Camera.ScreenSpaceViewportRect);
		if (viewportAspectRatio <= standardAspectRatio) // NOTE: Standard case of (<= 16:9) with a fixed sized lane being centered
		{
			Camera.WorldSpaceSize = vec2(standardSize.x, standardSize.x / viewportAspectRatio);
			Camera.WorldToScreenScaleFactor = Camera.ScreenSpaceViewportRect.GetWidth() / standardSize.x;
			Camera.LaneRect = Rect::FromTLSize(vec2(GameLanePaddingL, GameLanePaddingTop), vec2(GameLaneStandardWidth, GameLaneSlice.TotalHeight()));
			Camera.LaneRect += vec2(0.0f, (Camera.WorldSpaceSize.y * 0.5f) - (standardSize.y * 0.5f));
		}
		else // NOTE: Ultra wide case of (> 16:9) with the lane being extended further to the right
		{
			const f32 extendedLaneWidth = (standardSize.x * (viewportAspectRatio / standardAspectRatio)) - GameLanePaddingL - GameLanePaddingR;
			Camera.WorldSpaceSize = vec2(extendedLaneWidth, standardSize.y);
			Camera.WorldToScreenScaleFactor = Camera.ScreenSpaceViewportRect.GetHeight() / Camera.WorldSpaceSize.y;
			Camera.LaneRect = Rect::FromTLSize(vec2(GameLanePaddingL, GameLanePaddingTop), vec2(extendedLaneWidth, GameLaneSlice.TotalHeight()));
		}

#if 0 // DEBUG: ...
		{
			Gui::GetForegroundDrawList()->AddRect(Camera.ScreenSpaceViewportRect.TL, Camera.ScreenSpaceViewportRect.BR, 0xFFFF00FF);
			Gui::GetForegroundDrawList()->AddRect(Camera.WorldToScreenSpace(Camera.LaneRect.TL), Camera.WorldToScreenSpace(Camera.LaneRect.BR), 0xFF00FFFF);
			char b[64];
			Gui::GetForegroundDrawList()->AddText(Camera.WorldToScreenSpace(vec2(0.0f, 0.0f)), 0xFFFF00FF, b, b + sprintf_s(b, "(%.2f, %.2f)", 0.0f, 0.0f));
			Gui::GetForegroundDrawList()->AddText(Camera.WorldToScreenSpace(Camera.WorldSpaceSize), 0xFFFF00FF, b, b + sprintf_s(b, "(%.2f, %.2f)", Camera.WorldSpaceSize.x, Camera.WorldSpaceSize.y));
		}
#endif

		if (!context.Gfx.IsAsyncLoading())
			context.Gfx.Rasterize(SprGroup::Game, Camera.WorldToScreenScaleFactor);

		ImDrawList* drawList = Gui::GetWindowDrawList();
		drawList->PushClipRect(Camera.ScreenSpaceViewportRect.TL, Camera.ScreenSpaceViewportRect.BR, true);
		{
			TempoMapAccelerationStructure& tempoChanges = context.ChartSelectedCourse->TempoMap.AccelerationStructure;
			SortedJPOSScrollChangesList& jposScrollChanges = context.ChartSelectedCourse->JPOSScrollChanges;
			const std::vector<TempoChange>& tempos = context.ChartSelectedCourse->TempoMap.Tempo.Sorted;

			const b8 isPlayback = context.GetIsPlayback();
			const BeatAndTime exactCursorBeatAndTime = context.GetCursorBeatAndTime(true);
			const Time cursorTimeOrAnimated = isPlayback ? exactCursorBeatAndTime.Time : animatedCursorTime;
			const Beat cursorBeatOrAnimatedTrunc = isPlayback ? exactCursorBeatAndTime.Beat : context.TimeToBeat(animatedCursorTime, true);
			const f64 cursorHBScrollBeatOrAnimated = context.BeatAndTimeToHBScrollBeatTick(cursorBeatOrAnimatedTrunc, cursorTimeOrAnimated);
			const Beat chartBeatDuration = context.TimeToBeat(context.Chart.GetDurationOrDefault());


			// NOTE: Lane background and borders
			{
				drawList->AddRectFilled( // NOTE: Top, middle and bottom border
					Camera.WorldToScreenSpace(Camera.LaneRect.TL),
					Camera.WorldToScreenSpace(Camera.LaneRect.BR),
					GameLaneBorderColor);
				drawList->AddRectFilled( // NOTE: Content
					Camera.WorldToScreenSpace(Camera.LaneRect.TL + vec2(0.0f, GameLaneSlice.TopBorder)),
					Camera.WorldToScreenSpace(Camera.LaneRect.TL + vec2(Camera.LaneWidth(), GameLaneSlice.TopBorder + GameLaneSlice.Content)),
					GameLaneContentBackgroundColor);
				drawList->AddRectFilled( // NOTE: Footer
					Camera.WorldToScreenSpace(Camera.LaneRect.TL + vec2(0.0f, GameLaneSlice.TopBorder + GameLaneSlice.Content + GameLaneSlice.MidBorder)),
					Camera.WorldToScreenSpace(Camera.LaneRect.TL + vec2(Camera.LaneWidth(), GameLaneSlice.TopBorder + GameLaneSlice.Content + GameLaneSlice.MidBorder + GameLaneSlice.Footer)),
					GameLaneFooterBackgroundColor);
			}

			// NOTE: Lane left / right foreground borders
			defer
			{
				drawList->AddRectFilled(Camera.WorldToScreenSpace(Camera.LaneRect.GetTL()), Camera.WorldToScreenSpace(Camera.LaneRect.GetBL() - vec2(GameLanePaddingL, 0.0f)), GameLaneBorderColor);
				drawList->AddRectFilled(Camera.WorldToScreenSpace(Camera.LaneRect.GetTR()), Camera.WorldToScreenSpace(Camera.LaneRect.GetBR() + vec2(GameLanePaddingR, 0.0f)), GameLaneBorderColor);
			};


			// NOTE: Hit indicator circle
			const vec2 hitCirclePosJPos = Camera.GetHitCircleCoordinatesJPOSScroll(jposScrollChanges, cursorTimeOrAnimated, tempoChanges);
			const vec2 hitCirclePosLane = Camera.JPOSScrollToLaneSpace(hitCirclePosJPos);
			const vec2 hitCirclePos = Camera.LaneToScreenSpace(hitCirclePosLane);
			drawList->AddCircleFilled(
				hitCirclePos,
				Camera.WorldToScreenScale(GameHitCircle.InnerFillRadius), GameLaneHitCircleInnerFillColor);
			drawList->AddCircle(
				hitCirclePos,
				Camera.WorldToScreenScale(GameHitCircle.InnerOutlineRadius), GameLaneHitCircleInnerOutlineColor, 0, Camera.WorldToScreenScale(GameHitCircle.InnerOutlineThickness));
			drawList->AddCircle(
				hitCirclePos,
				Camera.WorldToScreenScale(GameHitCircle.OuterOutlineRadius), GameLaneHitCircleOuterOutlineColor, 0, Camera.WorldToScreenScale(GameHitCircle.OuterOutlineThickness));

			ForEachBarOnNoteLane(*context.ChartSelectedCourse, context.ChartSelectedBranch, chartBeatDuration, [&](const ForEachBarLaneData& it)
			{
				const vec2 lane = Camera.GetNoteCoordinatesLane(hitCirclePosLane, cursorTimeOrAnimated, cursorHBScrollBeatOrAnimated, it.Time, it.Beat, it.Tempo, it.ScrollSpeed, it.ScrollType, tempoChanges, jposScrollChanges);
				const f32 laneX = lane.x, laneY = lane.y;

				if (Camera.IsPointVisibleOnLane(laneX))
				{
					const vec2 tl = Camera.LaneToWorldSpace(laneX, laneY) - Rotate(vec2(0.0f, GameLaneSlice.Content * 0.5f), GetNoteFaceRotationMirror(it.ScrollSpeed, NoteType::Count).first);
					const vec2 br = Camera.LaneToWorldSpace(laneX, laneY) + Rotate(vec2(0.0f, GameLaneSlice.Content * 0.5f), GetNoteFaceRotationMirror(it.ScrollSpeed, NoteType::Count).first);
					drawList->AddLine(Camera.WorldToScreenSpace(tl), Camera.WorldToScreenSpace(br), GameLaneBarLineColor, Camera.WorldToScreenScale(GameLaneBarLineThickness));

					char barLineStr[32];
					DrawGamePreviewNumericText(context.Gfx, Camera, drawList, SprTransform::FromTL(tl + vec2(5.0f, 1.0f), vec2(1.0f)),
						std::string_view(barLineStr, sprintf_s(barLineStr, "%d", it.BarIndex)));
				}
			});

#if 0 // DEBUG: ...
			if (Gui::Begin("Game Preview - Debug", nullptr, ImGuiWindowFlags_NoDocking))
			{
				static char text[64] = "0123456789+-./%";
				static u32 color = 0xFFFFFFFF;
				static SprTransform transform = SprTransform::FromTL(vec2(0.0f));
				Gui::InputText("Text", text, sizeof(text));
				Gui::ColorEdit4_U32("Color", &color);

				Gui::DragFloat2("Position", &transform.Position[0], 1.0f);
				Gui::SliderFloat2("Pivot", &transform.Pivot[0], 0.0f, 1.0f);
				Gui::DragFloat2("Scale", &transform.Scale[0], 0.1f);
				if (Gui::DragFloat("Scale (Uniform)", &transform.Scale[0], 0.1f)) { transform.Scale = vec2(transform.Scale[0]); }
				Gui::SliderFloat("Rotation", &transform.Rotation.Radians, 0.0f, PI * 2.0f);

				DrawGamePreviewNumericText(context.Gfx, Camera, drawList, transform, text, color);
			}
			Gui::End();
#endif

			ForEachNoteOnNoteLane(*context.ChartSelectedCourse, context.ChartSelectedBranch, [&](const ForEachNoteLaneData& it)
			{
				vec2 laneHead = Camera.GetNoteCoordinatesLane(hitCirclePosLane, cursorTimeOrAnimated, cursorHBScrollBeatOrAnimated, it.Time, it.Beat, it.Tempo, it.ScrollSpeed, it.ScrollType, tempoChanges, jposScrollChanges);
				vec2 laneTail = Camera.GetNoteCoordinatesLane(hitCirclePosLane, cursorTimeOrAnimated, cursorHBScrollBeatOrAnimated, it.Tail.Time, it.Tail.Beat, it.Tail.Tempo, it.Tail.ScrollSpeed, it.Tail.ScrollType, tempoChanges, jposScrollChanges);

				b8 isVisible = true;

				const Time timeSinceHeadHit = TimeSinceNoteHit(it.Time, cursorTimeOrAnimated);
				const Time timeSinceTailHit = TimeSinceNoteHit(it.Tail.Time, cursorTimeOrAnimated);
				if (IsRegularNote(it.OriginalNote->Type)) {
					if (timeSinceHeadHit >= Time::Zero())
						laneHead = laneTail = hitCirclePosLane;
					if (timeSinceHeadHit > GameNoteHitAnimationDuration)
						isVisible = false;
				}
				else if (IsBalloonNote(it.OriginalNote->Type)) {
					if (timeSinceTailHit >= Time::Zero()) {
						laneHead = laneTail;
						isVisible = false;
					}
					else if (timeSinceHeadHit >= Time::Zero())
						laneHead = hitCirclePosLane;
				}
				else { // is bar roll note
					// flying notes in screen?
					isVisible = ((timeSinceHeadHit >= Time::Zero() && timeSinceTailHit <= GameNoteHitAnimationDuration)
						// roll body in screen?
						|| Camera.IsRangeVisibleOnLane(Min(laneHead.x, laneTail.x), Max(laneHead.x, laneTail.x)));
				}
				if (isVisible)
					ReverseNoteDrawBuffer.push_back(DeferredNoteDrawData{ laneHead.x, laneTail.x, laneHead.y, laneTail.y, it.ScrollSpeed, it.OriginalNote, it.Time, it.Tail.Time });
			});

			const Beat drummrollHitInterval = GetGridBeatSnap(*Settings.General.DrumrollAutoHitBarDivision);
			for (auto it = ReverseNoteDrawBuffer.rbegin(); it != ReverseNoteDrawBuffer.rend(); it++)
			{
				const Time timeSinceHit = TimeSinceNoteHit(it->NoteStartTime, cursorTimeOrAnimated);

				if (IsLongNote(it->OriginalNote->Type))
				{
					if (IsBalloonNote(it->OriginalNote->Type))
					{
						if (IsFuseRoll(it->OriginalNote->Type))
							DrawGamePreviewNoteDuration(context.Gfx, Camera, drawList, Camera.LaneToWorldSpace(it->LaneHeadX, it->LaneHeadY), Camera.LaneToWorldSpace(it->LaneTailX, it->LaneTailY), it->OriginalNote->Type, 0xFFFFFFFF);
						DrawGamePreviewNote(context.Gfx, Camera, drawList, Camera.LaneToWorldSpace(it->LaneHeadX, it->LaneHeadY), it->ScrollSpeed, it->OriginalNote->Type);
						DrawGamePreviewNoteSEText(context.Gfx, Camera, drawList, Camera.LaneToWorldSpace(it->LaneHeadX, it->LaneHeadY), {}, it->ScrollSpeed, it->OriginalNote->TempSEType);
					}
					else
					{
						const i32 maxHitCount = (it->OriginalNote->BeatDuration.Ticks / drummrollHitInterval.Ticks);
						const Beat hitIntervalRoundedDuration = (drummrollHitInterval * maxHitCount);

						i32 drumrollHitsSoFar = 0;
						if (timeSinceHit >= Time::Zero())
						{
							for (Beat subBeat = hitIntervalRoundedDuration; subBeat >= Beat::Zero(); subBeat -= drummrollHitInterval)
							{
								const Time subHitTime = context.BeatToTime(it->OriginalNote->BeatTime + subBeat) + it->OriginalNote->TimeOffset;
								if (subHitTime <= cursorTimeOrAnimated)
									drumrollHitsSoFar++;
							}
						}

						const f32 hitPercentage = ConvertRangeClampOutput(0.0f, static_cast<f32>(ClampBot(maxHitCount, 4)), 0.0f, 1.0f, static_cast<f32>(drumrollHitsSoFar));
						const u32 hitNoteColor = InterpolateDrumrollHitColor(it->OriginalNote->Type, hitPercentage);
						DrawGamePreviewNoteDuration(context.Gfx, Camera, drawList, Camera.LaneToWorldSpace(it->LaneHeadX, it->LaneHeadY), Camera.LaneToWorldSpace(it->LaneTailX, it->LaneTailY), it->OriginalNote->Type, hitNoteColor);
						DrawGamePreviewNote(context.Gfx, Camera, drawList, Camera.LaneToWorldSpace(it->LaneHeadX, it->LaneHeadY), it->ScrollSpeed, it->OriginalNote->Type);
						DrawGamePreviewNoteSEText(context.Gfx, Camera, drawList, Camera.LaneToWorldSpace(it->LaneHeadX, it->LaneHeadY), Camera.LaneToWorldSpace(it->LaneTailX, it->LaneTailY), it->ScrollSpeed, it->OriginalNote->TempSEType);

						if (timeSinceHit >= Time::Zero())
						{
							for (Beat subBeat = hitIntervalRoundedDuration; subBeat >= Beat::Zero(); subBeat -= drummrollHitInterval)
							{
								const Time subHitTime = context.BeatToTime(it->OriginalNote->BeatTime + subBeat) + it->OriginalNote->TimeOffset;
								const Time timeSinceSubHit = TimeSinceNoteHit(subHitTime, cursorTimeOrAnimated);
								// `>` to avoid displaying extra notes when editing (still fails sometimes)
								if (timeSinceSubHit > Time::Zero() && timeSinceSubHit <= GameNoteHitAnimationDuration)
								{
									// TODO: Scale duration, animation speed and path by extended lane width
									const auto hitAnimation = GetNoteHitPathAnimation(timeSinceSubHit, Camera.ExtendedLaneWidthFactor());
									const vec2 laneOrigin = Camera.GetHitCircleCoordinatesLane(jposScrollChanges, subHitTime, tempoChanges);
									const vec2 noteCenter = Camera.LaneToWorldSpace(laneOrigin.x, laneOrigin.y) + hitAnimation.PositionOffset;

									if (hitAnimation.AlphaFadeOut >= 1.0f)
										DrawGamePreviewNote(context.Gfx, Camera, drawList, noteCenter, it->ScrollSpeed, ToBigNoteIf(NoteType::Don, IsBigNote(it->OriginalNote->Type)));
								}
							}
						}
					}
				}
				else
				{
					// TODO: Instead of offseting the lane x position just draw as HitCenter + PositionOffset directly (?)
					const auto hitAnimation = GetNoteHitPathAnimation(timeSinceHit, Camera.ExtendedLaneWidthFactor());
					const vec2 noteCenter = Camera.LaneToWorldSpace(it->LaneHeadX, it->LaneHeadY) + hitAnimation.PositionOffset;

					if (hitAnimation.AlphaFadeOut >= 1.0f)
						DrawGamePreviewNote(context.Gfx, Camera, drawList, noteCenter, it->ScrollSpeed, it->OriginalNote->Type);

					if (timeSinceHit <= Time::Zero())
						DrawGamePreviewNoteSEText(context.Gfx, Camera, drawList, noteCenter, {}, it->ScrollSpeed, it->OriginalNote->TempSEType);

					if (const f32 whiteAlpha = (hitAnimation.WhiteFadeIn * hitAnimation.AlphaFadeOut); whiteAlpha > 0.0f)
					{
						// TODO: ...
						// const auto radii = IsBigNote(it->OriginalNote->Type) ? GameRefNoteRadiiBig : GameRefNoteRadiiSmall;
						// drawList->AddCircleFilled(Camera.RefToScreenSpace(refSpaceCenter), Camera.RefToScreenScale(radii.BlackOuter), ImColor(1.0f, 1.0f, 1.0f, whiteAlpha));
					}
				}
			}
			ReverseNoteDrawBuffer.clear();
		}
		drawList->PopClipRect();
	}
}
