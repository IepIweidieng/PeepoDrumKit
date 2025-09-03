#pragma once
#include "core_types.h"
#include "audio/audio_engine.h"
#include <optional>

namespace PeepoDrumKit
{
	enum class SoundEffectType : u8
	{
		TaikoDon,
		TaikoKa,
		MetronomeBar,
		MetronomeBeat,
		Count
	};

	static constexpr cstr SoundEffectTypeFilePaths[] =
	{
		u8"assets/audio/taiko_don_16bit_44100.wav",
		u8"assets/audio/taiko_ka_16bit_44100.wav",
		u8"assets/audio/metronome_bar_16bit_44100.wav",
		u8"assets/audio/metronome_beat_16bit_44100.wav",
	};

	static_assert(ArrayCount(SoundEffectTypeFilePaths) == EnumCount<SoundEffectType>);

	struct AsyncLoadSoundEffectsResult
	{
		Audio::PCMSampleBuffer SampleBuffers[EnumCount<SoundEffectType>];
	};

	enum class SoundGroup : i32
	{
		Master = 0,
		Metronome = 1,
		SoundEffects = 2,
		Count,
	};

	struct SoundEffectsVoicePool
	{
		SoundEffectsVoicePool() { for (auto& handle : LoadedSources) handle = Audio::SourceHandle::Invalid; }
		void StartAsyncLoadingAndAddVoices();
		void UpdateAsyncLoading();
		void UnloadAllSourcesAndVoices();

		void PlaySound(SoundEffectType type, Time startTime = Time::Zero(), std::optional<Time> externalClock = {}, f32 pan = 0);
		void PauseAllFutureVoices();
		Audio::SourceHandle TryGetSourceForType(SoundEffectType type) const;

		inline void SetSoundGroupVolume(SoundGroup soundGroup, f32 value) { Audio::Engine.SetSoundGroupVolume(EnumToIndex(soundGroup), value); }
		inline f32 GetSoundGroupVolume(SoundGroup soundGroup) { return Audio::Engine.GetSoundGroupVolume(EnumToIndex(soundGroup)); }

		i32 VoicePoolRingIndex = 0;
		static constexpr size_t VoicePoolSize = 32;
		Audio::Voice VoicePool[VoicePoolSize] = {};

		Audio::SourceHandle LoadedSources[EnumCount<SoundEffectType>] = {};
		std::future<AsyncLoadSoundEffectsResult> LoadSoundEffectFuture = {};
	};
}
