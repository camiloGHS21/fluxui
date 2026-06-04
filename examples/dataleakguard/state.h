// DataLeak Guard — shared reactive state
#pragma once
#include <fluxui/dsl.h>
#include <array>
#include <string>

namespace dlg {

using namespace fluxui;

// --- Global app state (reactive where needed) ---
inline auto& scannerRunning() { static State<bool> s(false); return s; }
inline auto& scanProgress()   { static State<float> s(0.67f); return s; }
inline auto& blockMode()      { static State<bool> s(true); return s; }
inline auto& quarantineMode() { static State<bool> s(true); return s; }
inline auto& endpointIsolation() { static State<bool> s(false); return s; }
inline auto& digestEnabled()  { static State<bool> s(true); return s; }
inline auto& reportScheduler(){ static State<bool> s(true); return s; }
inline auto& cloudSync()      { static State<bool> s(true); return s; }

inline std::array<State<bool>, 5>& ruleEnabled() {
    static std::array<State<bool>, 5> rules = {
        State<bool>(true), State<bool>(true), State<bool>(true),
        State<bool>(false), State<bool>(true)
    };
    return rules;
}

// Static stats (could be made reactive too).
inline const char* filesMonitored = "12,847";
inline const char* threatsBlocked = "47";
inline const char* usbTransfers   = "156";
inline const char* complianceScore= "94%";

} // namespace dlg
