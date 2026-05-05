#pragma once
#include "core_types.h"
#include "imgui/imgui_include.h"

namespace PeepoDrumKit
{
	enum class SprGroup : u8
	{
		Timeline,
		Game,
		Count
	};

	enum class SprID : u8
	{
		Timeline_Note_Don,
		Timeline_Note_DonBig,
		Timeline_Note_DonHand,
		Timeline_Note_Ka,
		Timeline_Note_KaBig,
		Timeline_Note_KaHand,
		Timeline_Note_KaDon,
		Timeline_Note_Drumroll,
		Timeline_Note_DrumrollBig,
		Timeline_Note_DrumrollLong,
		Timeline_Note_DrumrollLongBig,
		Timeline_Note_Balloon,
		Timeline_Note_BalloonSpecial,
		Timeline_Note_BalloonLong,
		Timeline_Note_BalloonLongSpecial,
		Timeline_Note_Bomb,
		Timeline_Note_Adlib,
		Timeline_Note_Fuse,
		Timeline_Note_FuseLong,
		Timeline_Note_Arms, // contains all arms

		Timeline_Icon_InsertAtSelectedItems,
		Timeline_Icon_SetFromRangeSelection,

		Game_Note_Don,
		Game_Note_DonBig,
		Game_Note_DonHand,
		Game_Note_Ka,
		Game_Note_KaBig,
		Game_Note_KaHand,
		Game_Note_KaDon,
		Game_Note_Drumroll,
		Game_Note_DrumrollBig,
		Game_Note_DrumrollLong,
		Game_Note_DrumrollLongBig,
		Game_Note_Balloon,
		Game_Note_BalloonSpecial,
		Game_Note_Bomb,
		Game_Note_Adlib,
		Game_Note_Fuse,
		Game_Note_FuseLong,
		Game_Note_ArmDown, // one arm, arms are drawn separately
		Game_NoteTxt_Do,
		Game_NoteTxt_Ko,
		Game_NoteTxt_Don,
		Game_NoteTxt_DonBig,
		Game_NoteTxt_DonHand,
		Game_NoteTxt_Ka,
		Game_NoteTxt_Katsu,
		Game_NoteTxt_KatsuBig,
		Game_NoteTxt_KatsuHand,
		Game_NoteTxt_KaDon,
		Game_NoteTxt_Drumroll,
		Game_NoteTxt_DrumrollBig,
		Game_NoteTxt_Balloon,
		Game_NoteTxt_BalloonSpecial,
		Game_NoteTxt_Bomb,
		Game_NoteTxt_Adlib,
		Game_NoteTxt_Fuse,
		Game_Lane_GogoFire,
		
		// TODO: Split into individual sprites to correctly handle padding (?)
		Game_Font_Numerical,
		Game_Font_Combo,

		Count
	};

	constexpr cstr SupportedSpriteFileFormatExtensions[] = { ".svg", ".png", ".jpg", ".jpeg" };
	struct SprTypeDesc { SprID Spr; SprGroup Group; cstr FilePath; f32 BaseScale; };
	constexpr SprTypeDesc SprDescTable[] =
	{
		{ SprID::Timeline_Note_Don,					SprGroup::Timeline, u8"assets/graphics/timeline_note_don" },
		{ SprID::Timeline_Note_DonBig,				SprGroup::Timeline, u8"assets/graphics/timeline_note_don_big" },
		{ SprID::Timeline_Note_DonHand,				SprGroup::Timeline, u8"assets/graphics/timeline_note_don_hand" },
		{ SprID::Timeline_Note_Ka,					SprGroup::Timeline, u8"assets/graphics/timeline_note_ka" },
		{ SprID::Timeline_Note_KaBig,				SprGroup::Timeline, u8"assets/graphics/timeline_note_ka_big" },
		{ SprID::Timeline_Note_KaHand,				SprGroup::Timeline, u8"assets/graphics/timeline_note_ka_hand" },
		{ SprID::Timeline_Note_KaDon,				SprGroup::Timeline, u8"assets/graphics/timeline_note_kadon" },
		{ SprID::Timeline_Note_Drumroll,			SprGroup::Timeline, u8"assets/graphics/timeline_note_renda" },
		{ SprID::Timeline_Note_DrumrollBig,			SprGroup::Timeline, u8"assets/graphics/timeline_note_renda_big" },
		{ SprID::Timeline_Note_DrumrollLong,		SprGroup::Timeline, u8"assets/graphics/timeline_note_renda_long" },
		{ SprID::Timeline_Note_DrumrollLongBig,		SprGroup::Timeline, u8"assets/graphics/timeline_note_renda_long_big" },
		{ SprID::Timeline_Note_Balloon,				SprGroup::Timeline, u8"assets/graphics/timeline_note_fuusen" },
		{ SprID::Timeline_Note_BalloonSpecial,		SprGroup::Timeline, u8"assets/graphics/timeline_note_fuusen_big" },
		{ SprID::Timeline_Note_BalloonLong,			SprGroup::Timeline, u8"assets/graphics/timeline_note_fuusen_long" },
		{ SprID::Timeline_Note_BalloonLongSpecial,	SprGroup::Timeline, u8"assets/graphics/timeline_note_fuusen_long_big" },
		{ SprID::Timeline_Note_Bomb,				SprGroup::Timeline, u8"assets/graphics/timeline_note_bomb" },
		{ SprID::Timeline_Note_Adlib,				SprGroup::Timeline, u8"assets/graphics/timeline_note_adlib" },
		{ SprID::Timeline_Note_Fuse,				SprGroup::Timeline, u8"assets/graphics/timeline_note_fuse" },
		{ SprID::Timeline_Note_FuseLong,			SprGroup::Timeline, u8"assets/graphics/timeline_note_fuse_long" },
		{ SprID::Timeline_Note_Arms,				SprGroup::Timeline, u8"assets/graphics/timeline_note_arms" },

		{ SprID::Timeline_Icon_InsertAtSelectedItems,	SprGroup::Timeline, u8"assets/graphics/timeline_icon_insert_at_selected_items" },
		{ SprID::Timeline_Icon_SetFromRangeSelection,	SprGroup::Timeline, u8"assets/graphics/timeline_icon_set_from_range_selection" },

		{ SprID::Game_Note_Don,						SprGroup::Game, u8"assets/graphics/game_note_don" },
		{ SprID::Game_Note_DonBig,					SprGroup::Game, u8"assets/graphics/game_note_don_big" },
		{ SprID::Game_Note_DonHand,					SprGroup::Game, u8"assets/graphics/game_note_don_hand" },
		{ SprID::Game_Note_Ka,						SprGroup::Game, u8"assets/graphics/game_note_ka" },
		{ SprID::Game_Note_KaBig,					SprGroup::Game, u8"assets/graphics/game_note_ka_big" },
		{ SprID::Game_Note_KaHand,					SprGroup::Game, u8"assets/graphics/game_note_ka_hand" },
		{ SprID::Game_Note_KaDon,					SprGroup::Game, u8"assets/graphics/game_note_kadon" },
		{ SprID::Game_Note_Drumroll,				SprGroup::Game, u8"assets/graphics/game_note_renda" },
		{ SprID::Game_Note_DrumrollBig,				SprGroup::Game, u8"assets/graphics/game_note_renda_big" },
		{ SprID::Game_Note_DrumrollLong,			SprGroup::Game, u8"assets/graphics/game_note_renda_long" },
		{ SprID::Game_Note_DrumrollLongBig,			SprGroup::Game, u8"assets/graphics/game_note_renda_long_big" },
		{ SprID::Game_Note_Balloon,					SprGroup::Game, u8"assets/graphics/game_note_fuusen" },
		{ SprID::Game_Note_BalloonSpecial,			SprGroup::Game, u8"assets/graphics/game_note_fuusen_big" },
		{ SprID::Game_Note_Bomb,					SprGroup::Game, u8"assets/graphics/game_note_bomb" },
		{ SprID::Game_Note_Adlib,					SprGroup::Game, u8"assets/graphics/game_note_adlib" },
		{ SprID::Game_Note_Fuse,					SprGroup::Game, u8"assets/graphics/game_note_fuse" },
		{ SprID::Game_Note_FuseLong,				SprGroup::Game, u8"assets/graphics/game_note_fuse_long" },
		{ SprID::Game_Note_ArmDown,					SprGroup::Game, u8"assets/graphics/game_note_arm_down" }, // workaround for size bug (?), need to update ThorVG
		{ SprID::Game_NoteTxt_Do,					SprGroup::Game, u8"assets/graphics/game_note_txt_do" },
		{ SprID::Game_NoteTxt_Ko,					SprGroup::Game, u8"assets/graphics/game_note_txt_ko" },
		{ SprID::Game_NoteTxt_Don,					SprGroup::Game, u8"assets/graphics/game_note_txt_don" },
		{ SprID::Game_NoteTxt_DonBig,				SprGroup::Game, u8"assets/graphics/game_note_txt_don_big" },
		{ SprID::Game_NoteTxt_DonHand,				SprGroup::Game, u8"assets/graphics/game_note_txt_don_hand" },
		{ SprID::Game_NoteTxt_Ka,					SprGroup::Game, u8"assets/graphics/game_note_txt_ka" },
		{ SprID::Game_NoteTxt_Katsu,				SprGroup::Game, u8"assets/graphics/game_note_txt_katsu" },
		{ SprID::Game_NoteTxt_KatsuBig,				SprGroup::Game, u8"assets/graphics/game_note_txt_katsu_big" },
		{ SprID::Game_NoteTxt_KatsuHand,			SprGroup::Game, u8"assets/graphics/game_note_txt_katsu_hand" },
		{ SprID::Game_NoteTxt_KaDon,				SprGroup::Game, u8"assets/graphics/game_note_txt_kadon" },
		{ SprID::Game_NoteTxt_Drumroll,				SprGroup::Game, u8"assets/graphics/game_note_txt_renda" },
		{ SprID::Game_NoteTxt_DrumrollBig,			SprGroup::Game, u8"assets/graphics/game_note_txt_renda_big" },
		{ SprID::Game_NoteTxt_Balloon,				SprGroup::Game, u8"assets/graphics/game_note_txt_fuusen" },
		{ SprID::Game_NoteTxt_BalloonSpecial,		SprGroup::Game, u8"assets/graphics/game_note_txt_fuusen_big" },
		{ SprID::Game_NoteTxt_Bomb,					SprGroup::Game, u8"assets/graphics/game_note_txt_bomb" },
		{ SprID::Game_NoteTxt_Adlib,				SprGroup::Game, u8"assets/graphics/game_note_txt_adlib" },
		{ SprID::Game_NoteTxt_Fuse,					SprGroup::Game, u8"assets/graphics/game_note_txt_fuse" },
		{ SprID::Game_Lane_GogoFire,				SprGroup::Game, u8"assets/graphics/game_lane_gogo_fire" },
		// TODO: ...
		{ SprID::Game_Font_Numerical,				SprGroup::Game, u8"assets/graphics/game_font_numerical" },
		{ SprID::Game_Font_Combo,					SprGroup::Game, u8"assets/graphics/game_combo_numerical", 1.5f }, // maximum GameComboDisplay scale
	};

	constexpr SprGroup GetSprGroup(SprID spr) { return (spr < SprID::Count) ? SprDescTable[EnumToIndex(spr)].Group : SprGroup::Count; }

	struct SprInfo { vec2 SourceSize; f32 RasterScale; };

	struct SprTransform
	{
		vec2 Position;	// NOTE: In absolute screen space
		vec2 Pivot;		// NOTE: In normalized 0 to 1 range
		vec2 Scale;		// NOTE: Around pivot
		Angle Rotation; // NOTE: Around pivot

		static constexpr SprTransform FromTL(vec2 tl, vec2 scale = vec2(1.0f), Angle rot = {}) { return { tl, vec2(0.0f, 0.0f), scale, rot }; }
		static constexpr SprTransform FromTR(vec2 tr, vec2 scale = vec2(1.0f), Angle rot = {}) { return { tr, vec2(1.0f, 0.0f), scale, rot }; }
		static constexpr SprTransform FromBL(vec2 bl, vec2 scale = vec2(1.0f), Angle rot = {}) { return { bl, vec2(0.0f, 1.0f), scale, rot }; }
		static constexpr SprTransform FromBR(vec2 br, vec2 scale = vec2(1.0f), Angle rot = {}) { return { br, vec2(1.0f, 1.0f), scale, rot }; }
		static constexpr SprTransform FromCenter(vec2 center, vec2 scale = vec2(1.0f), Angle rot = {}) { return { center, vec2(0.5f, 0.5f), scale, rot }; }
		static constexpr SprTransform FromCenterL(vec2 center, vec2 scale = vec2(1.0f), Angle rot = {}) { return { center, vec2(0.0f, 0.5f), scale, rot }; }
		static constexpr SprTransform FromCenterR(vec2 center, vec2 scale = vec2(1.0f), Angle rot = {}) { return { center, vec2(1.0f, 0.5f), scale, rot }; }
		constexpr SprTransform operator+(vec2 offset) const { SprTransform copy = *this; copy.Position += offset; return copy; }
		constexpr SprTransform operator-(vec2 offset) const { SprTransform copy = *this; copy.Position -= offset; return copy; }
	};

	// NOTE: Normalized (0.0f <-> 1.0f) per quad corner
	struct SprUV
	{
		vec2 TL, TR, BL, BR;
		static constexpr SprUV FromRect(vec2 tl, vec2 br) { return SprUV { tl, vec2(br.x, tl.y), vec2(tl.x, br.y), br }; }
	};

	// NOTE: Stored in the the Dear ImGui native clockwise triangle winding order of { TL, TR, BR, BL }
	struct ImImageQuad
	{
		ImTextureID TexID; ImVec2 Pos[4]; ImVec2 UV[4]; ImU32 Color;
		inline ImVec2 TL() const { return Pos[0]; } inline void TL(ImVec2 v) { Pos[0] = v; }
		inline ImVec2 TR() const { return Pos[1]; } inline void TR(ImVec2 v) { Pos[1] = v; }
		inline ImVec2 BR() const { return Pos[2]; } inline void BR(ImVec2 v) { Pos[2] = v; }
		inline ImVec2 BL() const { return Pos[3]; } inline void BL(ImVec2 v) { Pos[3] = v; }
		inline ImVec2 UV_TL() const { return UV[0]; } inline void UV_TL(ImVec2 v) { UV[0] = v; }
		inline ImVec2 UV_TR() const { return UV[1]; } inline void UV_TR(ImVec2 v) { UV[1] = v; }
		inline ImVec2 UV_BR() const { return UV[2]; } inline void UV_BR(ImVec2 v) { UV[2] = v; }
		inline ImVec2 UV_BL() const { return UV[3]; } inline void UV_BL(ImVec2 v) { UV[3] = v; }
	};

	struct RasterizedBitmap { std::unique_ptr<u32[]> BGRA; ivec2 Resolution; };
	struct SvgRasterizer
	{
		struct Impl;
		std::unique_ptr<Impl> pImpl = nullptr;
		vec2 PictureSize = {};
		f32 BaseScale;

		SvgRasterizer();
		~SvgRasterizer();

		bool ParseMemory(std::string_view svgFileContent, f32 baseScale = 1);
		bool ParseFromPath(std::string imagePath, f32 baseScale = 1);
		RasterizedBitmap Rasterize(f32 scale = 1);
	};

	static inline void Rasterize(SvgRasterizer& rasterizer, CustomDraw::GPUTexture& tecture, f32 scale = 1)
	{
		auto bitmap = rasterizer.Rasterize(scale);
		tecture.Unload();
		if (bitmap.Resolution.x > 0 && bitmap.Resolution.y > 0)
			tecture.Load(CustomDraw::GPUTextureDesc{ CustomDraw::GPUPixelFormat::BGRA, CustomDraw::GPUAccessType::Static, bitmap.Resolution, bitmap.BGRA.get() });
	}

	struct ChartGraphicsResources : NonCopyable
	{
		ChartGraphicsResources();
		~ChartGraphicsResources();

		void StartAsyncLoading();
		void UpdateAsyncLoading();
		b8 IsAsyncLoading() const;

		// TODO: Only actually rebulid textures after *completeing* a resize (?)
		void Rasterize(SprGroup group, f32 scale = 1);

		SprInfo GetInfo(SprID spr) const;
		b8 GetImageQuad(ImImageQuad& out, SprID spr, const vec2& p_min, const vec2& p_max, const vec2& uv_min, const vec2& uv_max, u32 colorTint = 0xFFFFFFFF) const;
		b8 GetImageQuad(ImImageQuad& out, SprID spr, SprTransform transform, u32 colorTint, const SprUV* uv) const;

		inline void DrawSprite(ImDrawList* drawList, const ImImageQuad& quad) { drawList->AddImageQuad(quad.TexID, quad.Pos[0], quad.Pos[1], quad.Pos[2], quad.Pos[3], quad.UV[0], quad.UV[1], quad.UV[2], quad.UV[3], quad.Color); }
		inline void DrawSprite(ImDrawList* drawList, SprID spr, const vec2& p_min, const vec2& p_max, const vec2& uv_min, const vec2& uv_max, u32 colorTint = 0xFFFFFFFF)
		{
			if (ImImageQuad quad; GetImageQuad(quad, spr, p_min, p_max, uv_min, uv_max, colorTint))
				DrawSprite(drawList, quad); 
		}
		inline void DrawSprite(ImDrawList* drawList, SprID spr, SprTransform transform, u32 colorTint = 0xFFFFFFFF, const SprUV* uv = nullptr)
		{
			if (ImImageQuad quad; GetImageQuad(quad, spr, transform, colorTint, uv))
				DrawSprite(drawList, quad);
		}

		struct OpaqueData;
		std::unique_ptr<OpaqueData> Data;
	};

	// NOTE: Per segment scale factor and per split normalized percentage
	struct SprStretchtParam { f32 Scales[3]; /*f32 Splits[3];*/ };
	struct SprStretchtOut { SprTransform BaseTransform; ImImageQuad Quads[3]; };
	// NOTE: Take a sprite of source transform, split it into N parts and stretch those parts by varying amounts without gaps as if they were one
	SprStretchtOut StretchMultiPartSpr(ChartGraphicsResources& gfx, SprID spr, SprTransform transform, u32 color, SprStretchtParam param, size_t splitCount);
}
