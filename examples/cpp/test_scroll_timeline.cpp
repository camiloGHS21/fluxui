// FluxUI Scroll-driven Animations CSS Properties Test
// CSS Scroll-driven Animations Level 1 / Blink parity

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

int test_animation_timeline() {
    std::cout << "[1] animation-timeline" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { animation-timeline: scroll(); }"
               ".b { animation-timeline: view(); }"
               ".c { animation-timeline: --my-timeline; }"
               ".d { animation-timeline: auto; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    auto wc = std::make_shared<Panel>("c");
    auto wd = std::make_shared<Panel>("d");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    Style sc = sheet.resolve("c","","panel",{},nullptr,wc.get());
    Style sd = sheet.resolve("d","","panel",{},nullptr,wd.get());
    CHECK(sa.hasAnimationTimeline); CHECK(sa.animationTimeline[0] == "scroll()");
    CHECK(sb.hasAnimationTimeline); CHECK(sb.animationTimeline[0] == "view()");
    CHECK(sc.hasAnimationTimeline); CHECK(sc.animationTimeline[0] == "--my-timeline");
    CHECK(sd.hasAnimationTimeline); CHECK(sd.animationTimeline[0] == "auto");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_animation_range() {
    std::cout << "[2] animation-range" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { animation-range: entry 0% exit 100%; }"
               ".b { animation-range-start: entry 25%; animation-range-end: exit 75%; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    CHECK(!sa.animationRangeStart.empty());
    CHECK(!sa.animationRangeEnd.empty());
    CHECK(sb.animationRangeStart == "entry 25%");
    CHECK(sb.animationRangeEnd == "exit 75%");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_scroll_timeline() {
    std::cout << "[3] scroll-timeline" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { scroll-timeline: --scroller block; }"
               ".b { scroll-timeline-name: --page; scroll-timeline-axis: inline; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    CHECK(sa.hasScrollTimeline);
    CHECK(sa.scrollTimelineName[0] == "--scroller");
    CHECK(sa.scrollTimelineAxis[0] == "block");
    CHECK(sb.hasScrollTimeline);
    CHECK(sb.scrollTimelineName[0] == "--page");
    CHECK(sb.scrollTimelineAxis[0] == "inline");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_view_timeline() {
    std::cout << "[4] view-timeline" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { view-timeline: --reveal inline; }"
               ".b { view-timeline-name: --fade; view-timeline-axis: y; }"
               ".c { view-timeline-inset: auto 20px; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    auto wc = std::make_shared<Panel>("c");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    Style sc = sheet.resolve("c","","panel",{},nullptr,wc.get());
    CHECK(sa.hasViewTimeline);
    CHECK(sa.viewTimelineName[0] == "--reveal");
    CHECK(sa.viewTimelineAxis[0] == "inline");
    CHECK(sb.hasViewTimeline);
    CHECK(sb.viewTimelineName[0] == "--fade");
    CHECK(sb.viewTimelineAxis[0] == "y");
    CHECK(sc.viewTimelineInset == "auto 20px");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_timeline_scope() {
    std::cout << "[5] timeline-scope" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { timeline-scope: --hero, --sidebar; }"
               ".b { timeline-scope: none; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    CHECK(sa.hasTimelineScope);
    CHECK(sa.timelineScope.size() == 2);
    CHECK(sa.timelineScope[0] == "--hero");
    CHECK(sa.timelineScope[1] == "--sidebar");
    CHECK(!sb.hasTimelineScope);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_multiple_timelines() {
    std::cout << "[6] animation-timeline: multiple comma-separated" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { animation-timeline: scroll(), view(), --custom; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.animationTimeline.size() == 3);
    CHECK(s.animationTimeline[0] == "scroll()");
    CHECK(s.animationTimeline[1] == "view()");
    CHECK(s.animationTimeline[2] == "--custom");
    std::cout << "  PASS" << std::endl; return 0;
}

int main() {
    std::cout << "=== FluxUI Scroll-driven Animations Tests ===" << std::endl;
    int rc = 0;
    rc |= test_animation_timeline();
    rc |= test_animation_range();
    rc |= test_scroll_timeline();
    rc |= test_view_timeline();
    rc |= test_timeline_scope();
    rc |= test_multiple_timelines();
    if (rc == 0) std::cout << "\nAll scroll-driven animation tests passed!" << std::endl;
    else std::cerr << "\nSome tests FAILED" << std::endl;
    return rc;
}
