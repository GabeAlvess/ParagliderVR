#pragma once

#include <SKSE/SKSE.h>

namespace HiggsPluginAPI
{
    struct IHiggsInterface001;
    IHiggsInterface001* GetHiggsInterface001(
        SKSE::PluginHandle a_pluginHandle,
        const SKSE::MessagingInterface* a_messagingInterface);

    struct IHiggsInterface001
    {
        virtual unsigned int GetBuildNumber() = 0;

        using PulledCallback = void (*)(bool, RE::TESObjectREFR*);
        virtual void AddPulledCallback(PulledCallback a_callback) = 0;
        using GrabbedCallback = void (*)(bool, RE::TESObjectREFR*);
        virtual void AddGrabbedCallback(GrabbedCallback a_callback) = 0;
        using DroppedCallback = void (*)(bool, RE::TESObjectREFR*);
        virtual void AddDroppedCallback(DroppedCallback a_callback) = 0;
        using StashedCallback = void (*)(bool, RE::TESForm*);
        virtual void AddStashedCallback(StashedCallback a_callback) = 0;
        using ConsumedCallback = void (*)(bool, RE::TESForm*);
        virtual void AddConsumedCallback(ConsumedCallback a_callback) = 0;
        using CollisionCallback = void (*)(bool, float, float);
        virtual void AddCollisionCallback(CollisionCallback a_callback) = 0;

        virtual void GrabObject(RE::TESObjectREFR* a_object, bool a_isLeft) = 0;
        virtual RE::TESObjectREFR* GetGrabbedObject(bool a_isLeft) = 0;
        virtual bool IsHandInGrabbableState(bool a_isLeft) = 0;
        virtual void DisableHand(bool a_isLeft) = 0;
        virtual void EnableHand(bool a_isLeft) = 0;
        virtual bool IsDisabled(bool a_isLeft) = 0;
        virtual void DisableWeaponCollision(bool a_isLeft) = 0;
        virtual void EnableWeaponCollision(bool a_isLeft) = 0;
        virtual bool IsWeaponCollisionDisabled(bool a_isLeft) = 0;
        virtual bool IsTwoHanding() = 0;

        using StartTwoHandingCallback = void (*)();
        virtual void AddStartTwoHandingCallback(StartTwoHandingCallback a_callback) = 0;
        using StopTwoHandingCallback = void (*)();
        virtual void AddStopTwoHandingCallback(StopTwoHandingCallback a_callback) = 0;
        virtual bool CanGrabObject(bool a_isLeft) = 0;

        enum class CollisionFilterComparisonResult : std::uint8_t
        {
            kContinue,
            kCollide,
            kIgnore
        };
        using CollisionFilterComparisonCallback = CollisionFilterComparisonResult (*)(void*, std::uint32_t, std::uint32_t);
        virtual void AddCollisionFilterComparisonCallback(CollisionFilterComparisonCallback a_callback) = 0;
        using PrePhysicsStepCallback = void (*)(void*);
        virtual void AddPrePhysicsStepCallback(PrePhysicsStepCallback a_callback) = 0;
        virtual std::uint64_t GetHiggsLayerBitfield() = 0;
        virtual void SetHiggsLayerBitfield(std::uint64_t a_bitfield) = 0;
        virtual RE::NiObject* GetHandRigidBody(bool a_isLeft) = 0;
        virtual RE::NiObject* GetWeaponRigidBody(bool a_isLeft) = 0;
        virtual RE::NiObject* GetGrabbedRigidBody(bool a_isLeft) = 0;
        virtual void ForceWeaponCollisionEnabled(bool a_isLeft) = 0;
        virtual bool IsHoldingObject(bool a_isLeft) = 0;
        virtual void GetFingerValues(bool a_isLeft, float a_values[5]) = 0;

        using NoArgCallback = void (*)();
        virtual void AddPreVrikPreHiggsCallback(NoArgCallback a_callback) = 0;
        virtual void AddPreVrikPostHiggsCallback(NoArgCallback a_callback) = 0;
        virtual void AddPostVrikPreHiggsCallback(NoArgCallback a_callback) = 0;
        virtual void AddPostVrikPostHiggsCallback(NoArgCallback a_callback) = 0;
        virtual bool Deprecated1(const std::string_view& a_name, double& a_out) = 0;
        virtual bool Deprecated2(const std::string& a_name, double a_value) = 0;
        virtual RE::NiTransform GetGrabTransform(bool a_isLeft) = 0;
        virtual void SetGrabTransform(bool a_isLeft, const RE::NiTransform& a_transform) = 0;
        virtual bool GetSettingDouble(const char* a_name, double& a_out) = 0;
        virtual bool SetSettingDouble(const char* a_name, double a_value) = 0;
        virtual RE::BSFixedString GetGrabbedNodeName(bool a_isLeft) = 0;
    };
}

extern HiggsPluginAPI::IHiggsInterface001* g_higgsInterface;
