#pragma once

#include "typesetting/platform.h"
#include <cstdint>
#include <optional>

namespace typesetting {

/// Text alignment options
enum class TextAlignment {
    Left,
    Center,
    Right,
    Justified,
};

/// Complete set of typesetting style parameters.
/// Maps to the reader settings in iOS (ThemeManager) and Android.
struct Style {
    // Font
    FontDescriptor font;

    // Spacing
    float lineSpacingMultiplier = 1.4f;   // CSS line-height equivalent
    float letterSpacing = 0;               // Extra space between characters (px)
    float wordSpacing = 0;                 // Extra space between words (px)
    float paragraphSpacing = 12.0f;        // Space between paragraphs (px)

    // Alignment & hyphenation
    TextAlignment alignment = TextAlignment::Justified;
    bool hyphenation = true;
    std::string locale = "en";

    // Indentation
    float textIndent = 0;                  // First-line indent (px)

    // Page margins
    float marginTop = 50.0f;
    float marginBottom = 40.0f;
    float marginLeft = 20.0f;
    float marginRight = 20.0f;

    /// Computed line height based on font size and multiplier
    float lineHeight() const {
        return font.size * lineSpacingMultiplier;
    }

    /// Available content width given a page width
    float contentWidth(float pageWidth) const {
        return pageWidth - marginLeft - marginRight;
    }

    /// Available content height given a page height
    float contentHeight(float pageHeight) const {
        return pageHeight - marginTop - marginBottom;
    }
};

/// Horizontal rule visual properties
struct HRStyle {
    float borderWidth = 1.0f;
    float widthPercent = 25.0f;
    float marginTopEm = 1.5f;
    float marginBottomEm = 1.5f;
};

/// Computed style for a single block, combining CSS rules + user Style
struct BlockComputedStyle {
    FontDescriptor font;

    // Text layout
    float textIndent = 0;
    TextAlignment alignment = TextAlignment::Justified;
    bool hyphens = true;
    bool smallCaps = false;
    enum class Display { Block, None, InlineBlock };
    Display display = Display::Block;
    bool hidden = false;  // display: none

    // Spacing
    float lineSpacingMultiplier = 1.4f;
    float letterSpacing = 0;
    float wordSpacing = 0;
    float paragraphSpacingAfter = 12.0f;

    // Block margins (from CSS, in px after em conversion)
    float marginTop = 0;
    float marginBottom = 0;
    float marginLeft = 0;
    float marginRight = 0;
    float paddingLeft = 0;

    // Advanced typography
    bool oldstyleNums = false;
    bool hangingPunctuation = false;

    // HR specific
    std::optional<HRStyle> hrStyle;
};

} // namespace typesetting
