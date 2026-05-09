#include "core_types.h"
#include "core_string.h"
#include <stdio.h>
#include <time.h>
#include <charconv>
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

i32 Time::ToString(char* outBuffer, size_t bufferSize, b8 roundTrip, int minSecPostDigits) const
{
	assert(outBuffer != nullptr && minSecPostDigits >= 0);

	static constexpr const char invalidFormatString[] = "--:--.---";

	const auto seconds = std::abs(Seconds);
	if (!::isfinite(seconds))
	{
		// NOTE: Array count of a string literal char array already accounts for the null terminator
		memcpy(outBuffer, invalidFormatString, ArrayCount(invalidFormatString));
		return static_cast<i32>(ArrayCount(invalidFormatString) - 1);
	}

	const f64 hour = Floor(seconds / 3600.0);
	const f64 min = Floor(Mod(seconds, 3600.0) / 60.0);
	const f64 sec = Mod(seconds, 60.0);

	const char signPrefix[2] = { (seconds < 0.0 && !(static_cast<i32>(hour) < 0)) ? '-' : '\0', '\0' }; // in case hour overflows to negative
	// print the second separately for finer formatting
	i32 outStrLen = snprintf(outBuffer, bufferSize, (hour >= 1) ? "%s%02d:%02d:" : "%s%.0d%02d:",
		signPrefix, static_cast<i32>(hour), static_cast<i32>(min));
	if (outStrLen >= bufferSize) // too large to fit inside buffer? including ending '\0'
		return -1;
	std::to_chars_result result = ASCII::ToCharsFloating(outBuffer + outStrLen, outBuffer + bufferSize, sec, roundTrip, 2, minSecPostDigits);
	return (result.ec != std::errc{}) ? -1 : static_cast<i32>(result.ptr - outBuffer);
}

std::string Time::ToString(b8 roundTrip, int minSecPostDigits) const
{
	return ASCII::ToStringWithFixedBufferInput([&](auto&& begin, auto&& end) { return ToString(begin, end - begin, roundTrip, minSecPostDigits); });
}

// return NaN if invalid
Time Time::FromString(cstr inBuffer)
{
	if (inBuffer == nullptr || inBuffer[0] == '\0')
		return Time::FromSec(std::nan(""));

	b8 isNegative = false;
	if (inBuffer[0] == '-') { isNegative = true; inBuffer++; }
	else if (inBuffer[0] == '+') { isNegative = false; inBuffer++; }

	f64 hour = 0, min = 0, sec = 0;
	if (i32 hour_i, min_i; sscanf_s(inBuffer, "%d:%d:%lf", &hour_i, &min_i, &sec) == 3) {
		hour = hour_i;
		min = std::clamp(min_i, 0, 59);
		sec = std::clamp(sec, 0.0, std::nextafter(60.0, 0));
	} else if (sscanf_s(inBuffer, "%d:%lf", &min_i, &sec) == 2) {
		hour = 0;
		min = min_i;
		sec = std::clamp(sec, 0.0, std::nextafter(60.0, 0));
	} else if (sscanf_s(inBuffer, "%lf", &sec) == 1) {
		hour = min = 0;
	} else {
		return Time::FromSec(std::nan(""));
	}

	const f64 outSeconds = ((hour * 60 + min) * 60) + sec;
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
	assert(outBuffer != nullptr);
	const i32 yyyy = Year;
	const u32 mm = Clamp<u32>(Month, 1, 12);
	const u32 dd = Clamp<u32>(Day, 1, 31);
	auto outStrLen = snprintf(outBuffer, bufferSize, "%04d%c%02u%c%02u", yyyy, separator, mm, separator, dd);
	return (outStrLen >= bufferSize) ? -1 : outStrLen; // too large to fit inside buffer? including ending '\0'
}

std::string Date::ToString(char separator) const
{
	return ASCII::ToStringWithFixedBufferInput([&](auto&& begin, auto&& end) { return ToString(begin, end - begin, separator); });
}

Date Date::FromString(cstr inBuffer)
{
	Date result = {};
	i32 yyyy = 0;
	u32 mm = 0, dd = 0;
	if (sscanf_s(inBuffer, "%d%*c%u%*c%u", &yyyy, &mm, &dd) == 3) {
		result.Year = static_cast<i16>(yyyy);
		result.Month = static_cast<i8>(Clamp<u32>(mm, 1, 12));
		result.Day = static_cast<i8>(Clamp<u32>(dd, 1, 31));
	}
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
