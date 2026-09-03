#include "pch.h"
#include "ParagliderBallisticController.h"

namespace ParagliderVR
{
    namespace
    {
        thread_local bool g_applyingVelocityUpdate = false;

        constexpr float kGravityScale = 700.0f;
        constexpr float kMinimumFlightTime = 0.20f;
        constexpr float kMinimumValidGravity = 0.000001f;
        constexpr float kGroundProbeStartHeight = 4.0f;
        constexpr float kGroundProbeDepth = 28.0f;
        constexpr float kLandingProbeDistance = 8.0f;
        constexpr float kLandingAnimationRecoveryDelay = 0.08f;

        bool HasValidGravity(const RE::bhkCharacterController& a_controller)
        {
            return std::isfinite(a_controller.gravity) &&
                a_controller.gravity > kMinimumValidGravity;
        }

        void ResetFallState(RE::bhkCharacterController& a_controller)
        {
            const float worldScale = RE::bhkWorld::GetWorldScale();
            if (!std::isfinite(worldScale) || worldScale <= kMinimumValidGravity) {
                return;
            }
            RE::hkVector4 position;
            a_controller.GetPosition(position, true);
            a_controller.fallTime = 0.0f;
            a_controller.fallStartHeight = position.quad.m128_f32[2] / worldScale;
        }

        bool HasGroundBelow(RE::PlayerCharacter& a_player, float& a_hitDistance)
        {
            auto* world = RE::TES::GetSingleton();
            const float worldScale = RE::bhkWorld::GetWorldScale();
            if (!world || !std::isfinite(worldScale) || worldScale <= kMinimumValidGravity) {
                return false;
            }

            const auto playerPosition = a_player.GetPosition();
            const RE::NiPoint3 start{
                playerPosition.x,
                playerPosition.y,
                playerPosition.z + kGroundProbeStartHeight
            };
            const RE::NiPoint3 end{
                playerPosition.x,
                playerPosition.y,
                playerPosition.z - kGroundProbeDepth
            };
            RE::bhkPickData pickData{};
            pickData.rayInput.from = RE::hkVector4(
                start.x * worldScale,
                start.y * worldScale,
                start.z * worldScale,
                0.0f);
            pickData.rayInput.to = RE::hkVector4(
                end.x * worldScale,
                end.y * worldScale,
                end.z * worldScale,
                0.0f);
            pickData.ray = pickData.rayInput.to - pickData.rayInput.from;
            pickData.rayInput.filterInfo = static_cast<std::uint32_t>(RE::COL_LAYER::kProjectile);
            pickData.rayOutput.Reset();

            auto* hitObject = world->Pick(pickData);
            if (!pickData.rayOutput.HasHit()) {
                return false;
            }

            RE::TESObjectREFR* reference = nullptr;
            if (pickData.rayOutput.rootCollidable) {
                reference = RE::TESHavokUtilities::FindCollidableRef(*pickData.rayOutput.rootCollidable);
            }
            if (!reference && hitObject) {
                reference = hitObject->GetUserData();
            }
            if (reference == &a_player || (reference && reference->As<RE::Actor>())) {
                return false;
            }

            a_hitDistance = (start - end).Length() * pickData.rayOutput.hitFraction;
            return true;
        }
    }

    ParagliderBallisticController& ParagliderBallisticController::GetSingleton()
    {
        static ParagliderBallisticController singleton;
        return singleton;
    }

    bool ParagliderBallisticController::BeginGlideFlight()
    {
        if (_inFlight) {
            return true;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* controller = player ? player->GetCharController() : nullptr;
        const float worldScale = RE::bhkWorld::GetWorldScale();
        if (!controller || !std::isfinite(worldScale) || worldScale <= kMinimumValidGravity) {
            return false;
        }

        if (HasValidGravity(*controller)) {
            _lastKnownGravity = controller->gravity;
            _externalGravityOwner = false;
            _gravity = _lastKnownGravity * kGravityScale;
        } else {
            _externalGravityOwner = true;
            _gravity = 0.0f;
        }

        ResetFallState(*controller);
        controller->context.previousState = controller->context.currentState;
        controller->context.currentState = RE::hkpCharacterStateType::kInAir;
        controller->wantState = RE::hkpCharacterStateType::kInAir;
        controller->surfaceInfo.supportedState = RE::hkpSurfaceInfo::SupportedState::kUnsupported;
        controller->velocityTime = 0.0f;
        _flightTime = 0.0f;
        _logTimer = 0.0f;
        _landingRecoveryPending = false;
        _landingRecoveryTime = 0.0f;
        _inFlight = true;
        logger::info(
            "Paraglider ballistic flight began gravityOwner={} fallStartHeight={:.1f}",
            _externalGravityOwner ? "external" : "paraglider",
            controller->fallStartHeight);
        return true;
    }

    void ParagliderBallisticController::Update(
        float a_deltaTime,
        const FlightCommand& a_flightCommand)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* controller = player ? player->GetCharController() : nullptr;
        const float worldScale = RE::bhkWorld::GetWorldScale();
        if (!controller || !std::isfinite(worldScale) || worldScale <= kMinimumValidGravity) {
            if (_inFlight) {
                Finish(false);
            }
            return;
        }

        if (_landingRecoveryPending) {
            _landingRecoveryTime += a_deltaTime;
            if (_landingRecoveryTime >= kLandingAnimationRecoveryDelay) {
                RE::hkVector4 velocity;
                controller->GetLinearVelocityImpl(velocity);
                const float verticalVelocity = velocity.quad.m128_f32[2] / worldScale;
                const bool recoverableGroundState =
                    controller->supportBody.get() != nullptr && verticalVelocity <= 0.0f;
                if (recoverableGroundState) {
                    ResetFallState(*controller);
                    controller->context.previousState = RE::hkpCharacterStateType::kOnGround;
                    controller->context.currentState = RE::hkpCharacterStateType::kOnGround;
                    controller->wantState = RE::hkpCharacterStateType::kOnGround;
                    controller->surfaceInfo.supportedState =
                        RE::hkpSurfaceInfo::SupportedState::kSupported;
                    controller->velocityTime = 0.0f;
                }
                const bool recovered = recoverableGroundState &&
                    player->NotifyAnimationGraph("IdleForceDefaultState");
                logger::info(
                    "Landing animation recovery recoverableGround={} recovered={} state={} want={} support={} verticalVelocity={:.1f}",
                    recoverableGroundState,
                    recovered,
                    static_cast<std::uint32_t>(controller->context.currentState),
                    static_cast<std::uint32_t>(controller->wantState),
                    controller->supportBody.get() != nullptr,
                    verticalVelocity);
                _landingRecoveryPending = false;
                _landingRecoveryTime = 0.0f;
            }
        }

        const bool hasValidGravity = HasValidGravity(*controller);
        if (hasValidGravity) {
            _lastKnownGravity = controller->gravity;
        }
        if (!_inFlight) {
            return;
        }

        _flightTime += a_deltaTime;
        _logTimer += a_deltaTime;

        if (_externalGravityOwner) {
            if (HasLanded()) {
                Finish(false);
                return;
            }
            if (!hasValidGravity) {
                ResetFallState(*controller);
                return;
            }
            _externalGravityOwner = false;
            _gravity = _lastKnownGravity * kGravityScale;
            logger::info("Paraglider ballistic control resumed after external gravity release");
        }

        if (!hasValidGravity) {
            _externalGravityOwner = true;
            _gravity = 0.0f;
            ResetFallState(*controller);
            logger::info("Paraglider ballistic control yielded to external gravity owner");
            return;
        }

        if (HasLanded()) {
            Finish(true);
            return;
        }

        ResetFallState(*controller);
        RE::hkVector4 velocity;
        controller->GetLinearVelocityImpl(velocity);
        velocity.quad.m128_f32[2] -= _gravity * a_deltaTime * worldScale;
        if (a_flightCommand.active) {
            const RE::NiPoint3 baseVelocity{
                velocity.quad.m128_f32[0] / worldScale,
                velocity.quad.m128_f32[1] / worldScale,
                velocity.quad.m128_f32[2] / worldScale
            };
            const auto contribution = CalculateVelocityDelta(a_flightCommand, baseVelocity);
            velocity.quad.m128_f32[0] += contribution.x * worldScale;
            velocity.quad.m128_f32[1] += contribution.y * worldScale;
            velocity.quad.m128_f32[2] += contribution.z * worldScale;
        }

        g_applyingVelocityUpdate = true;
        controller->SetLinearVelocityImpl(velocity);
        g_applyingVelocityUpdate = false;
        controller->context.previousState = controller->context.currentState;
        controller->context.currentState = RE::hkpCharacterStateType::kInAir;
        controller->wantState = RE::hkpCharacterStateType::kInAir;
        controller->surfaceInfo.supportedState = RE::hkpSurfaceInfo::SupportedState::kUnsupported;
        controller->velocityTime = 0.0f;

        if (_logTimer >= 0.50f) {
            _logTimer = 0.0f;
            logger::info(
                "Ballistic sample glider={} state={} want={} support={} fallTime={:.3f} fallStart={:.1f} velocity=({:.1f},{:.1f},{:.1f})",
                a_flightCommand.active,
                static_cast<std::uint32_t>(controller->context.currentState),
                static_cast<std::uint32_t>(controller->wantState),
                controller->supportBody.get() != nullptr,
                controller->fallTime,
                controller->fallStartHeight,
                velocity.quad.m128_f32[0] / worldScale,
                velocity.quad.m128_f32[1] / worldScale,
                velocity.quad.m128_f32[2] / worldScale);
        }
    }

    void ParagliderBallisticController::Abort()
    {
        if (_inFlight) {
            Finish(false);
        }
    }

    bool ParagliderBallisticController::IsInFlight() const noexcept
    {
        return _inFlight;
    }

    bool ParagliderBallisticController::IsApplyingVelocityUpdate() const noexcept
    {
        return g_applyingVelocityUpdate;
    }

    bool ParagliderBallisticController::HasLanded() const
    {
        if (_flightTime < kMinimumFlightTime) {
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* controller = player ? player->GetCharController() : nullptr;
        const float worldScale = RE::bhkWorld::GetWorldScale();
        if (!controller || !std::isfinite(worldScale) || worldScale <= kMinimumValidGravity) {
            return true;
        }

        RE::hkVector4 velocity;
        controller->GetLinearVelocityImpl(velocity);
        const float verticalVelocity = velocity.quad.m128_f32[2] / worldScale;

        float groundDistance = 0.0f;
        if (!HasGroundBelow(*player, groundDistance)) {
            return false;
        }
        if (groundDistance > kLandingProbeDistance) {
            return false;
        }
        const bool onGround =
            controller->context.currentState == RE::hkpCharacterStateType::kOnGround;
        const bool hasSupport = controller->supportBody.get() != nullptr;
        if (!hasSupport) {
            return false;
        }
        logger::info(
            "Paraglider supported ground contact confirmed probeDistance={:.1f} verticalVelocity={:.1f} state={} onGround={} support={}",
            groundDistance,
            verticalVelocity,
            static_cast<std::uint32_t>(controller->context.currentState),
            onGround,
            hasSupport);
        return true;
    }

    void ParagliderBallisticController::Finish(bool a_landed)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* controller = player ? player->GetCharController() : nullptr;
        if (controller && a_landed) {
            logger::info(
                "Landing state before release state={} want={} support={} supportedState={}",
                static_cast<std::uint32_t>(controller->context.currentState),
                static_cast<std::uint32_t>(controller->wantState),
                controller->supportBody.get() != nullptr,
                controller->surfaceInfo.supportedState.underlying());
            ResetFallState(*controller);
            controller->context.previousState = RE::hkpCharacterStateType::kOnGround;
            controller->context.currentState = RE::hkpCharacterStateType::kOnGround;
            controller->wantState = RE::hkpCharacterStateType::kOnGround;
            if (controller->supportBody.get()) {
                controller->surfaceInfo.supportedState =
                    RE::hkpSurfaceInfo::SupportedState::kSupported;
            }
            controller->velocityTime = 0.0f;
            const bool landingGraphStarted = player &&
                (player->NotifyAnimationGraph("JumpLandSoft") ||
                    player->NotifyAnimationGraph("JumpLand") ||
                    player->NotifyAnimationGraph("JumpLandFailSafe"));
            _landingRecoveryPending = !landingGraphStarted;
            _landingRecoveryTime = 0.0f;
            logger::info(
                "Landing released to game state={} want={} support={} supportedState={} landingGraphStarted={}",
                static_cast<std::uint32_t>(controller->context.currentState),
                static_cast<std::uint32_t>(controller->wantState),
                controller->supportBody.get() != nullptr,
                controller->surfaceInfo.supportedState.underlying(),
                landingGraphStarted);
        }
        logger::info(
            "Paraglider ballistic flight {} after {:.2f}s gravityOwner={}",
            a_landed ? "landed" : "aborted",
            _flightTime,
            _externalGravityOwner ? "external" : "paraglider");
        _inFlight = false;
        _externalGravityOwner = false;
        _gravity = 0.0f;
        _flightTime = 0.0f;
        _logTimer = 0.0f;
    }
}
