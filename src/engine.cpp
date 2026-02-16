#include "typesetting/engine.h"
#include "typesetting/css.h"
#include "typesetting/style_resolver.h"

namespace typesetting {

Engine::Engine(std::shared_ptr<PlatformAdapter> platform)
    : platform_(std::move(platform))
    , layoutEngine_(std::make_unique<LayoutEngine>(platform_)) {}

Engine::~Engine() = default;

LayoutResult Engine::layoutHTML(const std::string& html,
                                const std::string& chapterId,
                                const Style& style,
                                const PageSize& pageSize) {
    lastBlocks_ = parseHTML(html);
    lastChapterId_ = chapterId;
    return layoutEngine_->layoutChapter(
        Chapter{chapterId, "", 0, lastBlocks_}, style, pageSize);
}

LayoutResult Engine::layoutHTML(const std::string& html,
                                const std::string& css,
                                const std::string& chapterId,
                                const Style& style,
                                const PageSize& pageSize) {
    lastStylesheet_ = CSSStylesheet::parse(css);
    hasStylesheet_ = true;
    lastBlocks_ = parseHTML(html);
    lastChapterId_ = chapterId;

    StyleResolver resolver(lastStylesheet_);
    lastStyles_ = resolver.resolve(lastBlocks_, style);

    return layoutEngine_->layoutChapter(
        Chapter{chapterId, "", 0, lastBlocks_}, lastStyles_, pageSize);
}

LayoutResult Engine::layoutBlocks(const std::vector<Block>& blocks,
                                   const std::string& chapterId,
                                   const Style& style,
                                   const PageSize& pageSize) {
    lastBlocks_ = blocks;
    lastChapterId_ = chapterId;
    return layoutEngine_->layoutChapter(
        Chapter{chapterId, "", 0, blocks}, style, pageSize);
}

LayoutResult Engine::relayout(const Style& style,
                               const PageSize& pageSize) {
    if (lastBlocks_.empty()) {
        return LayoutResult{lastChapterId_, {}, 0};
    }

    if (hasStylesheet_) {
        StyleResolver resolver(lastStylesheet_);
        lastStyles_ = resolver.resolve(lastBlocks_, style);
        return layoutEngine_->layoutChapter(
            Chapter{lastChapterId_, "", 0, lastBlocks_}, lastStyles_, pageSize);
    }

    return layoutEngine_->layoutChapter(
        Chapter{lastChapterId_, "", 0, lastBlocks_}, style, pageSize);
}

std::shared_ptr<PlatformAdapter> Engine::platform() const {
    return platform_;
}

} // namespace typesetting
