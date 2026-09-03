#include "pch.h"
#include "higgsinterface001.h"

HiggsPluginAPI::IHiggsInterface001* g_higgsInterface = nullptr;

namespace
{
    struct HiggsMessage
    {
        static constexpr std::uint32_t kGetInterface = 0xF9279A57;
        void* (*getApiFunction)(unsigned int) = nullptr;
    };
}

HiggsPluginAPI::IHiggsInterface001* HiggsPluginAPI::GetHiggsInterface001(
    SKSE::PluginHandle,
    const SKSE::MessagingInterface* a_messagingInterface)
{
    if (g_higgsInterface || !a_messagingInterface) {
        return g_higgsInterface;
    }
    HiggsMessage message;
    a_messagingInterface->Dispatch(
        HiggsMessage::kGetInterface,
        &message,
        sizeof(message),
        "HIGGS");
    if (message.getApiFunction) {
        g_higgsInterface = static_cast<IHiggsInterface001*>(message.getApiFunction(1));
    }
    return g_higgsInterface;
}
