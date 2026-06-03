
LayoutResult GridLayoutAlgorithm::layout(Widget* widget, const LayoutConstraints& constraints) {
    LayoutResult result;
    const Style& s = *widget->computedStyle;

    const float contentX = widget->bounds.x + s.padding.left;
    const float contentY = widget->bounds.y + s.padding.top;
    const float contentW = std::max(0.0f, widget->bounds.w - s.padding.horizontal());
    const float contentH = std::max(0.0f, widget->bounds.h - s.padding.vertical());
    const float colGap   = s.columnGap > 0.0f ? s.columnGap : s.gap;
    const float rowGap   = s.rowGap    > 0.0f ? s.rowGap    : s.gap;
    const float emBase   = constraints.emBase;

    // ── Collect in-flow children ─────────────────────────────
    struct GridItem {
        Widget*    widget   = nullptr;
        int        colStart = 0; // 0-based, inclusive
        int        colEnd   = 0; // 0-based, exclusive
        int        rowStart = 0;
        int        rowEnd   = 0;
    };
    std::vector<GridItem> items;
    for (auto& child : widget->children) {
        if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) continue;
        GridItem gi;
        gi.widget = child.get();
        items.push_back(gi);
    }
    if (items.empty()) {
        result.x = widget->bounds.x; result.y = widget->bounds.y;
        result.width = widget->bounds.w; result.height = widget->bounds.h;
        result.contentHeight = s.padding.vertical();
        return result;
    }

    // ── 1. Build explicit column/row track lists ─────────────
    // Use pre-parsed tracks from Style; fall back to single auto track.
    std::vector<GridTrackSize> colTracks = s.gridTemplateColumnTracks;
    std::vector<GridTrackSize> rowTracks = s.gridTemplateRowTracks;

    const std::vector<GridTrackSize>& autoColTrack =
        !s.gridAutoColumnTracks.empty() ? s.gridAutoColumnTracks
                                        : std::vector<GridTrackSize>{{GridTrackSizeType::Auto, 0}};
    const std::vector<GridTrackSize>& autoRowTrack =
        !s.gridAutoRowTracks.empty()    ? s.gridAutoRowTracks
                                        : std::vector<GridTrackSize>{{GridTrackSizeType::Auto, 0}};

    // ── 2. Resolve explicit item placements first ────────────
    // Mirrors Blink's PlacementAlgorithm: placed items first, then auto.
    // grid-area named lookup via gridTemplateAreas.
    auto resolveNamedArea = [&](const std::string& name,
                                 int& cs, int& ce, int& rs, int& re) -> bool {
        if (!s.hasGridTemplateAreas || name.empty()) return false;
        const auto& areas = s.gridTemplateAreas;
        for (int r = 0; r < areas.rowCount; r++) {
            for (int c = 0; c < areas.columnCount; c++) {
                int idx = r * areas.columnCount + c;
                if (idx >= (int)areas.areas.size()) continue;
                if (areas.areas[idx] == name) {
                    // Find extent
                    rs = r; re = r + 1;
                    cs = c; ce = c + 1;
                    while (re < areas.rowCount &&
                           (re * areas.columnCount + c) < (int)areas.areas.size() &&
                           areas.areas[re * areas.columnCount + c] == name) ++re;
                    while (ce < areas.columnCount &&
                           (r * areas.columnCount + ce) < (int)areas.areas.size() &&
                           areas.areas[r * areas.columnCount + ce] == name) ++ce;
                    return true;
                }
            }
        }
        return false;
    };

    // Resolve one GridPlacement start/end pair to 0-based [start, end) cell range.
    // numExplicit is the number of explicit tracks on that axis.
    auto resolveAxis = [](const GridPlacement& pStart, const GridPlacement& pEnd,
                          int numExplicit, int /*defaultSpan*/) -> std::pair<int,int> {
        using PT = GridPlacement::PlacementType;
        int start = -1, end = -1;

        // Resolve start
        if (pStart.type == PT::Line) {
            start = (pStart.line > 0) ? pStart.line - 1
                                      : numExplicit + pStart.line; // negative counts from end
        }
        // Resolve end
        if (pEnd.type == PT::Line) {
            end = (pEnd.line > 0) ? pEnd.line - 1
                                  : numExplicit + pEnd.line;
        }
        // Span resolution
        if (pStart.type == PT::Span && end >= 0) {
            start = end - pStart.span;
        } else if (pEnd.type == PT::Span && start >= 0) {
            end = start + pEnd.span;
        } else if (pEnd.type == PT::Span && start < 0) {
            // auto-start + span end: defer to auto-placement
        }
        return {start, end};
    };

    // First pass: assign placements where they are explicit
    for (auto& gi : items) {
        const Style& cs2 = *gi.widget->computedStyle;

        // Check grid-area named reference first
        if (!cs2.gridArea.empty() && cs2.gridColumnStart.isAuto() && cs2.gridRowStart.isAuto()) {
            int nc = (int)colTracks.size(), nr = (int)rowTracks.size();
            if (resolveNamedArea(cs2.gridArea,
                                  gi.colStart, gi.colEnd,
                                  gi.rowStart, gi.rowEnd)) continue;
        }

        int numCols = (int)colTracks.size();
        int numRows = (int)rowTracks.size();

        auto [colS, colE] = resolveAxis(cs2.gridColumnStart, cs2.gridColumnEnd, numCols, 1);
        auto [rowS, rowE] = resolveAxis(cs2.gridRowStart,    cs2.gridRowEnd,    numRows, 1);

        gi.colStart = colS;  gi.colEnd = colE;
        gi.rowStart = rowS;  gi.rowEnd = rowE;
    }

    // ── 3. Auto-placement algorithm ──────────────────────────
    // Blink: GridPlacementMixin::RunAutoPlacementAlgorithm
    bool denseFlow = (s.gridAutoFlow == GridAutoFlow::RowDense ||
                      s.gridAutoFlow == GridAutoFlow::ColumnDense);
    bool colFlow   = (s.gridAutoFlow == GridAutoFlow::Column ||
                      s.gridAutoFlow == GridAutoFlow::ColumnDense);

    int cursorRow = 0, cursorCol = 0;
    // Occupied cell set (row*1000+col for simple lookup, extended as grid grows)
    std::vector<std::pair<int,int>> occupied;
    auto isOccupied = [&](int r, int c) {
        for (auto& p : occupied) if (p.first == r && p.second == c) return true;
        return false;
    };
    auto markOccupied = [&](int rs, int re, int cs3, int ce) {
        for (int r = rs; r < re; r++)
            for (int c = cs3; c < ce; c++)
                occupied.push_back({r, c});
    };

    // Grow track lists on demand
    int maxCol = std::max(1, (int)colTracks.size());
    int maxRow = std::max(1, (int)rowTracks.size());

    auto ensureCols = [&](int needed) {
        while ((int)colTracks.size() < needed)
            colTracks.push_back(autoColTrack[0]);
        maxCol = (int)colTracks.size();
    };
    auto ensureRows = [&](int needed) {
        while ((int)rowTracks.size() < needed)
            rowTracks.push_back(autoRowTrack[0]);
        maxRow = (int)rowTracks.size();
    };

    // Mark explicitly placed items first
    for (auto& gi : items) {
        if (gi.colStart >= 0 && gi.colEnd > gi.colStart &&
            gi.rowStart >= 0 && gi.rowEnd > gi.rowStart) {
            ensureCols(gi.colEnd);
            ensureRows(gi.rowEnd);
            markOccupied(gi.rowStart, gi.rowEnd, gi.colStart, gi.colEnd);
        }
    }

    // Auto-place remaining items
    if (denseFlow) { cursorRow = 0; cursorCol = 0; }

    for (auto& gi : items) {
        // Already fully placed?
        if (gi.colStart >= 0 && gi.colEnd > gi.colStart &&
            gi.rowStart >= 0 && gi.rowEnd > gi.rowStart) continue;

        // Determine span
        int spanCol = 1, spanRow = 1;
        const Style& cs2 = *gi.widget->computedStyle;
        if (cs2.gridColumnEnd.type == GridPlacement::PlacementType::Span) spanCol = cs2.gridColumnEnd.span;
        if (cs2.gridRowEnd.type    == GridPlacement::PlacementType::Span) spanRow = cs2.gridRowEnd.span;
        if (cs2.gridColumnStart.type == GridPlacement::PlacementType::Span) spanCol = cs2.gridColumnStart.span;
        if (cs2.gridRowStart.type    == GridPlacement::PlacementType::Span) spanRow = cs2.gridRowStart.span;

        // Partially placed (one axis fixed)?
        if (gi.colStart >= 0 && gi.colEnd <= gi.colStart) gi.colEnd = gi.colStart + spanCol;
        if (gi.rowStart >= 0 && gi.rowEnd <= gi.rowStart) gi.rowEnd = gi.rowStart + spanRow;

        if (!colFlow) {
            // Row-flow: place across columns, wrap to next row
            int startCol = (gi.colStart >= 0) ? gi.colStart : 0;
            bool fixedCol = (gi.colStart >= 0);

            if (!fixedCol) {
                // Find next available position
                int searchRow = cursorRow;
                int searchCol = cursorCol;
                for (;;) {
                    if (searchCol + spanCol > maxCol) {
                        searchCol = 0; searchRow++;
                        ensureRows(searchRow + spanRow);
                    }
                    bool fits = true;
                    for (int dc = 0; dc < spanCol && fits; dc++)
                        for (int dr = 0; dr < spanRow && fits; dr++)
                            if (isOccupied(searchRow + dr, searchCol + dc)) fits = false;
                    if (fits) {
                        gi.rowStart = searchRow; gi.rowEnd = searchRow + spanRow;
                        gi.colStart = searchCol; gi.colEnd = searchCol + spanCol;
                        if (!denseFlow) { cursorRow = searchRow; cursorCol = searchCol + spanCol; }
                        break;
                    }
                    searchCol++;
                }
            } else {
                // Column fixed, find row
                int searchRow = (gi.rowStart >= 0) ? gi.rowStart : cursorRow;
                for (;;) {
                    ensureRows(searchRow + spanRow);
                    bool fits = true;
                    for (int dc = 0; dc < spanCol && fits; dc++)
                        for (int dr = 0; dr < spanRow && fits; dr++)
                            if (isOccupied(searchRow + dr, startCol + dc)) fits = false;
                    if (fits) {
                        gi.rowStart = searchRow; gi.rowEnd = searchRow + spanRow;
                        gi.colStart = startCol;  gi.colEnd = startCol + spanCol;
                        break;
                    }
                    searchRow++;
                }
            }
        } else {
            // Column-flow: place down columns, wrap to next column
            int startRow = (gi.rowStart >= 0) ? gi.rowStart : 0;
            bool fixedRow = (gi.rowStart >= 0);

            if (!fixedRow) {
                int searchRow = cursorRow;
                int searchCol = cursorCol;
                for (;;) {
                    if (searchRow + spanRow > maxRow) {
                        searchRow = 0; searchCol++;
                        ensureCols(searchCol + spanCol);
                    }
                    bool fits = true;
                    for (int dc = 0; dc < spanCol && fits; dc++)
                        for (int dr = 0; dr < spanRow && fits; dr++)
                            if (isOccupied(searchRow + dr, searchCol + dc)) fits = false;
                    if (fits) {
                        gi.colStart = searchCol; gi.colEnd = searchCol + spanCol;
                        gi.rowStart = searchRow; gi.rowEnd = searchRow + spanRow;
                        if (!denseFlow) { cursorCol = searchCol; cursorRow = searchRow + spanRow; }
                        break;
                    }
                    searchRow++;
                }
            } else {
                int searchCol = (gi.colStart >= 0) ? gi.colStart : cursorCol;
                for (;;) {
                    ensureCols(searchCol + spanCol);
                    bool fits = true;
                    for (int dc = 0; dc < spanCol && fits; dc++)
                        for (int dr = 0; dr < spanRow && fits; dr++)
                            if (isOccupied(startRow + dr, searchCol + dc)) fits = false;
                    if (fits) {
                        gi.colStart = searchCol; gi.colEnd = searchCol + spanCol;
                        gi.rowStart = startRow;  gi.rowEnd = startRow + spanRow;
                        break;
                    }
                    searchCol++;
                }
            }
        }
        ensureCols(gi.colEnd);
        ensureRows(gi.rowEnd);
        markOccupied(gi.rowStart, gi.rowEnd, gi.colStart, gi.colEnd);
    }

    int numCols = (int)colTracks.size();
    int numRows = (int)rowTracks.size();
    if (numCols < 1) numCols = 1;
    if (numRows < 1) numRows = 1;

    // ── 4. Track sizing algorithm ────────────────────────────
    // Blink: GridTrackSizingAlgorithm (init → item contributions → fr)
    // Phase 1: Resolve fixed and percentage tracks.
    auto resolveTrack = [&](const GridTrackSize& t, float available) -> float {
        switch (t.type) {
            case GridTrackSizeType::Fixed:
                return std::max(0.0f, t.value);
            case GridTrackSizeType::MinContent:
            case GridTrackSizeType::MaxContent:
            case GridTrackSizeType::Auto:
                return -1.0f; // deferred
            case GridTrackSizeType::Flex:
                return -2.0f; // deferred fr
            case GridTrackSizeType::FitContent:
                return std::min(t.value, available);
            case GridTrackSizeType::MinMax: {
                // Resolve min side
                float minV = 0.0f;
                if (t.minType == GridTrackSizeType::Fixed)      minV = t.minValue;
                else if (t.minType == GridTrackSizeType::Auto)  minV = 0.0f;
                // Resolve max side
                float maxV = available;
                if (t.maxType == GridTrackSizeType::Fixed)      maxV = t.maxValue;
                else if (t.maxType == GridTrackSizeType::Flex)  return -2.0f; // fr max
                else if (t.maxType == GridTrackSizeType::Auto ||
                         t.maxType == GridTrackSizeType::MaxContent) maxV = -1.0f;
                if (maxV < 0) return std::max(minV, 0.0f); // auto max → deferred
                return std::max(minV, maxV);
            }
            default: return -1.0f;
        }
    };

    std::vector<float> colSizes(numCols, 0.0f);
    std::vector<float> rowSizes(numRows, 0.0f);
    float totalFixedCol = 0.0f, totalFixedRow = 0.0f;
    float totalFrCol = 0.0f,    totalFrRow = 0.0f;
    int   autoColCount = 0,     autoRowCount = 0;

    for (int c = 0; c < numCols; c++) {
        float v = resolveTrack(colTracks[c], contentW);
        if (v >= 0) { colSizes[c] = v; totalFixedCol += v; }
        else if (v == -2.0f) { totalFrCol += colTracks[c].type == GridTrackSizeType::Flex ? colTracks[c].value
                                            : (colTracks[c].maxType == GridTrackSizeType::Flex ? colTracks[c].maxValue : 1.0f); }
        else autoColCount++;
    }
    for (int r = 0; r < numRows; r++) {
        float v = resolveTrack(rowTracks[r], contentH);
        if (v >= 0) { rowSizes[r] = v; totalFixedRow += v; }
        else if (v == -2.0f) { totalFrRow += rowTracks[r].type == GridTrackSizeType::Flex ? rowTracks[r].value
                                            : (rowTracks[r].maxType == GridTrackSizeType::Flex ? rowTracks[r].maxValue : 1.0f); }
        else autoRowCount++;
    }

    // Phase 2: Distribute fr columns
    float colGapTotal = colGap * (numCols - 1);
    float rowGapTotal = rowGap * (numRows - 1);
    float freeCol = std::max(0.0f, contentW - totalFixedCol - colGapTotal);
    float freeRow = std::max(0.0f, contentH - totalFixedRow - rowGapTotal);

    if (totalFrCol > 0.0f) {
        float pxPerFr = freeCol / totalFrCol;
        for (int c = 0; c < numCols; c++) {
            const GridTrackSize& t = colTracks[c];
            if (t.type == GridTrackSizeType::Flex) {
                colSizes[c] = std::max(0.0f, t.value * pxPerFr);
                freeCol -= colSizes[c];
            } else if (t.type == GridTrackSizeType::MinMax && t.maxType == GridTrackSizeType::Flex) {
                colSizes[c] = std::max(t.minValue, t.maxValue * pxPerFr);
                freeCol -= colSizes[c];
            }
        }
    }
    if (totalFrRow > 0.0f && contentH > 0.0f) {
        float pxPerFr = freeRow / totalFrRow;
        for (int r = 0; r < numRows; r++) {
            const GridTrackSize& t = rowTracks[r];
            if (t.type == GridTrackSizeType::Flex) {
                rowSizes[r] = std::max(0.0f, t.value * pxPerFr);
            } else if (t.type == GridTrackSizeType::MinMax && t.maxType == GridTrackSizeType::Flex) {
                rowSizes[r] = std::max(t.minValue, t.maxValue * pxPerFr);
            }
        }
    }

    // Phase 3: Distribute auto columns equally from remaining free space
    if (autoColCount > 0) {
        float usedFr = 0.0f;
        for (int c = 0; c < numCols; c++) if (colSizes[c] > 0) usedFr += colSizes[c];
        float autoW = std::max(0.0f, (contentW - usedFr - colGapTotal)) / autoColCount;
        for (int c = 0; c < numCols; c++) {
            float v = resolveTrack(colTracks[c], contentW);
            if (v < 0 && colSizes[c] == 0.0f) colSizes[c] = autoW;
        }
    }
    // Auto rows will be sized by content in phase 5.

    // ── 5. Lay out items into their grid areas ────────────────
    // Compute row heights from item intrinsic heights (two-pass for auto rows).
    // Pass A: lay out items, measure intrinsic heights, update auto row sizes.
    for (auto& gi : items) {
        const Style& cs2 = *gi.widget->computedStyle;

        // Compute column span width
        float itemX = contentX;
        float itemW = 0.0f;
        for (int c = gi.colStart; c < gi.colEnd && c < numCols; c++) {
            if (c > gi.colStart) itemW += colGap;
            itemW += colSizes[c];
        }
        for (int c = 0; c < gi.colStart && c < numCols; c++)
            itemX += colSizes[c] + colGap;

        // Compute row span height (may be 0 for auto rows — will be updated)
        float itemH = 0.0f;
        for (int r = gi.rowStart; r < gi.rowEnd && r < numRows; r++) {
            if (r > gi.rowStart) itemH += rowGap;
            itemH += rowSizes[r];
        }
        // If height explicitly set on item, use it
        if (cs2.height.isSet())
            itemH = cs2.height.resolve(contentH, constraints.parentWidth, constraints.parentHeight, emBase);

        float itemY = contentY;
        for (int r = 0; r < gi.rowStart && r < numRows; r++)
            itemY += rowSizes[r] + rowGap;

        float availW = std::max(0.0f, itemW - cs2.margin.horizontal());
        float availH = std::max(0.0f, itemH - cs2.margin.vertical());

        Rect childArea = {
            itemX + cs2.margin.left,
            itemY + cs2.margin.top,
            availW, availH
        };
        gi.widget->layout(childArea);

        // Update auto row heights from content
        if (gi.rowEnd - gi.rowStart == 1) {
            int r = gi.rowStart;
            if (r < numRows) {
                const GridTrackSize& rt = rowTracks[r];
                bool isAutoRow = (rt.type == GridTrackSizeType::Auto ||
                                  rt.type == GridTrackSizeType::MinContent ||
                                  rt.type == GridTrackSizeType::MaxContent ||
                                 (rt.type == GridTrackSizeType::MinMax &&
                                  rt.maxType == GridTrackSizeType::Auto));
                if (isAutoRow) {
                    float needed = gi.widget->bounds.h + cs2.margin.vertical();
                    rowSizes[r] = std::max(rowSizes[r], needed);
                }
            }
        }
    }

    // Pass B: Re-layout items now that auto row sizes are known, applying alignment.
    JustifyItems containerJI = s.hasJustifyItems ? s.justifyItems : JustifyItems::Normal;
    AlignItems   containerAI = s.alignItems;     // reuse flex AlignItems enum

    for (auto& gi : items) {
        const Style& cs2 = *gi.widget->computedStyle;

        // Recompute geometry with final track sizes
        float cellX = contentX, cellW = 0.0f;
        for (int c = 0; c < gi.colStart && c < numCols; c++) cellX += colSizes[c] + colGap;
        for (int c = gi.colStart; c < gi.colEnd && c < numCols; c++) {
            if (c > gi.colStart) cellW += colGap;
            cellW += colSizes[c];
        }

        float cellY = contentY, cellH = 0.0f;
        for (int r = 0; r < gi.rowStart && r < numRows; r++) cellY += rowSizes[r] + rowGap;
        for (int r = gi.rowStart; r < gi.rowEnd && r < numRows; r++) {
            if (r > gi.rowStart) cellH += rowGap;
            cellH += rowSizes[r];
        }

        // Determine item's inline/block size
        float itemW = cs2.width.isSet()  ? cs2.width.resolve(cellW,  constraints.parentWidth, constraints.parentHeight, emBase) : cellW - cs2.margin.horizontal();
        float itemH = cs2.height.isSet() ? cs2.height.resolve(cellH, constraints.parentWidth, constraints.parentHeight, emBase) : gi.widget->bounds.h;

        // Clamped to cell
        itemW = std::clamp(itemW, 0.0f, cellW - cs2.margin.horizontal());
        itemH = std::clamp(itemH, 0.0f, cellH > 0 ? cellH : itemH);

        // ── justify-self (inline axis / X) ──────────────────
        JustifySelf jself = cs2.hasJustifySelf ? cs2.justifySelf : JustifySelf::Auto;
        // Auto inherits justify-items from container
        JustifyItems ji = (jself == JustifySelf::Auto)
            ? containerJI
            : static_cast<JustifyItems>(static_cast<int>(jself));

        float finalX = cellX + cs2.margin.left;
        float innerW = cellW - cs2.margin.horizontal();
        if (ji == JustifyItems::Center && innerW > itemW)
            finalX = cellX + cs2.margin.left + (innerW - itemW) / 2.0f;
        else if (ji == JustifyItems::FlexEnd && innerW > itemW)
            finalX = cellX + cellW - cs2.margin.right - itemW;
        else if (ji == JustifyItems::Stretch)
            itemW = innerW;

        // ── align-self (block axis / Y) ──────────────────────
        AlignSelf aself = cs2.alignSelf;
        AlignItems ai = containerAI;
        if (aself != AlignSelf::Auto) ai = static_cast<AlignItems>(static_cast<int>(aself));

        float finalY = cellY + cs2.margin.top;
        float innerH = (cellH > 0) ? cellH - cs2.margin.vertical() : itemH;
        if (ai == AlignItems::Center && cellH > 0 && innerH > itemH)
            finalY = cellY + cs2.margin.top + (innerH - itemH) / 2.0f;
        else if (ai == AlignItems::FlexEnd && cellH > 0 && innerH > itemH)
            finalY = cellY + cellH - cs2.margin.bottom - itemH;
        else if (ai == AlignItems::Stretch && cellH > 0) {
            itemH = innerH;
            finalY = cellY + cs2.margin.top;
        }

        Rect finalArea = { finalX, finalY, std::max(0.0f, itemW), std::max(0.0f, itemH) };
        gi.widget->layout(finalArea);
    }

    // ── 6. Compute container height ───────────────────────────
    float totalRowH = rowGapTotal;
    for (int r = 0; r < numRows; r++) totalRowH += rowSizes[r];

    result.contentHeight = totalRowH + s.padding.vertical();
    if (!s.height.isSet() && !consumesParentMainAxisHeight(widget, s))
        widget->bounds.h = std::max(widget->bounds.h, result.contentHeight);

    if (!widget->parent) {
        widget->bounds.x = 0.0f;
        widget->bounds.y = 0.0f;
        widget->bounds.w = constraints.availableWidth;
        widget->bounds.h = constraints.availableHeight;
    }

    result.x = widget->bounds.x;
    result.y = widget->bounds.y;
    result.width  = widget->bounds.w;
    result.height = widget->bounds.h;
    return result;
}
