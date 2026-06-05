#pragma once
// ============================================================================
//  FluxUI — widget tree & application (umbrella header)
// ----------------------------------------------------------------------------
//  Stable public entry point for the widget object model. The declarations are
//  split into focused sub-headers under detail/ to keep them navigable, but the
//  public API is unchanged: just include <fluxui/widgets.h>.
//
//    detail/widget_base.h      Widget base class, DOM Event, lifecycle,
//                              PaintProperties, arena allocator, forward decls
//    detail/widget_elements.h  Concrete widgets (Panel/Text/Button/Image/Svg/…)
//                              plus the inline Widget factory helpers
//    detail/application.h      UIEvent, ResizeObserver, ViewTransition,
//                              Application
//
//  Include order matters and is encoded by each sub-header including its own
//  dependency, so most code should just include this umbrella header.
// ============================================================================
#include "fluxui/detail/widget_base.h"
#include "fluxui/detail/widget_elements.h"
#include "fluxui/detail/application.h"
