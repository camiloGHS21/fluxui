// FluxUI CSS counters test (CSS Lists L3).
// Covers counter value formatting (decimal/roman/alpha), counter-reset /
// counter-increment parsing, and counter()/counters() substitution.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <unordered_map>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

// ── [1] formatCounter styles ──
int test_format() {
    std::cout << "[1] formatCounter" << std::endl;
    CHECK(StyleSheet::formatCounter(5, "decimal") == "5");
    CHECK(StyleSheet::formatCounter(4, "lower-roman") == "iv");
    CHECK(StyleSheet::formatCounter(9, "upper-roman") == "IX");
    CHECK(StyleSheet::formatCounter(1, "lower-alpha") == "a");
    CHECK(StyleSheet::formatCounter(27, "lower-alpha") == "aa");
    CHECK(StyleSheet::formatCounter(28, "upper-alpha") == "AB");
    CHECK(StyleSheet::formatCounter(3, "decimal-leading-zero") == "03");
    CHECK(StyleSheet::formatCounter(2014, "upper-roman") == "MMXIV");
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] parseCounterList ──
int test_parse() {
    std::cout << "[2] counter-reset / counter-increment parsing" << std::endl;
    auto r = StyleSheet::parseCounterList("section", 0);
    CHECK(r.size() == 1 && r[0].first == "section" && r[0].second == 0);

    auto r2 = StyleSheet::parseCounterList("chapter 2 page", 0);
    CHECK(r2.size() == 2);
    CHECK(r2[0].first == "chapter" && r2[0].second == 2);
    CHECK(r2[1].first == "page" && r2[1].second == 0);

    auto inc = StyleSheet::parseCounterList("item", 1);
    CHECK(inc.size() == 1 && inc[0].second == 1);   // default increment 1
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] substituteCounters ──
int test_substitute() {
    std::cout << "[3] counter() / counters() substitution" << std::endl;
    std::unordered_map<std::string,int> c = {{"item", 3}, {"sec", 5}};
    CHECK(StyleSheet::substituteCounters("counter(item)", c) == "3");
    CHECK(StyleSheet::substituteCounters("counter(sec, upper-roman)", c) == "V");
    CHECK(StyleSheet::substituteCounters("Item counter(item): ", c) == "Item 3: ");
    CHECK(StyleSheet::substituteCounters("counters(item, '.')", c) == "3");
    // unknown counter resolves to 0
    CHECK(StyleSheet::substituteCounters("counter(missing)", c) == "0");
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] parsed into Style via CSS ──
int test_style() {
    std::cout << "[4] counter-reset/-increment land on Style" << std::endl;
    StyleSheet sheet;
    sheet.parse(".list { counter-reset: item 0; } .row { counter-increment: item; }");
    auto list = std::make_shared<Panel>("list");
    Style sl = sheet.resolve("list", "", "panel", {}, nullptr, list.get());
    CHECK(sl.rare().counterReset.size() == 1);
    CHECK(sl.rare().counterReset[0].first == "item");

    auto row = std::make_shared<Panel>("row");
    Style sr = sheet.resolve("row", "", "panel", {}, nullptr, row.get());
    CHECK(sr.rare().counterIncrement.size() == 1);
    CHECK(sr.rare().counterIncrement[0].second == 1);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI CSS Counters Tests ===" << std::endl;
    int rc = 0;
    rc |= test_format();
    rc |= test_parse();
    rc |= test_substitute();
    rc |= test_style();
    if (rc == 0) std::cout << "\nAll counter tests passed!" << std::endl;
    return rc;
}
