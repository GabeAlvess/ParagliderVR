#include "pch.h"
#include "Config.h"
#include "PhysicalParagliderController.h"
#include "higgsinterface001.h"

namespace ParagliderVR
{
    namespace
    {
        constexpr std::string_view kPluginName = "ParagliderVR.esp";
        constexpr RE::FormID kArmorLocalFormID = 0x000800;
        constexpr RE::FormID kArmorAddonLocalFormID = 0x000801;
        constexpr RE::FormID kPhysicalCarrierLocalFormID = 0x000802;
        constexpr std::string_view kPhysicalModelPath = "Paraglider\\GliderPhysical.nif";
        constexpr std::uint32_t kPreferredSlot = 42;
        constexpr std::uint32_t kMinimumSlot = 30;
        constexpr std::uint32_t kMaximumSlot = 61;
        constexpr float kSlotRefreshSeconds = 0.25f;
        constexpr float kGrabRetrySeconds = 0.15f;
        constexpr float kSpawnSettleSeconds = 0.10f;
        constexpr std::string_view kLeftGrabNode = "HIGGS:GrabL";
        constexpr std::string_view kRightGrabNode = "HIGGS:GrabR";

        constexpr std::string_view GrabNodeForHand(bool a_isLeft)
        {
            return a_isLeft ? kLeftGrabNode : kRightGrabNode;
        }

        struct SpawnFrame
        {
            RE::NiTransform world{};
            RE::NiPoint3 localX{};
            RE::NiPoint3 forward{};
            RE::NiPoint3 up{};
            RE::NiPoint3 leftHand{};
            RE::NiPoint3 rightHand{};
            RE::NiPoint3 hmdForward{};
        };

        RE::NiTransform CalibratedHmdToGlider()
        {
            RE::NiTransform transform{};
            transform.translate = { 15.930664f, 9.117188f, 1.521729f };
            transform.scale = 1.0f;
            transform.rotate.entry[0][0] = 0.984169722f;
            transform.rotate.entry[0][1] = 0.176323295f;
            transform.rotate.entry[0][2] = -0.017881911f;
            transform.rotate.entry[1][0] = -0.176799282f;
            transform.rotate.entry[1][1] = 0.983791769f;
            transform.rotate.entry[1][2] = -0.029926598f;
            transform.rotate.entry[2][0] = 0.012315316f;
            transform.rotate.entry[2][1] = 0.032614365f;
            transform.rotate.entry[2][2] = 0.999392152f;
            return transform;
        }

        RE::NiTransform CalibratedHandToGlider(bool a_isLeft)
        {
            RE::NiTransform transform{};
            transform.scale = 1.176471f;
            if (a_isLeft) {
                transform.translate = { -15.662109f, -30.972656f, 17.057617f };
                transform.rotate.entry[0][0] = -0.334620357f;
                transform.rotate.entry[0][1] = 0.902088463f;
                transform.rotate.entry[0][2] = 0.272516280f;
                transform.rotate.entry[1][0] = -0.902805030f;
                transform.rotate.entry[1][1] = -0.389777184f;
                transform.rotate.entry[1][2] = 0.181702793f;
                transform.rotate.entry[2][0] = 0.270132720f;
                transform.rotate.entry[2][1] = -0.185227498f;
                transform.rotate.entry[2][2] = 0.944837868f;
            } else {
                transform.translate = { 7.255859f, -0.496582f, 8.871094f };
                transform.rotate.entry[0][0] = -0.449148178f;
                transform.rotate.entry[0][1] = -0.868204474f;
                transform.rotate.entry[0][2] = -0.210918888f;
                transform.rotate.entry[1][0] = 0.893406689f;
                transform.rotate.entry[1][1] = -0.438945770f;
                transform.rotate.entry[1][2] = -0.095661044f;
                transform.rotate.entry[2][0] = -0.009528650f;
                transform.rotate.entry[2][1] = -0.231402293f;
                transform.rotate.entry[2][2] = 0.972811162f;
            }
            return transform;
        }

        float LengthSquared(const RE::NiPoint3& a_vector)
        {
            return a_vector.x * a_vector.x + a_vector.y * a_vector.y + a_vector.z * a_vector.z;
        }

        RE::NiPoint3 Normalize(
            const RE::NiPoint3& a_vector,
            const RE::NiPoint3& a_fallback)
        {
            const auto lengthSquared = LengthSquared(a_vector);
            if (lengthSquared <= 1.0e-6f) {
                return a_fallback;
            }
            return a_vector / std::sqrt(lengthSquared);
        }

        RE::NiPoint3 Cross(const RE::NiPoint3& a_left, const RE::NiPoint3& a_right)
        {
            return {
                a_left.y * a_right.z - a_left.z * a_right.y,
                a_left.z * a_right.x - a_left.x * a_right.z,
                a_left.x * a_right.y - a_left.y * a_right.x
            };
        }

        float Dot(const RE::NiPoint3& a_left, const RE::NiPoint3& a_right)
        {
            return a_left.x * a_right.x + a_left.y * a_right.y + a_left.z * a_right.z;
        }

        void LogCalibrationTransform(std::string_view a_label, const RE::NiTransform& a_transform)
        {
            RE::NiPoint3 angles{};
            a_transform.rotate.ToEulerAnglesXYZ(angles);
            constexpr float radiansToDegrees = 57.29577951308232f;
            logger::info(
                "CALIBRATION {} translate=({:.6f},{:.6f},{:.6f}) eulerDegrees=({:.6f},{:.6f},{:.6f}) scale={:.6f} matrix=[{:.9f},{:.9f},{:.9f};{:.9f},{:.9f},{:.9f};{:.9f},{:.9f},{:.9f}]",
                a_label,
                a_transform.translate.x,
                a_transform.translate.y,
                a_transform.translate.z,
                angles.x * radiansToDegrees,
                angles.y * radiansToDegrees,
                angles.z * radiansToDegrees,
                a_transform.scale,
                a_transform.rotate.entry[0][0],
                a_transform.rotate.entry[0][1],
                a_transform.rotate.entry[0][2],
                a_transform.rotate.entry[1][0],
                a_transform.rotate.entry[1][1],
                a_transform.rotate.entry[1][2],
                a_transform.rotate.entry[2][0],
                a_transform.rotate.entry[2][1],
                a_transform.rotate.entry[2][2]);
        }

        RE::NiPoint3 ResolveHmdForward(RE::PlayerCharacter& a_player)
        {
            auto* vrData = a_player.GetVRNodeData();
            auto* hmd = vrData ? vrData->UprightHmdNode.get() : nullptr;
            if (!hmd) {
                const auto yaw = a_player.GetAngleZ();
                return { -std::sin(yaw), std::cos(yaw), 0.0f };
            }
            return Normalize(
                { hmd->world.rotate.entry[0][1], hmd->world.rotate.entry[1][1], 0.0f },
                { 0.0f, 1.0f, 0.0f });
        }

        bool ResolveSpawnFrame(RE::PlayerCharacter& a_player, SpawnFrame& a_frame)
        {
            auto* vrData = a_player.GetVRNodeData();
            auto* left = vrData ? vrData->LeftWandNode.get() : nullptr;
            auto* right = vrData ? vrData->RightWandNode.get() : nullptr;
            auto* hmd = vrData ? vrData->UprightHmdNode.get() : nullptr;
            if (!left || !right || !hmd) {
                return false;
            }

            a_frame.leftHand = left->world.translate;
            a_frame.rightHand = right->world.translate;
            a_frame.hmdForward = ResolveHmdForward(a_player);
            a_frame.world = hmd->world * CalibratedHmdToGlider();
            a_frame.world.scale = 1.0f;
            a_frame.localX = {
                a_frame.world.rotate.entry[0][0],
                a_frame.world.rotate.entry[1][0],
                a_frame.world.rotate.entry[2][0]
            };
            a_frame.forward = {
                a_frame.world.rotate.entry[0][1],
                a_frame.world.rotate.entry[1][1],
                a_frame.world.rotate.entry[2][1]
            };
            a_frame.up = {
                a_frame.world.rotate.entry[0][2],
                a_frame.world.rotate.entry[1][2],
                a_frame.world.rotate.entry[2][2]
            };
            return true;
        }

        bool ApplySpawnFrame(RE::TESObjectREFR& a_reference, const SpawnFrame& a_frame)
        {
            RE::NiPoint3 angles{};
            a_frame.world.rotate.ToEulerAnglesXYZ(angles);
            a_reference.SetScale(1.0f);
            a_reference.SetPosition(a_frame.world.translate);
            a_reference.SetAngle(angles);

            auto* root = a_reference.Get3D();
            if (!root) {
                root = a_reference.Load3D(false);
            }
            if (!root) {
                return false;
            }

            root->local = root->parent ? root->parent->world.Invert() * a_frame.world : a_frame.world;
            RE::NiUpdateData updateData{};
            updateData.flags = RE::NiUpdateData::Flag::kDirty;
            root->UpdateWorldData(std::addressof(updateData));
            return true;
        }

    }

    PhysicalParagliderController& PhysicalParagliderController::GetSingleton()
    {
        static PhysicalParagliderController singleton;
        return singleton;
    }

    void PhysicalParagliderController::InitializeHiggs()
    {
        g_higgsInterface = HiggsPluginAPI::GetHiggsInterface001(
            SKSE::GetPluginHandle(),
            SKSE::GetMessagingInterface());
        if (g_higgsInterface) {
            if (!_higgsGrabbedCallbackRegistered) {
                g_higgsInterface->AddGrabbedCallback(OnHiggsGrabbed);
                _higgsGrabbedCallbackRegistered = true;
            }
            logger::info("HIGGS interface ready build={}", g_higgsInterface->GetBuildNumber());
        } else {
            logger::warn("HIGGS interface unavailable; physical two-hand spawning is disabled");
        }
    }

    void PhysicalParagliderController::SetEnabled(bool a_enabled)
    {
        _enabled = a_enabled;
        if (!a_enabled) {
            Reset();
        }
    }

    PhysicalParagliderController::BipedSlot PhysicalParagliderController::SlotMask(
        std::uint32_t a_slotNumber)
    {
        return static_cast<BipedSlot>(1u << (a_slotNumber - kMinimumSlot));
    }

    void PhysicalParagliderController::ResolveForms()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return;
        }
        if (!_armor) {
            _armor = dataHandler->LookupForm<RE::TESObjectARMO>(kArmorLocalFormID, kPluginName);
        }
        if (!_armorAddon) {
            _armorAddon = dataHandler->LookupForm<RE::TESObjectARMA>(kArmorAddonLocalFormID, kPluginName);
        }
        if (!_physicalBase) {
            _physicalBase = dataHandler->LookupForm<RE::TESObjectACTI>(kPhysicalCarrierLocalFormID, kPluginName);
        }
    }

    void PhysicalParagliderController::PreloadPhysicalModel()
    {
        if (_physicalModelPreloadAttempted) {
            return;
        }
        _physicalModelPreloadAttempted = true;
        logger::info("Physical paraglider model preload begin path='{}'", kPhysicalModelPath);
        const auto started = std::chrono::steady_clock::now();
        RE::BSModelDB::DBTraits::ArgsType arguments{};
        const auto result = RE::BSModelDB::Demand(
            kPhysicalModelPath.data(),
            _preloadedPhysicalModel,
            arguments);
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        if (result != RE::BSResource::ErrorCode::kNone || !_preloadedPhysicalModel) {
            logger::warn(
                "Physical paraglider model preload failed error={} elapsedMs={:.1f}",
                static_cast<int>(result),
                elapsed);
            return;
        }
        logger::info("Physical paraglider model preload complete elapsedMs={:.1f}", elapsed);
    }

    bool PhysicalParagliderController::IsEquipped() const
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return player && _armor && player->GetWornArmor(_armor->GetSlotMask()) == _armor;
    }

    bool PhysicalParagliderController::IsDeployed() const
    {
        return _physicalReference != nullptr;
    }

    bool PhysicalParagliderController::IsHeldByHand(bool a_isLeft) const
    {
        return g_higgsInterface && _physicalReference &&
            g_higgsInterface->GetGrabbedObject(a_isLeft) == _physicalReference.get();
    }

    std::string PhysicalParagliderController::GetGrabbedNodeName(bool a_isLeft) const
    {
        if (!g_higgsInterface || !IsHeldByHand(a_isLeft)) {
            return {};
        }
        const auto nodeName = g_higgsInterface->GetGrabbedNodeName(a_isLeft);
        const auto* value = nodeName.c_str();
        return value ? std::string(value) : std::string{};
    }

    bool PhysicalParagliderController::ResolveAuthoredGrabTransform(
        bool a_isLeft,
        RE::NiTransform& a_transform) const
    {
        if (!_physicalReference || !g_higgsInterface) {
            return false;
        }
        a_transform = CalibratedHandToGlider(a_isLeft);
        return true;
    }

    bool PhysicalParagliderController::ApplyAuthoredGrabTransform(bool a_isLeft) const
    {
        if (!IsHeldByHand(a_isLeft)) {
            return false;
        }
        RE::NiTransform transform{};
        if (!ResolveAuthoredGrabTransform(a_isLeft, transform)) {
            return false;
        }
        g_higgsInterface->SetGrabTransform(a_isLeft, transform);
        return true;
    }

    bool PhysicalParagliderController::IsHeldByBothHands() const
    {
        return IsHeldByHand(true) && IsHeldByHand(false);
    }

    bool PhysicalParagliderController::IsHeldByEitherHand() const
    {
        return IsHeldByHand(true) || IsHeldByHand(false);
    }

    bool PhysicalParagliderController::IsReadyForFlight() const
    {
        return _initialGrabComplete && IsHeldByBothHands();
    }

    void PhysicalParagliderController::OnHiggsGrabbed(bool a_isLeft, RE::TESObjectREFR* a_reference)
    {
        GetSingleton().HandleHiggsGrabbed(a_isLeft, a_reference);
    }

    void PhysicalParagliderController::HandleHiggsGrabbed(bool a_isLeft, RE::TESObjectREFR* a_reference)
    {
        if (!g_higgsInterface || !_physicalReference || a_reference != _physicalReference.get()) {
            return;
        }
        if (Config::GetSingleton().Get().calibrationMode) {
            logger::info(
                "Calibration retained natural HIGGS grab for {} hand without applying a fixed transform",
                a_isLeft ? "left" : "right");
            return;
        }
        RE::NiTransform transform{};
        if (!ResolveAuthoredGrabTransform(a_isLeft, transform)) {
            logger::warn("Failed to resolve calibrated transform in HIGGS grabbed callback");
            return;
        }
        g_higgsInterface->SetGrabTransform(a_isLeft, transform);
        logger::info(
            "HIGGS grabbed callback immediately aligned {} hand",
            a_isLeft ? "left" : "right");
    }

    bool PhysicalParagliderController::CaptureCalibration(RE::PlayerCharacter& a_player) const
    {
        if (!g_higgsInterface || !_physicalReference || !IsHeldByBothHands()) {
            return false;
        }
        auto* vrData = a_player.GetVRNodeData();
        auto* hmd = vrData ? vrData->UprightHmdNode.get() : nullptr;
        auto* root = _physicalReference->Get3D();
        if (!hmd || !root) {
            return false;
        }
        RE::NiUpdateData updateData{};
        updateData.flags = RE::NiUpdateData::Flag::kDirty;
        root->UpdateWorldData(std::addressof(updateData));
        logger::info("CALIBRATION CAPTURE BEGIN ref={:08X}", _physicalReference->formID);
        LogCalibrationTransform("HmdToGlider", hmd->world.Invert() * root->world);
        LogCalibrationTransform("RightHandToGlider", g_higgsInterface->GetGrabTransform(false));
        LogCalibrationTransform("LeftHandToGlider", g_higgsInterface->GetGrabTransform(true));
        logger::info("CALIBRATION CAPTURE END");
        return true;
    }

    void PhysicalParagliderController::DisableAutomaticGrabNodes() const
    {
        if (!_physicalReference) {
            return;
        }
        auto* root = _physicalReference->Get3D();
        if (!root) {
            return;
        }
        const std::array<std::pair<std::string_view, std::string_view>, 2> names{
            std::pair{ kLeftGrabNode, std::string_view("ParagliderRuntimeGrabL") },
            std::pair{ kRightGrabNode, std::string_view("ParagliderRuntimeGrabR") }
        };
        for (const auto& [source, replacement] : names) {
            if (auto* node = root->GetObjectByName(RE::BSFixedString(source.data()))) {
                node->name = RE::BSFixedString(replacement.data());
            }
        }
        logger::info("Physical reference prepared without conflicting automatic HIGGS grab nodes");
    }

    void PhysicalParagliderController::BlockHiggsHandsForSpawn()
    {
        if (!g_higgsInterface || _higgsHandsBlockedForSpawn) {
            return;
        }
        _restoreLeftHiggsHand = !g_higgsInterface->IsDisabled(true);
        _restoreRightHiggsHand = !g_higgsInterface->IsDisabled(false);
        if (_restoreLeftHiggsHand) {
            g_higgsInterface->DisableHand(true);
        }
        if (_restoreRightHiggsHand) {
            g_higgsInterface->DisableHand(false);
        }
        _higgsHandsBlockedForSpawn = true;
        logger::info("HIGGS hands blocked before physical paraglider creation");
    }

    void PhysicalParagliderController::RestoreHiggsHandsAfterSpawn()
    {
        if (!g_higgsInterface || !_higgsHandsBlockedForSpawn) {
            return;
        }
        if (_restoreLeftHiggsHand) {
            g_higgsInterface->EnableHand(true);
        }
        if (_restoreRightHiggsHand) {
            g_higgsInterface->EnableHand(false);
        }
        _higgsHandsBlockedForSpawn = false;
        _restoreLeftHiggsHand = false;
        _restoreRightHiggsHand = false;
        logger::info("HIGGS hands restored after physical paraglider stabilization");
    }

    void PhysicalParagliderController::EnableCalibrationGrabbing()
    {
        if (!Config::GetSingleton().Get().calibrationMode || !_physicalReference) {
            return;
        }
        RestoreHiggsHandsAfterSpawn();
        logger::info("Calibration HIGGS grabbing enabled after activation grips were released");
    }

    void PhysicalParagliderController::RefreshAvailableSlot(RE::PlayerCharacter& a_player)
    {
        if (!_armor || !_armorAddon || IsEquipped()) {
            return;
        }
        BipedSlot selected = SlotMask(kPreferredSlot);
        bool found = false;
        for (std::uint32_t distance = 0; distance <= kMaximumSlot - kMinimumSlot && !found; ++distance) {
            const std::array candidates{
                static_cast<std::int32_t>(kPreferredSlot) - static_cast<std::int32_t>(distance),
                static_cast<std::int32_t>(kPreferredSlot) + static_cast<std::int32_t>(distance)
            };
            for (std::size_t index = 0; index < candidates.size(); ++index) {
                if (distance == 0 && index == 1) {
                    continue;
                }
                const auto slotNumber = candidates[index];
                if (slotNumber < static_cast<std::int32_t>(kMinimumSlot) ||
                    slotNumber > static_cast<std::int32_t>(kMaximumSlot)) {
                    continue;
                }
                const auto mask = SlotMask(static_cast<std::uint32_t>(slotNumber));
                if (!a_player.GetWornArmor(mask)) {
                    selected = mask;
                    found = true;
                    break;
                }
            }
        }
        if (selected != _assignedSlot) {
            _assignedSlot = selected;
            _armor->SetSlotMask(selected);
            _armorAddon->SetSlotMask(selected);
            logger::info(
                "Paraglider biped slot selected={} mask=0x{:08X}",
                std::countr_zero(static_cast<std::uint32_t>(selected)) + kMinimumSlot,
                static_cast<std::uint32_t>(selected));
        }
    }

    void PhysicalParagliderController::SpawnPhysicalReference(RE::PlayerCharacter& a_player)
    {
        if (_physicalReference || !_physicalBase || !g_higgsInterface) {
            return;
        }
        BlockHiggsHandsForSpawn();
        const auto placementStarted = std::chrono::steady_clock::now();
        auto reference = a_player.PlaceObjectAtMe(_physicalBase, false);
        const auto placementElapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - placementStarted).count();
        logger::info("Physical carrier PlaceObjectAtMe completed elapsedMs={:.1f}", placementElapsed);
        if (!reference) {
            logger::warn("Failed to spawn physical paraglider carrier");
            RestoreHiggsHandsAfterSpawn();
            return;
        }
        reference->SetTemporary();
        reference->SetActivationBlocked(true);
        SpawnFrame spawnFrame{};
        if (!ResolveSpawnFrame(a_player, spawnFrame) || !ApplySpawnFrame(*reference, spawnFrame)) {
            logger::warn("Failed to resolve stable physical paraglider spawn frame");
            reference->Disable();
            reference->SetDelete(true);
            RestoreHiggsHandsAfterSpawn();
            return;
        }
        _physicalReference = reference;
        _initialGrabComplete = false;
        _grabPhase = GrabPhase::kSettling;
        _grabRetryTimer = 0.0f;
        DisableAutomaticGrabNodes();
        RE::NiPoint3 spawnAngles{};
        spawnFrame.world.rotate.ToEulerAnglesXYZ(spawnAngles);
        logger::info(
            "Physical paraglider spawned ref={:08X} left=({:.3f},{:.3f},{:.3f}) right=({:.3f},{:.3f},{:.3f}) scale={:.3f} localX=({:.3f},{:.3f},{:.3f}) basisForward=({:.3f},{:.3f},{:.3f}) basisUp=({:.3f},{:.3f},{:.3f}) hmdForwardDot={:.3f} angles=({:.3f},{:.3f},{:.3f})",
            reference->formID,
            spawnFrame.leftHand.x,
            spawnFrame.leftHand.y,
            spawnFrame.leftHand.z,
            spawnFrame.rightHand.x,
            spawnFrame.rightHand.y,
            spawnFrame.rightHand.z,
            spawnFrame.world.scale,
            spawnFrame.localX.x,
            spawnFrame.localX.y,
            spawnFrame.localX.z,
            spawnFrame.forward.x,
            spawnFrame.forward.y,
            spawnFrame.forward.z,
            spawnFrame.up.x,
            spawnFrame.up.y,
            spawnFrame.up.z,
            Dot(spawnFrame.forward, spawnFrame.hmdForward),
            spawnAngles.x,
            spawnAngles.y,
            spawnAngles.z);
    }

    void PhysicalParagliderController::RequestInitialTwoHandGrab()
    {
        if (!g_higgsInterface || !_physicalReference || _initialGrabComplete) {
            return;
        }
        auto* reference = _physicalReference.get();
        if (_grabPhase == GrabPhase::kNone || _grabPhase == GrabPhase::kSettling) {
            return;
        }

        if (_grabPhase == GrabPhase::kReady) {
            if (!g_higgsInterface->CanGrabObject(reference, false) ||
                !g_higgsInterface->CanGrabObject(reference, true)) {
                return;
            }
            g_higgsInterface->GrabObject(reference, false);
            g_higgsInterface->GrabObject(reference, true);
            _grabPhase = GrabPhase::kBothRequested;
            _grabRetryTimer = 0.0f;
            logger::info("Requested simultaneous right and left HIGGS grabs");
            return;
        }

        if (_grabPhase == GrabPhase::kBothRequested) {
            const bool rightHeld = IsHeldByHand(false);
            const bool leftHeld = IsHeldByHand(true);
            if (rightHeld && !ApplyAuthoredGrabTransform(false)) {
                logger::warn("Failed to maintain calibrated right-hand transform");
                return;
            }
            if (leftHeld && !ApplyAuthoredGrabTransform(true)) {
                logger::warn("Failed to maintain calibrated left-hand transform");
                return;
            }
            if (rightHeld && leftHeld) {
                _grabPhase = GrabPhase::kAligning;
                logger::info("Both HIGGS hands accepted the glider; confirming calibrated alignment");
                return;
            }
            if (_grabRetryTimer >= kGrabRetrySeconds) {
                if (!rightHeld && g_higgsInterface->CanGrabObject(reference, false)) {
                    g_higgsInterface->GrabObject(reference, false);
                }
                if (!leftHeld && g_higgsInterface->CanGrabObject(reference, true)) {
                    g_higgsInterface->GrabObject(reference, true);
                }
                _grabRetryTimer = 0.0f;
                logger::info(
                    "Retrying missing HIGGS hand rightHeld={} leftHeld={}",
                    rightHeld,
                    leftHeld);
            }
            return;
        }

        if (_grabPhase == GrabPhase::kAligning) {
            if (!IsHeldByBothHands()) {
                _grabPhase = GrabPhase::kBothRequested;
                logger::warn("A HIGGS hand was lost during final alignment; retrying missing hand");
                return;
            }
            if (!ApplyAuthoredGrabTransform(false) || !ApplyAuthoredGrabTransform(true)) {
                logger::warn("Failed to maintain authored lateral-bar transforms during alignment");
                return;
            }
            _grabPhase = GrabPhase::kComplete;
            _initialGrabComplete = true;
            logger::info(
                "Simultaneous two-hand alignment complete; fixed spawn anchor released");
        }
    }

    void PhysicalParagliderController::DestroyPhysicalReference()
    {
        RestoreHiggsHandsAfterSpawn();
        if (_physicalReference) {
            logger::info("Removing physical paraglider ref={:08X}", _physicalReference->formID);
            _physicalReference->Disable();
            _physicalReference->SetDelete(true);
            _physicalReference.reset();
        }
        _initialGrabComplete = false;
        _grabPhase = GrabPhase::kNone;
        _grabRetryTimer = 0.0f;
    }

    void PhysicalParagliderController::Deploy(RE::PlayerCharacter& a_player)
    {
        ResolveForms();
        if (!IsEquipped()) {
            return;
        }
        SpawnPhysicalReference(a_player);
    }

    void PhysicalParagliderController::Retract()
    {
        DestroyPhysicalReference();
    }

    void PhysicalParagliderController::Update(RE::PlayerCharacter& a_player, float a_delta)
    {
        if (!_enabled) {
            return;
        }
        ResolveForms();
        if (!_armor || !_armorAddon || !_physicalBase) {
            return;
        }
        const bool equipped = IsEquipped();
        if (equipped) {
            PreloadPhysicalModel();
        }
        if (!equipped) {
            if (_physicalReference) {
                DestroyPhysicalReference();
            }
            _slotRefreshTimer += a_delta;
            if (_slotRefreshTimer >= kSlotRefreshSeconds) {
                _slotRefreshTimer = 0.0f;
                RefreshAvailableSlot(a_player);
            }
            return;
        }
        if (_physicalReference && !_initialGrabComplete) {
            if (Config::GetSingleton().Get().calibrationMode) {
                SpawnFrame stableFrame{};
                if (ResolveSpawnFrame(a_player, stableFrame)) {
                    ApplySpawnFrame(*_physicalReference, stableFrame);
                }
                if (IsHeldByEitherHand()) {
                    _initialGrabComplete = true;
                    _grabPhase = GrabPhase::kComplete;
                    logger::info("Calibration object touched by HIGGS; fixed spawn anchor released for manual adjustment");
                    RE::DebugNotification("Paraglider calibration object released");
                }
                return;
            }
            _grabRetryTimer += a_delta;
            SpawnFrame stableFrame{};
            if (ResolveSpawnFrame(a_player, stableFrame)) {
                ApplySpawnFrame(*_physicalReference, stableFrame);
            }
            if (_grabPhase == GrabPhase::kSettling) {
                if (_grabRetryTimer >= kSpawnSettleSeconds) {
                    RestoreHiggsHandsAfterSpawn();
                    _grabPhase = GrabPhase::kReady;
                    _grabRetryTimer = kGrabRetrySeconds;
                    logger::info("Physical paraglider spawn stabilized; requesting right HIGGS primary hand first");
                }
            }
            if (_grabPhase != GrabPhase::kSettling) {
                RequestInitialTwoHandGrab();
            }
        }
    }

    void PhysicalParagliderController::Reset()
    {
        DestroyPhysicalReference();
        _slotRefreshTimer = 0.0f;
    }
}
