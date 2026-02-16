#pragma once

#include "typesetting/css.h"
#include "typesetting/style.h"
#include "typesetting/document.h"
#include <vector>

namespace typesetting {

/// Resolves CSS rules + user Style into per-block computed styles
class StyleResolver {
public:
    explicit StyleResolver(const CSSStylesheet& stylesheet);

    /// Resolve styles for all blocks given user settings
    std::vector<BlockComputedStyle> resolve(
        const std::vector<Block>& blocks,
        const Style& userStyle) const;

private:
    CSSStylesheet stylesheet_;

    /// Create default style for a block based on its BlockType
    BlockComputedStyle defaultStyleForBlock(const Block& block, const Style& userStyle) const;

    /// Check if a CSS selector matches a given block
    bool selectorMatches(const CSSSelector& selector, const Block& block) const;

    /// Apply CSS properties onto a computed style
    void applyProperties(const CSSProperties& props, BlockComputedStyle& style, float baseFontSize) const;

    /// Apply user Style overrides as final layer
    void applyUserOverrides(BlockComputedStyle& style, const Style& userStyle, const Block& block) const;
};

} // namespace typesetting
