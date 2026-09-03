#include "pch.h"
#include "VrikIntegration.h"

namespace ParagliderVR::VrikIntegration
{
    namespace
    {
        struct Interface001
        {
            virtual std::uint32_t getBuildNumber() = 0;
            virtual double getSettingDouble(const char* a_name) = 0;
            virtual void setSettingDouble(const char* a_name, double a_value) = 0;
        };

        struct InterfaceRequest
        {
            static constexpr std::uint32_t kMessageType = 0xF2AFAEE6;
            void* (*getApiFunction)(unsigned int a_revisionNumber){ nullptr };
        };

        Interface001* g_api = nullptr;
        bool g_overrideActive = false;
        double g_previousPosture = 1.0;
        double g_previousBody = 1.0;

        bool ResolveApi()
        {
            if (g_api) {
                return true;
            }
            InterfaceRequest request;
            auto* messaging = SKSE::GetMessagingInterface();
            if (!messaging ||
                !messaging->Dispatch(
                    InterfaceRequest::kMessageType,
                    &request,
                    sizeof(request),
                    "VRIK") ||
                !request.getApiFunction) {
                return false;
            }
            g_api = static_cast<Interface001*>(request.getApiFunction(1));
            if (g_api) {
                logger::info("VRIK interface ready build={}", g_api->getBuildNumber());
            }
            return g_api != nullptr;
        }
    }

    void SetLowerBodyOverride(bool a_enabled)
    {
        if (a_enabled) {
            if (g_overrideActive || !ResolveApi()) {
                return;
            }
            g_previousPosture = g_api->getSettingDouble("enablePosture");
            g_previousBody = g_api->getSettingDouble("enableBody");
            g_api->setSettingDouble("enablePosture", 0.0);
            g_api->setSettingDouble("enableBody", 0.0);
            g_overrideActive = true;
            logger::info(
                "VRIK lower-body override enabled previousPosture={} previousBody={}",
                g_previousPosture,
                g_previousBody);
            return;
        }
        if (!g_overrideActive || !g_api) {
            return;
        }
        g_api->setSettingDouble("enablePosture", g_previousPosture);
        g_api->setSettingDouble("enableBody", g_previousBody);
        g_overrideActive = false;
        logger::info("VRIK lower-body override restored");
    }
}
