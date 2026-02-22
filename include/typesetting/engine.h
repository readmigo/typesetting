#pragma once

#include "typesetting/document.h"
#include "typesetting/style.h"
#include "typesetting/css.h"
#include "typesetting/layout.h"
#include "typesetting/page.h"
#include "typesetting/platform.h"
#include "typesetting/interaction.h"
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

    /// Set the chapter title for page info queries
    void setChapterTitle(const std::string& title);

    /// Layout a cover page (full-bleed image)
    LayoutResult layoutCover(const std::string& imageSrc, const PageSize& pageSize);

    // -- Interaction queries (delegate to InteractionManager) ----------------

    HitTestResult hitTest(int pageIndex, float x, float y) const;
    WordRange wordAtPoint(int pageIndex, float x, float y) const;
    std::vector<SentenceRange> getSentences(int pageIndex) const;
    std::vector<SentenceRange> getAllSentences() const;
    std::vector<TextRect> getRectsForRange(int pageIndex, int blockIndex,
                                            int charOffset, int charLength) const;
    TextRect getBlockRect(int pageIndex, int blockIndex) const;
    ImageHitResult hitTestImage(int pageIndex, float x, float y) const;
    PageInfo getPageInfo(int pageIndex) const;

private:
    std::shared_ptr<PlatformAdapter> platform_;
    std::unique_ptr<LayoutEngine> layoutEngine_;
    std::vector<Block> lastBlocks_;
    std::string lastChapterId_;
    CSSStylesheet lastStylesheet_;
    bool hasStylesheet_ = false;
    std::vector<BlockComputedStyle> lastStyles_;
    std::string lastChapterTitle_;
    InteractionManager interactionMgr_;

    void updateInteractionCache(const LayoutResult& result);
};

} // namespace typesetting
