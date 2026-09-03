#include "pch.h"
#include "ParagliderController.h"
#include "AnimationOar.h"
#include "Config.h"
#include "ParagliderAudio.h"
#include "ParagliderBallisticController.h"
#include "PhysicalParagliderController.h"

#include <RE/B/BSModelDB.h>

namespace ParagliderVR
{
    namespace
    {
        constexpr float kAirborneAnimationRefreshSeconds = 1.25f;
        constexpr float kCalibrationHoldSeconds = 5.0f;

        float MoveTowards(float a_current, float a_target, float a_maximumDelta)
        {
            const float difference = a_target - a_current;
            if (std::abs(difference) <= a_maximumDelta) {
                return a_target;
            }
            return a_current + std::copysign(a_maximumDelta, difference);
        }

        float ApplyDeadzone(float a_value, float a_deadzone)
        {
            const float magnitude = std::abs(a_value);
            if (magnitude <= a_deadzone) {
                return 0.0f;
            }
            return std::copysign(
                (magnitude - a_deadzone) / (1.0f - a_deadzone),
                a_value);
        }

        RE::NiPoint3 Normalize(const RE::NiPoint3& a_value, const RE::NiPoint3& a_fallback = {})
        {
            const float length = a_value.Length();
            return length > 0.0001f ? a_value / length : a_fallback;
        }

        RE::NiPoint3 Cross(const RE::NiPoint3& a_first, const RE::NiPoint3& a_second)
        {
            return {
                (a_first.y * a_second.z) - (a_first.z * a_second.y),
                (a_first.z * a_second.x) - (a_first.x * a_second.z),
                (a_first.x * a_second.y) - (a_first.y * a_second.x)
            };
        }

        struct VrikInterface001
        {
            virtual std::uint32_t getBuildNumber() = 0;
            virtual double getSettingDouble(const char* a_name) = 0;
            virtual void setSettingDouble(const char* a_name, double a_value) = 0;
        };

        struct VrikInterfaceRequest
        {
            static constexpr std::uint32_t kMessageType = 0xF2AFAEE6;
            void* (*getApiFunction)(unsigned int a_revisionNumber){ nullptr };
        };

        VrikInterface001* g_vrikApi = nullptr;
        bool g_vrikOverrideActive = false;
        double g_previousVrikPosture = 1.0;
        double g_previousVrikBody = 1.0;

        bool ResolveVrikApi()
        {
            if (g_vrikApi) {
                return true;
            }
            VrikInterfaceRequest request;
            auto* messaging = SKSE::GetMessagingInterface();
            if (!messaging ||
                !messaging->Dispatch(
                    VrikInterfaceRequest::kMessageType,
                    &request,
                    sizeof(request),
                    "VRIK") ||
                !request.getApiFunction) {
                return false;
            }
            g_vrikApi = static_cast<VrikInterface001*>(request.getApiFunction(1));
            if (g_vrikApi) {
                logger::info("VRIK interface ready build={}", g_vrikApi->getBuildNumber());
            }
            return g_vrikApi != nullptr;
        }

        void SetVrikLowerBodyOverride(bool a_enabled)
        {
            if (a_enabled) {
                if (g_vrikOverrideActive || !ResolveVrikApi()) {
                    return;
                }
                g_previousVrikPosture = g_vrikApi->getSettingDouble("enablePosture");
                g_previousVrikBody = g_vrikApi->getSettingDouble("enableBody");
                g_vrikApi->setSettingDouble("enablePosture", 0.0);
                g_vrikApi->setSettingDouble("enableBody", 0.0);
                g_vrikOverrideActive = true;
                logger::info(
                    "VRIK lower-body override enabled previousPosture={} previousBody={}",
                    g_previousVrikPosture,
                    g_previousVrikBody);
                return;
            }
            if (!g_vrikOverrideActive || !g_vrikApi) {
                return;
            }
            g_vrikApi->setSettingDouble("enablePosture", g_previousVrikPosture);
            g_vrikApi->setSettingDouble("enableBody", g_previousVrikBody);
            g_vrikOverrideActive = false;
            logger::info("VRIK lower-body override restored");
        }

        RE::NiPoint3 ControllerForward(const RE::NiAVObject& a_node)
        {
            return Normalize({
                -a_node.world.rotate.entry[0][2],
                -a_node.world.rotate.entry[1][2],
                -a_node.world.rotate.entry[2][2]
            });
        }

        RE::NiPoint3 HmdForward(const RE::NiAVObject& a_hmd)
        {
            return Normalize({
                a_hmd.world.rotate.entry[0][1],
                a_hmd.world.rotate.entry[1][1],
                0.0f
            }, { 0.0f, 1.0f, 0.0f });
        }

        RE::NiPoint3 HmdRight(const RE::NiPoint3& a_forward)
        {
            return Normalize({ a_forward.y, -a_forward.x, 0.0f }, { 1.0f, 0.0f, 0.0f });
        }

        RE::NiMatrix3 BuildRotation(const RE::NiPoint3& a_forward)
        {
            const RE::NiPoint3 up{ 0.0f, 0.0f, 1.0f };
            const auto right = Normalize(Cross(a_forward, up), { 1.0f, 0.0f, 0.0f });
            const auto forward = Normalize(Cross(up, right), { 0.0f, 1.0f, 0.0f });
            RE::NiMatrix3 rotation{};
            rotation.entry[0][0] = right.x;
            rotation.entry[1][0] = right.y;
            rotation.entry[2][0] = right.z;
            rotation.entry[0][1] = forward.x;
            rotation.entry[1][1] = forward.y;
            rotation.entry[2][1] = forward.z;
            rotation.entry[0][2] = up.x;
            rotation.entry[1][2] = up.y;
            rotation.entry[2][2] = up.z;
            return rotation;
        }

        bool RestartAirborneAnimation(RE::PlayerCharacter& a_player)
        {
            return a_player.NotifyAnimationGraph("JumpFall") ||
                a_player.NotifyAnimationGraph("JumpFallDirectional");
        }

        void SetWorldTransform(
            RE::NiAVObject& a_object,
            const RE::NiNode& a_parent,
            const RE::NiPoint3& a_position,
            const RE::NiMatrix3& a_rotation,
            float a_scale)
        {
            const float parentScale = (std::max)(a_parent.world.scale, 0.0001f);
            const auto inverseParentRotation = a_parent.world.rotate.Transpose();
            a_object.local.translate = inverseParentRotation * (a_position - a_parent.world.translate) / parentScale;
            a_object.local.rotate = inverseParentRotation * a_rotation;
            a_object.local.scale = a_scale / parentScale;
        }

        RE::NiNode* ResolveThirdPersonRoot(RE::PlayerCharacter& a_player)
        {
            auto* thirdPerson = a_player.Get3D(false);
            if (!thirdPerson) {
                return nullptr;
            }
            if (auto* root = thirdPerson->GetObjectByName("NPC Root [Root]"); root) {
                return root->AsNode();
            }
            return thirdPerson->AsNode();
        }

        RE::NiPointer<RE::NiNode> LoadModel(const std::string& a_path, std::string_view a_label)
        {
            RE::NiPointer<RE::NiNode> modelRoot;
            RE::BSModelDB::DBTraits::ArgsType arguments{};
            const auto result = RE::BSModelDB::Demand(a_path.c_str(), modelRoot, arguments);
            if (result != RE::BSResource::ErrorCode::kNone || !modelRoot) {
                logger::error("{} model load failed path='{}' error={}", a_label, a_path, static_cast<int>(result));
                return nullptr;
            }
            auto* clone = modelRoot->Clone();
            auto* node = clone ? clone->AsNode() : nullptr;
            if (node) {
                node->collisionObject = nullptr;
                node->SetAppCulled(true);
                std::size_t geometryCount = 0;
                RE::BSVisit::TraverseScenegraphGeometries(
                    node,
                    [&geometryCount](RE::BSGeometry* a_geometry) {
                        if (!a_geometry) {
                            return RE::BSVisit::BSVisitControl::kContinue;
                        }
                        ++geometryCount;
                        a_geometry->SetAppCulled(false);
                        if (auto* property = a_geometry->lightingShaderProp_cast()) {
                            property->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kTwoSided);
                            property->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kNoFade);
                            property->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kZBufferTest);
                            property->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kZBufferWrite);
                        }
                        return RE::BSVisit::BSVisitControl::kContinue;
                    });
                logger::info("{} model loaded path='{}' geometries={} twoSided=true", a_label, a_path, geometryCount);
            } else {
                logger::error("{} model clone is not a NiNode path='{}'", a_label, a_path);
            }
            return RE::NiPointer<RE::NiNode>(node);
        }
    }

    ParagliderController& ParagliderController::GetSingleton()
    {
        static ParagliderController singleton;
        return singleton;
    }

    void ParagliderController::InstallInput()
    {
        if (_inputInstalled) {
            return;
        }
        if (auto* manager = RE::BSInputDeviceManager::GetSingleton()) {
            manager->AddEventSink(this);
            _inputInstalled = true;
            logger::info("VR grip input installed");
        } else {
            logger::critical("BSInputDeviceManager unavailable");
        }
    }

    void ParagliderController::SetEnabled(bool a_enabled)
    {
        _enabled = a_enabled;
        if (!a_enabled) {
            StopFlight("gameplay suspended");
            ParagliderBallisticController::GetSingleton().Abort();
            for (auto& hand : _hands) {
                hand.gripDown = false;
                hand.valid = false;
            }
            _calibrationHoldSeconds.fill(0.0f);
            _calibrationLoggedSeconds.fill(0);
            _calibrationCaptured = false;
            _calibrationAwaitingGripRelease = false;
            _mainThumbstick.y = 0.0f;
            _mainThumbstick.x = 0.0f;
            _offThumbstick.y = 0.0f;
            _offThumbstick.x = 0.0f;
        }
    }

    RE::BSEventNotifyControl ParagliderController::ProcessEvent(
        RE::InputEvent* const* a_eventList,
        RE::BSTEventSource<RE::InputEvent*>*)
    {
        if (!_enabled || !a_eventList) {
            return RE::BSEventNotifyControl::kContinue;
        }
        for (auto* event = *a_eventList; event; event = event->next) {
            if (auto* thumbstick = event->AsThumbstickEvent()) {
                if (thumbstick->IsMainHand()) {
                    _mainThumbstick.x = std::clamp(thumbstick->xValue, -1.0f, 1.0f);
                    _mainThumbstick.y = std::clamp(thumbstick->yValue, -1.0f, 1.0f);
                } else if (thumbstick->IsOffHand()) {
                    _offThumbstick.x = std::clamp(thumbstick->xValue, -1.0f, 1.0f);
                    _offThumbstick.y = std::clamp(thumbstick->yValue, -1.0f, 1.0f);
                }
                continue;
            }
            auto* button = event->AsButtonEvent();
            if (!button) {
                continue;
            }
            const auto id = button->GetIDCode();
            const bool isGrip = id == static_cast<std::uint32_t>(vr::k_EButton_Grip);
            if (!isGrip) {
                continue;
            }
            const auto device = button->GetDevice();
            const bool left = device == RE::INPUT_DEVICE::kOculusSecondary ||
                device == RE::INPUT_DEVICE::kViveSecondary ||
                device == RE::INPUT_DEVICE::kWMRSecondary;
            const bool right = device == RE::INPUT_DEVICE::kOculusPrimary ||
                device == RE::INPUT_DEVICE::kVivePrimary ||
                device == RE::INPUT_DEVICE::kWMRPrimary;
            if (!left && !right) {
                continue;
            }
            auto& hand = _hands[left ? 0 : 1];
            const bool pressed = button->IsPressed();
            if (hand.gripDown != pressed) {
                hand.gripDown = pressed;
                logger::info("{} grip {}", left ? "Left" : "Right", pressed ? "pressed" : "released");
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    bool ParagliderController::IsAirborne(const RE::PlayerCharacter& a_player) const
    {
        auto* controller = a_player.GetCharController();
        return controller &&
            (controller->supportBody.get() == nullptr ||
                controller->context.currentState == RE::hkpCharacterStateType::kInAir);
    }

    void ParagliderController::RefreshHands(RE::PlayerCharacter& a_player)
    {
        auto* vrData = a_player.GetVRNodeData();
        if (!vrData) {
            _hands[0].valid = false;
            _hands[1].valid = false;
            return;
        }
        std::array<RE::NiAVObject*, 2> nodes{
            vrData->LeftWandNode ? vrData->LeftWandNode.get() : vrData->NPCLHnd.get(),
            vrData->RightWandNode ? vrData->RightWandNode.get() : vrData->NPCRHnd.get()
        };
        for (std::size_t index = 0; index < _hands.size(); ++index) {
            auto& hand = _hands[index];
            auto* node = nodes[index];
            hand.valid = node != nullptr;
            if (node) {
                hand.position = node->world.translate;
                hand.forward = ControllerForward(*node);
            }
        }
    }

    void ParagliderController::StartFlight()
    {
        if (!ParagliderBallisticController::GetSingleton().BeginGlideFlight()) {
            logger::warn("Flight activation rejected because ballistic control could not start");
            return;
        }
        float initialHorizontalSpeed = Config::GetSingleton().Get().minimumForwardSpeed;
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            if (auto* controller = player->GetCharController()) {
                const float worldScale = RE::bhkWorld::GetWorldScale();
                if (std::isfinite(worldScale) && worldScale > 0.000001f) {
                    RE::hkVector4 velocity;
                    controller->GetLinearVelocityImpl(velocity);
                    const float x = velocity.quad.m128_f32[0] / worldScale;
                    const float y = velocity.quad.m128_f32[1] / worldScale;
                    initialHorizontalSpeed = std::sqrt((x * x) + (y * y));
                }
            }
        }
        const auto& settings = Config::GetSingleton().Get();
        _commandedHorizontalSpeed = std::clamp(
            initialHorizontalSpeed,
            settings.minimumForwardSpeed,
            settings.steeringSpeed);
        _staminaExhausted = false;
        {
            std::scoped_lock lock(_flightLock);
            _flight.active = true;
        }
        AnimationOar::SetActive(true);
        SetVrikLowerBodyOverride(true);
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            ParagliderAudio::GetSingleton().StartFlightAudio(*player);
        }
        _animationRefreshTimer = 0.0f;
        const bool graphStarted = [&]() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            return player && RestartAirborneAnimation(*player);
        }();
        RE::DebugNotification("Paraglider opened");
        logger::info(
            "Flight started airborneGraphStarted={} initialHorizontalSpeed={:.1f}",
            graphStarted,
            _commandedHorizontalSpeed);
    }

    void ParagliderController::StopFlight(std::string_view a_reason)
    {
        bool wasActive = false;
        {
            std::scoped_lock lock(_flightLock);
            wasActive = _flight.active;
            _flight = {};
        }
        PhysicalParagliderController::GetSingleton().Retract();
        HideVisual();
        ParagliderAudio::GetSingleton().StopFlightAudio();
        if (wasActive) {
            if (a_reason == "landed") {
                _activationBlockedUntilGripRelease = true;
            }
            AnimationOar::SetActive(false);
            SetVrikLowerBodyOverride(false);
            _staminaBypassLogged = false;
            _staminaExhausted = false;
            _gestureLogTimer = 0.0f;
            _animationRefreshTimer = 0.0f;
            _commandedHorizontalSpeed = 0.0f;
            _singleHandVisualActive = false;
            logger::info("Flight stopped: {}", a_reason);
        }
    }

    void ParagliderController::Update()
    {
        if (!_enabled) {
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || player->IsDead()) {
            StopFlight("player unavailable");
            ParagliderBallisticController::GetSingleton().Abort();
            return;
        }
        float delta = 0.011f;
        if (auto* timer = RE::BSTimer::GetSingleton(); timer && timer->delta > 0.0f) {
            delta = (std::min)(timer->delta, 0.05f);
        }
        RefreshHands(*player);
        auto& physical = PhysicalParagliderController::GetSingleton();
        physical.Update(*player, delta);
        if (!physical.IsEquipped()) {
            StopFlight("paraglider unequipped");
            return;
        }
        if (_activationBlockedUntilGripRelease) {
            if (!_hands[0].gripDown && !_hands[1].gripDown) {
                _activationBlockedUntilGripRelease = false;
                logger::info("Paraglider activation unblocked after both grips released");
            }
            return;
        }
        const bool active = [&]() {
            std::scoped_lock lock(_flightLock);
            return _flight.active;
        }();
        if (!active) {
            auto* vrData = player->GetVRNodeData();
            auto* hmd = vrData ? vrData->UprightHmdNode.get() : nullptr;
            const auto& settings = Config::GetSingleton().Get();
            const bool activationGesture = hmd && (settings.calibrationMode || IsAirborne(*player)) &&
                _hands[0].valid && _hands[1].valid &&
                _hands[0].gripDown && _hands[1].gripDown &&
                _hands[0].position.z >= hmd->world.translate.z + settings.handsAboveHead &&
                _hands[1].position.z >= hmd->world.translate.z + settings.handsAboveHead;
            if (!physical.IsDeployed() && activationGesture) {
                physical.Deploy(*player);
                if (settings.calibrationMode && physical.IsDeployed()) {
                    _calibrationAwaitingGripRelease = true;
                    _calibrationHoldSeconds.fill(0.0f);
                    _calibrationLoggedSeconds.fill(0);
                    _calibrationCaptured = false;
                    logger::info("Calibration spawned with HIGGS blocked; waiting for both activation grips to release");
                    RE::DebugNotification("Release both grips, then grab anywhere");
                }
            }
            if (settings.calibrationMode) {
                if (!physical.IsDeployed()) {
                    _calibrationHoldSeconds.fill(0.0f);
                    _calibrationLoggedSeconds.fill(0);
                    _calibrationCaptured = false;
                    _calibrationAwaitingGripRelease = false;
                    return;
                }
                if (_calibrationAwaitingGripRelease) {
                    _calibrationHoldSeconds.fill(0.0f);
                    _calibrationLoggedSeconds.fill(0);
                    if (!_hands[0].gripDown && !_hands[1].gripDown) {
                        physical.EnableCalibrationGrabbing();
                        _calibrationAwaitingGripRelease = false;
                        logger::info("Calibration ready for unrestricted natural HIGGS grabs");
                        RE::DebugNotification("Calibration ready: grab where you want");
                    }
                    return;
                }
                const std::array held{
                    physical.IsHeldByHand(true),
                    physical.IsHeldByHand(false)
                };
                for (std::size_t index = 0; index < held.size(); ++index) {
                    if (held[index]) {
                        _calibrationHoldSeconds[index] =
                            (std::min)(_calibrationHoldSeconds[index] + delta, kCalibrationHoldSeconds);
                        const int completedSeconds = static_cast<int>(_calibrationHoldSeconds[index]);
                        if (completedSeconds > _calibrationLoggedSeconds[index] && completedSeconds < 5) {
                            _calibrationLoggedSeconds[index] = completedSeconds;
                            logger::info(
                                "Calibration {} hand hold {}/5 seconds",
                                index == 0 ? "left" : "right",
                                completedSeconds);
                        }
                    } else {
                        if (_calibrationHoldSeconds[index] > 0.0f) {
                            logger::info(
                                "Calibration {} hand hold reset before 5 seconds",
                                index == 0 ? "left" : "right");
                        }
                        _calibrationHoldSeconds[index] = 0.0f;
                        _calibrationLoggedSeconds[index] = 0;
                    }
                }
                if (!held[0] && !held[1]) {
                    _calibrationCaptured = false;
                }
                if (!_calibrationCaptured &&
                    _calibrationHoldSeconds[0] >= kCalibrationHoldSeconds &&
                    _calibrationHoldSeconds[1] >= kCalibrationHoldSeconds) {
                    if (physical.CaptureCalibration(*player)) {
                        _calibrationCaptured = true;
                        RE::DebugNotification("Paraglider calibration captured");
                        logger::info("Calibration automatically captured after both hands held for 5 seconds");
                    } else {
                        RE::DebugNotification("Hold the paraglider with both hands");
                        logger::warn("Timed calibration completed but both HIGGS hands were not available at capture");
                    }
                }
                return;
            }
            if (physical.IsDeployed() && !_hands[0].gripDown && !_hands[1].gripDown) {
                physical.Retract();
            } else if (physical.IsReadyForFlight()) {
                StartFlight();
            }
            return;
        }
        UpdateFlight(*player, delta);
    }

    void ParagliderController::UpdateFlight(RE::PlayerCharacter& a_player, float a_delta)
    {
        if (!ParagliderBallisticController::GetSingleton().IsInFlight() && !IsAirborne(a_player)) {
            StopFlight("landed");
            return;
        }
        if (!PhysicalParagliderController::GetSingleton().IsHeldByEitherHand()) {
            StopFlight("physical paraglider released");
            return;
        }
        const int gripCount = static_cast<int>(_hands[0].gripDown) + static_cast<int>(_hands[1].gripDown);
        if (gripCount == 0) {
            StopFlight("both grips released");
            return;
        }
        const auto& settings = Config::GetSingleton().Get();
        auto* vrData = a_player.GetVRNodeData();
        auto* hmd = vrData ? vrData->UprightHmdNode.get() : nullptr;
        if (!hmd) {
            StopFlight("HMD unavailable");
            return;
        }
        const float minimumActiveHandHeight = hmd->world.translate.z - settings.handsBelowHeadLimit;
        for (std::size_t index = 0; index < _hands.size(); ++index) {
            const auto& hand = _hands[index];
            if (hand.gripDown && hand.valid && hand.position.z < minimumActiveHandHeight) {
                logger::info(
                    "Flight hand-height limit crossed hand={} handZ={:.1f} hmdZ={:.1f} minimumZ={:.1f}",
                    index == 0 ? "left" : "right",
                    hand.position.z,
                    hmd->world.translate.z,
                    minimumActiveHandHeight);
                StopFlight("hand lowered below active height limit");
                return;
            }
        }
        ParagliderAudio::GetSingleton().UpdateFlightAudio(a_player, a_delta);
        _animationRefreshTimer += a_delta;
        if (_animationRefreshTimer >= kAirborneAnimationRefreshSeconds) {
            _animationRefreshTimer = 0.0f;
            logger::info("Airborne animation refreshed={}", RestartAirborneAnimation(a_player));
        }
        if (settings.staminaPerSecond > 0.0f && !_staminaExhausted) {
            auto* actorValueOwner = a_player.AsActorValueOwner();
            const float stamina = actorValueOwner ? actorValueOwner->GetActorValue(RE::ActorValue::kStamina) : 0.0f;
            const float cost = settings.staminaPerSecond * a_delta;
            if (RE::PlayerCharacter::IsGodMode()) {
                if (!_staminaBypassLogged) {
                    _staminaBypassLogged = true;
                    logger::info("Stamina consumption bypassed by player god mode");
                }
            } else if (!actorValueOwner || !std::isfinite(stamina) || stamina <= cost) {
                _staminaExhausted = true;
                RE::DebugNotification("Stamina depleted - paraglider performance reduced");
                logger::info(
                    "Paraglider stamina exhausted stamina={:.2f} horizontalScale={:.2f} fallMultiplier={:.2f}",
                    stamina,
                    settings.exhaustedHorizontalSpeedScale,
                    settings.exhaustedFallMultiplier);
            } else {
                const float before = stamina;
                actorValueOwner->RestoreActorValue(
                    RE::ACTOR_VALUE_MODIFIER::kDamage,
                    RE::ActorValue::kStamina,
                    -cost);
                const float after = actorValueOwner->GetActorValue(RE::ActorValue::kStamina);
                if (std::isfinite(after) && after >= before - 0.0001f) {
                    if (!_staminaBypassLogged) {
                        _staminaBypassLogged = true;
                        logger::info(
                            "Stamina consumption bypassed because the game kept the value locked before={:.2f} after={:.2f}",
                            before,
                            after);
                    }
                } else {
                    _staminaBypassLogged = false;
                }
            }
        }

        const bool dualHanded = gripCount == 2;

        const auto hmdForward = HmdForward(*hmd);
        const auto hmdRight = HmdRight(hmdForward);
        const float dominantThrottle = ApplyDeadzone(_mainThumbstick.y, settings.thumbstickDeadzone);
        const float offhandThrottle = ApplyDeadzone(_offThumbstick.y, settings.thumbstickDeadzone);
        const float lateralThrottle = ApplyDeadzone(_offThumbstick.x, settings.thumbstickDeadzone);
        if (offhandThrottle > 0.0f) {
            _commandedHorizontalSpeed += settings.horizontalAcceleration * offhandThrottle * a_delta;
        } else if (offhandThrottle < 0.0f) {
            _commandedHorizontalSpeed += settings.horizontalDeceleration * offhandThrottle * a_delta;
        }
        _commandedHorizontalSpeed = std::clamp(
            _commandedHorizontalSpeed,
            settings.minimumForwardSpeed,
            settings.steeringSpeed);
        AnimationOar::Direction animationDirection = AnimationOar::Direction::kIdle;
        if (offhandThrottle > 0.10f) {
            animationDirection = AnimationOar::Direction::kForward;
        } else if (offhandThrottle < -0.10f) {
            animationDirection = AnimationOar::Direction::kBackward;
        }
        AnimationOar::SetDirection(animationDirection);

        FlightState state{};
        state.active = true;
        state.dualHanded = dualHanded;
        state.referenceFallSpeed = settings.referenceFallSpeed;
        const float gentleFallSpeed = -settings.referenceFallSpeed * settings.dualMinimumFallMultiplier;
        const float maximumDescentSpeed = -settings.referenceFallSpeed * settings.dualMaximumFallMultiplier;
        if (!dualHanded) {
            state.verticalTargetSpeed = -settings.referenceFallSpeed * settings.singleFallMultiplier;
        } else if (dominantThrottle >= 0.0f) {
            state.verticalTargetSpeed = gentleFallSpeed +
                ((settings.maximumClimbSpeed - gentleFallSpeed) * dominantThrottle);
        } else {
            state.verticalTargetSpeed = gentleFallSpeed +
                ((maximumDescentSpeed - gentleFallSpeed) * -dominantThrottle);
        }
        state.fallMultiplier = state.verticalTargetSpeed < 0.0f ?
            -state.verticalTargetSpeed / state.referenceFallSpeed :
            0.0f;
        if (_staminaExhausted) {
            state.verticalTargetSpeed =
                -settings.referenceFallSpeed * settings.exhaustedFallMultiplier;
            state.fallMultiplier = settings.exhaustedFallMultiplier;
        }
        state.steeringDirection = hmdForward;
        state.steeringSpeed = _commandedHorizontalSpeed *
            (_staminaExhausted ? settings.exhaustedHorizontalSpeedScale : 1.0f);
        state.lateralDirection = hmdRight;
        state.lateralSpeed = _commandedHorizontalSpeed * settings.lateralSpeedScale * lateralThrottle *
            (_staminaExhausted ? settings.exhaustedHorizontalSpeedScale : 1.0f);
        state.horizontalAcceleration = settings.horizontalAcceleration;
        state.horizontalDeceleration = settings.horizontalDeceleration;
        state.verticalTransitionAcceleration = settings.verticalTransitionAcceleration;
        state.deltaTime = a_delta;
        {
            std::scoped_lock lock(_flightLock);
            _flight = state;
        }
        EnsureWindVisual(a_player);
        UpdateWindVisual(a_player);
        _gestureLogTimer += a_delta;
        if (_gestureLogTimer >= 0.50f) {
            _gestureLogTimer = 0.0f;
            logger::info(
                "Control sample hands={} exhausted={} dominantY={:.2f} offhand=({:.2f},{:.2f}) verticalThrottle={:.2f} horizontalThrottle={:.2f} lateralThrottle={:.2f} commandedSpeed={:.1f} effectiveSpeed={:.1f} lateralSpeed={:.1f} verticalTarget={:.1f}",
                gripCount,
                _staminaExhausted,
                _mainThumbstick.y,
                _offThumbstick.x,
                _offThumbstick.y,
                dominantThrottle,
                offhandThrottle,
                lateralThrottle,
                _commandedHorizontalSpeed,
                state.steeringSpeed,
                state.lateralSpeed,
                state.verticalTargetSpeed);
        }
    }

    bool ParagliderController::IsFlightActive() const
    {
        std::scoped_lock lock(_flightLock);
        return _flight.active;
    }

    RE::NiPoint3 ParagliderController::BuildVelocityDelta(const RE::NiPoint3& a_baseVelocity) const
    {
        FlightState state{};
        {
            std::scoped_lock lock(_flightLock);
            state = _flight;
        }
        if (!state.active) {
            return {};
        }
        RE::NiPoint3 target = a_baseVelocity;
        const float maximumVerticalChange = state.verticalTransitionAcceleration * state.deltaTime;
        target.z = MoveTowards(a_baseVelocity.z, state.verticalTargetSpeed, maximumVerticalChange);
        if (state.steeringDirection.Length() > 0.001f) {
            const auto desiredHorizontal =
                (state.steeringDirection * state.steeringSpeed) +
                (state.lateralDirection * state.lateralSpeed);
            RE::NiPoint3 horizontalDifference{
                desiredHorizontal.x - target.x,
                desiredHorizontal.y - target.y,
                0.0f
            };
            const float currentHorizontalSpeed = std::sqrt((target.x * target.x) + (target.y * target.y));
            const float desiredHorizontalSpeed = desiredHorizontal.Length();
            const float rate = desiredHorizontalSpeed >= currentHorizontalSpeed ?
                state.horizontalAcceleration : state.horizontalDeceleration;
            const float maximumHorizontalChange = rate * state.deltaTime;
            const float differenceLength = horizontalDifference.Length();
            if (differenceLength > maximumHorizontalChange && differenceLength > 0.0001f) {
                horizontalDifference = horizontalDifference / differenceLength * maximumHorizontalChange;
            }
            target.x += horizontalDifference.x;
            target.y += horizontalDifference.y;
        }
        return target - a_baseVelocity;
    }

    void ParagliderController::EnsureVisual(RE::PlayerCharacter& a_player)
    {
        auto* thirdPersonRoot = ResolveThirdPersonRoot(a_player);
        if (!thirdPersonRoot) {
            return;
        }
        if (!_visual) {
            _visual = LoadModel(Config::GetSingleton().Get().modelPath, "Paraglider");
        }
        if (!_visual) {
            return;
        }
        if (_visualParent != thirdPersonRoot) {
            if (_visual->parent) {
                _visual->parent->DetachChild(_visual.get());
            }
            thirdPersonRoot->AttachChild(_visual.get(), true);
            _visualParent = thirdPersonRoot;
            _visualTransformLogged = false;
            logger::info(
                "Paraglider visual attached to third-person node name='{}'",
                thirdPersonRoot->name.c_str());
        }
        _visual->SetAppCulled(false);
    }

    void ParagliderController::UpdateVisual(RE::PlayerCharacter& a_player)
    {
        static_cast<void>(a_player);
        if (!_visual || !_visualParent) {
            return;
        }
        const int gripCount = static_cast<int>(_hands[0].gripDown && _hands[0].valid) +
            static_cast<int>(_hands[1].gripDown && _hands[1].valid);
        if (gripCount == 0) {
            return;
        }
        const auto& settings = Config::GetSingleton().Get();
        constexpr float degreesToRadians = 0.017453292519943295f;
        RE::NiMatrix3 visualAdjustment{};
        visualAdjustment.SetEulerAnglesXYZ(
            settings.visualRotationDegrees.x * degreesToRadians,
            settings.visualRotationDegrees.y * degreesToRadians,
            settings.visualRotationDegrees.z * degreesToRadians);
        RE::NiPoint3 position{};
        RE::NiMatrix3 rotation{};
        if (gripCount == 2) {
            const auto handSpan = _hands[1].position - _hands[0].position;
            const auto right = Normalize({ handSpan.x, handSpan.y, 0.0f }, { 1.0f, 0.0f, 0.0f });
            const auto forward = Normalize(Cross({ 0.0f, 0.0f, 1.0f }, right), { 0.0f, 1.0f, 0.0f });
            const auto midpoint = (_hands[0].position + _hands[1].position) * 0.5f;
            position = midpoint + RE::NiPoint3{ 0.0f, 0.0f, settings.visualHeight } +
                (forward * settings.visualForward);
            rotation = BuildRotation(forward) * visualAdjustment;
            _singleHandVisualActive = false;
        } else {
            const std::size_t activeIndex = _hands[0].gripDown && _hands[0].valid ? 0 : 1;
            const auto& activeHand = _hands[activeIndex];
            if (!_singleHandVisualActive || _singleHandVisualIndex != activeIndex) {
                _singleHandVisualActive = true;
                _singleHandVisualIndex = activeIndex;
                _singleHandVisualOffset = _visual->world.translate - activeHand.position;
                _singleHandVisualRotation = _visual->world.rotate;
                logger::info("Single-hand visual anchor captured hand={}", activeIndex == 0 ? "left" : "right");
            }
            position = activeHand.position + _singleHandVisualOffset;
            rotation = _singleHandVisualRotation;
        }
        SetWorldTransform(
            *_visual,
            *_visualParent,
            position,
            rotation,
            settings.visualScale);
        RE::NiUpdateData updateData;
        updateData.flags = RE::NiUpdateData::Flag::kDirty;
        updateData.time = 0.0f;
        _visual->Update(updateData);
        if (!_visualTransformLogged) {
            _visualTransformLogged = true;
            logger::info(
                "Paraglider visual transform grips={} target=({:.2f},{:.2f},{:.2f}) local=({:.2f},{:.2f},{:.2f}) world=({:.2f},{:.2f},{:.2f}) localScale={:.3f} worldScale={:.3f} leftHand=({:.2f},{:.2f},{:.2f}) rightHand=({:.2f},{:.2f},{:.2f})",
                gripCount,
                position.x,
                position.y,
                position.z,
                _visual->local.translate.x,
                _visual->local.translate.y,
                _visual->local.translate.z,
                _visual->world.translate.x,
                _visual->world.translate.y,
                _visual->world.translate.z,
                _visual->local.scale,
                _visual->world.scale,
                _hands[0].position.x,
                _hands[0].position.y,
                _hands[0].position.z,
                _hands[1].position.x,
                _hands[1].position.y,
                _hands[1].position.z);
        }
    }

    void ParagliderController::EnsureWindVisual(RE::PlayerCharacter& a_player)
    {
        const auto& settings = Config::GetSingleton().Get();
        if (!settings.windVisualEnabled) {
            HideWindVisual();
            return;
        }
        auto* thirdPersonRoot = ResolveThirdPersonRoot(a_player);
        if (!thirdPersonRoot) {
            return;
        }
        if (!_windVisual) {
            _windVisual = LoadModel(settings.windVisualModelPath, "Wind visual");
        }
        if (!_windVisual) {
            return;
        }
        if (_windVisualParent != thirdPersonRoot) {
            if (_windVisual->parent) {
                _windVisual->parent->DetachChild(_windVisual.get());
            }
            thirdPersonRoot->AttachChild(_windVisual.get(), true);
            _windVisualParent = thirdPersonRoot;
            _windVisualTransformLogged = false;
            logger::info(
                "Wind visual attached to third-person node name='{}'",
                thirdPersonRoot->name.c_str());
        }
        _windVisual->SetAppCulled(false);
    }

    void ParagliderController::UpdateWindVisual(RE::PlayerCharacter& a_player)
    {
        if (!_windVisual || !_windVisualParent) {
            return;
        }
        const auto& settings = Config::GetSingleton().Get();
        FlightState state{};
        {
            std::scoped_lock lock(_flightLock);
            state = _flight;
        }
        const auto forward = Normalize(
            { state.steeringDirection.x, state.steeringDirection.y, 0.0f },
            { 0.0f, 1.0f, 0.0f });
        constexpr float degreesToRadians = 0.017453292519943295f;
        RE::NiMatrix3 adjustment{};
        adjustment.SetEulerAnglesXYZ(
            settings.windVisualRotationDegrees.x * degreesToRadians,
            settings.windVisualRotationDegrees.y * degreesToRadians,
            settings.windVisualRotationDegrees.z * degreesToRadians);
        const auto position = a_player.GetPosition() +
            RE::NiPoint3{ 0.0f, 0.0f, settings.windVisualHeight } +
            (forward * settings.windVisualForward);
        const float speedRange = (std::max)(settings.steeringSpeed - settings.minimumForwardSpeed, 1.0f);
        const float speedRatio = std::clamp(
            (state.steeringSpeed - settings.minimumForwardSpeed) / speedRange,
            0.0f,
            1.0f);
        const float scale = settings.windVisualScale * (0.75f + (0.35f * speedRatio));
        SetWorldTransform(
            *_windVisual,
            *_windVisualParent,
            position,
            BuildRotation(forward) * adjustment,
            scale);
        RE::NiUpdateData updateData;
        updateData.flags = RE::NiUpdateData::Flag::kDirty;
        _windVisual->Update(updateData);
        if (!_windVisualTransformLogged) {
            _windVisualTransformLogged = true;
            logger::info(
                "Wind visual transform path='{}' position=({:.1f},{:.1f},{:.1f}) scale={:.2f} speedRatio={:.2f}",
                settings.windVisualModelPath,
                position.x,
                position.y,
                position.z,
                scale,
                speedRatio);
        }
    }

    void ParagliderController::HideWindVisual()
    {
        if (_windVisual) {
            _windVisual->SetAppCulled(true);
            if (_windVisual->parent) {
                _windVisual->parent->DetachChild(_windVisual.get());
            }
        }
        _windVisualParent = nullptr;
        _windVisualTransformLogged = false;
    }

    void ParagliderController::HideVisual()
    {
        if (_visual) {
            _visual->SetAppCulled(true);
            if (_visual->parent) {
                _visual->parent->DetachChild(_visual.get());
            }
        }
        _visualParent = nullptr;
        _visualTransformLogged = false;
        _singleHandVisualActive = false;
        HideWindVisual();
    }
}
