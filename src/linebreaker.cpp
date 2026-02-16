#include "typesetting/layout.h"
#include "typesetting/platform.h"
#include <vector>
#include <string>
#include <cmath>

namespace typesetting {

namespace linebreaker {

/// Return the byte length of the UTF-8 character starting at ptr.
/// Returns 1 for invalid lead bytes (safe fallback).
static size_t utf8CharLen(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1; // invalid byte, advance by 1
}

/// Convert a UTF-16 character count to a UTF-8 byte offset.
static size_t charCountToByteOffset(const std::string& text, size_t charCount) {
    size_t bytePos = 0;
    size_t chars = 0;
    while (bytePos < text.size() && chars < charCount) {
        size_t len = utf8CharLen(static_cast<unsigned char>(text[bytePos]));
        // Ensure we don't read past the string
        if (bytePos + len > text.size()) break;
        bytePos += len;
        // A 4-byte UTF-8 char maps to 2 UTF-16 chars (surrogate pair)
        chars += (len == 4) ? 2 : 1;
    }
    return bytePos;
}

/// A candidate break point in the text
struct BreakPoint {
    size_t charIndex = 0;     // Byte index in the text
    float widthBefore = 0;    // Total width up to this point
    bool isHyphen = false;    // Whether this break requires a hyphen
};

/// Find all candidate break points (spaces and hyphens) in text
std::vector<BreakPoint> findBreakPoints(const std::string& text,
                                        const FontDescriptor& font,
                                        PlatformAdapter& platform) {
    std::vector<BreakPoint> points;
    float currentWidth = 0;

    size_t i = 0;
    while (i < text.size()) {
        size_t charLen = utf8CharLen(static_cast<unsigned char>(text[i]));
        if (i + charLen > text.size()) break; // incomplete character at end

        size_t nextI = i + charLen;
        char c = text[i];
        auto measurement = platform.measureText(text.substr(0, nextI), font);
        currentWidth = measurement.width;

        // Space is always a valid break point (ASCII, single byte)
        if (c == ' ' || c == '\t') {
            points.push_back({nextI, currentWidth, false});
        }
        // Existing hyphen is a valid break point
        else if (c == '-' && i > 0 && nextI < text.size()) {
            points.push_back({nextI, currentWidth, false});
        }

        i = nextI;
    }

    return points;
}

/// Greedy line breaking: fit as many words as possible per line.
/// Returns character indices where each line should break.
std::vector<size_t> breakGreedy(const std::string& text,
                                const FontDescriptor& font,
                                float maxWidth,
                                PlatformAdapter& platform) {
    std::vector<size_t> breaks;

    size_t lineStart = 0;
    while (lineStart < text.size()) {
        // Skip leading spaces
        while (lineStart < text.size() && text[lineStart] == ' ') {
            ++lineStart;
        }
        if (lineStart >= text.size()) break;

        // Use platform to find where this line should break
        size_t breakPos = platform.findLineBreak(
            text.substr(lineStart), font, maxWidth);

        if (breakPos == 0) {
            // Can't fit even one character — force at least one
            breakPos = 1;
        }

        breaks.push_back(lineStart + breakPos);
        lineStart += breakPos;
    }

    return breaks;
}

} // namespace linebreaker

} // namespace typesetting
