// FluxUI - GPU-Accelerated UI Framework with CSS Styling
// ┌──────────────────────────────────────────────────────────────────────────┐
// │  INTERNAL HEADER — Do NOT include this directly in user applications.    │
// │  Use #include <fluxui/dsl.h> for the public declarative API.             │
// │  This header exposes the engine internals used by the DSL, bindings,     │
// │  and the test suite.                                                      │
// └──────────────────────────────────────────────────────────────────────────┘
#pragma once

#ifndef FLUXUI_INTERNAL
#if defined(__GNUC__) || defined(__clang__)
#pragma message("WARNING: <fluxui/FluxUI.h> is an internal header. Use <fluxui/dsl.h> instead.")
#elif defined(_MSC_VER)
// Uncomment the line below to enable the warning during user builds:
// #pragma message("WARNING: <fluxui/FluxUI.h> is an internal header. Use <fluxui/dsl.h> instead.")
#endif
#endif

#if defined(_WIN32) && !defined(_USRDLL) && !defined(_WINDLL)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/renderer.h"
#include "fluxui/widgets.h"
