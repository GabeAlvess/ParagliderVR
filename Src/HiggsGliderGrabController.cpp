#include "pch.h"
#include "Config.h"
#include "GliderPose.h"
#include "HiggsGliderGrabController.h"
#include "higgsinterface001.h"

namespace ParagliderVR
{
    namespace
    {
        constexpr float kGrabRetrySeconds = 0.15f;
        constexpr float kSpawnSettleSeconds = 0.10f;

    }

    void HiggsGliderGrabController::Initialize()
    {
        activeInstance_ = this;
        _higgsInterface = HiggsPluginAPI::GetHiggsInterface001(
            SKSE::GetPluginHandle(),
            SKSE::GetMessagingInterface());
        if (_higgsInterface) {
            if (!_higgsGrabbedCallbackRegistered) {
                _higgsInterface->AddGrabbedCallback(OnHiggsGrabbed);
                _higgsGrabbedCallbackRegistered = true;
            }
            logger::info("HIGGS interface ready build={}", _higgsInterface->GetBuildNumber());
        } else {
            logger::warn("HIGGS interface unavailable; physical two-hand spawning is disabled");
        }
    }

    bool HiggsGliderGrabController::IsAvailable() const
    {
        return _higgsInterface != nullptr;
    }

    void HiggsGliderGrabController::BlockHandsForSpawn()
    {
        if (!_higgsInterface || _higgsHandsBlockedForSpawn) {
            return;
        }
        _restoreLeftHiggsHand = !_higgsInterface->IsDisabled(true);
        _restoreRightHiggsHand = !_higgsInterface->IsDisabled(false);
        if (_restoreLeftHiggsHand) {
            _higgsInterface->DisableHand(true);
        }
        if (_restoreRightHiggsHand) {
            _higgsInterface->DisableHand(false);
        }
        _higgsHandsBlockedForSpawn = true;
        logger::info("HIGGS hands blocked before physical paraglider creation");
    }

    void HiggsGliderGrabController::RestoreHandsAfterSpawn()
    {
        if (!_higgsInterface || !_higgsHandsBlockedForSpawn) {
            return;
        }
        if (_restoreLeftHiggsHand) {
            _higgsInterface->EnableHand(true);
        }
        if (_restoreRightHiggsHand) {
            _higgsInterface->EnableHand(false);
        }
        _higgsHandsBlockedForSpawn = false;
        _restoreLeftHiggsHand = false;
        _restoreRightHiggsHand = false;
        logger::info("HIGGS hands restored after physical paraglider stabilization");
    }

    void HiggsGliderGrabController::SetReference(RE::TESObjectREFR* a_reference)
    {
        _reference = a_reference;
        _forcedReleasePending = false;
        _restoreLeftAfterForcedRelease = false;
        _restoreRightAfterForcedRelease = false;
        _initialGrabComplete = false;
        _wasHeldByBothHands = false;
        _grabPhase = a_reference ? GrabPhase::kSettling : GrabPhase::kNone;
        _grabRetryTimer = 0.0f;
    }

    void HiggsGliderGrabController::Cancel()
    {
        RestoreHandsAfterSpawn();
        if (_higgsInterface) {
            if (_restoreLeftAfterForcedRelease) {
                _higgsInterface->EnableHand(true);
            }
            if (_restoreRightAfterForcedRelease) {
                _higgsInterface->EnableHand(false);
            }
        }
        _reference = nullptr;
        _forcedReleasePending = false;
        _restoreLeftAfterForcedRelease = false;
        _restoreRightAfterForcedRelease = false;
        _initialGrabComplete = false;
        _wasHeldByBothHands = false;
        _grabPhase = GrabPhase::kNone;
        _grabRetryTimer = 0.0f;
    }

    void HiggsGliderGrabController::ForceReleaseReference()
    {
        if (!_higgsInterface || !_reference || _forcedReleasePending) {
            return;
        }
        const bool leftHeld = _higgsInterface->GetGrabbedObject(true) == _reference;
        const bool rightHeld = _higgsInterface->GetGrabbedObject(false) == _reference;
        if (leftHeld && !_higgsInterface->IsDisabled(true)) {
            _higgsInterface->DisableHand(true);
            _restoreLeftAfterForcedRelease = true;
        }
        if (rightHeld && !_higgsInterface->IsDisabled(false)) {
            _higgsInterface->DisableHand(false);
            _restoreRightAfterForcedRelease = true;
        }
        _forcedReleasePending = leftHeld || rightHeld;
        if (_forcedReleasePending) {
            logger::info(
                "Forced HIGGS release requested leftHeld={} rightHeld={}",
                leftHeld,
                rightHeld);
        }
    }

    bool HiggsGliderGrabController::UpdateForcedRelease()
    {
        if (!_forcedReleasePending || !_higgsInterface || !_reference) {
            return true;
        }
        const bool leftHeld = _higgsInterface->GetGrabbedObject(true) == _reference;
        const bool rightHeld = _higgsInterface->GetGrabbedObject(false) == _reference;
        if (leftHeld || rightHeld) {
            return false;
        }
        _forcedReleasePending = false;
        logger::info("Forced HIGGS release completed");
        return true;
    }

    bool HiggsGliderGrabController::IsHeldByHand(bool a_isLeft) const
    {
        return _higgsInterface && _reference &&
            _higgsInterface->GetGrabbedObject(a_isLeft) == _reference;
    }

    bool HiggsGliderGrabController::IsHeldByBothHands() const
    {
        return IsHeldByHand(true) && IsHeldByHand(false);
    }

    bool HiggsGliderGrabController::IsHeldByEitherHand() const
    {
        return IsHeldByHand(true) || IsHeldByHand(false);
    }

    bool HiggsGliderGrabController::IsReadyForFlight() const
    {
        return _initialGrabComplete && IsHeldByBothHands();
    }

    bool HiggsGliderGrabController::IsAnchorReleased() const
    {
        return _initialGrabComplete;
    }

    bool HiggsGliderGrabController::ApplyAuthoredGrabTransform(bool a_isLeft) const
    {
        if (!_higgsInterface || !IsHeldByHand(a_isLeft)) {
            return false;
        }
        _higgsInterface->SetGrabTransform(a_isLeft, CalibratedHandToGlider(a_isLeft));
        return true;
    }

    void HiggsGliderGrabController::OnHiggsGrabbed(bool a_isLeft, RE::TESObjectREFR* a_reference)
    {
        if (activeInstance_) {
            activeInstance_->HandleHiggsGrabbed(a_isLeft, a_reference);
        }
    }

    void HiggsGliderGrabController::HandleHiggsGrabbed(bool a_isLeft, RE::TESObjectREFR* a_reference)
    {
        if (!_higgsInterface || !_reference || a_reference != _reference) {
            return;
        }
        if (Config::GetSingleton().Get().calibrationMode) {
            logger::info(
                "Calibration retained natural HIGGS grab for {} hand without applying a fixed transform",
                a_isLeft ? "left" : "right");
            return;
        }
        _higgsInterface->SetGrabTransform(a_isLeft, CalibratedHandToGlider(a_isLeft));
        logger::info(
            "HIGGS grabbed callback immediately aligned {} hand",
            a_isLeft ? "left" : "right");
    }

    void HiggsGliderGrabController::EnableCalibrationGrabbing()
    {
        if (!Config::GetSingleton().Get().calibrationMode || !_reference) {
            return;
        }
        RestoreHandsAfterSpawn();
        logger::info("Calibration HIGGS grabbing enabled after activation grips were released");
    }

    bool HiggsGliderGrabController::ReleaseCalibrationAnchorIfHeld()
    {
        if (!_reference || _initialGrabComplete || !IsHeldByEitherHand()) {
            return false;
        }
        _initialGrabComplete = true;
        _grabPhase = GrabPhase::kComplete;
        logger::info("Calibration object touched by HIGGS; fixed spawn anchor released for manual adjustment");
        RE::DebugNotification("Paraglider calibration object released");
        return true;
    }

    bool HiggsGliderGrabController::CanCaptureCalibration() const
    {
        return _higgsInterface && _reference && IsHeldByBothHands();
    }

    void HiggsGliderGrabController::LogGrabTransforms() const
    {
        if (!_higgsInterface) {
            return;
        }
        LogCalibrationTransform("RightHandToGlider", _higgsInterface->GetGrabTransform(false));
        LogCalibrationTransform("LeftHandToGlider", _higgsInterface->GetGrabTransform(true));
    }

    void HiggsGliderGrabController::UpdateAlignment(
        float a_delta,
        bool a_leftGripDown,
        bool a_rightGripDown)
    {
        if (!_higgsInterface || !_reference) {
            return;
        }
        if (_initialGrabComplete) {
            const bool leftHeld = IsHeldByHand(true);
            const bool rightHeld = IsHeldByHand(false);
            const bool heldByBothHands = leftHeld && rightHeld;
            if (!heldByBothHands) {
                if (_wasHeldByBothHands) {
                    logger::info("HIGGS two-hand alignment suspended until the released hand grabs the glider again");
                    _grabRetryTimer = kGrabRetrySeconds;
                }
                _wasHeldByBothHands = false;
                _grabRetryTimer += a_delta;
                if (_grabRetryTimer >= kGrabRetrySeconds) {
                    bool requested = false;
                    if (!rightHeld && a_rightGripDown && _higgsInterface->CanGrabObject(false)) {
                        _higgsInterface->GrabObject(_reference, false);
                        requested = true;
                        logger::info("Requested HIGGS right-hand recovery after grip press");
                    }
                    if (!leftHeld && a_leftGripDown && _higgsInterface->CanGrabObject(true)) {
                        _higgsInterface->GrabObject(_reference, true);
                        requested = true;
                        logger::info("Requested HIGGS left-hand recovery after grip press");
                    }
                    if (requested) {
                        _grabRetryTimer = 0.0f;
                    }
                }
                return;
            }
            if (_wasHeldByBothHands) {
                return;
            }
            if (!ApplyAuthoredGrabTransform(false) || !ApplyAuthoredGrabTransform(true)) {
                logger::warn("Failed to restore authored two-hand alignment after HIGGS re-grab");
                return;
            }
            _wasHeldByBothHands = true;
            _grabRetryTimer = 0.0f;
            logger::info("HIGGS two-hand alignment restored after re-grab");
            return;
        }
        _grabRetryTimer += a_delta;
        if (_grabPhase == GrabPhase::kSettling) {
            if (_grabRetryTimer >= kSpawnSettleSeconds) {
                RestoreHandsAfterSpawn();
                _grabPhase = GrabPhase::kReady;
                _grabRetryTimer = kGrabRetrySeconds;
                logger::info("Physical paraglider spawn stabilized; requesting right HIGGS primary hand first");
            }
        }
        if (_grabPhase != GrabPhase::kSettling) {
            RequestInitialTwoHandGrab();
        }
    }

    void HiggsGliderGrabController::RequestInitialTwoHandGrab()
    {
        if (!_higgsInterface || !_reference || _initialGrabComplete) {
            return;
        }
        if (_grabPhase == GrabPhase::kNone || _grabPhase == GrabPhase::kSettling) {
            return;
        }

        if (_grabPhase == GrabPhase::kReady) {
            if (!_higgsInterface->CanGrabObject(false) ||
                !_higgsInterface->CanGrabObject(true)) {
                return;
            }
            _higgsInterface->GrabObject(_reference, false);
            _higgsInterface->GrabObject(_reference, true);
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
                if (!rightHeld && _higgsInterface->CanGrabObject(false)) {
                    _higgsInterface->GrabObject(_reference, false);
                }
                if (!leftHeld && _higgsInterface->CanGrabObject(true)) {
                    _higgsInterface->GrabObject(_reference, true);
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
            _wasHeldByBothHands = true;
            logger::info("Simultaneous two-hand alignment complete; fixed spawn anchor released");
        }
    }
}
