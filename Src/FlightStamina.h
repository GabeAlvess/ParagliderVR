#pragma once

namespace ParagliderVR
{
    struct Settings;

    class FlightStamina final
    {
    public:
        void Update(RE::PlayerCharacter& a_player, float a_delta, const Settings& a_settings);
        void Reset();
        [[nodiscard]] bool IsExhausted() const noexcept;

    private:
        bool _bypassLogged = false;
        bool _exhausted = false;
    };
}
