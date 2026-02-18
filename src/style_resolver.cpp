#include "typesetting/style_resolver.h"
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

std::vector<BlockComputedStyle> StyleResolver::resolve(
    const std::vector<Block>& blocks,
    const Style& userStyle) const {

    std::vector<BlockComputedStyle> result;
    result.reserve(blocks.size());

    for (const auto& block : blocks) {
        // 1. Start with defaults based on BlockType
        BlockComputedStyle style = defaultStyleForBlock(block, userStyle);

        // 2. Collect matching CSS rules
        std::vector<const CSSRule*> matches;
        for (const auto& rule : stylesheet_.rules) {
            if (selectorMatches(rule.selector, block)) {
                matches.push_back(&rule);
            }
        }

        // 3. Sort by specificity (ascending — lower specificity applied first)
        std::sort(matches.begin(), matches.end(),
                  [](const CSSRule* a, const CSSRule* b) {
                      return a->selector.specificity() < b->selector.specificity();
                  });

        // 4. Apply each matched rule's properties in order
        float baseFontSize = userStyle.font.size;
        for (const auto* rule : matches) {
            applyProperties(rule->properties, style, baseFontSize);
        }

        // 5. Apply user overrides as final layer
        applyUserOverrides(style, userStyle, block);

        result.push_back(std::move(style));
    }

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

    switch (selector.type) {
        case SelectorType::Element:
            return iequals(selector.element, effectiveTag);

        case SelectorType::Class:
            return containsClass(block.className, selector.className);

        case SelectorType::Descendant: {
            // The main element/class must match the block
            bool mainMatch = false;
            if (!selector.element.empty()) {
                mainMatch = iequals(selector.element, effectiveTag);
            } else if (!selector.className.empty()) {
                mainMatch = containsClass(block.className, selector.className);
            }
            if (!mainMatch) return false;

            // Check pseudo-class on main element
            if (!selector.pseudoClass.empty()) {
                if (selector.pseudoClass == "first-child" && !block.isFirstChild) {
                    return false;
                }
            }

            // Parent must match
            if (!selector.parent) return false;

            const auto& parentSel = *selector.parent;
            if (parentSel.type == SelectorType::Element) {
                return iequals(parentSel.element, block.parentTag);
            } else if (parentSel.type == SelectorType::Class) {
                return containsClass(block.parentClassName, parentSel.className);
            } else if (parentSel.type == SelectorType::Attribute) {
                // Check parent's epub:type
                std::string attrToCheck = block.parentEpubType;
                return containsClass(attrToCheck, parentSel.attributeValue);
            }
            return false;
        }

        case SelectorType::AdjacentSibling: {
            // Main element matches block's tag
            if (!iequals(selector.element, effectiveTag)) return false;
            // Adjacent sibling matches previousSiblingTag
            if (!selector.adjacentSibling) return false;
            return iequals(selector.adjacentSibling->element, block.previousSiblingTag);
        }

        case SelectorType::PseudoFirstChild: {
            if (!iequals(selector.element, effectiveTag)) return false;
            return block.isFirstChild;
        }

        case SelectorType::Attribute: {
            // Check block's epubType or parentEpubType
            return containsClass(block.epubType, selector.attributeValue) ||
                   containsClass(block.parentEpubType, selector.attributeValue);
        }

        case SelectorType::Universal:
            return true;
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

    if (props.displayNone.has_value() && props.displayNone.value()) {
        style.hidden = true;
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
    BlockComputedStyle& style, const Style& userStyle, const Block& block) const {

    // Font family: always override
    style.font.family = userStyle.font.family;

    // Font size: don't override for headings (they should remain proportionally larger)
    // or for CodeBlock (should remain proportionally smaller).
    // The base font size was already used in em conversion.
    if (!isHeadingType(block.type) && block.type != BlockType::CodeBlock &&
        block.type != BlockType::Figcaption) {
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

} // namespace typesetting
