#pragma once

#include <RE/B/BSSoundHandle.h>

namespace ParagliderVR
{
    class ParagliderAudio final
    {
    public:
        static ParagliderAudio& GetSingleton();

        void StartFlightAudio(RE::PlayerCharacter& a_player);
        void UpdateFlightAudio(RE::PlayerCharacter& a_player, float a_delta);
        void StopFlightAudio();

    private:
        bool PlaySound(
            RE::BSSoundHandle& a_handle,
            std::string_view a_path,
            RE::PlayerCharacter& a_player,
            bool a_fadeIn);
        void ReleaseHandle(RE::BSSoundHandle& a_handle, std::uint16_t a_fadeTimeMS);

        RE::BSSoundHandle _activationHandle{};
        RE::BSSoundHandle _windHandle{};
        bool _flightAudioActive = false;
        float _windRetryTimer = 0.0f;
        std::uint32_t _windRestartCount = 0;
    };
}
