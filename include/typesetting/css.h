#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

#include "typesetting/platform.h"
#include "typesetting/style.h"

namespace typesetting {

enum class SelectorType {
    Element,           // p, h2, blockquote
    Class,             // .classname
    Descendant,        // parent child (space separated)
    AdjacentSibling,   // prev + next
    PseudoFirstChild,  // element:first-child
    Attribute,         // [epub|type~="value"]
    Universal,         // *
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
    std::optional<bool> hyphens;           // true = auto, false = none
    std::optional<bool> displayNone;
    std::optional<bool> hangingPunctuation;
    std::optional<float> borderTopWidth;   // in px
    std::optional<float> widthPercent;     // percentage (0-100)

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
