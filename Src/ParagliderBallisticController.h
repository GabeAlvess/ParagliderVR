#pragma once

namespace ParagliderVR
{
    class ParagliderBallisticController final
    {
    public:
        static ParagliderBallisticController& GetSingleton();

        bool BeginGlideFlight();
        void Update(float a_deltaTime, bool a_gliderActive);
        void Abort();
        [[nodiscard]] bool IsInFlight() const noexcept;
        [[nodiscard]] bool IsApplyingVelocityUpdate() const noexcept;

    private:
        [[nodiscard]] bool HasLanded() const;
        void Finish(bool a_landed);

        bool _inFlight = false;
        bool _externalGravityOwner = false;
        bool _landingRecoveryPending = false;
        float _lastKnownGravity = 1.0f;
        float _gravity = 0.0f;
        float _flightTime = 0.0f;
        float _logTimer = 0.0f;
        float _landingRecoveryTime = 0.0f;
    };
}
