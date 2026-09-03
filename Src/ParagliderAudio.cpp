#include "pch.h"
#include "ParagliderAudio.h"

#include <RE/B/BSAudioManager.h>

namespace ParagliderVR
{
    namespace
    {
        constexpr std::string_view kActivationSoundPath = "sound\\fx\\ParagliderVR\\ParagliderOn.wav";
        constexpr std::string_view kWindSoundPath = "sound\\fx\\ParagliderVR\\ParagliderWind.wav";
        constexpr float kWindRetryDelaySeconds = 0.25f;
        constexpr std::uint16_t kActivationReleaseFadeMS = 30;
        constexpr std::uint16_t kWindFadeInMS = 100;
        constexpr std::uint16_t kWindFadeOutMS = 200;
    }

    ParagliderAudio& ParagliderAudio::GetSingleton()
    {
        static ParagliderAudio singleton;
        return singleton;
    }

    void ParagliderAudio::StartFlightAudio(RE::PlayerCharacter& a_player)
    {
        StopFlightAudio();
        _flightAudioActive = true;
        _windRestartCount = 0;
        _windRetryTimer = 0.0f;

        PlaySound(_activationHandle, kActivationSoundPath, a_player, false);
        if (PlaySound(_windHandle, kWindSoundPath, a_player, true)) {
            _windRestartCount = 1;
        }
    }

    void ParagliderAudio::UpdateFlightAudio(RE::PlayerCharacter& a_player, float a_delta)
    {
        if (!_flightAudioActive) {
            return;
        }

        if (_activationHandle.IsValid() && !_activationHandle.IsPlaying()) {
            ReleaseHandle(_activationHandle, kActivationReleaseFadeMS);
        }

        if (_windHandle.IsValid() && _windHandle.IsPlaying()) {
            _windRetryTimer = 0.0f;
            return;
        }

        _windRetryTimer += a_delta;
        if (_windRetryTimer < kWindRetryDelaySeconds) {
            return;
        }

        _windRetryTimer = 0.0f;
        ReleaseHandle(_windHandle, 0);
        if (PlaySound(_windHandle, kWindSoundPath, a_player, true)) {
            ++_windRestartCount;
            logger::info("Paraglider wind SFX restarted count={}", _windRestartCount);
        }
    }

    void ParagliderAudio::StopFlightAudio()
    {
        const bool hadAudio = _flightAudioActive || _activationHandle.IsValid() || _windHandle.IsValid();
        _flightAudioActive = false;
        _windRetryTimer = 0.0f;
        ReleaseHandle(_activationHandle, kActivationReleaseFadeMS);
        ReleaseHandle(_windHandle, kWindFadeOutMS);
        if (hadAudio) {
            logger::info("Paraglider flight SFX stopped windStarts={}", _windRestartCount);
        }
        _windRestartCount = 0;
    }

    bool ParagliderAudio::PlaySound(
        RE::BSSoundHandle& a_handle,
        std::string_view a_path,
        RE::PlayerCharacter& a_player,
        bool a_fadeIn)
    {
        auto* audioManager = RE::BSAudioManager::GetSingleton();
        if (!audioManager) {
            logger::warn("Paraglider SFX skipped: BSAudioManager unavailable path='{}'", a_path);
            return false;
        }

        RE::BSResource::ID soundFile;
        soundFile.GenerateFromPath(a_path.data());
        a_handle = RE::BSSoundHandle{};
        audioManager->BuildSoundDataFromFile(a_handle, soundFile, 0x1A, 128);
        if (!a_handle.IsValid()) {
            logger::warn("Paraglider SFX handle invalid path='{}'", a_path);
            return false;
        }

        if (auto* thirdPerson = a_player.Get3D(false)) {
            a_handle.SetObjectToFollow(thirdPerson);
        } else {
            a_handle.SetPosition(a_player.GetPosition());
        }

        const bool played = a_fadeIn ? a_handle.FadeInPlay(kWindFadeInMS) : a_handle.Play();
        logger::info("Paraglider SFX request path='{}' fadeIn={} played={}", a_path, a_fadeIn, played);
        if (!played) {
            ReleaseHandle(a_handle, 0);
        }
        return played;
    }

    void ParagliderAudio::ReleaseHandle(RE::BSSoundHandle& a_handle, std::uint16_t a_fadeTimeMS)
    {
        if (a_handle.IsValid()) {
            if (!a_handle.FadeOutAndRelease(a_fadeTimeMS)) {
                a_handle.Stop();
            }
            a_handle = RE::BSSoundHandle{};
        }
    }
}
