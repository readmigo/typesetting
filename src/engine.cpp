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
    lastStyles_ = resolver.resolve(lastBlocks_, style);

    auto result = layoutEngine_->layoutChapter(
        Chapter{chapterId, "", 0, lastBlocks_}, lastStyles_, pageSize);

    if (lastBlocks_.empty()) {
        result.warnings.push_back(LayoutWarning::EmptyContent);
    }

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
        lastStyles_ = resolver.resolve(lastBlocks_, style);
        result = layoutEngine_->layoutChapter(
            Chapter{lastChapterId_, "", 0, lastBlocks_}, lastStyles_, pageSize);
    } else {
        result = layoutEngine_->layoutChapter(
            Chapter{lastChapterId_, "", 0, lastBlocks_}, style, pageSize);
    }

    TS_LOGI("relayout: chapter='%s' pages=%zu", lastChapterId_.c_str(), result.pages.size());
    return result;
}

std::shared_ptr<PlatformAdapter> Engine::platform() const {
    return platform_;
}

} // namespace typesetting
