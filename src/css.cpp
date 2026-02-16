#include "typesetting/css.h"
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

/// Parse a single CSS selector string into a CSSSelector
CSSSelector parseSelector(const std::string& selectorStr) {
    std::string sel = trim(selectorStr);
    if (sel.empty()) return {};

    // Check for adjacent sibling combinator: "h2 + p"
    auto plusPos = sel.find('+');
    if (plusPos != std::string::npos) {
        std::string leftStr = trim(sel.substr(0, plusPos));
        std::string rightStr = trim(sel.substr(plusPos + 1));

        CSSSelector result;
        result.type = SelectorType::AdjacentSibling;
        result.element = rightStr;

        auto sibling = std::make_shared<CSSSelector>();
        sibling->type = SelectorType::Element;
        sibling->element = leftStr;
        result.adjacentSibling = sibling;
        return result;
    }

    // Check for descendant (space-separated tokens)
    // Tokenize by whitespace
    std::vector<std::string> tokens;
    std::istringstream iss(sel);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.size() == 1) {
        // Single token: could be element, class, pseudo, or attribute
        const auto& t = tokens[0];

        // Attribute selector: [epub|type~="value"]
        if (t.front() == '[') {
            CSSSelector result;
            result.type = SelectorType::Attribute;
            // Parse attribute: [name~="value"] or [name="value"]
            std::string inner = t.substr(1);
            if (!inner.empty() && inner.back() == ']') {
                inner.pop_back();
            }
            // Find operator
            auto tildeEq = inner.find("~=");
            auto eq = inner.find('=');
            if (tildeEq != std::string::npos) {
                result.attribute = inner.substr(0, tildeEq);
                std::string attrVal = inner.substr(tildeEq + 2);
                // Remove quotes
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
            // Convert pipe to colon for namespace: epub|type -> epub:type
            std::replace(result.attribute.begin(), result.attribute.end(), '|', ':');
            return result;
        }

        // Universal selector
        if (t == "*") {
            CSSSelector result;
            result.type = SelectorType::Universal;
            return result;
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

    // Multiple tokens: descendant selector (parent child)
    // The last token is the main element, previous tokens form parent chain
    CSSSelector result;
    result.type = SelectorType::Descendant;

    // Parse the last token as the main element
    const auto& lastToken = tokens.back();

    // Check if last token has pseudo-class
    auto colonPos = lastToken.find(':');
    if (colonPos != std::string::npos) {
        result.element = lastToken.substr(0, colonPos);
        result.pseudoClass = lastToken.substr(colonPos + 1);
    } else if (lastToken.front() == '.') {
        result.className = lastToken.substr(1);
    } else {
        result.element = lastToken;
    }

    // Build parent chain from remaining tokens (right to left)
    std::shared_ptr<CSSSelector> parentSel;
    for (int i = static_cast<int>(tokens.size()) - 2; i >= 0; --i) {
        auto parent = std::make_shared<CSSSelector>();
        const auto& pt = tokens[i];

        if (pt.front() == '.') {
            parent->type = SelectorType::Class;
            parent->className = pt.substr(1);
        } else if (pt.front() == '[') {
            parent->type = SelectorType::Attribute;
            // Simplified attribute parsing for parent
            std::string inner = pt.substr(1);
            if (!inner.empty() && inner.back() == ']') inner.pop_back();
            auto tildeEq = inner.find("~=");
            auto eq = inner.find('=');
            if (tildeEq != std::string::npos) {
                parent->attribute = inner.substr(0, tildeEq);
                std::string attrVal = inner.substr(tildeEq + 2);
                if (attrVal.size() >= 2 &&
                    (attrVal.front() == '"' || attrVal.front() == '\'')) {
                    attrVal = attrVal.substr(1, attrVal.size() - 2);
                }
                parent->attributeValue = attrVal;
            } else if (eq != std::string::npos) {
                parent->attribute = inner.substr(0, eq);
                std::string attrVal = inner.substr(eq + 1);
                if (attrVal.size() >= 2 &&
                    (attrVal.front() == '"' || attrVal.front() == '\'')) {
                    attrVal = attrVal.substr(1, attrVal.size() - 2);
                }
                parent->attributeValue = attrVal;
            }
            std::replace(parent->attribute.begin(), parent->attribute.end(), '|', ':');
        } else {
            parent->type = SelectorType::Element;
            parent->element = pt;
        }

        if (parentSel) {
            parent->parent = parentSel;
        }
        parentSel = parent;
    }
    result.parent = parentSel;

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
        } else if (property == "hyphens") {
            if (value == "auto") props.hyphens = true;
            else if (value == "none") props.hyphens = false;
        } else if (property == "display") {
            if (value == "none") props.displayNone = true;
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
            break;
        case SelectorType::Class:
            classes += 1;
            break;
        case SelectorType::Descendant:
            if (!element.empty()) elements += 1;
            if (!className.empty()) classes += 1;
            if (!pseudoClass.empty()) classes += 1;
            break;
        case SelectorType::AdjacentSibling:
            if (!element.empty()) elements += 1;
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
    }

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
    if (other.hyphens.has_value()) hyphens = other.hyphens;
    if (other.displayNone.has_value()) displayNone = other.displayNone;
    if (other.hangingPunctuation.has_value()) hangingPunctuation = other.hangingPunctuation;
    if (other.borderTopWidth.has_value()) borderTopWidth = other.borderTopWidth;
    if (other.widthPercent.has_value()) widthPercent = other.widthPercent;
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

    return sheet;
}

} // namespace typesetting
