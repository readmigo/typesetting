#include "typesetting/style_resolver.h"
#include "typesetting/log.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace typesetting {

namespace {

/// Case-insensitive string comparison
bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

/// Check if needle is found within a space-separated list of values in haystack
bool containsClass(const std::string& haystack, const std::string& needle) {
    if (haystack.empty() || needle.empty()) return false;
    std::istringstream iss(haystack);
    std::string token;
    while (iss >> token) {
        if (token == needle) return true;
    }
    return false;
}

/// Map BlockType to its expected HTML tag name
std::string blockTypeToTag(BlockType type) {
    switch (type) {
        case BlockType::Paragraph:     return "p";
        case BlockType::Heading1:      return "h1";
        case BlockType::Heading2:      return "h2";
        case BlockType::Heading3:      return "h3";
        case BlockType::Heading4:      return "h4";
        case BlockType::Blockquote:    return "blockquote";
        case BlockType::CodeBlock:     return "pre";
        case BlockType::Image:         return "img";
        case BlockType::HorizontalRule:return "hr";
        case BlockType::ListItem:      return "li";
        case BlockType::Figcaption:    return "figcaption";
        case BlockType::Table:         return "table";
    }
    return "";
}

/// Check if a BlockType is a heading
bool isHeadingType(BlockType type) {
    return type == BlockType::Heading1 || type == BlockType::Heading2 ||
           type == BlockType::Heading3 || type == BlockType::Heading4;
}

} // anonymous namespace

StyleResolver::StyleResolver(const CSSStylesheet& stylesheet)
    : stylesheet_(stylesheet) {}

ResolvedStyles StyleResolver::resolve(
    const std::vector<Block>& blocks,
    const Style& userStyle) const {

    ResolvedStyles result;
    result.blockStyles.reserve(blocks.size());
    result.inlineStyles.reserve(blocks.size());

    for (const auto& block : blocks) {
        // === Block resolution (existing logic) ===
        BlockComputedStyle style = defaultStyleForBlock(block, userStyle);

        std::vector<const CSSRule*> matches;
        for (const auto& rule : stylesheet_.rules) {
            if (selectorMatches(rule.selector, block)) {
                matches.push_back(&rule);
            }
        }

        std::sort(matches.begin(), matches.end(),
                  [](const CSSRule* a, const CSSRule* b) {
                      return a->selector.specificity() < b->selector.specificity();
                  });

        float baseFontSize = userStyle.font.size;
        bool cssFontSizeSet = false;
        for (const auto* rule : matches) {
            if (rule->properties.fontSize.has_value()) cssFontSizeSet = true;
            applyProperties(rule->properties, style, baseFontSize);
        }

        applyUserOverrides(style, userStyle, block, cssFontSizeSet);
        result.blockStyles.push_back(std::move(style));

        // === Inline resolution ===
        std::vector<InlineComputedStyle> inlineStyles;
        inlineStyles.reserve(block.inlines.size());
        for (const auto& inl : block.inlines) {
            InlineComputedStyle istyle;
            std::vector<const CSSRule*> inlineMatches;
            for (const auto& rule : stylesheet_.rules) {
                if (inlineSelectorMatches(rule.selector, inl, block)) {
                    inlineMatches.push_back(&rule);
                }
            }
            std::sort(inlineMatches.begin(), inlineMatches.end(),
                      [](const CSSRule* a, const CSSRule* b) {
                          return a->selector.specificity() < b->selector.specificity();
                      });
            for (const auto* rule : inlineMatches) {
                applyInlineProperties(rule->properties, istyle);
            }
            inlineStyles.push_back(std::move(istyle));
        }
        result.inlineStyles.push_back(std::move(inlineStyles));
    }

    TS_LOGI("StyleResolver::resolve: blocks=%zu rules=%zu", result.blockStyles.size(), stylesheet_.rules.size());
    return result;
}

BlockComputedStyle StyleResolver::defaultStyleForBlock(
    const Block& block, const Style& userStyle) const {

    BlockComputedStyle style;
    float em = userStyle.font.size;

    // Base font from user style
    style.font = userStyle.font;

    // Common defaults from user style
    style.lineSpacingMultiplier = userStyle.lineSpacingMultiplier;
    style.letterSpacing = userStyle.letterSpacing;
    style.wordSpacing = userStyle.wordSpacing;
    style.paragraphSpacingAfter = userStyle.paragraphSpacing;

    switch (block.type) {
        case BlockType::Paragraph:
            style.textIndent = em;  // 1em
            style.alignment = TextAlignment::Justified;
            style.hyphens = true;
            break;

        case BlockType::Heading1:
            style.font.size = em * 1.5f;
            style.smallCaps = true;
            style.alignment = TextAlignment::Center;
            style.hyphens = false;
            style.textIndent = 0;
            style.marginTop = 3.0f * em;
            style.marginBottom = 1.0f * em;
            break;

        case BlockType::Heading2:
            style.font.size = em * 1.3f;
            style.smallCaps = true;
            style.alignment = TextAlignment::Center;
            style.hyphens = false;
            style.textIndent = 0;
            style.marginTop = 3.0f * em;
            style.marginBottom = 1.0f * em;
            break;

        case BlockType::Heading3:
            style.font.size = em * 1.1f;
            style.smallCaps = true;
            style.alignment = TextAlignment::Center;
            style.hyphens = false;
            style.textIndent = 0;
            style.marginTop = 2.0f * em;
            style.marginBottom = 0.5f * em;
            break;

        case BlockType::Heading4:
            style.font.size = em * 1.0f;
            style.smallCaps = true;
            style.alignment = TextAlignment::Center;
            style.hyphens = false;
            style.textIndent = 0;
            style.marginTop = 1.5f * em;
            style.marginBottom = 0.5f * em;
            break;

        case BlockType::Blockquote:
            style.marginLeft = 2.5f * em;
            style.marginRight = 2.5f * em;
            style.alignment = TextAlignment::Justified;
            style.hyphens = true;
            break;

        case BlockType::CodeBlock:
            style.font.family = "monospace";
            style.font.size = em * 0.9f;
            style.hyphens = false;
            style.alignment = TextAlignment::Left;
            style.textIndent = 0;
            break;

        case BlockType::HorizontalRule:
            style.hrStyle = HRStyle{};
            style.hidden = false;
            style.textIndent = 0;
            break;

        case BlockType::ListItem:
            style.marginLeft = 2.0f * em;
            style.alignment = TextAlignment::Justified;
            style.hyphens = true;
            break;

        case BlockType::Image:
            style.textIndent = 0;
            break;

        case BlockType::Figcaption:
            style.font.size = em * 0.85f;
            style.font.style = FontStyle::Italic;
            style.alignment = TextAlignment::Center;
            style.textIndent = 0;
            style.marginTop = 0.5f * em;
            style.hyphens = false;
            break;

        case BlockType::Table:
            style.textIndent = 0;
            style.alignment = TextAlignment::Left;
            style.hyphens = false;
            style.marginTop = 1.0f * em;
            style.marginBottom = 1.0f * em;
            break;
    }

    return style;
}

bool StyleResolver::selectorMatches(const CSSSelector& selector, const Block& block) const {
    // Determine the effective tag for this block
    std::string effectiveTag = block.htmlTag;
    if (effectiveTag.empty()) {
        effectiveTag = blockTypeToTag(block.type);
    }

    // Helper: check if a parent selector matches the block's parent metadata
    auto parentMatches = [&](const CSSSelector& parentSel) -> bool {
        if (parentSel.type == SelectorType::Element) {
            bool match = true;
            if (!parentSel.element.empty()) {
                match = iequals(parentSel.element, block.parentTag);
            }
            if (match && !parentSel.className.empty()) {
                match = containsClass(block.parentClassName, parentSel.className);
            }
            if (match && !parentSel.id.empty()) {
                match = (block.parentId == parentSel.id);
            }
            return match;
        } else if (parentSel.type == SelectorType::Class) {
            return containsClass(block.parentClassName, parentSel.className);
        } else if (parentSel.type == SelectorType::Attribute) {
            return containsClass(block.parentEpubType, parentSel.attributeValue);
        } else if (parentSel.type == SelectorType::Id) {
            return block.parentId == parentSel.id;
        } else if (parentSel.type == SelectorType::Universal) {
            return true;
        }
        return false;
    };

    switch (selector.type) {
        case SelectorType::Element:
            if (!selector.className.empty()) {
                return iequals(selector.element, effectiveTag) &&
                       containsClass(block.className, selector.className);
            }
            if (!selector.id.empty()) {
                return iequals(selector.element, effectiveTag) &&
                       block.id == selector.id;
            }
            return iequals(selector.element, effectiveTag);

        case SelectorType::Class:
            return containsClass(block.className, selector.className);

        case SelectorType::Descendant: {
            // The main element/class must match the block
            bool mainMatch = false;
            if (selector.element == "*") {
                mainMatch = true;  // Universal match
            } else if (!selector.element.empty()) {
                mainMatch = iequals(selector.element, effectiveTag);
            } else if (!selector.className.empty()) {
                mainMatch = containsClass(block.className, selector.className);
            }
            if (!mainMatch) return false;

            // Check className on main element (compound like p.class in descendant)
            if (!selector.element.empty() && selector.element != "*" &&
                !selector.className.empty()) {
                if (!containsClass(block.className, selector.className)) return false;
            }

            // Check pseudo-class on main element
            if (!selector.pseudoClass.empty()) {
                if (selector.pseudoClass == "first-child" && !block.isFirstChild) {
                    return false;
                }
            }

            // Parent must match
            if (!selector.parent) return false;
            return parentMatches(*selector.parent);
        }

        case SelectorType::AdjacentSibling: {
            // Main element matches block's tag
            if (selector.element != "*" && !iequals(selector.element, effectiveTag)) return false;
            if (!selector.adjacentSibling) return false;

            // Walk the adjacent sibling chain against previousSiblingTags
            const CSSSelector* sib = selector.adjacentSibling.get();
            size_t sibIdx = 0;
            while (sib) {
                if (sibIdx >= block.previousSiblingTags.size()) return false;
                if (sib->element != "*" &&
                    !iequals(sib->element, block.previousSiblingTags[sibIdx])) return false;
                sib = sib->adjacentSibling.get();
                sibIdx++;
            }

            // Check descendant context (parent) if present
            if (selector.parent) {
                return parentMatches(*selector.parent);
            }
            return true;
        }

        case SelectorType::PseudoFirstChild: {
            if (!iequals(selector.element, effectiveTag)) return false;
            if (!selector.className.empty() && !containsClass(block.className, selector.className)) return false;
            return block.isFirstChild;
        }

        case SelectorType::Attribute: {
            // Check block's epubType or parentEpubType
            return containsClass(block.epubType, selector.attributeValue) ||
                   containsClass(block.parentEpubType, selector.attributeValue);
        }

        case SelectorType::Universal:
            return true;

        case SelectorType::Id:
            return !selector.id.empty() && block.id == selector.id;
    }

    return false;
}

void StyleResolver::applyProperties(
    const CSSProperties& props, BlockComputedStyle& style, float baseFontSize) const {

    if (props.textIndent.has_value()) {
        style.textIndent = props.textIndent.value() * baseFontSize;
    }

    if (props.marginTop.has_value()) {
        style.marginTop = props.marginTop.value() * baseFontSize;
    }

    if (props.marginBottom.has_value()) {
        style.marginBottom = props.marginBottom.value() * baseFontSize;
    }

    if (props.marginLeft.has_value()) {
        style.marginLeft = props.marginLeft.value() * baseFontSize;
    }

    if (props.marginRight.has_value()) {
        style.marginRight = props.marginRight.value() * baseFontSize;
    }

    if (props.textAlign.has_value()) {
        style.alignment = props.textAlign.value();
    }

    if (props.fontStyle.has_value()) {
        style.font.style = props.fontStyle.value();
    }

    if (props.fontWeight.has_value()) {
        style.font.weight = props.fontWeight.value();
    }

    if (props.fontSize.has_value()) {
        style.font.size = props.fontSize.value() * baseFontSize;
    }

    if (props.paddingLeft.has_value()) {
        style.paddingLeft = props.paddingLeft.value() * baseFontSize;
    }

    if (props.fontVariant.has_value()) {
        if (props.fontVariant.value() == FontVariant::SmallCaps) {
            style.smallCaps = true;
        } else {
            style.smallCaps = false;
        }
    }

    if (props.hyphens.has_value()) {
        style.hyphens = props.hyphens.value();
    }

    if (props.display.has_value()) {
        if (props.display.value() == "none") {
            style.display = BlockComputedStyle::Display::None;
            style.hidden = true;
        } else if (props.display.value() == "inline-block") {
            style.display = BlockComputedStyle::Display::InlineBlock;
        } else if (props.display.value() == "block") {
            style.display = BlockComputedStyle::Display::Block;
        }
    }

    if (props.hangingPunctuation.has_value()) {
        style.hangingPunctuation = props.hangingPunctuation.value();
    }

    // HR style: construct from border-top-width and width-percent
    if (props.borderTopWidth.has_value() || props.widthPercent.has_value()) {
        if (!style.hrStyle.has_value()) {
            style.hrStyle = HRStyle{};
        }
        if (props.borderTopWidth.has_value()) {
            style.hrStyle->borderWidth = props.borderTopWidth.value();
        }
        if (props.widthPercent.has_value()) {
            style.hrStyle->widthPercent = props.widthPercent.value();
        }
    }
}

void StyleResolver::applyUserOverrides(
    BlockComputedStyle& style, const Style& userStyle, const Block& block, bool cssFontSizeSet) const {

    // Font family: always override
    style.font.family = userStyle.font.family;

    // Font size: don't override for headings (they should remain proportionally larger),
    // CodeBlock (should remain proportionally smaller), Figcaption,
    // or any block where CSS explicitly set font-size.
    if (!isHeadingType(block.type) && block.type != BlockType::CodeBlock &&
        block.type != BlockType::Figcaption && !cssFontSizeSet) {
        style.font.size = userStyle.font.size;
    }

    // Spacing: always override
    style.lineSpacingMultiplier = userStyle.lineSpacingMultiplier;
    style.letterSpacing = userStyle.letterSpacing;
    style.wordSpacing = userStyle.wordSpacing;
    style.paragraphSpacingAfter = userStyle.paragraphSpacing;

    // Alignment: override UNLESS block is a heading with Center alignment
    if (!(isHeadingType(block.type) && style.alignment == TextAlignment::Center)) {
        style.alignment = userStyle.alignment;
    }

    // Hyphenation: override UNLESS CSS explicitly set hyphens=false
    if (style.hyphens) {
        style.hyphens = userStyle.hyphenation;
    }
    // If CSS set hyphens=false, keep it false regardless of user preference
}

bool StyleResolver::inlineSelectorMatches(
    const CSSSelector& selector, const InlineElement& inl, const Block& parentBlock) const {

    // Check if a tag name is an inline tag
    auto isInlineTag = [](const std::string& tag) -> bool {
        return tag == "a" || tag == "abbr" || tag == "span" ||
               tag == "b" || tag == "i" || tag == "em" || tag == "strong" ||
               tag == "cite" || tag == "code" || tag == "small" ||
               tag == "sub" || tag == "sup";
    };

    switch (selector.type) {
        case SelectorType::Element:
            if (inl.htmlTag.empty()) return false;
            if (!isInlineTag(selector.element)) return false;
            if (!selector.className.empty()) {
                return iequals(selector.element, inl.htmlTag) &&
                       containsClass(inl.className, selector.className);
            }
            return iequals(selector.element, inl.htmlTag);

        case SelectorType::Class:
            return containsClass(inl.className, selector.className);

        case SelectorType::Attribute:
            return containsClass(inl.epubType, selector.attributeValue);

        case SelectorType::Descendant: {
            // Leaf must match inline
            bool leafMatch = false;
            if (selector.element == "*") {
                leafMatch = true;
            } else if (!selector.element.empty()) {
                if (inl.htmlTag.empty()) return false;
                if (!isInlineTag(selector.element)) return false;
                leafMatch = iequals(selector.element, inl.htmlTag);
            } else if (!selector.className.empty()) {
                leafMatch = containsClass(inl.className, selector.className);
            }
            if (!leafMatch) return false;

            // Compound: element + className
            if (!selector.element.empty() && selector.element != "*" &&
                !selector.className.empty()) {
                if (!containsClass(inl.className, selector.className)) return false;
            }

            // Parent must match the block
            if (!selector.parent) return false;
            return selectorMatches(*selector.parent, parentBlock);
        }

        case SelectorType::Universal:
            return true;

        default:
            return false;
    }
}

void StyleResolver::applyInlineProperties(
    const CSSProperties& props, InlineComputedStyle& style) const {

    if (props.fontSize.has_value()) {
        style.fontSizeMultiplier = props.fontSize.value();
    }
    if (props.fontStyle.has_value()) {
        style.fontStyle = props.fontStyle.value();
    }
    if (props.fontWeight.has_value()) {
        style.fontWeight = props.fontWeight.value();
    }
    if (props.fontVariant.has_value()) {
        if (props.fontVariant.value() == FontVariant::SmallCaps) {
            style.smallCaps = true;
        } else {
            style.smallCaps = false;
        }
    }
}

} // namespace typesetting
