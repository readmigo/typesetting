#include "typesetting/engine.h"
#include "typesetting/css.h"
#include "typesetting/style_resolver.h"
#include "typesetting/log.h"
#include <stdexcept>

namespace typesetting {

Engine::Engine(std::shared_ptr<PlatformAdapter> platform)
    : platform_(std::move(platform))
    , layoutEngine_(std::make_unique<LayoutEngine>(platform_)) {}

Engine::~Engine() = default;

LayoutResult Engine::layoutHTML(const std::string& html,
                                const std::string& chapterId,
                                const Style& style,
                                const PageSize& pageSize) {
    TS_LOGI("layoutHTML: chapter='%s' html=%zu page=%.0fx%.0f",
            chapterId.c_str(), html.size(), pageSize.width, pageSize.height);

    try {
        lastBlocks_ = parseHTML(html);
    } catch (const std::exception& e) {
        TS_LOGW("layoutHTML: parse failed for '%s': %s", chapterId.c_str(), e.what());
        LayoutResult result;
        result.chapterId = chapterId;
        result.warnings.push_back(LayoutWarning::ParseError);
        return result;
    } catch (...) {
        TS_LOGW("layoutHTML: unknown exception parsing '%s'", chapterId.c_str());
        LayoutResult result;
        result.chapterId = chapterId;
        result.warnings.push_back(LayoutWarning::ParseError);
        return result;
    }
    lastChapterId_ = chapterId;

    auto result = layoutEngine_->layoutChapter(
        Chapter{chapterId, "", 0, lastBlocks_}, style, pageSize);

    if (lastBlocks_.empty()) {
        result.warnings.push_back(LayoutWarning::EmptyContent);
    }

    updateInteractionCache(result);

    TS_LOGI("layoutHTML: chapter='%s' blocks=%d pages=%zu warnings=%zu",
            chapterId.c_str(), result.totalBlocks,
            result.pages.size(), result.warnings.size());
    return result;
}

LayoutResult Engine::layoutHTML(const std::string& html,
                                const std::string& css,
                                const std::string& chapterId,
                                const Style& style,
                                const PageSize& pageSize) {
    TS_LOGI("layoutHTML+CSS: chapter='%s' html=%zu css=%zu page=%.0fx%.0f",
            chapterId.c_str(), html.size(), css.size(),
            pageSize.width, pageSize.height);

    try {
        lastStylesheet_ = CSSStylesheet::parse(css);
        hasStylesheet_ = true;
        lastBlocks_ = parseHTML(html);
    } catch (const std::exception& e) {
        TS_LOGW("layoutHTML+CSS: parse failed for '%s': %s", chapterId.c_str(), e.what());
        LayoutResult result;
        result.chapterId = chapterId;
        result.warnings.push_back(LayoutWarning::ParseError);
        return result;
    } catch (...) {
        TS_LOGW("layoutHTML+CSS: unknown exception parsing '%s'", chapterId.c_str());
        LayoutResult result;
        result.chapterId = chapterId;
        result.warnings.push_back(LayoutWarning::ParseError);
        return result;
    }
    lastChapterId_ = chapterId;

    StyleResolver resolver(lastStylesheet_);
    auto resolved = resolver.resolve(lastBlocks_, style);

    // Use expanded blocks if display:block expansion occurred
    if (!resolved.expandedBlocks.empty()) {
        lastBlocks_ = std::move(resolved.expandedBlocks);
    }
    lastStyles_ = resolved.blockStyles;

    auto result = layoutEngine_->layoutChapter(
        Chapter{chapterId, "", 0, lastBlocks_}, lastStyles_, pageSize);

    if (lastBlocks_.empty()) {
        result.warnings.push_back(LayoutWarning::EmptyContent);
    }

    updateInteractionCache(result);

    TS_LOGI("layoutHTML+CSS: chapter='%s' blocks=%d pages=%zu warnings=%zu",
            chapterId.c_str(), result.totalBlocks,
            result.pages.size(), result.warnings.size());
    return result;
}

LayoutResult Engine::layoutBlocks(const std::vector<Block>& blocks,
                                   const std::string& chapterId,
                                   const Style& style,
                                   const PageSize& pageSize) {
    TS_LOGI("layoutBlocks: chapter='%s' blocks=%zu page=%.0fx%.0f",
            chapterId.c_str(), blocks.size(), pageSize.width, pageSize.height);

    lastBlocks_ = blocks;
    lastChapterId_ = chapterId;

    auto result = layoutEngine_->layoutChapter(
        Chapter{chapterId, "", 0, blocks}, style, pageSize);

    if (blocks.empty()) {
        result.warnings.push_back(LayoutWarning::EmptyContent);
    }

    updateInteractionCache(result);

    TS_LOGI("layoutBlocks: chapter='%s' pages=%zu warnings=%zu",
            chapterId.c_str(), result.pages.size(), result.warnings.size());
    return result;
}

LayoutResult Engine::relayout(const Style& style,
                               const PageSize& pageSize) {
    TS_LOGI("relayout: chapter='%s' blocks=%zu page=%.0fx%.0f",
            lastChapterId_.c_str(), lastBlocks_.size(),
            pageSize.width, pageSize.height);

    if (lastBlocks_.empty()) {
        TS_LOGW("relayout: empty content for '%s'", lastChapterId_.c_str());
        LayoutResult result;
        result.chapterId = lastChapterId_;
        result.warnings.push_back(LayoutWarning::EmptyContent);
        return result;
    }

    LayoutResult result;
    if (hasStylesheet_) {
        StyleResolver resolver(lastStylesheet_);
        auto resolved = resolver.resolve(lastBlocks_, style);
        if (!resolved.expandedBlocks.empty()) {
            lastBlocks_ = std::move(resolved.expandedBlocks);
        }
        lastStyles_ = resolved.blockStyles;
        result = layoutEngine_->layoutChapter(
            Chapter{lastChapterId_, "", 0, lastBlocks_}, lastStyles_, pageSize);
    } else {
        result = layoutEngine_->layoutChapter(
            Chapter{lastChapterId_, "", 0, lastBlocks_}, style, pageSize);
    }

    updateInteractionCache(result);

    TS_LOGI("relayout: chapter='%s' pages=%zu", lastChapterId_.c_str(), result.pages.size());
    return result;
}

std::shared_ptr<PlatformAdapter> Engine::platform() const {
    return platform_;
}

// ---------------------------------------------------------------------------
// Chapter title & cover
// ---------------------------------------------------------------------------

void Engine::setChapterTitle(const std::string& title) {
    lastChapterTitle_ = title;
    interactionMgr_.setChapterTitle(title);
}

LayoutResult Engine::layoutCover(const std::string& imageSrc, const PageSize& pageSize) {
    TS_LOGI("layoutCover: image='%s' page=%.0fx%.0f",
            imageSrc.c_str(), pageSize.width, pageSize.height);

    LayoutResult result;
    result.chapterId = "__cover__";

    Page page;
    page.pageIndex = 0;
    page.width = pageSize.width;
    page.height = pageSize.height;
    page.contentX = 0;
    page.contentY = 0;
    page.contentWidth = pageSize.width;
    page.contentHeight = pageSize.height;

    Decoration deco;
    deco.type = DecorationType::ImagePlaceholder;
    deco.x = 0;
    deco.y = 0;
    deco.width = pageSize.width;
    deco.height = pageSize.height;
    deco.imageSrc = imageSrc;
    page.decorations.push_back(deco);

    result.pages.push_back(page);
    return result;
}

// ---------------------------------------------------------------------------
// Interaction query delegates
// ---------------------------------------------------------------------------

HitTestResult Engine::hitTest(int pageIndex, float x, float y) const {
    return interactionMgr_.hitTest(pageIndex, x, y);
}

WordRange Engine::wordAtPoint(int pageIndex, float x, float y) const {
    return interactionMgr_.wordAtPoint(pageIndex, x, y);
}

std::vector<SentenceRange> Engine::getSentences(int pageIndex) const {
    return interactionMgr_.getSentences(pageIndex);
}

std::vector<SentenceRange> Engine::getAllSentences() const {
    return interactionMgr_.getAllSentences();
}

std::vector<TextRect> Engine::getRectsForRange(int pageIndex, int blockIndex,
                                                int charOffset, int charLength) const {
    return interactionMgr_.getRectsForRange(pageIndex, blockIndex, charOffset, charLength);
}

TextRect Engine::getBlockRect(int pageIndex, int blockIndex) const {
    return interactionMgr_.getBlockRect(pageIndex, blockIndex);
}

ImageHitResult Engine::hitTestImage(int pageIndex, float x, float y) const {
    return interactionMgr_.hitTestImage(pageIndex, x, y);
}

PageInfo Engine::getPageInfo(int pageIndex) const {
    return interactionMgr_.getPageInfo(pageIndex);
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void Engine::updateInteractionCache(const LayoutResult& result) {
    interactionMgr_.setLayoutResult(result, lastBlocks_, lastChapterTitle_);
}

} // namespace typesetting
