#pragma once

#include "typesetting/document.h"
#include "typesetting/style.h"
#include "typesetting/css.h"
#include "typesetting/page.h"
#include "typesetting/platform.h"
#include <memory>

namespace typesetting {

/// Page dimensions in points
struct PageSize {
    float width = 390.0f;    // iPhone logical width
    float height = 844.0f;   // iPhone logical height
};

/// The layout engine: takes document blocks + style + page size,
/// produces a list of pages with positioned text runs.
class LayoutEngine {
public:
    explicit LayoutEngine(std::shared_ptr<PlatformAdapter> platform);
    ~LayoutEngine();

    /// Lay out a chapter's blocks into pages
    LayoutResult layoutChapter(const Chapter& chapter,
                               const Style& style,
                               const PageSize& pageSize);

    /// Lay out a chapter's blocks with per-block computed styles
    LayoutResult layoutChapter(const Chapter& chapter,
                               const std::vector<BlockComputedStyle>& styles,
                               const PageSize& pageSize);

    /// Lay out a single block (for incremental updates)
    std::vector<Line> layoutBlock(const Block& block,
                                  const Style& style,
                                  float availableWidth);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace typesetting
