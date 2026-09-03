#include "pch.h"
#include "AnimationOar.h"

#include "oar_api/OpenAnimationReplacerAPI-Conditions.h"

#include <atomic>

namespace
{
    std::atomic_bool g_paragliderActive = false;
    std::atomic<ParagliderVR::AnimationOar::Direction> g_paragliderDirection =
        ParagliderVR::AnimationOar::Direction::kIdle;
    std::atomic_uint32_t g_conditionLogMask = 0;

    void LogConditionOnce(std::uint32_t a_bit, std::string_view a_name)
    {
        const auto previous = g_conditionLogMask.fetch_or(a_bit, std::memory_order_relaxed);
        if ((previous & a_bit) == 0) {
            logger::info("OAR condition evaluated true name={}", a_name);
        }
    }

    bool IsActiveForPlayer(RE::TESObjectREFR* a_refr)
    {
        return a_refr == RE::PlayerCharacter::GetSingleton() &&
               g_paragliderActive.load(std::memory_order_acquire);
    }

    class IsParagliderActiveCondition final : public Conditions::CustomCondition
    {
    public:
        inline static constexpr std::string_view CONDITION_NAME = "ParagliderVRIsActive";

        [[nodiscard]] RE::BSString GetName() const override { return CONDITION_NAME.data(); }
        [[nodiscard]] RE::BSString GetDescription() const override
        {
            return "Checks whether the player is currently using the ParagliderVR glider.";
        }
        [[nodiscard]] REL::Version GetRequiredVersion() const override { return Plugin::VERSION; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*) const override
        {
            const bool result = IsActiveForPlayer(a_refr);
            if (result) {
                LogConditionOnce(1u, CONDITION_NAME);
            }
            return result;
        }
    };

    template <ParagliderVR::AnimationOar::Direction DirectionValue>
    class IsParagliderDirectionCondition final : public Conditions::CustomCondition
    {
    public:
        inline static constexpr std::string_view CONDITION_NAME = []() constexpr {
            if constexpr (DirectionValue == ParagliderVR::AnimationOar::Direction::kForward) {
                return std::string_view{ "ParagliderVRIsForward" };
            } else if constexpr (DirectionValue == ParagliderVR::AnimationOar::Direction::kBackward) {
                return std::string_view{ "ParagliderVRIsBackward" };
            } else if constexpr (DirectionValue == ParagliderVR::AnimationOar::Direction::kLeft) {
                return std::string_view{ "ParagliderVRIsLeft" };
            } else {
                return std::string_view{ "ParagliderVRIsRight" };
            }
        }();

        [[nodiscard]] RE::BSString GetName() const override
        {
            return CONDITION_NAME.data();
        }
        [[nodiscard]] RE::BSString GetDescription() const override
        {
            return "Checks the current ParagliderVR lower-body steering direction.";
        }
        [[nodiscard]] REL::Version GetRequiredVersion() const override { return Plugin::VERSION; }

    protected:
        bool EvaluateImpl(RE::TESObjectREFR* a_refr, RE::hkbClipGenerator*, void*) const override
        {
            const bool result = IsActiveForPlayer(a_refr) &&
                                g_paragliderDirection.load(std::memory_order_relaxed) == DirectionValue;
            if (result) {
                constexpr std::uint32_t bit =
                    DirectionValue == ParagliderVR::AnimationOar::Direction::kForward ? 1u << 1 :
                    DirectionValue == ParagliderVR::AnimationOar::Direction::kBackward ? 1u << 2 :
                    DirectionValue == ParagliderVR::AnimationOar::Direction::kLeft ? 1u << 3 : 1u << 4;
                LogConditionOnce(bit, CONDITION_NAME);
            }
            return result;
        }
    };
}

namespace ParagliderVR::AnimationOar
{
    void RegisterCondition()
    {
        g_oarConditionsInterface = OAR_API::Conditions::GetAPI(OAR_API::Conditions::InterfaceVersion::V2);
        if (!g_oarConditionsInterface) {
            logger::warn("OAR conditions API unavailable; lower-body animation disabled");
            return;
        }
        const auto activeResult = OAR_API::Conditions::AddCustomCondition<IsParagliderActiveCondition>();
        const auto forwardResult = OAR_API::Conditions::AddCustomCondition<
            IsParagliderDirectionCondition<Direction::kForward>>();
        const auto backwardResult = OAR_API::Conditions::AddCustomCondition<
            IsParagliderDirectionCondition<Direction::kBackward>>();
        const auto leftResult = OAR_API::Conditions::AddCustomCondition<
            IsParagliderDirectionCondition<Direction::kLeft>>();
        const auto rightResult = OAR_API::Conditions::AddCustomCondition<
            IsParagliderDirectionCondition<Direction::kRight>>();
        logger::info(
            "OAR condition registration active={} forward={} backward={} left={} right={}",
            static_cast<int>(activeResult),
            static_cast<int>(forwardResult),
            static_cast<int>(backwardResult),
            static_cast<int>(leftResult),
            static_cast<int>(rightResult));
    }

    void SetActive(bool a_active)
    {
        if (!a_active) {
            g_paragliderDirection.store(Direction::kIdle, std::memory_order_relaxed);
        }
        g_conditionLogMask.store(0, std::memory_order_relaxed);
        g_paragliderActive.store(a_active, std::memory_order_release);
    }

    void SetDirection(Direction a_direction)
    {
        g_paragliderDirection.store(a_direction, std::memory_order_relaxed);
    }
}
