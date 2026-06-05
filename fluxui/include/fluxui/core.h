#pragma once
// ============================================================================
//  FluxUI — core public types (umbrella header)
// ----------------------------------------------------------------------------
//  This header is the stable public entry point for FluxUI's core CSS / paint
//  data model. To keep individual translation units small and navigable, the
//  declarations live in focused sub-headers under detail/. Including
//  <fluxui/core.h> pulls in the full set, exactly as before — the public API
//  is unchanged.
//
//    detail/geometry.h   Vec2/Vec4/Color/Rect/EdgeInsets/BorderRadius/
//                        BoxShadow/Filter*/Border/Gradient + float parsing
//    detail/css_enums.h  Display/Position/flex/grid enums + grid value types
//    detail/css_value.h  TimingFunction/CSSValue/units/calc() math tree
//    detail/transform.h  Transform2D/TransformOperation + 3D transform types
//    detail/style.h      Style + ComputedStyle (the big computed-style struct)
//    detail/events.h     EventType/InputEvent/InputState
//
//  Include order matters: each sub-header also includes its own dependencies,
//  so any single one can be included standalone, but most code should just use
//  <fluxui/core.h>.
// ============================================================================
#include "fluxui/config.h"
#include "fluxui/atomic_string.h"

#include "fluxui/detail/geometry.h"
#include "fluxui/detail/css_enums.h"
#include "fluxui/detail/css_value.h"
#include "fluxui/detail/transform.h"
#include "fluxui/detail/style.h"
#include "fluxui/detail/events.h"
