#include "pch.h"
#include "Config.h"

#include <charconv>
#include <fstream>
#include <string_view>
#include <unordered_map>

namespace ParagliderVR
{
    namespace
    {
        using Values = std::unordered_map<std::string, std::string>;

        std::string_view Trim(std::string_view a_value)
        {
            const auto first = a_value.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos) {
                return {};
            }
            return a_value.substr(first, a_value.find_last_not_of(" \t\r\n") - first + 1);
        }

        std::string Lower(std::string_view a_value)
        {
            std::string result(a_value);
            std::ranges::transform(result, result.begin(), [](unsigned char a_character) {
                return static_cast<char>(std::tolower(a_character));
            });
            return result;
        }

        Values ReadValues(const std::filesystem::path& a_path)
        {
            Values values;
            std::ifstream stream(a_path);
            std::string section;
            std::string line;
            while (std::getline(stream, line)) {
                auto view = Trim(line);
                if (view.empty() || view.front() == ';' || view.front() == '#') {
                    continue;
                }
                if (view.front() == '[' && view.back() == ']') {
                    section = Lower(Trim(view.substr(1, view.size() - 2)));
                    continue;
                }
                const auto separator = view.find('=');
                if (separator == std::string_view::npos) {
                    continue;
                }
                auto key = Lower(Trim(view.substr(0, separator)));
                auto value = std::string(Trim(view.substr(separator + 1)));
                values[section + "." + key] = std::move(value);
            }
            return values;
        }

        const std::string* Find(const Values& a_values, std::string_view a_key)
        {
            const auto found = a_values.find(std::string(a_key));
            return found != a_values.end() ? std::addressof(found->second) : nullptr;
        }

        float ParseFloat(const std::string* a_value, float a_fallback, float a_minimum, float a_maximum)
        {
            if (!a_value) {
                return a_fallback;
            }
            float result = a_fallback;
            const auto view = Trim(*a_value);
            const auto parsed = std::from_chars(view.data(), view.data() + view.size(), result);
            if (parsed.ec != std::errc{} || parsed.ptr != view.data() + view.size() || !std::isfinite(result)) {
                return a_fallback;
            }
            return std::clamp(result, a_minimum, a_maximum);
        }

        bool ParseBool(const std::string* a_value, bool a_fallback)
        {
            if (!a_value) {
                return a_fallback;
            }
            const auto value = Lower(Trim(*a_value));
            if (value == "true" || value == "1" || value == "yes" || value == "on") {
                return true;
            }
            if (value == "false" || value == "0" || value == "no" || value == "off") {
                return false;
            }
            return a_fallback;
        }
    }

    Config& Config::GetSingleton()
    {
        static Config singleton;
        return singleton;
    }

    void Config::Load()
    {
        const auto path = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "ParagliderVR.ini";
        const auto values = ReadValues(path);
        Settings loaded{};
        loaded.calibrationMode = ParseBool(Find(values, "calibration.benabled"), loaded.calibrationMode);
        loaded.handsAboveHead = ParseFloat(Find(values, "activation.fhandsabovehead"), loaded.handsAboveHead, 0.0f, 100.0f);
        loaded.handsBelowHeadLimit = ParseFloat(Find(values, "activation.fhandsbelowheadlimit"), loaded.handsBelowHeadLimit, 0.0f, 100.0f);
        loaded.dualMinimumFallMultiplier = ParseFloat(Find(values, "flight.fdualminimumfallmultiplier"), loaded.dualMinimumFallMultiplier, 0.01f, 1.0f);
        loaded.dualMaximumFallMultiplier = ParseFloat(Find(values, "flight.fdualmaximumfallmultiplier"), loaded.dualMaximumFallMultiplier, loaded.dualMinimumFallMultiplier, 1.0f);
        loaded.singleFallMultiplier = ParseFloat(Find(values, "flight.fsinglefallmultiplier"), loaded.singleFallMultiplier, 0.01f, 1.0f);
        loaded.referenceFallSpeed = ParseFloat(Find(values, "flight.freferencefallspeed"), loaded.referenceFallSpeed, 50.0f, 2000.0f);
        loaded.maximumClimbSpeed = ParseFloat(Find(values, "flight.fmaximumclimbspeed"), loaded.maximumClimbSpeed, 0.0f, 1000.0f);
        loaded.steeringSpeed = ParseFloat(Find(values, "flight.fsteeringspeed"), loaded.steeringSpeed, 0.0f, 2000.0f);
        loaded.minimumForwardSpeed = ParseFloat(Find(values, "flight.fminimumforwardspeed"), loaded.minimumForwardSpeed, 0.0f, loaded.steeringSpeed);
        loaded.horizontalAcceleration = ParseFloat(Find(values, "flight.fhorizontalacceleration"), loaded.horizontalAcceleration, 1.0f, 2000.0f);
        loaded.horizontalDeceleration = ParseFloat(Find(values, "flight.fhorizontaldeceleration"), loaded.horizontalDeceleration, 1.0f, 2000.0f);
        loaded.verticalTransitionAcceleration = ParseFloat(Find(values, "flight.fverticaltransitionacceleration"), loaded.verticalTransitionAcceleration, 1.0f, 2000.0f);
        loaded.thumbstickDeadzone = ParseFloat(Find(values, "flight.fthumbstickdeadzone"), loaded.thumbstickDeadzone, 0.0f, 0.95f);
        loaded.lateralSpeedScale = ParseFloat(Find(values, "flight.flateralspeedscale"), loaded.lateralSpeedScale, 0.0f, 2.0f);
        loaded.gestureControlEnabled = ParseBool(Find(values, "gesturecontrol.benabled"), loaded.gestureControlEnabled);
        loaded.gesturePositionTolerance = ParseFloat(Find(values, "gesturecontrol.fpositiontolerance"), loaded.gesturePositionTolerance, 1.0f, 100.0f);
        loaded.gestureRotationToleranceDegrees = ParseFloat(Find(values, "gesturecontrol.frotationtolerancedegrees"), loaded.gestureRotationToleranceDegrees, 1.0f, 180.0f);
        loaded.gestureMinimumConfidence = ParseFloat(Find(values, "gesturecontrol.fminimumconfidence"), loaded.gestureMinimumConfidence, 0.0f, 0.95f);
        loaded.gestureTransitionSpeed = ParseFloat(Find(values, "gesturecontrol.ftransitionspeed"), loaded.gestureTransitionSpeed, 0.1f, 30.0f);
        loaded.staminaPerSecond = ParseFloat(Find(values, "stamina.fstaminapersecond"), loaded.staminaPerSecond, 0.0f, 1000.0f);
        loaded.exhaustedHorizontalSpeedScale = ParseFloat(Find(values, "stamina.fexhaustedhorizontalspeedscale"), loaded.exhaustedHorizontalSpeedScale, 0.0f, 1.0f);
        loaded.exhaustedFallMultiplier = ParseFloat(Find(values, "stamina.fexhaustedfallmultiplier"), loaded.exhaustedFallMultiplier, loaded.dualMaximumFallMultiplier, 2.0f);
        loaded.fireLiftEnabled = ParseBool(Find(values, "firelift.benabled"), loaded.fireLiftEnabled);
        loaded.fireLiftHorizontalRadius = ParseFloat(Find(values, "firelift.fhorizontalradius"), loaded.fireLiftHorizontalRadius, 10.0f, 1000.0f);
        loaded.fireLiftMaximumHeight = ParseFloat(Find(values, "firelift.fmaximumheight"), loaded.fireLiftMaximumHeight, 10.0f, 3000.0f);
        loaded.fireLiftVerticalImpulse = ParseFloat(Find(values, "firelift.fverticalimpulse"), loaded.fireLiftVerticalImpulse, 0.0f, 10000.0f);
        loaded.fireLiftImpulseDuration = ParseFloat(Find(values, "firelift.fimpulseduration"), loaded.fireLiftImpulseDuration, 0.05f, 5.0f);
        loaded.fireLiftScanInterval = ParseFloat(Find(values, "firelift.fscaninterval"), loaded.fireLiftScanInterval, 0.02f, 1.0f);
        loaded.visualHeight = ParseFloat(Find(values, "visual.fvisualheight"), loaded.visualHeight, -500.0f, 500.0f);
        loaded.visualForward = ParseFloat(Find(values, "visual.fvisualforward"), loaded.visualForward, -500.0f, 500.0f);
        loaded.visualScale = ParseFloat(Find(values, "visual.fvisualscale"), loaded.visualScale, 0.01f, 50.0f);
        loaded.visualRotationDegrees.x = ParseFloat(Find(values, "visual.fvisualrotationx"), loaded.visualRotationDegrees.x, -360.0f, 360.0f);
        loaded.visualRotationDegrees.y = ParseFloat(Find(values, "visual.fvisualrotationy"), loaded.visualRotationDegrees.y, -360.0f, 360.0f);
        loaded.visualRotationDegrees.z = ParseFloat(Find(values, "visual.fvisualrotationz"), loaded.visualRotationDegrees.z, -360.0f, 360.0f);
        if (const auto* modelPath = Find(values, "visual.smodelpath"); modelPath && !Trim(*modelPath).empty()) {
            loaded.modelPath = std::string(Trim(*modelPath));
        }
        loaded.windVisualEnabled = ParseBool(Find(values, "windvisual.benabled"), loaded.windVisualEnabled);
        loaded.windVisualHeight = ParseFloat(Find(values, "windvisual.fheight"), loaded.windVisualHeight, -500.0f, 500.0f);
        loaded.windVisualForward = ParseFloat(Find(values, "windvisual.fforward"), loaded.windVisualForward, -500.0f, 500.0f);
        loaded.windVisualScale = ParseFloat(Find(values, "windvisual.fscale"), loaded.windVisualScale, 0.01f, 20.0f);
        loaded.windVisualRotationDegrees.x = ParseFloat(Find(values, "windvisual.frotationx"), loaded.windVisualRotationDegrees.x, -360.0f, 360.0f);
        loaded.windVisualRotationDegrees.y = ParseFloat(Find(values, "windvisual.frotationy"), loaded.windVisualRotationDegrees.y, -360.0f, 360.0f);
        loaded.windVisualRotationDegrees.z = ParseFloat(Find(values, "windvisual.frotationz"), loaded.windVisualRotationDegrees.z, -360.0f, 360.0f);
        if (const auto* windModelPath = Find(values, "windvisual.smodelpath"); windModelPath && !Trim(*windModelPath).empty()) {
            loaded.windVisualModelPath = std::string(Trim(*windModelPath));
        }
        _settings = std::move(loaded);
        logger::info("Calibration mode enabled={}", _settings.calibrationMode);
        logger::info(
            "Activation height config handsAboveHead={:.1f} handsBelowHeadLimit={:.1f}",
            _settings.handsAboveHead,
            _settings.handsBelowHeadLimit);
        logger::info(
            "Config loaded path='{}' dualFall={:.2f}-{:.2f} singleFall={:.2f} referenceFall={:.1f} climb={:.1f} forward={:.1f}-{:.1f} acceleration={:.1f}/{:.1f}/{:.1f} thumbDeadzone={:.2f} lateralScale={:.2f} stamina={:.1f}/s exhausted={:.2f}x horizontal/{:.2f}x fall model='{}' rotation=({:.1f},{:.1f},{:.1f})",
            path.string(),
            _settings.dualMinimumFallMultiplier,
            _settings.dualMaximumFallMultiplier,
            _settings.singleFallMultiplier,
            _settings.referenceFallSpeed,
            _settings.maximumClimbSpeed,
            _settings.minimumForwardSpeed,
            _settings.steeringSpeed,
            _settings.horizontalAcceleration,
            _settings.horizontalDeceleration,
            _settings.verticalTransitionAcceleration,
            _settings.thumbstickDeadzone,
            _settings.lateralSpeedScale,
            _settings.staminaPerSecond,
            _settings.exhaustedHorizontalSpeedScale,
            _settings.exhaustedFallMultiplier,
            _settings.modelPath,
            _settings.visualRotationDegrees.x,
            _settings.visualRotationDegrees.y,
            _settings.visualRotationDegrees.z);
        logger::info(
            "Fire lift config enabled={} horizontalRadius={:.1f} maximumHeight={:.1f} verticalImpulse={:.1f} impulseDuration={:.2f} scanInterval={:.2f}",
            _settings.fireLiftEnabled,
            _settings.fireLiftHorizontalRadius,
            _settings.fireLiftMaximumHeight,
            _settings.fireLiftVerticalImpulse,
            _settings.fireLiftImpulseDuration,
            _settings.fireLiftScanInterval);
        logger::info(
            "Gesture control enabled={} positionTolerance={:.1f} rotationTolerance={:.1f} minimumConfidence={:.2f} transitionSpeed={:.1f}",
            _settings.gestureControlEnabled,
            _settings.gesturePositionTolerance,
            _settings.gestureRotationToleranceDegrees,
            _settings.gestureMinimumConfidence,
            _settings.gestureTransitionSpeed);
        logger::info(
            "Wind visual config enabled={} model='{}' height={:.1f} forward={:.1f} scale={:.2f} rotation=({:.1f},{:.1f},{:.1f})",
            _settings.windVisualEnabled,
            _settings.windVisualModelPath,
            _settings.windVisualHeight,
            _settings.windVisualForward,
            _settings.windVisualScale,
            _settings.windVisualRotationDegrees.x,
            _settings.windVisualRotationDegrees.y,
            _settings.windVisualRotationDegrees.z);
    }

    const Settings& Config::Get() const noexcept
    {
        return _settings;
    }
}
