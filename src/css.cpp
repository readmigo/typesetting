#include "typesetting/css.h"
#include "typesetting/log.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace typesetting {

namespace {

/// Trim whitespace from both ends
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

/// Remove all CSS comments (/* ... */)
std::string stripComments(const std::string& css) {
    std::string result;
    result.reserve(css.size());
    size_t i = 0;
    while (i < css.size()) {
        if (i + 1 < css.size() && css[i] == '/' && css[i + 1] == '*') {
            auto end = css.find("*/", i + 2);
            if (end == std::string::npos) break;
            i = end + 2;
        } else {
            result += css[i];
            ++i;
        }
    }
    return result;
}

/// Parse a float value from a CSS value string (e.g., "1em", "2.5px", "25%", "0")
/// Returns the numeric part, and sets the unit suffix via reference.
bool parseNumericValue(const std::string& val, float& number, std::string& unit) {
    if (val.empty()) return false;

    size_t numEnd = 0;
    bool hasDigit = false;
    bool hasDot = false;
    if (numEnd < val.size() && val[numEnd] == '-') ++numEnd;
    while (numEnd < val.size()) {
        if (std::isdigit(static_cast<unsigned char>(val[numEnd]))) {
            hasDigit = true;
            ++numEnd;
        } else if (val[numEnd] == '.' && !hasDot) {
            hasDot = true;
            ++numEnd;
        } else {
            break;
        }
    }

    if (!hasDigit) return false;

    number = std::stof(val.substr(0, numEnd));
    unit = val.substr(numEnd);
    return true;
}

/// Parse a single CSS selector token (no combinators, no whitespace)
CSSSelector parseSingleToken(const std::string& t) {
    if (t.empty()) return {};

    // Attribute selector: [epub|type~="value"]
    if (t.front() == '[') {
        CSSSelector result;
        result.type = SelectorType::Attribute;
        std::string inner = t.substr(1);
        if (!inner.empty() && inner.back() == ']') {
            inner.pop_back();
        }
        auto tildeEq = inner.find("~=");
        auto eq = inner.find('=');
        if (tildeEq != std::string::npos) {
            result.attribute = inner.substr(0, tildeEq);
            std::string attrVal = inner.substr(tildeEq + 2);
            if (attrVal.size() >= 2 &&
                (attrVal.front() == '"' || attrVal.front() == '\'')) {
                attrVal = attrVal.substr(1, attrVal.size() - 2);
            }
            result.attributeValue = attrVal;
        } else if (eq != std::string::npos) {
            result.attribute = inner.substr(0, eq);
            std::string attrVal = inner.substr(eq + 1);
            if (attrVal.size() >= 2 &&
                (attrVal.front() == '"' || attrVal.front() == '\'')) {
                attrVal = attrVal.substr(1, attrVal.size() - 2);
            }
            result.attributeValue = attrVal;
        }
        std::replace(result.attribute.begin(), result.attribute.end(), '|', ':');
        return result;
    }

    // Universal selector
    if (t == "*") {
        CSSSelector result;
        result.type = SelectorType::Universal;
        result.element = "*";
        return result;
    }

    // ID selector: #id
    if (t.front() == '#') {
        CSSSelector result;
        result.type = SelectorType::Id;
        result.id = t.substr(1);
        return result;
    }

    // Compound with ID: element#id or .class#id
    {
        auto hashPos = t.find('#');
        if (hashPos != std::string::npos && hashPos > 0) {
            std::string idPart = t.substr(hashPos + 1);
            std::string beforeHash = t.substr(0, hashPos);
            CSSSelector result;
            result.id = idPart;
            auto dotPos = beforeHash.find('.');
            if (dotPos == 0) {
                result.type = SelectorType::Class;
                result.className = beforeHash.substr(1);
            } else if (dotPos != std::string::npos) {
                result.type = SelectorType::Element;
                result.element = beforeHash.substr(0, dotPos);
                result.className = beforeHash.substr(dotPos + 1);
            } else {
                result.type = SelectorType::Element;
                result.element = beforeHash;
            }
            return result;
        }
    }

    // Compound selector: element.classname or element.classname:pseudo
    {
        auto dotPos = t.find('.');
        if (dotPos != std::string::npos && dotPos > 0) {
            CSSSelector result;
            std::string beforeDot = t.substr(0, dotPos);
            std::string afterDot = t.substr(dotPos + 1);

            auto colonPos = afterDot.find(':');
            if (colonPos != std::string::npos) {
                result.type = SelectorType::PseudoFirstChild;
                result.element = beforeDot;
                result.className = afterDot.substr(0, colonPos);
                result.pseudoClass = afterDot.substr(colonPos + 1);
            } else {
                result.type = SelectorType::Element;
                result.element = beforeDot;
                result.className = afterDot;
            }
            return result;
        }
    }

    // Pseudo-class: element:first-child
    auto colonPos = t.find(':');
    if (colonPos != std::string::npos) {
        CSSSelector result;
        result.type = SelectorType::PseudoFirstChild;
        result.element = t.substr(0, colonPos);
        result.pseudoClass = t.substr(colonPos + 1);
        return result;
    }

    // Class selector: .classname
    if (t.front() == '.') {
        CSSSelector result;
        result.type = SelectorType::Class;
        result.className = t.substr(1);
        return result;
    }

    // Element selector
    CSSSelector result;
    result.type = SelectorType::Element;
    result.element = t;
    return result;
}

/// Parse a CSS selector string into a CSSSelector, handling combinators (+, >, space)
CSSSelector parseSelector(const std::string& selectorStr) {
    std::string sel = trim(selectorStr);
    if (sel.empty()) return {};

    // Tokenize by whitespace
    std::vector<std::string> rawTokens;
    {
        std::istringstream iss(sel);
        std::string t;
        while (iss >> t) rawTokens.push_back(t);
    }

    if (rawTokens.size() == 1) {
        return parseSingleToken(rawTokens[0]);
    }

    // Build parts: non-combinator tokens with their combinator-to-next.
    // Combinator tokens (+, >) are consumed and applied to the preceding part.
    struct SelectorPart {
        std::string token;
        char combinatorToNext; // '+', '>', ' ', or 0 for last
    };
    std::vector<SelectorPart> parts;
    for (size_t i = 0; i < rawTokens.size(); ++i) {
        if (rawTokens[i] == "+" || rawTokens[i] == ">") {
            if (!parts.empty()) {
                parts.back().combinatorToNext = rawTokens[i][0];
            }
        } else {
            parts.push_back({rawTokens[i], ' '});
        }
    }
    if (!parts.empty()) parts.back().combinatorToNext = 0;

    if (parts.empty()) return {};
    if (parts.size() == 1) return parseSingleToken(parts[0].token);

    // Find where the adjacent sibling chain starts (rightmost consecutive '+' group)
    int siblingChainStart = static_cast<int>(parts.size()) - 1;
    for (int i = static_cast<int>(parts.size()) - 2; i >= 0; --i) {
        if (parts[i].combinatorToNext == '+') {
            siblingChainStart = i;
        } else {
            break;
        }
    }

    // Parse the main element (rightmost part)
    CSSSelector result = parseSingleToken(parts.back().token);

    // Build adjacent sibling chain if present
    if (siblingChainStart < static_cast<int>(parts.size()) - 1) {
        result.type = SelectorType::AdjacentSibling;
        std::shared_ptr<CSSSelector>* siblingPtr = &result.adjacentSibling;
        for (int i = static_cast<int>(parts.size()) - 2; i >= siblingChainStart; --i) {
            auto sib = std::make_shared<CSSSelector>(parseSingleToken(parts[i].token));
            *siblingPtr = sib;
            siblingPtr = &sib->adjacentSibling;
        }
    }

    // Build parent/ancestor chain from parts[0..siblingChainStart-1]
    if (siblingChainStart > 0) {
        if (result.type != SelectorType::AdjacentSibling) {
            result.type = SelectorType::Descendant;
        }
        int parentIdx = siblingChainStart - 1;
        auto parent = std::make_shared<CSSSelector>(parseSingleToken(parts[parentIdx].token));
        result.parent = parent;
        if (parts[parentIdx].combinatorToNext == '>') {
            result.isChildCombinator = true;
        }

        // Build further ancestor chain if multiple ancestor tokens
        std::shared_ptr<CSSSelector> currentParent = parent;
        for (int i = parentIdx - 1; i >= 0; --i) {
            auto pp = std::make_shared<CSSSelector>(parseSingleToken(parts[i].token));
            currentParent->parent = pp;
            currentParent = pp;
        }
    } else if (result.type != SelectorType::AdjacentSibling) {
        // All parts connected by space/> with no '+': pure descendant
        result.type = SelectorType::Descendant;
        auto parent = std::make_shared<CSSSelector>(parseSingleToken(parts[0].token));
        result.parent = parent;
    }

    return result;
}

/// Parse CSS property declarations from a declaration block string
CSSProperties parseProperties(const std::string& block) {
    CSSProperties props;

    // Split by semicolons
    std::istringstream iss(block);
    std::string declaration;
    while (std::getline(iss, declaration, ';')) {
        declaration = trim(declaration);
        if (declaration.empty()) continue;

        // Split by first colon
        auto colonPos = declaration.find(':');
        if (colonPos == std::string::npos) continue;

        std::string property = trim(declaration.substr(0, colonPos));
        std::string value = trim(declaration.substr(colonPos + 1));

        // Lowercase the property name
        std::transform(property.begin(), property.end(), property.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (property == "text-indent") {
            float num;
            std::string unit;
            if (parseNumericValue(value, num, unit)) {
                props.textIndent = num;
            }
        } else if (property == "text-align") {
            if (value == "center") props.textAlign = TextAlignment::Center;
            else if (value == "left") props.textAlign = TextAlignment::Left;
            else if (value == "right") props.textAlign = TextAlignment::Right;
            else if (value == "justify") props.textAlign = TextAlignment::Justified;
        } else if (property == "font-style") {
            if (value == "italic") props.fontStyle = FontStyle::Italic;
            else if (value == "normal") props.fontStyle = FontStyle::Normal;
        } else if (property == "font-weight") {
            if (value == "bold") {
                props.fontWeight = FontWeight::Bold;
            } else if (value == "normal") {
                props.fontWeight = FontWeight::Regular;
            } else {
                float num;
                std::string unit;
                if (parseNumericValue(value, num, unit)) {
                    props.fontWeight = static_cast<FontWeight>(static_cast<uint16_t>(num));
                }
            }
        } else if (property == "font-variant") {
            if (value == "small-caps") props.fontVariant = FontVariant::SmallCaps;
            else if (value == "normal") props.fontVariant = FontVariant::Normal;
        } else if (property == "font-size") {
            if (value == "smaller") {
                props.fontSize = 0.833f;  // ~5/6
            } else if (value == "larger") {
                props.fontSize = 1.2f;
            } else {
                float num;
                std::string unit;
                if (parseNumericValue(value, num, unit)) {
                    if (unit == "em" || unit == "rem") {
                        props.fontSize = num;
                    } else if (unit == "px") {
                        // Store as em relative to 16px base; actual conversion
                        // happens in style_resolver using real baseFontSize
                        props.fontSize = num / 16.0f;
                    } else if (unit == "%" ) {
                        props.fontSize = num / 100.0f;
                    } else if (unit.empty()) {
                        props.fontSize = num;  // unitless treated as em
                    }
                }
            }
        } else if (property == "hyphens") {
            if (value == "auto") props.hyphens = true;
            else if (value == "none") props.hyphens = false;
        } else if (property == "display") {
            if (value == "none" || value == "block" || value == "inline-block") {
                props.display = value;
            }
        } else if (property == "hanging-punctuation") {
            if (value == "first" || value == "first last" || value == "last") {
                props.hangingPunctuation = true;
            } else if (value == "none") {
                props.hangingPunctuation = false;
            }
        } else if (property == "margin") {
            // Margin shorthand: 1-4 values
            std::vector<float> values;
            std::istringstream valStream(value);
            std::string part;
            while (valStream >> part) {
                // Skip "auto" keyword
                if (part == "auto") {
                    values.push_back(0.0f);
                    continue;
                }
                float num;
                std::string unit;
                if (parseNumericValue(part, num, unit)) {
                    values.push_back(num);
                }
            }
            if (values.size() == 1) {
                // margin: X -> all sides
                props.marginTop = values[0];
                props.marginRight = values[0];
                props.marginBottom = values[0];
                props.marginLeft = values[0];
            } else if (values.size() == 2) {
                // margin: Y X -> top/bottom=Y, left/right=X
                props.marginTop = values[0];
                props.marginBottom = values[0];
                props.marginRight = values[1];
                props.marginLeft = values[1];
            } else if (values.size() == 3) {
                // margin: T X B
                props.marginTop = values[0];
                props.marginRight = values[1];
                props.marginLeft = values[1];
                props.marginBottom = values[2];
            } else if (values.size() >= 4) {
                // margin: T R B L
                props.marginTop = values[0];
                props.marginRight = values[1];
                props.marginBottom = values[2];
                props.marginLeft = values[3];
            }
        } else if (property == "margin-top") {
            float num;
            std::string unit;
            if (parseNumericValue(value, num, unit)) {
                props.marginTop = num;
            }
        } else if (property == "margin-bottom") {
            float num;
            std::string unit;
            if (parseNumericValue(value, num, unit)) {
                props.marginBottom = num;
            }
        } else if (property == "margin-left") {
            float num;
            std::string unit;
            if (parseNumericValue(value, num, unit)) {
                props.marginLeft = num;
            }
        } else if (property == "margin-right") {
            float num;
            std::string unit;
            if (parseNumericValue(value, num, unit)) {
                props.marginRight = num;
            }
        } else if (property == "padding-left") {
            float num;
            std::string unit;
            if (parseNumericValue(value, num, unit)) {
                props.paddingLeft = num;
            }
        } else if (property == "text-transform") {
            if (value == "uppercase") props.textTransform = TextTransform::Uppercase;
            else if (value == "lowercase") props.textTransform = TextTransform::Lowercase;
            else if (value == "capitalize") props.textTransform = TextTransform::Capitalize;
            else if (value == "none") props.textTransform = TextTransform::None;
        } else if (property == "vertical-align") {
            if (value == "super" || value == "sub" || value == "baseline") {
                props.verticalAlign = value;
            }
        } else if (property == "white-space") {
            if (value == "nowrap" || value == "normal") {
                props.whiteSpace = value;
            }
        } else if (property == "font-variant-numeric") {
            if (value == "oldstyle-nums") props.fontVariantNumeric = true;
            else if (value == "normal") props.fontVariantNumeric = false;
        } else if (property == "border-top") {
            // border-top: 1px solid ...
            std::istringstream btStream(value);
            std::string part;
            while (btStream >> part) {
                float num;
                std::string unit;
                if (parseNumericValue(part, num, unit) && unit == "px") {
                    props.borderTopWidth = num;
                    break;
                }
            }
        } else if (property == "border-top-width") {
            float num;
            std::string unit;
            if (parseNumericValue(value, num, unit)) {
                props.borderTopWidth = num;
            }
        } else if (property == "width") {
            float num;
            std::string unit;
            if (parseNumericValue(value, num, unit) && unit == "%") {
                props.widthPercent = num;
            }
        } else if (property == "max-width") {
            float num;
            std::string unit;
            if (parseNumericValue(value, num, unit) && unit == "%") {
                props.maxWidthPercent = num;
            }
        }
    }

    return props;
}

} // anonymous namespace

// --- CSSSelector ---

int CSSSelector::specificity() const {
    int ids = 0;
    int classes = 0;  // classes + attributes + pseudo-classes
    int elements = 0;

    // Count this selector's contributions
    switch (type) {
        case SelectorType::Element:
            elements += 1;
            if (!className.empty()) classes += 1;
            break;
        case SelectorType::Class:
            classes += 1;
            break;
        case SelectorType::Descendant:
            if (!element.empty() && element != "*") elements += 1;
            if (!className.empty()) classes += 1;
            if (!pseudoClass.empty()) classes += 1;
            break;
        case SelectorType::AdjacentSibling:
            if (!element.empty() && element != "*") elements += 1;
            if (!className.empty()) classes += 1;
            break;
        case SelectorType::PseudoFirstChild:
            if (!element.empty()) elements += 1;
            classes += 1;  // pseudo-class counts as class-level
            break;
        case SelectorType::Attribute:
            classes += 1;
            break;
        case SelectorType::Universal:
            // Universal selector adds 0
            break;
        case SelectorType::Id:
            ids += 1;
            break;
    }

    // ID component adds to specificity regardless of primary type
    if (!id.empty() && type != SelectorType::Id) ids += 1;

    // Add parent specificity
    if (parent) {
        int parentSpec = parent->specificity();
        ids += parentSpec / 100;
        classes += (parentSpec / 10) % 10;
        elements += parentSpec % 10;
    }

    // Add adjacent sibling specificity
    if (adjacentSibling) {
        int sibSpec = adjacentSibling->specificity();
        ids += sibSpec / 100;
        classes += (sibSpec / 10) % 10;
        elements += sibSpec % 10;
    }

    return ids * 100 + classes * 10 + elements;
}

// --- CSSProperties ---

void CSSProperties::merge(const CSSProperties& other) {
    if (other.textIndent.has_value()) textIndent = other.textIndent;
    if (other.marginTop.has_value()) marginTop = other.marginTop;
    if (other.marginBottom.has_value()) marginBottom = other.marginBottom;
    if (other.marginLeft.has_value()) marginLeft = other.marginLeft;
    if (other.marginRight.has_value()) marginRight = other.marginRight;
    if (other.textAlign.has_value()) textAlign = other.textAlign;
    if (other.fontStyle.has_value()) fontStyle = other.fontStyle;
    if (other.fontWeight.has_value()) fontWeight = other.fontWeight;
    if (other.fontVariant.has_value()) fontVariant = other.fontVariant;
    if (other.fontSize.has_value()) fontSize = other.fontSize;
    if (other.hyphens.has_value()) hyphens = other.hyphens;
    if (other.display.has_value()) display = other.display;
    if (other.paddingLeft.has_value()) paddingLeft = other.paddingLeft;
    if (other.hangingPunctuation.has_value()) hangingPunctuation = other.hangingPunctuation;
    if (other.textTransform.has_value()) textTransform = other.textTransform;
    if (other.verticalAlign.has_value()) verticalAlign = other.verticalAlign;
    if (other.whiteSpace.has_value()) whiteSpace = other.whiteSpace;
    if (other.fontVariantNumeric.has_value()) fontVariantNumeric = other.fontVariantNumeric;
    if (other.borderTopWidth.has_value()) borderTopWidth = other.borderTopWidth;
    if (other.widthPercent.has_value()) widthPercent = other.widthPercent;
    if (other.maxWidthPercent.has_value()) maxWidthPercent = other.maxWidthPercent;
}

// --- CSSStylesheet ---

CSSStylesheet CSSStylesheet::parse(const std::string& css) {
    CSSStylesheet sheet;

    // Step 1: Strip comments
    std::string cleaned = stripComments(css);

    // Step 2: Process the cleaned CSS character by character
    size_t pos = 0;
    while (pos < cleaned.size()) {
        // Skip whitespace
        while (pos < cleaned.size() && std::isspace(static_cast<unsigned char>(cleaned[pos]))) {
            ++pos;
        }
        if (pos >= cleaned.size()) break;

        // Skip @-rules (e.g., @media, @supports, @font-face, @namespace)
        if (cleaned[pos] == '@') {
            // Find the matching closing brace or semicolon
            auto semiPos = cleaned.find(';', pos);
            auto bracePos = cleaned.find('{', pos);

            if (bracePos != std::string::npos &&
                (semiPos == std::string::npos || bracePos < semiPos)) {
                // @-rule with block: skip to matching closing brace
                int depth = 1;
                size_t p = bracePos + 1;
                while (p < cleaned.size() && depth > 0) {
                    if (cleaned[p] == '{') ++depth;
                    else if (cleaned[p] == '}') --depth;
                    ++p;
                }
                pos = p;
            } else if (semiPos != std::string::npos) {
                // @-rule ending with semicolon (e.g., @namespace)
                pos = semiPos + 1;
            } else {
                break;
            }
            continue;
        }

        // Find the opening brace for a rule block
        auto openBrace = cleaned.find('{', pos);
        if (openBrace == std::string::npos) break;

        // Find the matching closing brace
        auto closeBrace = cleaned.find('}', openBrace + 1);
        if (closeBrace == std::string::npos) break;

        std::string selectorPart = trim(cleaned.substr(pos, openBrace - pos));
        std::string declarationBlock = trim(cleaned.substr(openBrace + 1, closeBrace - openBrace - 1));

        if (!selectorPart.empty() && !declarationBlock.empty()) {
            CSSProperties properties = parseProperties(declarationBlock);

            // Handle comma-separated selectors
            std::vector<std::string> selectors;
            size_t start = 0;
            for (size_t i = 0; i < selectorPart.size(); ++i) {
                if (selectorPart[i] == ',') {
                    selectors.push_back(trim(selectorPart.substr(start, i - start)));
                    start = i + 1;
                }
            }
            selectors.push_back(trim(selectorPart.substr(start)));

            for (const auto& selStr : selectors) {
                if (selStr.empty()) continue;
                CSSRule rule;
                rule.selector = parseSelector(selStr);
                rule.properties = properties;
                sheet.rules.push_back(rule);
            }
        }

        pos = closeBrace + 1;
    }

    TS_LOGI("CSSStylesheet::parse: css=%zu rules=%zu", css.size(), sheet.rules.size());
    return sheet;
}

} // namespace typesetting
