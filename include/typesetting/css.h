#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

#include "typesetting/platform.h"
#include "typesetting/style.h"

namespace typesetting {

/// Bit flags indicating which CSS properties were declared with !important
enum : uint32_t {
    kImpTextIndent      = 1u << 0,
    kImpMarginTop       = 1u << 1,
    kImpMarginBottom    = 1u << 2,
    kImpMarginLeft      = 1u << 3,
    kImpMarginRight     = 1u << 4,
    kImpTextAlign       = 1u << 5,
    kImpFontStyle       = 1u << 6,
    kImpFontWeight      = 1u << 7,
    kImpFontVariant     = 1u << 8,
    kImpFontSize        = 1u << 9,
    kImpHyphens         = 1u << 10,
    kImpDisplay         = 1u << 11,
    kImpPaddingLeft     = 1u << 12,
    kImpHangingPunct    = 1u << 13,
    kImpTextTransform   = 1u << 14,
    kImpVerticalAlign   = 1u << 15,
    kImpWhiteSpace      = 1u << 16,
    kImpFontVariantNum  = 1u << 17,
    kImpBorderTopWidth  = 1u << 18,
    kImpWidthPercent    = 1u << 19,
    kImpMaxWidthPercent = 1u << 20,
    kImpMarginLeftAuto  = 1u << 21,
    kImpMarginRightAuto = 1u << 22,
    kImpLineHeight      = 1u << 23,
};

enum class SelectorType {
    Element,           // p, h2, blockquote
    Class,             // .classname
    Descendant,        // parent child (space separated)
    AdjacentSibling,   // prev + next
    PseudoFirstChild,  // element:first-child
    Attribute,         // [epub|type~="value"]
    Universal,         // *
    Id,                // #id
};

enum class FontVariant {
    Normal,
    SmallCaps,
};

struct CSSSelector {
    SelectorType type = SelectorType::Element;
    std::string element;         // Tag name (for Element, PseudoFirstChild)
    std::string className;       // Class name (for Class type)
    std::string pseudoClass;     // e.g. "first-child"
    std::string attribute;       // Attribute name (for Attribute type)
    std::string attributeValue;  // Attribute value
    std::string id;              // ID value (for #id selectors)
    bool isChildCombinator = false;  // True if combinator with parent is > (child) vs space (descendant)

    // For compound selectors
    std::shared_ptr<CSSSelector> parent;           // For Descendant: the ancestor selector
    std::shared_ptr<CSSSelector> adjacentSibling;  // For AdjacentSibling: the preceding sibling

    int specificity() const;
};

struct CSSProperties {
    std::optional<float> textIndent;       // in em units
    std::optional<float> marginTop;        // in em units
    std::optional<float> marginBottom;     // in em units
    std::optional<float> marginLeft;       // in em units
    std::optional<float> marginRight;      // in em units
    std::optional<TextAlignment> textAlign;
    std::optional<FontStyle> fontStyle;
    std::optional<FontWeight> fontWeight;
    std::optional<FontVariant> fontVariant;
    std::optional<float> fontSize;         // in em units (relative multiplier)
    std::optional<bool> hyphens;           // true = auto, false = none
    std::optional<std::string> display;    // "none", "block", "inline-block"
    std::optional<float> paddingLeft;     // in em units
    std::optional<bool> hangingPunctuation;
    std::optional<TextTransform> textTransform;
    std::optional<std::string> verticalAlign;   // "super", "sub", "baseline"
    std::optional<std::string> whiteSpace;      // "nowrap", "normal"
    std::optional<bool> fontVariantNumeric;     // true = oldstyle-nums
    std::optional<float> borderTopWidth;   // in px
    std::optional<float> widthPercent;     // percentage (0-100)
    std::optional<float> maxWidthPercent;   // percentage (0-100)
    std::optional<bool> marginLeftAuto;    // true if margin-left is "auto"
    std::optional<bool> marginRightAuto;   // true if margin-right is "auto"
    std::optional<float> lineHeight;       // multiplier (1.5 = 150% of font-size; negative = px value)

    uint32_t importantFlags = 0;  // Bitfield of kImp* flags

    /// Merge another set of properties into this one (other overrides)
    void merge(const CSSProperties& other);
};

struct CSSRule {
    CSSSelector selector;
    CSSProperties properties;
};

class CSSStylesheet {
public:
    std::vector<CSSRule> rules;

    /// Parse a CSS string into a stylesheet
    static CSSStylesheet parse(const std::string& css);
};

} // namespace typesetting
