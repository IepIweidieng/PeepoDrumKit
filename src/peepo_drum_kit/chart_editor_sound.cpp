#include "chart_editor_sound.h"
#include "core_io.h"
#include "audio/audio_file_formats.h"

namespace PeepoDrumKit
{
	void SoundEffectsVoicePool::StartAsyncLoadingAndAddVoices()
	{
		assert(!LoadSoundEffectFuture.valid());
		LoadSoundEffectFuture = std::async(std::launch::async, []() -> AsyncLoadSoundEffectsResult
		{
			AsyncLoadSoundEffectsResult result {};
			for (size_t i = 0; i < EnumCount<SoundEffectType>; i++)
			{
				const std::string_view inFilePath = SoundEffectTypeFilePaths[i];
				auto& resultBuffer = result.SampleBuffers[i];

				auto[fileContent, fileSize] = File::ReadAllBytes(inFilePath);
				if (fileContent == nullptr || fileSize == 0)
				{
					printf("Failed to read file '%.*s'\n", FmtStrViewArgs(inFilePath));
					continue;
				}

				if (Audio::DecodeEntireFile(inFilePath, fileContent.get(), fileSize, resultBuffer) != Audio::DecodeFileResult::FeelsGoodMan)
				{
					printf("Failed to decode audio file '%.*s'\n", FmtStrViewArgs(inFilePath));
					continue;
				}

				// HACK: ...
				if (resultBuffer.SampleRate != Audio::Engine.OutputSampleRate)
					Audio::LinearlyResampleBuffer<i16>(resultBuffer.InterleavedSamples, resultBuffer.FrameCount, resultBuffer.SampleRate, resultBuffer.ChannelCount, Audio::Engine.OutputSampleRate);
			}
			return result;
		});

		char nameBuffer[64];
		for (size_t i = 0; i < VoicePoolSize; i++)
		{
			VoicePool[i] = Audio::Engine.AddVoice(Audio::SourceHandle::Invalid, std::string_view(nameBuffer, sprintf_s(nameBuffer, "SoundEffectVoicePool[%02zu]", i)), false);
			VoicePool[i].SetPauseOnEnd(true);
		}
	}

	void SoundEffectsVoicePool::UpdateAsyncLoading()
	{
		if (LoadSoundEffectFuture.valid() && LoadSoundEffectFuture._Is_ready())
		{
			AsyncLoadSoundEffectsResult loadResult = LoadSoundEffectFuture.get();
			for (size_t i = 0; i < EnumCount<SoundEffectType>; i++)
			{
				if (loadResult.SampleBuffers[i].InterleavedSamples != nullptr)
					LoadedSources[i] = Audio::Engine.LoadSourceFromBufferMove(Path::GetFileName(SoundEffectTypeFilePaths[i]), std::move(loadResult.SampleBuffers[i]));
			}
		}
	}

	void SoundEffectsVoicePool::UnloadAllSourcesAndVoices()
	{
		for (auto& voice : VoicePool)
			Audio::Engine.RemoveVoice(voice);

		if (LoadSoundEffectFuture.valid())
		{
			LoadSoundEffectFuture.wait();
			UpdateAsyncLoading();
		}

		for (auto& handle : LoadedSources)
		{
			if (handle != Audio::SourceHandle::Invalid)
				Audio::Engine.UnloadSource(handle);
		}
	}

	void SoundEffectsVoicePool::PlaySound(SoundEffectType type, Time startTime, std::optional<Time> externalClock, f32 pan)
	{
		Audio::Engine.EnsureStreamRunning();
		const b8 isMetronome = (type >= SoundEffectType::MetronomeBar);
		const SoundGroup soundGroup = isMetronome ? SoundGroup::Metronome : SoundGroup::SoundEffects;
		const b8 audible = (GetSoundGroupVolume(soundGroup) != 0) && (GetSoundGroupVolume(SoundGroup::Master) != 0);
		if (audible)
		{
			Audio::Voice voice = VoicePool[VoicePoolRingIndex];
			voice.SetSource(TryGetSourceForType(type));
			voice.SetSoundGroup(EnumToIndex(soundGroup));
			voice.SetPosition(startTime);
			voice.SetVolume(1.0);
			voice.SetPan(pan);
			voice.SetIsPlaying(true);

			VoicePoolRingIndex++;
			if (VoicePoolRingIndex >= VoicePoolSize)
				VoicePoolRingIndex = 0;
		}
	}

	void SoundEffectsVoicePool::PauseAllFutureVoices()
	{
		for (auto& voice : VoicePool)
		{
			if (voice.GetIsPlaying() && voice.GetPosition() < Time::Zero())
				voice.SetIsPlaying(false);
		}
	}

	Audio::SourceHandle SoundEffectsVoicePool::TryGetSourceForType(SoundEffectType type) const
	{
		assert(type < SoundEffectType::Count);

#if 0 // TODO: Hmm... maybe make user configurable
		if (type == SoundEffectType::MetronomeBar) return LoadedSources[EnumToIndex(SoundEffectType::TaikoDon)];
		if (type == SoundEffectType::MetronomeBeat) return LoadedSources[EnumToIndex(SoundEffectType::TaikoKa)];
#endif

		return LoadedSources[EnumToIndex(type)];
	}
}
