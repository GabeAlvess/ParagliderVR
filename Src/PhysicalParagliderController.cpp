#include "pch.h"
#include "Config.h"
#include "GliderPose.h"
#include "ParagliderInput.h"
#include "PhysicalParagliderController.h"

namespace ParagliderVR
{
    namespace
    {
        constexpr std::string_view kLeftGrabNode = "HIGGS:GrabL";
        constexpr std::string_view kRightGrabNode = "HIGGS:GrabR";

    }

    PhysicalParagliderController& PhysicalParagliderController::GetSingleton()
    {
        static PhysicalParagliderController singleton;
        return singleton;
    }

    void PhysicalParagliderController::InitializeHiggs()
    {
        _higgsGrab.Initialize();
    }

    void PhysicalParagliderController::SetEnabled(bool a_enabled)
    {
        _enabled = a_enabled;
        if (!a_enabled) {
            Reset();
        }
    }

    bool PhysicalParagliderController::IsEquipped() const
    {
        return _equipment.IsEquipped();
    }

    bool PhysicalParagliderController::IsDeployed() const
    {
        return _physicalReference != nullptr;
    }

    bool PhysicalParagliderController::IsHeldByHand(bool a_isLeft) const
    {
        return _higgsGrab.IsHeldByHand(a_isLeft);
    }

    bool PhysicalParagliderController::IsHeldByBothHands() const
    {
        return _higgsGrab.IsHeldByBothHands();
    }

    bool PhysicalParagliderController::IsHeldByEitherHand() const
    {
        return _higgsGrab.IsHeldByEitherHand();
    }

    bool PhysicalParagliderController::IsReadyForFlight() const
    {
        return _higgsGrab.IsReadyForFlight();
    }

    bool PhysicalParagliderController::CaptureCalibration(RE::PlayerCharacter& a_player) const
    {
        if (!_physicalReference || !_higgsGrab.CanCaptureCalibration()) {
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
        _higgsGrab.LogGrabTransforms();
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

    void PhysicalParagliderController::EnableCalibrationGrabbing()
    {
        _higgsGrab.EnableCalibrationGrabbing();
    }

    void PhysicalParagliderController::SpawnPhysicalReference(RE::PlayerCharacter& a_player)
    {
        auto* physicalBase = _equipment.GetPhysicalBase();
        if (_physicalReference || !physicalBase || !_higgsGrab.IsAvailable()) {
            return;
        }
        _higgsGrab.BlockHandsForSpawn();
        const auto placementStarted = std::chrono::steady_clock::now();
        auto reference = a_player.PlaceObjectAtMe(physicalBase, false);
        const auto placementElapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - placementStarted).count();
        logger::info("Physical carrier PlaceObjectAtMe completed elapsedMs={:.1f}", placementElapsed);
        if (!reference) {
            logger::warn("Failed to spawn physical paraglider carrier");
            _higgsGrab.Cancel();
            return;
        }
        reference->SetTemporary();
        reference->SetActivationBlocked(true);
        GliderSpawnFrame spawnFrame{};
        if (!ResolveGliderSpawnFrame(a_player, spawnFrame) || !ApplyGliderSpawnFrame(*reference, spawnFrame)) {
            logger::warn("Failed to resolve stable physical paraglider spawn frame");
            reference->Disable();
            reference->SetDelete(true);
            _higgsGrab.Cancel();
            return;
        }
        _physicalReference = reference;
        _higgsGrab.SetReference(reference.get());
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
            GetHmdForwardAlignment(spawnFrame),
            spawnAngles.x,
            spawnAngles.y,
            spawnAngles.z);
    }

    void PhysicalParagliderController::DestroyPhysicalReference()
    {
        if (_physicalReference) {
            logger::info("Removing physical paraglider ref={:08X}", _physicalReference->formID);
            _physicalReference->Disable();
            _physicalReference->SetDelete(true);
            _physicalReference.reset();
        }
        _higgsGrab.Cancel();
        _destroyPending = false;
    }

    void PhysicalParagliderController::RequestPhysicalReferenceDestroy()
    {
        if (!_physicalReference) {
            _destroyPending = false;
            _higgsGrab.Cancel();
            return;
        }
        if (_higgsGrab.IsHeldByEitherHand()) {
            if (!_destroyPending) {
                logger::info(
                    "Forcing HIGGS release before physical paraglider removal ref={:08X}",
                    _physicalReference->formID);
            }
            _destroyPending = true;
            _higgsGrab.ForceReleaseReference();
            return;
        }
        DestroyPhysicalReference();
    }

    void PhysicalParagliderController::Deploy(RE::PlayerCharacter& a_player)
    {
        _equipment.Refresh(a_player, 0.0f);
        if (!IsEquipped()) {
            return;
        }
        SpawnPhysicalReference(a_player);
    }

    void PhysicalParagliderController::Retract()
    {
        RequestPhysicalReferenceDestroy();
    }

    void PhysicalParagliderController::Update(
        RE::PlayerCharacter& a_player,
        float a_delta,
        const ParagliderInputState& a_input)
    {
        if (!_enabled) {
            return;
        }
        if (_destroyPending) {
            if (_higgsGrab.UpdateForcedRelease()) {
                logger::info("HIGGS force-release completed; removing physical paraglider");
                DestroyPhysicalReference();
            }
            return;
        }
        _equipment.Refresh(a_player, a_delta);
        if (!_equipment.IsAvailable()) {
            return;
        }
        const bool equipped = IsEquipped();
        if (!equipped) {
            if (_physicalReference) {
                DestroyPhysicalReference();
            }
            return;
        }
        if (_physicalReference) {
            if (Config::GetSingleton().Get().calibrationMode) {
                if (!_higgsGrab.IsAnchorReleased()) {
                    GliderSpawnFrame stableFrame{};
                    if (ResolveGliderSpawnFrame(a_player, stableFrame)) {
                        if (!ApplyGliderSpawnFrame(*_physicalReference, stableFrame)) {
                            logger::warn("Failed to maintain calibration spawn frame");
                        }
                    }
                    _higgsGrab.ReleaseCalibrationAnchorIfHeld();
                }
                return;
            }
            if (!_higgsGrab.IsAnchorReleased()) {
                GliderSpawnFrame stableFrame{};
                if (ResolveGliderSpawnFrame(a_player, stableFrame)) {
                    if (!ApplyGliderSpawnFrame(*_physicalReference, stableFrame)) {
                        logger::warn("Failed to maintain physical spawn anchor frame");
                    }
                }
            }
            _higgsGrab.UpdateAlignment(
                a_delta,
                a_input.hands[0].gripDown,
                a_input.hands[1].gripDown);
        }
    }

    void PhysicalParagliderController::Reset()
    {
        if (_physicalReference && _higgsGrab.IsHeldByEitherHand()) {
            logger::warn(
                "Reset abandoned held physical paraglider without Disable ref={:08X}",
                _physicalReference->formID);
            _physicalReference->SetDelete(true);
            _physicalReference.reset();
            _destroyPending = false;
            _higgsGrab.Cancel();
            return;
        }
        DestroyPhysicalReference();
    }
}
