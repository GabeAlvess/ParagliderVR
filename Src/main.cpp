#include "pch.h"
#include "AnimationOar.h"
#include "Config.h"
#include "Hooks.h"
#include "ParagliderController.h"
#include "PhysicalParagliderController.h"

namespace
{
    bool g_installed = false;

    void SetupLog()
    {
        auto logsFolder = SKSE::log::log_directory();
        if (!logsFolder) {
            SKSE::stl::report_and_fail("SKSE log_directory not provided.");
        }
        auto logPath = *logsFolder / std::filesystem::path(Plugin::NAME.data()).replace_extension(".log");
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
        auto loggerInstance = std::make_shared<spdlog::logger>("global log", std::move(sink));
        spdlog::set_default_logger(std::move(loggerInstance));
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
    }

    void ActivateGameplay()
    {
        ParagliderVR::Config::GetSingleton().Load();
        auto& controller = ParagliderVR::ParagliderController::GetSingleton();
        auto& physical = ParagliderVR::PhysicalParagliderController::GetSingleton();
        if (!g_installed) {
            SKSE::AllocTrampoline(32);
            ParagliderVR::Hooks::Install();
            controller.InstallInput();
            g_installed = true;
        }
        controller.SetEnabled(true);
        physical.SetEnabled(true);
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_message)
    {
        switch (a_message->type) {
        case SKSE::MessagingInterface::kPostLoad:
            ParagliderVR::AnimationOar::RegisterCondition();
            ParagliderVR::PhysicalParagliderController::GetSingleton().InitializeHiggs();
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
            ParagliderVR::ParagliderController::GetSingleton().SetEnabled(false);
            ParagliderVR::PhysicalParagliderController::GetSingleton().SetEnabled(false);
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            ActivateGameplay();
            logger::info("{} gameplay activated", Plugin::NAME);
            break;
        default:
            break;
        }
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
    SetupLog();
    logger::info("{} v{} loading", Plugin::NAME, Plugin::VERSION.string());
    SKSE::Init(a_skse);
    auto* messaging = reinterpret_cast<SKSE::MessagingInterface*>(
        a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
    if (!messaging || !messaging->RegisterListener("SKSE", MessageHandler)) {
        logger::critical("{}: failed to register SKSE listener", Plugin::NAME);
        return false;
    }
    logger::info("{} loaded successfully", Plugin::NAME);
    return true;
}
