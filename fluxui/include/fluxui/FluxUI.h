// FluxUI - GPU-Accelerated UI Framework with CSS Styling
// Single include header
#pragma once

#if defined(_WIN32) && !defined(_USRDLL) && !defined(_WINDLL)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/renderer.h"
#include "fluxui/widgets.h"
