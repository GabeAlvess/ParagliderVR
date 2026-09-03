#pragma once

#pragma warning(push)
#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#ifndef NDEBUG
#include <spdlog/sinks/msvc_sink.h>
#endif
#pragma warning(pop)

using namespace std::literals;

namespace logger = SKSE::log;
#define DLLEXPORT __declspec(dllexport)
#include "plugin.h"
