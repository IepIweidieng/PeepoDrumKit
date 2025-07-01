#include "imgui_application_host.h"
#include "imgui/3rdparty/imgui.h"
#include "imgui/3rdparty/imgui_internal.h"
#include "imgui/backend/imgui_impl_win32.h"
#include "imgui/backend/imgui_impl_d3d11.h"
#include "imgui/extension/imgui_input_binding.h"

#include "core_io.h"
#include "core_string.h"
#include "../src_res/resource.h"

#include <d3d11.h>
#include <dxgi1_3.h>

#define HAS_EMBEDDED_ICONS 1
#define REGENERATE_EMBEDDED_ICONS_SOURCE_CODE 0

#if HAS_EMBEDDED_ICONS || REGENERATE_EMBEDDED_ICONS_SOURCE_CODE
#define STBI_WINDOWS_UTF8
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize.h>
#endif

#if REGENERATE_EMBEDDED_ICONS_SOURCE_CODE 
#include "../src_res_icons/x_generate_icon_src_code.cpp"
#define ON_STARTUP_CODEGEN() CodeGen::WriteSourceCodeToFile(CodeGen::GenerateIconSourceCode(u8"../../../src_res_icons"), u8"../../../src_res_icons/x_embedded_icons_include.h");
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#endif /* REGENERATE_EMBEDDED_ICONS_SOURCE_CODE */

#if HAS_EMBEDDED_ICONS
#include "../src_res_icons/x_embedded_icons_include.h"
EmbeddedIconsList GetEmbeddedIconsList()
{
	static EmbeddedIconsList::Data out[ArrayCount(EmbeddedIcons)] = {};
	if (out[0].Name == nullptr)
		for (size_t i = 0; i < ArrayCount(out); i++) { out[i] = { EmbeddedIcons[i].Name }; ImTextCharToUtf8(out[i].UTF8, EmbeddedIcons[i].Codepoint); }
	return EmbeddedIconsList { out, ArrayCount(out) };
}
#else
EmbeddedIconsList GetEmbeddedIconsList() { return {}; }
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct WindowRectWithDecoration { ivec2 Position, Size; };
struct ClientRect { ivec2 Position, Size; };

static WindowRectWithDecoration Win32ClientToWindowRect(HWND hwnd, DWORD windowStyle, ClientRect clientRect)
{
	RECT inClientOutWindow;
	inClientOutWindow.left = clientRect.Position.x;
	inClientOutWindow.top = clientRect.Position.y;
	inClientOutWindow.right = clientRect.Position.x + clientRect.Size.x;
	inClientOutWindow.bottom = clientRect.Position.y + clientRect.Size.y;
	if (!::AdjustWindowRect(&inClientOutWindow, windowStyle, false))
		return WindowRectWithDecoration { clientRect.Position, clientRect.Size };

	return WindowRectWithDecoration
	{
		ivec2(inClientOutWindow.left, inClientOutWindow.top),
		ivec2(inClientOutWindow.right - inClientOutWindow.left, inClientOutWindow.bottom - inClientOutWindow.top),
	};
}

namespace ApplicationHost
{
	// HACK: Color seen briefly during startup before the first frame has been drawn.
	//		 Should match the primary ImGui style window background color to create a seamless transition.
	//		 Hardcoded for now but should probably be passed in as a parameter / updated dynamically as the ImGui style changes (?)
	static constexpr u32 Win32WindowBackgroundColor = 0x001F1F1F;
	static constexpr f32 D3D11SwapChainClearColor[4] = { 0.12f, 0.12f, 0.12f, 1.0f };
	static constexpr cstr FontFilePath = "assets/NotoSansCJKjp-Regular.otf";
	static constexpr i32 FontBaseSizes[EnumCount<BuiltInFont>] = { 16, 18, 22 };

	static LRESULT WINAPI MainWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	static ID3D11Device*            GlobalD3D11Device = nullptr;
	static ID3D11DeviceContext*     GlobalD3D11DeviceContext = nullptr;
	static IDXGISwapChain*          GlobalSwapChain = nullptr;
	static DXGI_SWAP_CHAIN_DESC		GlobalSwapChainCreationDesc = {};
	static ID3D11RenderTargetView*  GlobalMainRenderTargetView = nullptr;
	static UpdateFunc				GlobalOnUserUpdate = nullptr;
	static WindowCloseRequestFunc	GlobalOnUserWindowCloseRequest = nullptr;
	static WINDOWPLACEMENT			GlobalPreFullscreenWindowPlacement = {};
	static b8						GlobalIsWindowMinimized = false;
	static b8						GlobalIsWindowBeingDragged = false;
	static b8						GlobalIsWindowFocused = false;
	static UINT_PTR					GlobalWindowRedrawTimerID = {};
	static HANDLE					GlobalSwapChainWaitableObject = NULL;
	static struct { const ImWchar *CJKV, *EN; } GlobalGlyphRanges = {};
	static ImGuiStyle				GlobalOriginalScaleStyle = {};

	static b8 CreateGlobalD3D11(const StartupParam& startupParam, HWND hWnd);
	static void CleanupGlobalD3D11();
	static void CreateGlobalD3D11SwapchainRenderTarget();
	static void CleanupGlobalD3D11SwapchainRenderTarget();

#if HAS_EMBEDDED_ICONS
	// Based on imgui_draw.cpp: ImGui_ImplStbTrueType_* implementation
	static constexpr i32 EmbeddedIcons_pxIconPadding = 1;

	// Metric functions
	static i32 EmbeddedIcons_FindGlyphIndex(i32 unicode_codepoint) {
		// EmbeddedIcons is already sorted in ascending codepoint order
		// NOTE: valid index starts at 0, not 1 as stbtt_FindGlyphIndex()
		if (unicode_codepoint >= EmbeddedIcons[0].Codepoint
			&& unicode_codepoint <= EmbeddedIcons[ArrayCount(EmbeddedIcons) - 1].Codepoint
			) {
			return unicode_codepoint - EmbeddedIcons[0].Codepoint;
		}
		return -1;
	}

	static void EmbeddedIcons_GetFontVMetrics(const ImFontBaked* baked, i32* ascent, i32* descent, i32* lineGap)
	{
		if (ascent) *ascent = (baked ? baked->Ascent : 128) + EmbeddedIcons_pxIconPadding;
		if (descent) *descent = (baked ? baked->Descent : 0) - EmbeddedIcons_pxIconPadding;
		if (lineGap) *lineGap = 0;
	}

	static void EmbeddedIcons_GetGlyphHMetrics(const ImFontBaked* baked, i32 glyph_index, i32* advanceWidth, i32* leftSideBearing)
	{
		if (advanceWidth) *advanceWidth = baked->Ascent - baked->Descent + 2 * EmbeddedIcons_pxIconPadding;
		if (leftSideBearing) *leftSideBearing = 0;
	}

	static void EmbeddedIcons_GetGlyphBitmapBoxSubpixel(const ImFontBaked* baked, i32 glyph, f32 scale_x, f32 scale_y, f32 shift_x, f32 shift_y, i32* ix0, i32* iy0, i32* ix1, i32* iy1)
	{
		// move to integral bboxes (treating pixels as little squares, what pixels get touched)?
		if (ix0) *ix0 = static_cast<int>(floor(0 * scale_x + shift_x));
		if (iy0) *iy0 = static_cast<int>(floor(-(baked->Ascent + EmbeddedIcons_pxIconPadding) * scale_y + shift_y));
		if (ix1) *ix1 = static_cast<int>(ceil((baked->Ascent - baked->Descent + 2 * EmbeddedIcons_pxIconPadding) * scale_x + shift_x));
		if (iy1) *iy1 = static_cast<int>(ceil(-(baked->Descent - EmbeddedIcons_pxIconPadding) * scale_y + shift_y));
	}

	static void EmbeddedIcons_GetGlyphBitmapBox(const ImFontBaked* baked, i32 glyph, f32 scale_x, f32 scale_y, i32* ix0, i32* iy0, i32* ix1, i32* iy1)
	{
		EmbeddedIcons_GetGlyphBitmapBoxSubpixel(baked, glyph, scale_x, scale_y, 0.0f, 0.0f, ix0, iy0, ix1, iy1);
	}

	static void EmbeddedIcons_MakeGlyphBitmapSubpixel(const ImFontBaked* baked, u32* output, i32 out_w, i32 out_h, i32 out_stride, f32 scale_x, f32 scale_y, f32 shift_x, f32 shift_y, i32 glyph)
	{
		const auto& icon = EmbeddedIcons[glyph];
		const i32 resizeResult = ::stbir_resize_uint8(
			reinterpret_cast<const u8*>(&EmbeddedIconsPixelData[icon.OffsetIntoPixelData]), icon.Size.x, icon.Size.y, (icon.Size.x * sizeof(u32)),
			reinterpret_cast<u8*>(output), out_w, out_h, out_stride, 4);
	}

#ifndef STBTT_MAX_OVERSAMPLE
#define STBTT_MAX_OVERSAMPLE   8
#endif

#define STBTT__OVER_MASK  (STBTT_MAX_OVERSAMPLE-1)

	static void EmbeddedIcons_h_prefilter(u32* pixels_rgba, i32 w, i32 h, i32 stride_in_pixels, u32 kernel_width)
	{
		auto pixels = reinterpret_cast<u8(*)[4]>(pixels_rgba);
		u8 buffer[STBTT_MAX_OVERSAMPLE][4];
		i32 safe_w = w - kernel_width;
		memset(buffer, 0, STBTT_MAX_OVERSAMPLE * 4); // suppress bogus warning from VS2013 -analyze
		for (i32 j = 0; j < h; ++j) {
			i32 i;
			u32 total[4] = {};
			memset(buffer, 0, kernel_width * 4);

			// make kernel_width a constant in common cases so compiler can optimize out the divide
#define X_KERNEL(_kernel_width) do { \
				for (i = 0; i <= safe_w; ++i) { \
					for (int c = 0; c < 4; ++c) { \
						total[c] += pixels[i][c] - buffer[i & STBTT__OVER_MASK][c]; \
						buffer[(i + kernel_width) & STBTT__OVER_MASK][c] = pixels[i][c]; \
						pixels[i][c] = static_cast<u8>(total[c] / (_kernel_width)); \
					} \
				} \
			} while (false)

			switch (kernel_width) {
			case 2: X_KERNEL(2); break;
			case 3: X_KERNEL(3); break;
			case 4: X_KERNEL(4); break;
			case 5: X_KERNEL(5); break;
			default: X_KERNEL(kernel_width); break;
			}
#undef X_KERNEL

			for (; i < w; ++i) {
				for (i32 c = 0; c < 4; ++c) {
					assert(pixels[i][c] == 0);
					total[c] -= buffer[i & STBTT__OVER_MASK][c];
					pixels[i][c] = static_cast<u8>(total[c] / kernel_width);
				}
			}

			pixels += stride_in_pixels;
		}
	}

	static void EmbeddedIcons_v_prefilter(u32* pixels_rgba, i32 w, i32 h, i32 stride_in_pixels, u32 kernel_width)
	{
		auto pixels = reinterpret_cast<u8(*)[4]>(pixels_rgba);
		u8 buffer[STBTT_MAX_OVERSAMPLE][4];
		i32 safe_h = h - kernel_width;
		i32 j;
		memset(buffer, 0, STBTT_MAX_OVERSAMPLE * 4); // suppress bogus warning from VS2013 -analyze
		for (i32 j = 0; j < w; ++j) {
			i32 i;
			u32 total[4] = {};
			memset(buffer, 0, kernel_width * 4);

			// make kernel_width a constant in common cases so compiler can optimize out the divide
#define X_KERNEL(_kernel_width) do { \
				for (i = 0; i <= safe_h; ++i) { \
					for (int c = 0; c < 4; ++c) { \
						total[c] += pixels[i * stride_in_pixels][c] - buffer[i & STBTT__OVER_MASK][c]; \
						buffer[(i + kernel_width) & STBTT__OVER_MASK][c] = pixels[i * stride_in_pixels][c]; \
						pixels[i * stride_in_pixels][c] = static_cast<u8>(total[c] / (_kernel_width)); \
					} \
				} \
			} while (false)

			switch (kernel_width) {
			case 2: X_KERNEL(2); break;
			case 3: X_KERNEL(3); break;
			case 4: X_KERNEL(4); break;
			case 5: X_KERNEL(5); break;
			default: X_KERNEL(kernel_width); break;
			}
#undef X_KERNEL

			for (; i < h; ++i) {
				for (int c = 0; c < 4; ++c) {
					assert(pixels[i * stride_in_pixels][c] == 0);
					total[c] -= buffer[i & STBTT__OVER_MASK][c];
					pixels[i * stride_in_pixels][c] = static_cast<u8>(total[c] / kernel_width);
				}
			}

			pixels += 1;
		}
	}

	static f32 EmbeddedIcons_oversample_shift(i32 oversample)
	{
		if (!oversample)
			return 0.0f;

		// The prefilter is a box filter of width "oversample",
		// which shifts phase by (oversample - 1)/2 pixels in
		// oversampled space. We want to shift in the opposite
		// direction to counter this.
		return static_cast<f32>(- (oversample - 1) / (2.0f * static_cast<f32>(oversample)));
	}

	// Font loader functions

	// One for each ConfigData
	struct ImplEmbeddedIcon_FontSrcData {
		f32 ScaleFactor;
	};

	static bool ImplEmbeddedIcons_FontSrcInit(ImFontAtlas*, ImFontConfig* src)
	{
		auto bd_font_data = IM_NEW(ImplEmbeddedIcon_FontSrcData);
		IM_ASSERT(src->FontLoaderData == NULL);

		// Initialize helper structure for font loading
		src->FontLoaderData = bd_font_data;

		if (src->MergeMode && src->SizePixels == 0.0f)
			src->SizePixels = src->DstFont->Sources[0]->SizePixels;

		bd_font_data->ScaleFactor = (float)1 / 128; // scale max height to 1px
		if (src->MergeMode && src->SizePixels != 0.0f)
			bd_font_data->ScaleFactor *= src->SizePixels / src->DstFont->Sources[0]->SizePixels; // FIXME-NEWATLAS: Should tidy up that a bit

		return true;
	}

	static void ImplEmbeddedIcons_FontSrcDestroy(ImFontAtlas*, ImFontConfig* src)
	{
		auto bd_font_data = reinterpret_cast<ImplEmbeddedIcon_FontSrcData*>(src->FontLoaderData);
		IM_DELETE(bd_font_data);
		src->FontLoaderData = nullptr;
	}

	static bool ImplEmbeddedIcons_FontSrcContainsGlyph(ImFontAtlas*, ImFontConfig*, ImWchar codepoint)
	{
		return EmbeddedIcons_FindGlyphIndex(static_cast<i32>(codepoint)) >= 0;
	}

	static bool ImplEmbeddedIcons_FontBakedInit(ImFontAtlas*, ImFontConfig* src, ImFontBaked* baked, void*)
	{
		auto bd_font_data = reinterpret_cast<ImplEmbeddedIcon_FontSrcData*>(src->FontLoaderData);
		if (src->MergeMode == false) {
			// FIXME-NEWFONTS: reevaluate how to use sizing metrics
			// FIXME-NEWFONTS: make use of line gap value
			f32 scale_for_layout = bd_font_data->ScaleFactor * baked->Size;
			int unscaled_ascent, unscaled_descent, unscaled_line_gap;
			EmbeddedIcons_GetFontVMetrics(nullptr, &unscaled_ascent, &unscaled_descent, &unscaled_line_gap);
			baked->Ascent = ImCeil(unscaled_ascent * scale_for_layout);
			baked->Descent = ImFloor(unscaled_descent * scale_for_layout);
		}
		return true;
	}

	static bool ImplEmbeddedIcons_FontBakedLoadGlyph(ImFontAtlas* atlas, ImFontConfig* src, ImFontBaked* baked, void*, ImWchar codepoint, ImFontGlyph* out_glyph)
	{
		// Search for first font which has the glyph
		auto bd_font_data = reinterpret_cast<ImplEmbeddedIcon_FontSrcData*>(src->FontLoaderData);
		IM_ASSERT(bd_font_data);
		i32 glyph_index = EmbeddedIcons_FindGlyphIndex(static_cast<i32>(codepoint));
		if (!(glyph_index >= 0))
			return false;

		// Fonts unit to pixels
		i32 oversample_h, oversample_v;
		ImFontAtlasBuildGetOversampleFactors(src, baked, &oversample_h, &oversample_v);
		const f32 size_for_layout = bd_font_data->ScaleFactor * baked->Size;
		const f32 rasterizer_density = src->RasterizerDensity * baked->RasterizerDensity;
		const f32 scale_for_raster_x = rasterizer_density * oversample_h;
		const f32 scale_for_raster_y = rasterizer_density * oversample_v;

		// Obtain size and advance
		i32 x0, y0, x1, y1;
		i32 advance, lsb;
		EmbeddedIcons_GetGlyphBitmapBoxSubpixel(baked, glyph_index, scale_for_raster_x, scale_for_raster_y, 0, 0, &x0, &y0, &x1, &y1);
		EmbeddedIcons_GetGlyphHMetrics(baked, glyph_index, &advance, &lsb);
		const bool is_visible = (x0 != x1 && y0 != y1);

		// Prepare glyph
		out_glyph->Codepoint = codepoint;
		out_glyph->AdvanceX = advance * size_for_layout;

		// Pack and retrieve position inside texture atlas
		// (generally based on stbtt_PackFontRangesRenderIntoRects)
		if (is_visible)
		{
			static constexpr i32 nChans = 4; // RGBA
			const i32 w = (x1 - x0 + oversample_h - 1);
			const i32 h = (y1 - y0 + oversample_v - 1);
			ImFontAtlasRectId pack_id = ImFontAtlasPackAddRect(atlas, w, h);
			if (pack_id == ImFontAtlasRectId_Invalid)
			{
				// Pathological out of memory case (TexMaxWidth/TexMaxHeight set too small?)
				IM_ASSERT(pack_id != ImFontAtlasRectId_Invalid && "Out of texture memory.");
				return false;
			}
			ImTextureRect* r = ImFontAtlasPackGetRect(atlas, pack_id);

			// Render
			EmbeddedIcons_GetGlyphBitmapBox(baked, glyph_index, scale_for_raster_x, scale_for_raster_y, &x0, &y0, &x1, &y1);
			ImFontAtlasBuilder* builder = atlas->Builder;
			builder->TempBuffer.resize(w * h * nChans);
			auto bitmap_pixels = reinterpret_cast<u32*>(builder->TempBuffer.Data);
			memset(bitmap_pixels, 0, w * h * nChans);
			EmbeddedIcons_MakeGlyphBitmapSubpixel(baked, bitmap_pixels, r->w - oversample_h + 1, r->h - oversample_v + 1, w * nChans,
				scale_for_raster_x, scale_for_raster_y, 0, 0, glyph_index);

			// Oversampling
			// (those functions conveniently assert if pixels are not cleared, which is another safety layer)
			if (oversample_h > 1)
				EmbeddedIcons_h_prefilter(bitmap_pixels, r->w, r->h, r->w, oversample_h);
			if (oversample_v > 1)
				EmbeddedIcons_v_prefilter(bitmap_pixels, r->w, r->h, r->w, oversample_v);

			const f32 ref_size = baked->ContainerFont->Sources[0]->SizePixels;
			const f32 offsets_scale = (ref_size != 0.0f) ? (baked->Size / ref_size) : 1.0f;
			f32 font_off_x = (src->GlyphOffset.x * offsets_scale);
			f32 font_off_y = (src->GlyphOffset.y * offsets_scale);
			if (src->PixelSnapH) // Snap scaled offset. This is to mitigate backward compatibility issues for GlyphOffset, but a better design would be welcome.
				font_off_x = IM_ROUND(font_off_x);
			if (src->PixelSnapV)
				font_off_y = IM_ROUND(font_off_y);
			font_off_x += EmbeddedIcons_oversample_shift(oversample_h);
			font_off_y += EmbeddedIcons_oversample_shift(oversample_v) + IM_ROUND(baked->Ascent);
			f32 recip_h = 1.0f / (oversample_h * rasterizer_density);
			f32 recip_v = 1.0f / (oversample_v * rasterizer_density);

			// Register glyph
			// r->x r->y are coordinates inside texture (in pixels)
			// glyph.X0, glyph.Y0 are drawing coordinates from base text position, and accounting for oversampling.
			out_glyph->X0 = x0 * recip_h + font_off_x;
			out_glyph->Y0 = y0 * recip_v + font_off_y;
			out_glyph->X1 = (x0 + static_cast<i32>(r->w)) * recip_h + font_off_x;
			out_glyph->Y1 = (y0 + static_cast<i32>(r->h)) * recip_v + font_off_y;
			out_glyph->Visible = true;
			out_glyph->PackId = pack_id;
			out_glyph->Colored = true; // Don't want to tint bitmap RGB icons
			ImFontAtlasBakedSetFontGlyphBitmap(atlas, baked, src, out_glyph, r, reinterpret_cast<u8*>(bitmap_pixels), ImTextureFormat_RGBA32, w * nChans);
		}

		return true;
	}

	static const ImFontLoader* ImFontAtlasGetFontLoaderForEmbeddedIcons()
	{
		static ImFontLoader loader;
		loader.Name = "embedded_icons";
		loader.FontSrcInit = ImplEmbeddedIcons_FontSrcInit;
		loader.FontSrcDestroy = ImplEmbeddedIcons_FontSrcDestroy;
		loader.FontSrcContainsGlyph = ImplEmbeddedIcons_FontSrcContainsGlyph;
		loader.FontBakedInit = ImplEmbeddedIcons_FontBakedInit;
		loader.FontBakedDestroy = nullptr;
		loader.FontBakedLoadGlyph = ImplEmbeddedIcons_FontBakedLoadGlyph;
		return &loader;
	}
#endif

	static void ImGuiLoadCJKVGlyphRange()
	{
		if (ExternalGlobalFontGlyphsTarget.has_value()) {
			ExternalGlobalFontGlyphs = std::move(ExternalGlobalFontGlyphsTarget.value());
			ExternalGlobalFontGlyphsTarget.reset();
		}
		else if (GlobalGlyphRanges.CJKV != nullptr
			&& FontMainUseFullCJKVCurrent == FontMainUseFullCJKVTarget
			) {
			return;
		}

		ImGuiIO& io = ImGui::GetIO();

		// NOTE: Using the glyph ranges builder here takes around ~0.15ms in release and ~2ms in debug builds
		static ImVector<ImWchar>		globalRangesCJKV, globalRangesEN;
		static ImFontGlyphRangesBuilder globalRangesBuilderCJKV, globalRangesBuilderEN;

		// HACK: Somewhat arbitrary non-exhaustive list of glyphs sometimes seen in song names etc.
		static constexpr const char additionalGlyphs[] =
			u8"ΔΨαλμχд‐’“”…′※™ⅠⅡⅤⅥⅦⅩ↑→↓∞∫≠⊆⑨▼◆◇"
			u8"○◎★☆♂♡♢♥♦♨♪亰什儚兩凋區叩吠吼咄哭嗚嘘"
			u8"噛囃堡姐孩學對弩彡彷徨怎怯愴戀戈捌掴撥擺朧朶"
			u8"杓棍棕檄欅洩涵渕溟滾漾漿潘澤濤濱炸焉焔爛狗獨"
			u8"琲甜睛筐篭繋繚繧翡舘芒范蔀蔔蔡薇薔薛蘋蘿號蛻"
			u8"裙訶譚變賽逅邂郢雙霍霖靡韶餃驢髭魄麹麼鼠﻿";

		// HACK: The mere 常用 + 人名 kanji of course aren't anywhere near sufficient, **especially** for song names and file paths.
		//		 This *should* include *at least* the ~6000 漢字漢検１級 + common "fancy" unicode characters used as variations of the regular ASCII set.
		//		 Creating a font atlas that big upfront however absolutely kills startup times so the only sane solution is to use dynamic glyph rasterization
		//		 which will hopefully be fully implemented in the not too distant future :Copium: (https://github.com/ocornut/imgui/pull/3471)
		globalRangesCJKV.clear();
		globalRangesBuilderCJKV.Clear();
		globalRangesBuilderCJKV.AddText(additionalGlyphs, additionalGlyphs + (ArrayCount(additionalGlyphs) - sizeof('\0')));
		globalRangesBuilderCJKV.AddText(ExternalGlobalFontGlyphs.c_str(), ExternalGlobalFontGlyphs.c_str() + ExternalGlobalFontGlyphs.size());
		globalRangesBuilderCJKV.AddText(LanguageLabelsGlobalFontGlyphs.c_str(), LanguageLabelsGlobalFontGlyphs.c_str() + LanguageLabelsGlobalFontGlyphs.size());
		if (FontMainUseFullCJKVTarget) {
			globalRangesBuilderCJKV.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
		}
		// HACK: Only load default ranges for debug builds to compensate for slow font (re)building
		else if (PEEPO_DEBUG) {
			globalRangesBuilderCJKV.AddRanges(io.Fonts->GetGlyphRangesDefault());
		}
		else {
			globalRangesBuilderCJKV.AddRanges(io.Fonts->GetGlyphRangesJapanese());
			globalRangesBuilderCJKV.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
		}
		globalRangesBuilderCJKV.BuildRanges(&globalRangesCJKV);

		globalRangesEN.clear();
		globalRangesBuilderEN.Clear();
		globalRangesBuilderEN.AddRanges(io.Fonts->GetGlyphRangesDefault());
		globalRangesBuilderEN.AddText(ExternalGlobalFontGlyphs.data(), ExternalGlobalFontGlyphs.data() + ExternalGlobalFontGlyphs.size());
		globalRangesBuilderEN.BuildRanges(&globalRangesEN);

		GlobalGlyphRanges.CJKV = globalRangesCJKV.Data;
		GlobalGlyphRanges.EN = globalRangesEN.Data;

		FontMainUseFullCJKVCurrent = FontMainUseFullCJKVTarget;
	}

	static void ImGuiUpdateBuildFonts()
	{
		// TODO: Fonts should probably be set up by the application itself instead of being tucked away here but it doesn't really matter too much for now..
		ImGuiIO& io = ImGui::GetIO();

		ImGuiLoadCJKVGlyphRange();

		//const std::string_view fontFileName = Path::GetFileName(FontFilePath);

		enum class Ownership : u8 { Copy, Move };
		auto addFont = [&](i32 fontSizePixels, const ImWchar* glyphRanges, Ownership ownership) -> ImFont*
		{
			// load the base font
			ImFontConfig fontConfig = {};
			ImFont* font = nullptr;
#if HAS_EMBEDDED_ICONS
			static constexpr ImWchar ranges_embedded_icons[] = { EmbeddedIcons[0].Codepoint, EmbeddedIcons[ArrayCount(EmbeddedIcons) - 1].Codepoint, 0 };
			fontConfig.GlyphExcludeRanges = ranges_embedded_icons;
#endif
			if (GlobalState.FontFileContent != nullptr)
			{
				fontConfig.FontDataOwnedByAtlas = (ownership == Ownership::Move) ? true : false;
				fontConfig.EllipsisChar = '\0';
				//sprintf_s(fontConfig.Name, "%.*s, %dpx", FmtStrViewArgs(fontFileName), fontSizePixels);
				font = io.Fonts->AddFontFromMemoryTTF(GlobalState.FontFileContent, static_cast<int>(GlobalState.FontFileContentSize), static_cast<f32>(fontSizePixels), &fontConfig, glyphRanges);
			}
			else
			{
				fontConfig.SizePixels = static_cast<f32>(fontSizePixels);
				font = io.Fonts->AddFontDefault(&fontConfig);
			}
#if HAS_EMBEDDED_ICONS
			// create a font for embedded icons and merge it into the base font
			ImFontConfig cfgIcons = {};
			strcpy_s(cfgIcons.Name, ArrayCount(cfgIcons.Name), "Peepo Embedded Icons");
			cfgIcons.FontLoader = ImFontAtlasGetFontLoaderForEmbeddedIcons();
			cfgIcons.GlyphRanges = ranges_embedded_icons;
			cfgIcons.MergeMode = true;
			io.Fonts->AddFont(&cfgIcons);
#endif
			return font;
		};

		const b8 rebuild = !io.Fonts->Fonts.empty();
		if (rebuild)
			io.Fonts->Clear();

		// NOTE: Reset in case the default font was changed via the style editor font selector (as this ptr would no longer be valid)
		io.FontDefault = nullptr;

		// NOTE: Unfortunately Dear ImGui does not allow avoiding these copies at the moment as far as I can tell (except for maybe some super hacky "inject nullptrs before shutdown")
		FontMain_CJKV = addFont(GuiScaleI32_AtTarget(FontBaseSizes[0]), GlobalGlyphRanges.CJKV, Ownership::Copy);
		FontMedium_EN = addFont(GuiScaleI32_AtTarget(FontBaseSizes[1]), GlobalGlyphRanges.EN, Ownership::Copy);
		FontLarge_EN = addFont(GuiScaleI32_AtTarget(FontBaseSizes[2]), GlobalGlyphRanges.EN, Ownership::Copy);
	}

	static void LoadFontToGlobalState(std::string& fontFilePath)
	{
		std::string fullFontFilePath = "assets/" + fontFilePath;
		std::cout << "LoadFontToGlobalState: " << fullFontFilePath << std::endl;
		while (GlobalState.FontFileContent == nullptr)
		{
			GlobalState.FontFileContent = ImFileLoadToMemory(fullFontFilePath.c_str(), "rb", &GlobalState.FontFileContentSize, 0);
			if (GlobalState.FontFileContent == nullptr || GlobalState.FontFileContentSize <= 0)
			{
				char messageBuffer[2048];
				const int messageLength = sprintf_s(messageBuffer,
					"Failed to read font file:\n"
					"\"%s\"\n"
					"\n"
					"Please ensure the program was installed correctly and is run from within the correct working directory.\n"
					"\n"
					"Working directory:\n"
					"\"%s\"\n"
					"Executable directory:\n"
					"\"%s\"\n"
					"\n"
					"[ Abort ] to exit application\n"
					"[ Retry ]  to read file again\n"
					"[ Ignore ] to continue with fallback font\n"
					, fullFontFilePath.c_str(), Directory::GetWorkingDirectory().c_str(), Directory::GetExecutableDirectory().c_str());

				const Shell::MessageBoxResult result = Shell::ShowMessageBox(
					std::string_view(messageBuffer, messageLength), "Peepo Drum Kit - Startup Error",
					Shell::MessageBoxButtons::AbortRetryIgnore, Shell::MessageBoxIcon::Error, GlobalState.NativeWindowHandle);

				if (result == Shell::MessageBoxResult::Abort) { ::ExitProcess(-1); }
				if (result == Shell::MessageBoxResult::Retry) { continue; }
				if (result == Shell::MessageBoxResult::Ignore) { break; }
			}
		}
	}

	static void ImGuiAndUserUpdateThenRenderAndPresentFrame()
	{
		if (!GlobalIsWindowMinimized && GlobalSwapChainWaitableObject != NULL)
			::WaitForSingleObjectEx(GlobalSwapChainWaitableObject, 1000, true);
		
		if (FontMainFileNameCurrent != FontMainFileNameTarget) {
			IM_FREE(GlobalState.FontFileContent);
			GlobalState.FontFileContent = nullptr;
			LoadFontToGlobalState(FontMainFileNameTarget);
			ImGuiUpdateBuildFonts();
			FontMainFileNameCurrent = FontMainFileNameTarget;
		}
		else if (ExternalGlobalFontGlyphsTarget.has_value()
			|| FontMainUseFullCJKVCurrent != FontMainUseFullCJKVTarget
			) {
			ImGuiUpdateBuildFonts();
		}

		if (!ApproxmiatelySame(GuiScaleFactorTarget, GuiScaleFactorToSetNextFrame))
		{
			GuiScaleFactorLastAnimationStart = GuiScaleFactorCurrent;

			GuiScaleFactorTarget = ClampRoundGuiScaleFactor(GuiScaleFactorToSetNextFrame);
			GuiScaleFactorToSetNextFrame = GuiScaleFactorTarget;

			ImGui::GetStyle() = GlobalOriginalScaleStyle;
			if (!ApproxmiatelySame(GuiScaleFactorTarget, 1.0f))
				ImGui::GetStyle().ScaleAllSizes(GuiScaleFactorTarget);

			GuiScaleAnimationElapsed = 0.0f;
			if (EnableGuiScaleAnimation)
				IsGuiScaleCurrentlyAnimating = true;
			else
				GuiScaleFactorCurrent = GuiScaleFactorTarget;
		}

		if (IsGuiScaleCurrentlyAnimating)
		{
			GuiScaleAnimationElapsed += ClampBot(GImGui->IO.DeltaTime, (1.0f / 30.0f));
			if (GuiScaleAnimationElapsed >= GuiScaleAnimationDuration)
			{
				IsGuiScaleCurrentlyAnimating = false;
				GImGui->IO.FontGlobalScale = 1.0f;
				GuiScaleFactorCurrent = GuiScaleFactorTarget;
			}
			else
			{
				const f32 t = (GuiScaleAnimationElapsed / GuiScaleAnimationDuration);
				const f32 fontSizeCurrent = static_cast<f32>(GuiScaleI32(FontBaseSizes[0]));
				const f32 fontSizeTarget = static_cast<f32>(GuiScaleI32_AtTarget(FontBaseSizes[0]));
				GImGui->IO.FontGlobalScale = Lerp(fontSizeCurrent, fontSizeTarget, t) / fontSizeTarget;
				GuiScaleFactorCurrent = Lerp(GuiScaleFactorLastAnimationStart, GuiScaleFactorTarget, t);
			}
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui_UpdateInternalInputExtraDataAtStartOfFrame();

		assert(GlobalOnUserUpdate != nullptr);
		GlobalOnUserUpdate();

		ImGui::Render();
		ImGui_UpdateInternalInputExtraDataAtEndOfFrame();

		GlobalD3D11DeviceContext->OMSetRenderTargets(1, &GlobalMainRenderTargetView, nullptr);
		GlobalD3D11DeviceContext->ClearRenderTargetView(GlobalMainRenderTargetView, D3D11SwapChainClearColor);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		// TODO: Maybe handle this better somehow, not sure...
		if (!GlobalIsWindowMinimized)
			GlobalSwapChain->Present(Clamp(GlobalState.SwapInterval, 0, 4), 0);
		else
			::Sleep(33);
	}

	i32 EnterProgramLoop(const StartupParam& startupParam, UserCallbacks userCallbacks)
	{
#if REGENERATE_EMBEDDED_ICONS_SOURCE_CODE 
		ON_STARTUP_CODEGEN();
#endif

		ImGui_ImplWin32_EnableDpiAwareness();
		const HICON windowIcon = ::LoadIconW(::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(PEEPO_DRUM_KIT_ICON));

		WNDCLASSEXW windowClass = { sizeof(windowClass), CS_CLASSDC, MainWindowProc, 0L, 0L, ::GetModuleHandleW(nullptr), windowIcon, NULL, ::CreateSolidBrush(Win32WindowBackgroundColor), nullptr, L"ImGuiApplicationHost", NULL };
		::RegisterClassExW(&windowClass);

		static constexpr DWORD windowStyle = WS_OVERLAPPEDWINDOW;
		const HWND hwnd = ::CreateWindowExW(0, windowClass.lpszClassName, UTF8::WideArg(startupParam.WindowTitle).c_str(),
			windowStyle,
			startupParam.WindowPosition.has_value() ? startupParam.WindowPosition->x : CW_USEDEFAULT,
			startupParam.WindowPosition.has_value() ? startupParam.WindowPosition->y : CW_USEDEFAULT,
			startupParam.WindowSize.has_value() ? startupParam.WindowSize->x : CW_USEDEFAULT,
			startupParam.WindowSize.has_value() ? startupParam.WindowSize->y : CW_USEDEFAULT,
			NULL,
			NULL,
			windowClass.hInstance,
			nullptr);

		GlobalState.NativeWindowHandle = hwnd;
		GlobalState.WindowTitle = startupParam.WindowTitle;

		if (!CreateGlobalD3D11(startupParam, hwnd))
		{
			CleanupGlobalD3D11();
			::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
			return 1;
		}

		::ShowWindow(hwnd, SW_SHOWDEFAULT);
		::UpdateWindow(hwnd);
		::DragAcceptFiles(hwnd, true);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigScrollbarScrollByPage = false; // ImGui pre-1.90.8 default
		io.IniFilename = "settings_imgui.ini";

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplWin32_Init(hwnd, windowIcon);
		ImGui_ImplDX11_Init(GlobalD3D11Device, GlobalD3D11DeviceContext);

		userCallbacks.OnStartup();

		GuiScaleFactorToSetNextFrame = GuiScaleFactorCurrent = GuiScaleFactorTarget;
		GlobalOriginalScaleStyle = ImGui::GetStyle();
		if (!ApproxmiatelySame(GuiScaleFactorTarget, 1.0f))
			ImGui::GetStyle().ScaleAllSizes(GuiScaleFactorTarget);

		b8 done = false;
		while (!done)
		{
			if (GlobalState.RequestExitNextFrame.has_value())
			{
				const CloseResponse response = (GlobalOnUserWindowCloseRequest != nullptr) ? GlobalOnUserWindowCloseRequest() : CloseResponse::Exit;
				if (response != CloseResponse::SupressExit)
					::PostQuitMessage(GlobalState.RequestExitNextFrame.value());
				GlobalState.RequestExitNextFrame = std::nullopt;
			}

			if (!GlobalState.FilePathsDroppedThisFrame.empty())
				GlobalState.FilePathsDroppedThisFrame.clear();

			MSG msg = {};
			while (::PeekMessageW(&msg, NULL, 0U, 0U, PM_REMOVE))
			{
				::TranslateMessage(&msg);
				::DispatchMessageW(&msg);
				if (msg.message == WM_QUIT)
					done = true;
			}
			if (done)
				break;

			GlobalState.IsAnyWindowFocusedLastFrame = GlobalState.IsAnyWindowFocusedThisFrame;
			GlobalState.IsAnyWindowFocusedThisFrame = (GlobalIsWindowFocused || ImGui_ImplWin32_IsAnyViewportFocused());
			GlobalState.HasAnyFocusBeenGainedThisFrame = (GlobalState.IsAnyWindowFocusedThisFrame && !GlobalState.IsAnyWindowFocusedLastFrame);
			GlobalState.HasAllFocusBeenLostThisFrame = (!GlobalState.IsAnyWindowFocusedThisFrame && GlobalState.IsAnyWindowFocusedLastFrame);

			if (!GlobalState.SetWindowTitleNextFrame.empty())
			{
				if (GlobalState.SetWindowTitleNextFrame != GlobalState.WindowTitle)
				{
					::SetWindowTextW(hwnd, UTF8::WideArg(GlobalState.SetWindowTitleNextFrame).c_str());
					GlobalState.WindowTitle = GlobalState.SetWindowTitleNextFrame;
				}
				GlobalState.SetWindowTitleNextFrame.clear();
			}

			if (GlobalState.SetWindowPositionNextFrame.has_value() || GlobalState.SetWindowSizeNextFrame.has_value())
			{
				const ivec2 newPosition = GlobalState.SetWindowPositionNextFrame.value_or(GlobalState.WindowPosition);
				const ivec2 newSize = GlobalState.SetWindowSizeNextFrame.value_or(GlobalState.WindowSize);

				const WindowRectWithDecoration windowRect = Win32ClientToWindowRect(hwnd, windowStyle, ClientRect { newPosition, newSize });
				::SetWindowPos(hwnd, NULL, windowRect.Position.x, windowRect.Position.y, windowRect.Size.x, windowRect.Size.y, SWP_NOZORDER | SWP_NOACTIVATE);

				GlobalState.SetWindowPositionNextFrame = std::nullopt;
				GlobalState.SetWindowSizeNextFrame = std::nullopt;
			}

			if (GlobalState.SetBorderlessFullscreenNextFrame.has_value())
			{
				if (GlobalState.SetBorderlessFullscreenNextFrame.value() != GlobalState.IsBorderlessFullscreen)
				{
					// NOTE: https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
					const DWORD windowStyle = ::GetWindowLongW(hwnd, GWL_STYLE);
					if (windowStyle & WS_OVERLAPPEDWINDOW)
					{
						MONITORINFO monitor = { sizeof(monitor) };
						if (::GetWindowPlacement(hwnd, &GlobalPreFullscreenWindowPlacement) && ::GetMonitorInfoW(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
						{
							::SetWindowLongW(hwnd, GWL_STYLE, windowStyle & ~WS_OVERLAPPEDWINDOW);
							::SetWindowPos(hwnd, HWND_TOP,
								monitor.rcMonitor.left, monitor.rcMonitor.top,
								monitor.rcMonitor.right - monitor.rcMonitor.left,
								monitor.rcMonitor.bottom - monitor.rcMonitor.top,
								SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
							GlobalState.IsBorderlessFullscreen = true;
						}
					}
					else
					{
						::SetWindowLongW(hwnd, GWL_STYLE, windowStyle | WS_OVERLAPPEDWINDOW);
						::SetWindowPlacement(hwnd, &GlobalPreFullscreenWindowPlacement);
						::SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
						GlobalState.IsBorderlessFullscreen = false;
					}
				}
				GlobalState.SetBorderlessFullscreenNextFrame = std::nullopt;
			}

			GlobalOnUserUpdate = userCallbacks.OnUpdate;
			GlobalOnUserWindowCloseRequest = userCallbacks.OnWindowCloseRequest;
			ImGuiAndUserUpdateThenRenderAndPresentFrame();
		}

		userCallbacks.OnShutdown();

		IM_FREE(GlobalState.FontFileContent);
		GlobalState.FontFileContent = nullptr;
		GlobalState.FontFileContentSize = 0;

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		CleanupGlobalD3D11();
		::DestroyWindow(hwnd);
		::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);

		return 0;
	}

	static b8 CreateGlobalD3D11(const StartupParam& startupParam, HWND hWnd)
	{
		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = true;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.Flags = 0;

		if (startupParam.WaitableSwapChain)
			sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		if (startupParam.AllowSwapChainTearing)
			sd.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

		static constexpr UINT deviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED | (PEEPO_DEBUG ? D3D11_CREATE_DEVICE_DEBUG : 0);
		static constexpr D3D_FEATURE_LEVEL inFeatureLevels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
		D3D_FEATURE_LEVEL outFeatureLevel = {};

		HRESULT hr = S_OK;
		if (FAILED(hr = ::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, deviceFlags, inFeatureLevels, ArrayCountI32(inFeatureLevels), D3D11_SDK_VERSION, &sd, &GlobalSwapChain, &GlobalD3D11Device, &outFeatureLevel, &GlobalD3D11DeviceContext)))
		{
			// NOTE: In case FLIP_DISCARD and or FRAME_LATENCY_WAITABLE_OBJECT aren't supported (<= win7?) try again using the regular bitblt model.
			//		 For windows 8.1 specifically it might be better to first check for FLIP_SEQUENTIAL but whatever
			sd.BufferCount = 1;
			sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			sd.Flags = 0;

			if (FAILED(hr = ::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, deviceFlags, inFeatureLevels, ArrayCountI32(inFeatureLevels), D3D11_SDK_VERSION, &sd, &GlobalSwapChain, &GlobalD3D11Device, &outFeatureLevel, &GlobalD3D11DeviceContext)))
				return false;
		}

		// NOTE: Disable silly ALT+ENTER fullscreen toggle default behavior
		{
			IDXGIDevice* dxgiDevice = nullptr;
			IDXGIAdapter* dxgiAdapter = nullptr;
			IDXGIFactory* dxgiFactory = nullptr;

			hr = GlobalD3D11Device ? GlobalD3D11Device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)) : E_INVALIDARG;
			hr = dxgiDevice ? dxgiDevice->GetAdapter(&dxgiAdapter) : hr;
			hr = dxgiAdapter ? dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)) : hr;
			hr = dxgiFactory ? dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER) : hr;

			if (dxgiFactory) { dxgiFactory->Release(); dxgiFactory = nullptr; }
			if (dxgiAdapter) { dxgiAdapter->Release(); dxgiAdapter = nullptr; }
			if (dxgiDevice) { dxgiDevice->Release(); dxgiDevice = nullptr; }
		}

		if (sd.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
		{
			IDXGISwapChain2* swapChain2 = nullptr;
			hr = GlobalSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain2));
			if (swapChain2 != nullptr)
			{
				GlobalSwapChainWaitableObject = swapChain2->GetFrameLatencyWaitableObject();
				hr = swapChain2->SetMaximumFrameLatency(1);
				hr = swapChain2->Release();
			}
		}

		GlobalSwapChainCreationDesc = sd;
		CreateGlobalD3D11SwapchainRenderTarget();
		return true;
	}

	static void CleanupGlobalD3D11()
	{
		CleanupGlobalD3D11SwapchainRenderTarget();
		if (GlobalSwapChainWaitableObject) { ::CloseHandle(GlobalSwapChainWaitableObject); GlobalSwapChainWaitableObject = NULL; }
		if (GlobalSwapChain) { GlobalSwapChain->Release(); GlobalSwapChain = nullptr; }
		if (GlobalD3D11DeviceContext) { GlobalD3D11DeviceContext->Release(); GlobalD3D11DeviceContext = nullptr; }
		if (GlobalD3D11Device) { GlobalD3D11Device->Release(); GlobalD3D11Device = nullptr; }
	}

	static void CreateGlobalD3D11SwapchainRenderTarget()
	{
		ID3D11Texture2D* pBackBuffer;
		GlobalSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		GlobalD3D11Device->CreateRenderTargetView(pBackBuffer, nullptr, &GlobalMainRenderTargetView);
		pBackBuffer->Release();
	}

	static void CleanupGlobalD3D11SwapchainRenderTarget()
	{
		if (GlobalMainRenderTargetView) { GlobalMainRenderTargetView->Release(); GlobalMainRenderTargetView = nullptr; }
	}

	// Win32 message handler
	// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
	// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
	// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
	// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
	static LRESULT WINAPI MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
			return true;

		switch (msg)
		{
		case WM_CREATE:
			// NOTE: "It's hardly a coincidence that the timer ID space is the same size as the address space" - Raymond Chen
			GlobalWindowRedrawTimerID = ::SetTimer(hwnd, reinterpret_cast<UINT_PTR>(&GlobalWindowRedrawTimerID), USER_TIMER_MINIMUM, nullptr);
			break;

		case WM_TIMER:
			if (GlobalIsWindowBeingDragged && wParam == GlobalWindowRedrawTimerID)
			{
				// HACK: Again not great but still better than completly freezing while dragging the window
				if (GlobalD3D11Device != nullptr && GlobalOnUserUpdate != nullptr)
					ImGuiAndUserUpdateThenRenderAndPresentFrame();
				return 0;
			}
			break;

		case WM_ENTERSIZEMOVE: GlobalIsWindowBeingDragged = true; break;
		case WM_EXITSIZEMOVE: GlobalIsWindowBeingDragged = false; break;
		case WM_SETFOCUS: GlobalIsWindowFocused = true; break;
		case WM_KILLFOCUS: GlobalIsWindowFocused = false; break;

		case WM_MOVE:
			GlobalState.WindowPosition = ivec2(static_cast<i16>(LOWORD(lParam)), static_cast<i16>(HIWORD(lParam)));
			return 0;

		case WM_SIZE:
			GlobalIsWindowMinimized = (wParam == SIZE_MINIMIZED);
			if (GlobalD3D11Device != nullptr && wParam != SIZE_MINIMIZED)
			{
				GlobalState.WindowSize = ivec2(static_cast<i32>(LOWORD(lParam)), static_cast<i32>(HIWORD(lParam)));

				CleanupGlobalD3D11SwapchainRenderTarget();

				GlobalSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, GlobalSwapChainCreationDesc.Flags);
				CreateGlobalD3D11SwapchainRenderTarget();

				// HACK: Rendering during a resize is far from perfect but should still be much better than freezing completely
				if (GlobalOnUserUpdate != nullptr)
					ImGuiAndUserUpdateThenRenderAndPresentFrame();
			}
			return 0;

		case WM_SYSCOMMAND:
			if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
				return 0;
			break;

		case WM_DESTROY:
			::PostQuitMessage(0);
			return 0;

		case WM_DPICHANGED:
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
			{
				// const int dpi = HIWORD(wParam);
				// printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
				const RECT* suggested_rect = (RECT*)lParam;
				::SetWindowPos(hwnd, NULL, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
			}
			break;

		case WM_DROPFILES:
			if (HDROP dropHandle = reinterpret_cast<HDROP>(wParam); dropHandle != NULL)
			{
				const UINT droppedFileCount = ::DragQueryFileW(dropHandle, 0xFFFFFFFF, nullptr, 0u);
				std::wstring utf16Buffer;

				GlobalState.FilePathsDroppedThisFrame.clear();
				GlobalState.FilePathsDroppedThisFrame.reserve(droppedFileCount);
				for (UINT i = 0; i < droppedFileCount; i++)
				{
					const UINT requiredBufferSize = ::DragQueryFileW(dropHandle, i, nullptr, 0u);
					if (requiredBufferSize > 0)
					{
						utf16Buffer.resize(requiredBufferSize + 1);
						if (const UINT success = ::DragQueryFileW(dropHandle, i, utf16Buffer.data(), static_cast<UINT>(utf16Buffer.size())); success != 0)
							GlobalState.FilePathsDroppedThisFrame.push_back(UTF8::Narrow(std::wstring_view(utf16Buffer.data(), requiredBufferSize)));
					}
				}

				::DragFinish(dropHandle);
			}
			break;

		case WM_GETMINMAXINFO:
			if (GlobalState.MinWindowSizeRestraints.has_value())
			{
				MINMAXINFO* outMinMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
				outMinMaxInfo->ptMinTrackSize.x = GlobalState.MinWindowSizeRestraints->x;
				outMinMaxInfo->ptMinTrackSize.y = GlobalState.MinWindowSizeRestraints->y;
			}
			break;

		case WM_CLOSE:
			if (GlobalOnUserWindowCloseRequest != nullptr)
			{
				const CloseResponse response = GlobalOnUserWindowCloseRequest();
				if (response == CloseResponse::SupressExit)
					return 0;
			}
			break;

		}
		return ::DefWindowProcW(hwnd, msg, wParam, lParam);
	}
}
