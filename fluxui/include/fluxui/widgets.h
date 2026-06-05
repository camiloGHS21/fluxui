#pragma once
// ============================================================================
//  FluxUI — widget tree & application (umbrella header)
// ----------------------------------------------------------------------------
//  Stable public entry point for the widget object model. The declarations are
//  split into focused sub-headers under detail/ to keep them navigable, but the
//  public API is unchanged: just include <fluxui/widgets.h>.
//
//    detail/widgets/widget_base.h      Widget base class, DOM Event, lifecycle,
//                              PaintProperties, arena allocator, forward decls
//    detail/widgets/widget_elements.h  Concrete widgets (Panel/Text/Button/Image/Svg/…)
//    detail/widgets/widget_factory.h   Inline Widget factory helpers + StatCard/LazyPanel
//    detail/widgets/application.h      UIEvent, ResizeObserver, ViewTransition,
//                              Application
//
//  Include order matters and is encoded by each sub-header including its own
//  dependency, so most code should just include this umbrella header.
// ============================================================================
#include "fluxui/detail/widgets/widget_base.h"
#include "fluxui/detail/widgets/widget_elements.h"
#include "fluxui/detail/widgets/widget_factory.h"
#include "fluxui/detail/widgets/application.h"
