#pragma once

#include "typesetting/css.h"
#include "typesetting/style.h"
#include "typesetting/document.h"
#include <vector>

namespace typesetting {

/// Result of style resolution: block styles + inline styles
struct ResolvedStyles {
    std::vector<Block> expandedBlocks;  // Non-empty when display:block expansion occurred
    std::vector<BlockComputedStyle> blockStyles;
    std::vector<std::vector<InlineComputedStyle>> inlineStyles;  // [blockIdx][inlineIdx]
};

/// Resolves CSS rules + user Style into per-block computed styles
class StyleResolver {
public:
    explicit StyleResolver(const CSSStylesheet& stylesheet);

    /// Resolve styles for all blocks and their inlines
    ResolvedStyles resolve(
        const std::vector<Block>& blocks,
        const Style& userStyle) const;

private:
    CSSStylesheet stylesheet_;

    /// Create default style for a block based on its BlockType
    BlockComputedStyle defaultStyleForBlock(const Block& block, const Style& userStyle) const;

    /// Check if a CSS selector matches a given block
    bool selectorMatches(const CSSSelector& selector, const Block& block) const;

    /// Check if a CSS selector matches a given inline element within a parent block
    bool inlineSelectorMatches(const CSSSelector& selector, const InlineElement& inl, const Block& parentBlock) const;

    /// Apply CSS properties onto a computed style (importantOnly filters by !important flag)
    void applyProperties(const CSSProperties& props, BlockComputedStyle& style, float baseFontSize, bool importantOnly = false) const;

    /// Apply user Style overrides as final layer
    void applyUserOverrides(BlockComputedStyle& style, const Style& userStyle, const Block& block, bool cssFontSizeSet, bool cssLineHeightSet, bool cssTextAlignSet) const;

    /// Apply CSS properties onto an inline computed style (importantOnly filters by !important flag)
    void applyInlineProperties(const CSSProperties& props, InlineComputedStyle& style, bool importantOnly = false) const;
};

} // namespace typesetting
