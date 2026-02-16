#pragma once

#include "typesetting/document.h"
#include "typesetting/style.h"
#include "typesetting/css.h"
#include "typesetting/layout.h"
#include "typesetting/page.h"
#include "typesetting/platform.h"
#include <memory>

namespace typesetting {

/// Main entry point for the typesetting engine.
/// Coordinates document parsing, styling, and layout.
class Engine {
public:
    explicit Engine(std::shared_ptr<PlatformAdapter> platform);
    ~Engine();

    /// Parse HTML content and lay out into pages
    LayoutResult layoutHTML(const std::string& html,
                            const std::string& chapterId,
                            const Style& style,
                            const PageSize& pageSize);

    /// Parse HTML + CSS content and lay out into pages with style resolution
    LayoutResult layoutHTML(const std::string& html,
                            const std::string& css,
                            const std::string& chapterId,
                            const Style& style,
                            const PageSize& pageSize);

    /// Lay out pre-parsed blocks into pages
    LayoutResult layoutBlocks(const std::vector<Block>& blocks,
                              const std::string& chapterId,
                              const Style& style,
                              const PageSize& pageSize);

    /// Re-layout with new style (e.g., font size changed)
    /// Uses the last set of blocks without re-parsing
    LayoutResult relayout(const Style& style,
                          const PageSize& pageSize);

    /// Get the platform adapter
    std::shared_ptr<PlatformAdapter> platform() const;

private:
    std::shared_ptr<PlatformAdapter> platform_;
    std::unique_ptr<LayoutEngine> layoutEngine_;
    std::vector<Block> lastBlocks_;
    std::string lastChapterId_;
    CSSStylesheet lastStylesheet_;
    bool hasStylesheet_ = false;
    std::vector<BlockComputedStyle> lastStyles_;
};

} // namespace typesetting
