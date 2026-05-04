#include "core_types.h"
#include <stdio.h>
#include <time.h>
#include <functional>
#include <optional>

static_assert(BitsPerByte == 8);
static_assert((sizeof(u8) * BitsPerByte) == 8 && (sizeof(i8) * BitsPerByte) == 8);
static_assert((sizeof(u16) * BitsPerByte) == 16 && (sizeof(i16) * BitsPerByte) == 16);
static_assert((sizeof(u32) * BitsPerByte) == 32 && (sizeof(i32) * BitsPerByte) == 32);
static_assert((sizeof(u64) * BitsPerByte) == 64 && (sizeof(i64) * BitsPerByte) == 64);
static_assert((sizeof(f32) * BitsPerByte) == 32 && (sizeof(f64) * BitsPerByte) == 64);
static_assert((sizeof(b8) * BitsPerByte) == 8);

// return bounding rect, drawn rect
std::pair<Rect, Rect> FitInside(vec2 sizeSrc, Rect drawnRectSrc, Rect rectTarget, EFitInside fit, std::optional<f32> aspectRatioTargetGiven)
{
	if (fit == EFitInside::Fill)
		return { rectTarget, drawnRectSrc };

	constexpr f32 roundingAdd = 0.0f; // 0.5f;
	const f32 aspectRatioSrc = GetAspectRatio(sizeSrc);
	const vec2 sizeTarget = rectTarget.GetSize();
	const f32 aspectRatioTarget = aspectRatioTargetGiven.has_value() ? aspectRatioTargetGiven.value() : GetAspectRatio(sizeTarget);
	Rect rectRes = rectTarget;

	if (fit == EFitInside::ScaleDown)
		fit = (sizeSrc.x > sizeTarget.x || sizeSrc.y > sizeTarget.y) ? EFitInside::Contain : EFitInside::None;

	for (const auto& [u, v, exceedByAspectRatio, getPresentU] : {
		std::tuple{ &vec2::y, &vec2::x, std::function{ std::less<f32>() },    std::function{ [](f32 v, f32 aspectRatioTarget) { return v / aspectRatioTarget; } } },
		std::tuple{ &vec2::x, &vec2::y, std::function{ std::greater<f32>() }, std::function{ [](f32 v, f32 aspectRatioTarget) { return v * aspectRatioTarget; } } },
		}) {
		b8 exceed = (fit == EFitInside::None && sizeSrc.*u > sizeTarget.*u) || (fit == EFitInside::Cover && exceedByAspectRatio(aspectRatioSrc, aspectRatioTarget));
		b8 hasGap = (fit == EFitInside::None && sizeSrc.*u < sizeTarget.*u) || (fit == EFitInside::Contain && exceedByAspectRatio(aspectRatioTarget, aspectRatioSrc));
		if (exceed) {
			// NOTE: top & bottom bars from target edges to scaled src edges
			const f32 present = (fit == EFitInside::None) ? sizeTarget.*u : Round(getPresentU(sizeSrc.*v, aspectRatioTarget) + roundingAdd);
			const f32 bar = Round((sizeSrc.*u - present) / 2.0f);
			if (bar >= 0) {
				drawnRectSrc.TL.*u += bar;
				drawnRectSrc.BR.*u = drawnRectSrc.TL.*u + present;
			}
		}
		else if (hasGap) {
			// NOTE: top & bottom bars from scaled src edges to target edges
			const f32 present = (fit == EFitInside::None) ? sizeSrc.*u : Round(getPresentU(sizeTarget.*v, aspectRatioTarget) + roundingAdd);
			const f32 bar = Round((sizeTarget.*u - present) / 2.0f);
			if (bar >= 0) {
				rectTarget.TL.*u += bar;
				rectTarget.BR.*u = rectTarget.TL.*u + present;
			}
		}
	}

	return { rectRes, drawnRectSrc };
}

static constexpr Time RoundToMilliseconds(Time value) { return Time::FromSec((value.Seconds * 1000.0 + 0.5) * 0.001); }

i32 Time::ToString(char* outBuffer, size_t bufferSize) const
{
	assert(outBuffer != nullptr && bufferSize >= sizeof(FormatBuffer::Data));

	static constexpr Time maxDisplayableTime = Time::FromSec(3599.999999);
	static constexpr const char invalidFormatString[] = "--:--.---";

	const f64 msRoundSeconds = RoundToMilliseconds(Time::FromSec(Absolute(Seconds))).Seconds;
	if (::isnan(msRoundSeconds) || ::isinf(msRoundSeconds))
	{
		// NOTE: Array count of a string literal char array already accounts for the null terminator
		memcpy(outBuffer, invalidFormatString, ArrayCount(invalidFormatString));
		return static_cast<i32>(ArrayCount(invalidFormatString) - 1);
	}

	const f64 msRoundSecondsAbs = Min(msRoundSeconds, maxDisplayableTime.ToSec());
	const f64 min = Floor(Mod(msRoundSecondsAbs, 3600.0) / 60.0);
	const f64 sec = Mod(msRoundSecondsAbs, 60.0);
	const f64 ms = (sec - Floor(sec)) * 1000.0;

	const char signPrefix[2] = { (Seconds < 0.0) ? '-' : '\0', '\0' };
	return sprintf_s(outBuffer, bufferSize, "%s%02d:%02d.%03d", signPrefix, static_cast<i32>(min), static_cast<i32>(sec), static_cast<i32>(ms));
}

Time::FormatBuffer Time::ToString() const
{
	FormatBuffer buffer;
	ToString(buffer.Data, ArrayCount(buffer.Data));
	return buffer;
}

Time Time::FromString(cstr inBuffer)
{
	if (inBuffer == nullptr || inBuffer[0] == '\0')
		return Time::Zero();

	b8 isNegative = false;
	if (inBuffer[0] == '-') { isNegative = true; inBuffer++; }
	else if (inBuffer[0] == '+') { isNegative = false; inBuffer++; }

	i32 min = 0, sec = 0, ms = 0;
	sscanf_s(inBuffer, "%02d:%02d.%03d", &min, &sec, &ms);

	min = Clamp(min, 0, 59);
	sec = Clamp(sec, 0, 59);
	ms = Clamp(ms, 0, 999);

	const f64 outSeconds = (static_cast<f64>(min) * 60.0) + static_cast<f64>(sec) + (static_cast<f64>(ms) * 0.001);
	return Time::FromSec(isNegative ? -outSeconds : outSeconds);
}

Date Date::GetToday()
{
	const time_t inTimeNow = ::time(nullptr); tm outDateNow;
	const errno_t timeToDateError = ::localtime_s(&outDateNow, &inTimeNow);
	if (timeToDateError != 0)
		return Date::Zero();

	Date result = {};
	result.Year = static_cast<i16>(outDateNow.tm_year + 1900);
	result.Month = static_cast<i8>(outDateNow.tm_mon + 1);
	result.Day = static_cast<i8>(outDateNow.tm_mday);
	return result;
}

Date TestDateToday = Date::GetToday();

i32 Date::ToString(char* outBuffer, size_t bufferSize, char separator) const
{
	assert(outBuffer != nullptr && bufferSize >= sizeof(FormatBuffer::Data));
	const u32 yyyy = Clamp<u32>(Year, 0, 9999);
	const u32 mm = Clamp<u32>(Month, 0, 12);
	const u32 dd = Clamp<u32>(Day, 0, 31);
	return sprintf_s(outBuffer, bufferSize, "%04u%c%02u%c%02u", yyyy, separator, mm, separator, dd);
}

Date::FormatBuffer Date::ToString(char separator) const
{
	FormatBuffer buffer;
	ToString(buffer.Data, ArrayCount(buffer.Data), separator);
	return buffer;
}

Date Date::FromString(cstr inBuffer, char separator)
{
	assert(separator == '/' || separator == '-' || separator == '_');
	char formatString[] = "%04u_%02u_%02u";
	formatString[4] = separator;
	formatString[9] = separator;

	u32 yyyy = 0, mm = 0, dd = 0;
	sscanf_s(inBuffer, formatString, &yyyy, &mm, &dd);

	Date result = {};
	result.Year = static_cast<i16>(Clamp<u32>(yyyy, 0, 9999));
	result.Month = static_cast<i8>(Clamp<u32>(mm, 0, 12));
	result.Day = static_cast<i8>(Clamp<u32>(dd, 0, 31));
	return result;
}

#include <Windows.h>

static i64 Win32GetPerformanceCounterTicksPerSecond()
{
	::LARGE_INTEGER frequency = {};
	::QueryPerformanceFrequency(&frequency);
	return frequency.QuadPart;
}

static i64 Win32GetPerformanceCounterTicksNow()
{
	::LARGE_INTEGER timeNow = {};
	::QueryPerformanceCounter(&timeNow);
	return timeNow.QuadPart;
}

static struct Win32PerformanceCounterData
{
	i64 TicksPerSecond = Win32GetPerformanceCounterTicksPerSecond();
	i64 TicksOnProgramStartup = Win32GetPerformanceCounterTicksNow();
} Win32GlobalPerformanceCounter = {};

CPUTime CPUTime::GetNow()
{
	return CPUTime { Win32GetPerformanceCounterTicksNow() - Win32GlobalPerformanceCounter.TicksOnProgramStartup };
}

CPUTime CPUTime::GetNowAbsolute()
{
	return CPUTime { Win32GetPerformanceCounterTicksNow() };
}

Time CPUTime::DeltaTime(const CPUTime& startTime, const CPUTime& endTime)
{
	const i64 deltaTicks = (endTime.Ticks - startTime.Ticks);
	return Time::FromSec(static_cast<f64>(deltaTicks) / static_cast<f64>(Win32GlobalPerformanceCounter.TicksPerSecond));
}
