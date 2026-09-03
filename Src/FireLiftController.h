#pragma once

namespace ParagliderVR
{
    struct Settings;

    class FireLiftController final
    {
    public:
        void Reset();
        void Update(
            RE::PlayerCharacter& a_player,
            float a_delta,
            const Settings& a_settings);

    private:
        float _scanTimer = 0.0f;
        bool _insideFireColumn = false;
    };
}
