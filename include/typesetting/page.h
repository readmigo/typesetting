#pragma once

#include "typesetting/platform.h"
#include <string>
#include <vector>
#include <cstdint>

namespace typesetting {

/// A single text run positioned on a page.
/// The platform renderer uses this to draw text at the exact position.
struct TextRun {
    std::string text;
    FontDescriptor font;
    float x = 0;           // Horizontal position from left edge of page
    float y = 0;           // Vertical position (baseline) from top edge of page
    float width = 0;       // Measured width of the run

    // Source tracking (for TTS highlight, selection, etc.)
    int blockIndex = -1;   // Index of source Block in chapter
    int inlineIndex = -1;  // Index of source InlineElement in block
    int charOffset = 0;    // Character offset within the inline element
    int charLength = 0;    // Number of characters in this run

    bool smallCaps = false;    // Render with small-caps variant
    bool isLink = false;       // This run is a hyperlink
    std::string href;          // Link target URL
    bool isSuperscript = false; // Render as superscript (footnote refs)
};

/// A laid-out line on a page
struct Line {
    std::vector<TextRun> runs;
    float x = 0;           // Line start x (after margin)
    float y = 0;           // Baseline y position
    float width = 0;       // Total line width
    float height = 0;      // Line height (ascent + descent + leading)
    float ascent = 0;
    float descent = 0;

    bool isLastLineOfParagraph = false;
    bool endsWithHyphen = false;
};

/// Types of visual decorations on a page
enum class DecorationType {
    HorizontalRule,
    ImagePlaceholder,
    TableBorder,
};

/// A visual decoration element on a page (non-text)
struct Decoration {
    DecorationType type = DecorationType::HorizontalRule;
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    std::string imageSrc;  // For ImagePlaceholder
    std::string imageAlt;  // For ImagePlaceholder
};

/// A single laid-out page
struct Page {
    int pageIndex = 0;
    std::vector<Line> lines;
    std::vector<Decoration> decorations;  // Non-text visual elements (HR lines, etc.)

    // Page dimensions
    float width = 0;
    float height = 0;

    // Content bounds (within margins)
    float contentX = 0;
    float contentY = 0;
    float contentWidth = 0;
    float contentHeight = 0;

    // Source tracking
    int firstBlockIndex = -1;   // First block that appears on this page
    int lastBlockIndex = -1;    // Last block that appears on this page
};

/// Warning types that may occur during layout
enum class LayoutWarning {
    None,
    EmptyContent,
    ParseError,
    LayoutOverflow,
};

/// Result of laying out an entire chapter
struct LayoutResult {
    std::string chapterId;
    std::vector<Page> pages;
    int totalBlocks = 0;
    std::vector<LayoutWarning> warnings;
};

} // namespace typesetting
