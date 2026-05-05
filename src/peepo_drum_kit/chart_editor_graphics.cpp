#include "chart_editor_graphics.h"
#include "core_io.h"
#include <thread>
#include <future>
#include <thorvg/thorvg.h>

// TODO: Use for packing texture atlases (?)
// #include "imgui/3rdparty/imstb_rectpack.h"

namespace PeepoDrumKit
{
	static constexpr b8 CompileTimeValidateSprDescTable()
	{
		for (size_t i = 0; i < ArrayCount(SprDescTable); i++)
			if (SprDescTable[i].Spr != static_cast<SprID>(i))
				return false;
		return (ArrayCount(SprDescTable) == EnumCount<SprID>);
	}
	static_assert(CompileTimeValidateSprDescTable(), "Missing or bad entry in sprite table");

	static constexpr i32 PerSideRasterizedTexPadding = 2;
	static constexpr i32 CombinedRasterizedTexPadding = (PerSideRasterizedTexPadding * 2);

	struct SvgRasterizer::Impl {
		tvg::SwCanvas* Canvas = nullptr;
		tvg::Picture* PictureView = nullptr;

		Impl::~Impl()
		{
			if (Canvas != nullptr) {
				Canvas->remove();
				Canvas->~SwCanvas(); // PictureView freed by canvas
			}
			else if (PictureView != nullptr)
				PictureView->unref();
		}
	};

	SvgRasterizer::SvgRasterizer() : pImpl{ std::make_unique<Impl>() } { }
	SvgRasterizer::~SvgRasterizer() = default;

	static bool Parse(SvgRasterizer& rasterizer, std::function<tvg::Result(tvg::Picture*)> load, f32 baseScale)
	{
		auto* picture = tvg::Picture::gen();
		if (load(picture) != tvg::Result::Success) {
			picture->unref(true);
			return false;
		}

		picture->size(&rasterizer.PictureSize.x, &rasterizer.PictureSize.y);
		rasterizer.PictureSize *= baseScale;
		rasterizer.BaseScale = baseScale;
		rasterizer.pImpl->PictureView = picture;

		assert(rasterizer.pImpl->Canvas == nullptr);
		auto* canvas = rasterizer.pImpl->Canvas = tvg::SwCanvas::gen();
		canvas->add(picture);
		return true;
	}

	bool SvgRasterizer::ParseMemory(std::string_view svgFileContent, f32 baseScale)
	{
		return Parse(*this, { [&](tvg::Picture* picture) { return picture->load(svgFileContent.data(), static_cast<u32>(svgFileContent.size()), ""); } }, baseScale);
	}

	bool SvgRasterizer::ParseFromPath(std::string imagePath, f32 baseScale)
	{
		return Parse(*this, { [&](tvg::Picture* picture) { return picture->load(imagePath.c_str()); } }, baseScale);
	}


	// TODO: Rasterize directly into final atlas using stride
	RasterizedBitmap SvgRasterizer::Rasterize(f32 scale)
	{
		const vec2 scaledPictureSize = (PictureSize * scale);
		const ivec2 resolutionWithoutPadding = { static_cast<i32>(Ceil(scaledPictureSize.x)), static_cast<i32>(Ceil(scaledPictureSize.y)) };
		const ivec2 resolution = resolutionWithoutPadding + ivec2(CombinedRasterizedTexPadding);
		RasterizedBitmap out { std::make_unique<u32[]>(resolution.x * resolution.y), resolution };
		if (resolutionWithoutPadding.x <= 0 || resolutionWithoutPadding.y <= 0)
			return out;

		const vec2 position = vec2(PerSideRasterizedTexPadding, PerSideRasterizedTexPadding);
		auto* pictureView = pImpl->PictureView;
		pictureView->scale(scale * BaseScale);
		pictureView->translate(position.x, position.y);

		auto* canvas = pImpl->Canvas;
		canvas->target(out.BGRA.get(), resolution.x, resolution.x, resolution.y, tvg::ColorSpace::ARGB8888/*S*/);
		canvas->update();
		canvas->draw();
		canvas->sync();

		if constexpr (PerSideRasterizedTexPadding > 0)
		{
			auto pixelAt = [&](i32 x, i32 y) -> u32& { return out.BGRA[(y * resolution.x) + x]; };
			auto pixelAtWithoutPadding = [&](i32 x, i32 y) -> u32& { return pixelAt(x + PerSideRasterizedTexPadding, y + PerSideRasterizedTexPadding); };

			for (i32 x = 0; x < PerSideRasterizedTexPadding; x++)
				for (i32 y = 0; y < PerSideRasterizedTexPadding; y++) // NOTE: Top-left / bottom-left / top-right / bottom-right corner
				{
					const ivec2 tl = ivec2(x, y);
					const ivec2 br = ivec2(x, y) + (resolutionWithoutPadding + ivec2(PerSideRasterizedTexPadding));
					pixelAt(tl.x, tl.y) = pixelAtWithoutPadding(0, 0);
					pixelAt(tl.x, br.y) = pixelAtWithoutPadding(0, resolutionWithoutPadding.y - 1);
					pixelAt(br.x, tl.y) = pixelAtWithoutPadding(resolutionWithoutPadding.x - 1, 0);
					pixelAt(br.x, br.y) = pixelAtWithoutPadding(resolutionWithoutPadding.x - 1, resolutionWithoutPadding.y - 1);
				}

			for (i32 y = 0; y < PerSideRasterizedTexPadding; y++)
				for (i32 x = 0; x < resolutionWithoutPadding.x; x++) // NOTE: Top / bottom row
				{
					pixelAt(PerSideRasterizedTexPadding + x, y) = pixelAtWithoutPadding(x, 0);
					pixelAt(PerSideRasterizedTexPadding + x, resolutionWithoutPadding.y + y + PerSideRasterizedTexPadding) = pixelAtWithoutPadding(x, resolutionWithoutPadding.y - 1);
				}
			for (i32 y = 0; y < resolutionWithoutPadding.y; y++)
				for (i32 x = 0; x < PerSideRasterizedTexPadding; x++) // NOTE: Left / right row
				{
					pixelAt(x, PerSideRasterizedTexPadding + y) = pixelAtWithoutPadding(0, y);
					pixelAt(resolutionWithoutPadding.x + x + PerSideRasterizedTexPadding, PerSideRasterizedTexPadding + y) = pixelAtWithoutPadding(resolutionWithoutPadding.x - 1, y);
				}
		}

		return out;
	}

	struct ChartGraphicsResources::OpaqueData
	{
		f32 PerGroupRasterScale[EnumCount<SprGroup>];

		b8 FinishedLoading;
		std::future<void> LoadFuture;

		// TODO: Combine multiple spirites into texture atlases (?)
		SvgRasterizer PerSprSvg[EnumCount<SprID>];
		CustomDraw::GPUTexture PerSprTexture[EnumCount<SprID>];

		// TODO: Global alpha to handle async load fade-ins (?)
		// f32 PerGroupGlobalAlpha[EnumCount<SprGroup>];
	};

	ChartGraphicsResources::ChartGraphicsResources()
	{
		const u32 threadCount = static_cast<u32>(ClampBot(static_cast<i32>(std::thread::hardware_concurrency()) - 1, 0));
		tvg::Initializer::init(threadCount);

		Data = std::make_unique<OpaqueData>();
	}

	ChartGraphicsResources::~ChartGraphicsResources()
	{
		for (auto& it : Data->PerSprTexture) { it.Unload(); }
	}

	void ChartGraphicsResources::StartAsyncLoading()
	{
		assert(!Data->LoadFuture.valid());
		Data->FinishedLoading = false;
		Data->LoadFuture = std::async(std::launch::async, [this]()
		{
#if PEEPO_DEBUG // DEBUG: ...
			auto sw = CPUStopwatch::StartNew();
			defer { auto elapsed = sw.Stop(); printf("Took %g ms to load all sprites\n", elapsed.ToMS()); };
#endif

			for (const SprTypeDesc& it : SprDescTable)
			{
				auto [fileContent, extension] = File::ReadAllBytes(it.FilePath, SupportedSpriteFileFormatExtensions);
#if PEEPO_DEBUG // DEBUG: ...
				if (fileContent.Content == nullptr)
					printf("Failed to read sprite file '%s'\n", it.FilePath);
#endif

				Data->PerSprSvg[EnumToIndex(it.Spr)].ParseMemory(fileContent.AsString(), (it.BaseScale != 0.0f) ? it.BaseScale : 1.0f);
			}
		});
	}

	void ChartGraphicsResources::UpdateAsyncLoading()
	{
		if (Data->LoadFuture.valid() && Data->LoadFuture._Is_ready())
		{
			Data->LoadFuture.get();
			Data->FinishedLoading = true;
		}
	}

	b8 ChartGraphicsResources::IsAsyncLoading() const
	{
		return Data->LoadFuture.valid();
	}

	void ChartGraphicsResources::Rasterize(SprGroup group, f32 scale)
	{
		assert(Data->FinishedLoading && group < SprGroup::Count);

		f32& currentRasterScale = Data->PerGroupRasterScale[EnumToIndex(group)];
		if (ApproxmiatelySame(currentRasterScale, scale))
			return;
		currentRasterScale = scale;

		for (i32 sprIndex = 0; sprIndex < EnumCountI32<SprID>; sprIndex++)
		{
			if (GetSprGroup(static_cast<SprID>(sprIndex)) != group)
				continue;

			PeepoDrumKit::Rasterize(Data->PerSprSvg[sprIndex], Data->PerSprTexture[sprIndex], currentRasterScale);
		}
	}

	SprInfo ChartGraphicsResources::GetInfo(SprID spr) const
	{
		if (!Data->FinishedLoading || spr >= SprID::Count)
			return {};

		SprInfo info;
		info.SourceSize = Data->PerSprSvg[EnumToIndex(spr)].PictureSize;
		info.RasterScale = Data->PerGroupRasterScale[EnumToIndex(GetSprGroup(spr))];
		return info;
	}

	static b8 GetQuad(ImImageQuad& out, const CustomDraw::GPUTexture& tex, const vec2& scaledPictureSize, const vec2& texSizePadded, const SprUV p, u32 colorTint, const SprUV* uv)
	{
		vec2 quadUV[4] = { { 0.0f, 0.0f },{ 1.0f, 0.0f },{ 1.0f, 1.0f },{ 0.0f, 1.0f } };
		if (uv != nullptr) { quadUV[0] = uv->TL; quadUV[1] = uv->TR; quadUV[2] = uv->BR; quadUV[3] = uv->BL; }

		for (vec2& it : quadUV)
		{
			it *= scaledPictureSize;
			it += vec2(PerSideRasterizedTexPadding, PerSideRasterizedTexPadding);
			it /= texSizePadded;
		}

		out.TexID = tex.GetTexID();
		out.Pos[0] = p.TL; out.Pos[1] = p.TR;
		out.Pos[2] = p.BR; out.Pos[3] = p.BL;
		out.UV[0] = quadUV[0]; out.UV[1] = quadUV[1];
		out.UV[2] = quadUV[2]; out.UV[3] = quadUV[3];
		out.Color = colorTint;
		return true;
	}

	b8 ChartGraphicsResources::GetImageQuad(ImImageQuad& out, SprID spr, const vec2& p_min, const vec2& p_max, const vec2& uv_min, const vec2& uv_max, u32 colorTint) const
	{
		if (!Data->FinishedLoading || spr >= SprID::Count)
			return false;

		const f32 rasterScale = Data->PerGroupRasterScale[EnumToIndex(GetSprGroup(spr))];
		const vec2 scaledPictureSize = Data->PerSprSvg[EnumToIndex(spr)].PictureSize * rasterScale;

		const auto& tex = Data->PerSprTexture[EnumToIndex(spr)];
		const vec2 texSizePadded = tex.GetSizeF32();
		return GetQuad(out, tex, scaledPictureSize, texSizePadded, SprUV::FromRect(p_min, p_max), colorTint, &SprUV::FromRect(uv_min, uv_max));
	}

	b8 ChartGraphicsResources::GetImageQuad(ImImageQuad& out, SprID spr, SprTransform transform, u32 colorTint, const SprUV* uv) const
	{
		if (!Data->FinishedLoading || spr >= SprID::Count)
			return false;

		const f32 rasterScale = Data->PerGroupRasterScale[EnumToIndex(GetSprGroup(spr))];;
		transform.Scale /= rasterScale;
		const vec2 scaledPictureSize = Data->PerSprSvg[EnumToIndex(spr)].PictureSize * rasterScale;

		const auto& tex = Data->PerSprTexture[EnumToIndex(spr)];
		const vec2 texSizePadded = tex.GetSizeF32();
		const vec2 size = (transform.Scale * scaledPictureSize);
		const vec2 pivot = (-transform.Pivot * transform.Scale * scaledPictureSize);

		SprUV p;
		if (transform.Rotation.Radians == 0.0f)
		{
			const vec2 pos = (transform.Position + pivot);
			p.TL.x = pos.x;
			p.TL.y = pos.y;
			p.TR.x = pos.x + size.x;
			p.TR.y = pos.y;
			p.BL.x = pos.x;
			p.BL.y = pos.y + size.y;
			p.BR.x = pos.x + size.x;
			p.BR.y = pos.y + size.y;
		}
		else
		{
			const vec2 pos = transform.Position;
			const f32 sin = Sin(transform.Rotation);
			const f32 cos = Cos(transform.Rotation);
			p.TL.x = pos.x + pivot.x * cos - pivot.y * sin;
			p.TL.y = pos.y + pivot.x * sin + pivot.y * cos;
			p.TR.x = pos.x + (pivot.x + size.x) * cos - pivot.y * sin;
			p.TR.y = pos.y + (pivot.x + size.x) * sin + pivot.y * cos;
			p.BL.x = pos.x + pivot.x * cos - (pivot.y + size.y) * sin;
			p.BL.y = pos.y + pivot.x * sin + (pivot.y + size.y) * cos;
			p.BR.x = pos.x + (pivot.x + size.x) * cos - (pivot.y + size.y) * sin;
			p.BR.y = pos.y + (pivot.x + size.x) * sin + (pivot.y + size.y) * cos;
		}

		return GetQuad(out, tex, scaledPictureSize, texSizePadded, p, colorTint, uv);
	}

	SprStretchtOut StretchMultiPartSpr(ChartGraphicsResources& gfx, SprID spr, SprTransform transform, u32 color, SprStretchtParam param, size_t splitCount)
	{
		// TODO: "i32 Axis" param for x/y (?)
		SprStretchtOut out {};

		// TODO: ...
		//param.Splits[...];

		if (splitCount == 2)
		{
			static constexpr f32 perSplitPercentage = (1.0f / 2.0f);;
			transform.Scale.x *= (param.Scales[0] + param.Scales[1]) / 2.0f;
			ImImageQuad baseQuad {}; gfx.GetImageQuad(baseQuad, spr, transform, color, nullptr);

			const f32 splitT = param.Scales[0] / (param.Scales[1] + param.Scales[0]);
			const vec2 topM = Lerp<vec2>(baseQuad.TL(), baseQuad.TR(), splitT);
			const vec2 botM = Lerp<vec2>(baseQuad.BL(), baseQuad.BR(), splitT);
			const vec2 uvTopM = Lerp<vec2>(baseQuad.UV_TL(), baseQuad.UV_TR(), perSplitPercentage);
			const vec2 uvBotM = Lerp<vec2>(baseQuad.UV_BL(), baseQuad.UV_BR(), perSplitPercentage);

			out.BaseTransform = transform;
			out.Quads[0] = baseQuad;
			out.Quads[0].TR(topM); out.Quads[0].UV_TR(uvTopM);
			out.Quads[0].BR(botM); out.Quads[0].UV_BR(uvBotM);

			out.Quads[1] = baseQuad;
			out.Quads[1].TL(topM); out.Quads[1].UV_TL(uvTopM);
			out.Quads[1].BL(botM); out.Quads[1].UV_BL(uvBotM);
		}
		else if (splitCount == 3)
		{
			static constexpr f32 perSplitPercentage = (1.0f / 3.0f);;
			transform.Scale.x *= (param.Scales[0] + param.Scales[1] + param.Scales[2]) / 3.0f;
			ImImageQuad baseQuad {}; gfx.GetImageQuad(baseQuad, spr, transform, color, nullptr);

			const f32 splitLT = param.Scales[0] / (param.Scales[2] + param.Scales[1] + param.Scales[0]);
			const f32 splitRT = 1.0f - (param.Scales[2] / (param.Scales[2] + param.Scales[1] + param.Scales[0]));
			const vec2 topLM = Lerp<vec2>(baseQuad.TL(), baseQuad.TR(), splitLT);
			const vec2 botLM = Lerp<vec2>(baseQuad.BL(), baseQuad.BR(), splitLT);
			const vec2 topRM = Lerp<vec2>(baseQuad.TL(), baseQuad.TR(), splitRT);
			const vec2 botRM = Lerp<vec2>(baseQuad.BL(), baseQuad.BR(), splitRT);
			const vec2 uvTopLM = Lerp<vec2>(baseQuad.UV_TL(), baseQuad.UV_TR(), perSplitPercentage);
			const vec2 uvBotLM = Lerp<vec2>(baseQuad.UV_BL(), baseQuad.UV_BR(), perSplitPercentage);
			const vec2 uvTopRM = Lerp<vec2>(baseQuad.UV_TL(), baseQuad.UV_TR(), 1.0f - perSplitPercentage);
			const vec2 uvBotRM = Lerp<vec2>(baseQuad.UV_BL(), baseQuad.UV_BR(), 1.0f - perSplitPercentage);

			out.BaseTransform = transform;
			out.Quads[0] = baseQuad;
			out.Quads[0].TR(topLM); out.Quads[0].UV_TR(uvTopLM);
			out.Quads[0].BR(botLM); out.Quads[0].UV_BR(uvBotLM);

			out.Quads[1] = baseQuad;
			out.Quads[1].TL(topLM); out.Quads[1].UV_TL(uvTopLM);
			out.Quads[1].BL(botLM); out.Quads[1].UV_BL(uvBotLM);
			out.Quads[1].TR(topRM); out.Quads[1].UV_TR(uvTopRM);
			out.Quads[1].BR(botRM); out.Quads[1].UV_BR(uvBotRM);

			out.Quads[2] = baseQuad;
			out.Quads[2].TL(topRM); out.Quads[2].UV_TL(uvTopRM);
			out.Quads[2].BL(botRM); out.Quads[2].UV_BL(uvBotRM);
		}
		else
		{
			assert(false);
		}

		return out;
	}
}
