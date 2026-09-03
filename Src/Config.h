#pragma once

namespace ParagliderVR
{
    struct Settings
    {
        bool calibrationMode = false;
        float handsAboveHead = 8.0f;
        float handsBelowHeadLimit = 10.0f;
        float dualMinimumFallMultiplier = 0.10f;
        float dualMaximumFallMultiplier = 0.40f;
        float singleFallMultiplier = 0.65f;
        float referenceFallSpeed = 500.0f;
        float maximumClimbSpeed = 25.0f;
        float steeringSpeed = 560.0f;
        float minimumForwardSpeed = 80.0f;
        float horizontalAcceleration = 160.0f;
        float horizontalDeceleration = 220.0f;
        float verticalTransitionAcceleration = 1400.0f;
        float thumbstickDeadzone = 0.20f;
        float lateralSpeedScale = 0.60f;
        float exhaustedHorizontalSpeedScale = 0.50f;
        float exhaustedFallMultiplier = 0.65f;
        float staminaPerSecond = 8.0f;
        float visualHeight = -15.0f;
        float visualForward = 0.0f;
        float visualScale = 1.0f;
        RE::NiPoint3 visualRotationDegrees{};
        std::string modelPath = "Paraglider\\Glider.nif";
        bool windVisualEnabled = true;
        float windVisualHeight = 90.0f;
        float windVisualForward = 0.0f;
        float windVisualScale = 0.75f;
        RE::NiPoint3 windVisualRotationDegrees{};
        std::string windVisualModelPath = "effects\\fxcameraattachblowingfog.nif";
    };

    class Config final
    {
    public:
        static Config& GetSingleton();
        void Load();
        [[nodiscard]] const Settings& Get() const noexcept;

    private:
        Settings _settings{};
    };
}
