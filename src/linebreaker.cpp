#include "typesetting/layout.h"
#include "typesetting/platform.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

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

// ── Unicode character detection ──────────────────────────────────────

/// Soft hyphen U+00AD (C2 AD) — breakable, shows visible hyphen at line end
static bool isSoftHyphen(const std::string& text, size_t pos) {
    return pos + 1 < text.size() &&
           static_cast<unsigned char>(text[pos]) == 0xC2 &&
           static_cast<unsigned char>(text[pos + 1]) == 0xAD;
}

/// Non-breaking space U+00A0 (C2 A0) — NOT a break opportunity
static bool isNBSP(const std::string& text, size_t pos) {
    return pos + 1 < text.size() &&
           static_cast<unsigned char>(text[pos]) == 0xC2 &&
           static_cast<unsigned char>(text[pos + 1]) == 0xA0;
}

/// Word joiner U+2060 (E2 81 A0) or ZWNBSP U+FEFF (EF BB BF) — NOT breakable
static bool isWordJoiner(const std::string& text, size_t pos) {
    if (pos + 2 < text.size()) {
        unsigned char b0 = static_cast<unsigned char>(text[pos]);
        unsigned char b1 = static_cast<unsigned char>(text[pos + 1]);
        unsigned char b2 = static_cast<unsigned char>(text[pos + 2]);
        if (b0 == 0xE2 && b1 == 0x81 && b2 == 0xA0) return true;  // U+2060
        if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) return true;  // U+FEFF
    }
    return false;
}

/// Non-breaking hyphen U+2011 (E2 80 91) — NOT a break opportunity
static bool isNBHyphen(const std::string& text, size_t pos) {
    return pos + 2 < text.size() &&
           static_cast<unsigned char>(text[pos]) == 0xE2 &&
           static_cast<unsigned char>(text[pos + 1]) == 0x80 &&
           static_cast<unsigned char>(text[pos + 2]) == 0x91;
}

/// Unicode spaces U+2000–U+200A (E2 80 80–8A) — breakable
/// Includes: en space, em space, thin space, hair space, etc.
static bool isUnicodeSpace(const std::string& text, size_t pos) {
    if (pos + 2 >= text.size()) return false;
    unsigned char b0 = static_cast<unsigned char>(text[pos]);
    unsigned char b1 = static_cast<unsigned char>(text[pos + 1]);
    unsigned char b2 = static_cast<unsigned char>(text[pos + 2]);
    return b0 == 0xE2 && b1 == 0x80 && b2 >= 0x80 && b2 <= 0x8A;
}

/// Em dash U+2014 (E2 80 94) or en dash U+2013 (E2 80 93) — break after
static bool isDash(const std::string& text, size_t pos) {
    if (pos + 2 >= text.size()) return false;
    unsigned char b0 = static_cast<unsigned char>(text[pos]);
    unsigned char b1 = static_cast<unsigned char>(text[pos + 1]);
    unsigned char b2 = static_cast<unsigned char>(text[pos + 2]);
    return b0 == 0xE2 && b1 == 0x80 && (b2 == 0x93 || b2 == 0x94);
}

// ── Break point structures ───────────────────────────────────────────

/// A candidate break point in the text
struct BreakPoint {
    size_t charIndex = 0;     // Byte index in the text
    float widthBefore = 0;    // Total width up to this point
    bool isHyphen = false;    // Whether this break requires a visible hyphen
};

/// Find all candidate break points in text (Unicode-aware).
/// Recognizes: ASCII space/tab, ASCII hyphen, soft hyphen (U+00AD),
/// Unicode spaces (U+2000-U+200A), em/en dashes.
/// Respects: NBSP (U+00A0), NB-hyphen (U+2011), word joiner (U+2060/U+FEFF).
std::vector<BreakPoint> findBreakPoints(const std::string& text,
                                        const FontDescriptor& font,
                                        PlatformAdapter& platform) {
    std::vector<BreakPoint> points;
    float currentWidth = 0;

    size_t i = 0;
    while (i < text.size()) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        size_t charLen = utf8CharLen(c);
        if (i + charLen > text.size()) break;

        size_t nextI = i + charLen;
        auto measurement = platform.measureText(text.substr(0, nextI), font);
        currentWidth = measurement.width;

        // ── Non-breakable characters — skip explicitly ──
        if (isNBSP(text, i) || isNBHyphen(text, i) || isWordJoiner(text, i)) {
            i = nextI;
            continue;
        }

        // ── Breakable characters ──
        // ASCII space/tab
        if (c == ' ' || c == '\t') {
            points.push_back({nextI, currentWidth, false});
        }
        // ASCII hyphen (break after, not at string boundaries)
        else if (c == '-' && i > 0 && nextI < text.size()) {
            points.push_back({nextI, currentWidth, false});
        }
        // Soft hyphen U+00AD — breakable with visible hyphen at line end
        else if (isSoftHyphen(text, i)) {
            points.push_back({nextI, currentWidth, true});
        }
        // Unicode spaces U+2000–U+200A (hair space, thin space, em space, etc.)
        else if (isUnicodeSpace(text, i)) {
            points.push_back({nextI, currentWidth, false});
        }
        // Em dash / en dash — break after the dash
        else if (isDash(text, i)) {
            points.push_back({nextI, currentWidth, false});
        }

        i = nextI;
    }

    return points;
}

/// Find break points with hyphenation opportunities for words.
/// Calls platform hyphenation for words between natural break points.
std::vector<BreakPoint> findBreakPointsWithHyphenation(
    const std::string& text,
    const FontDescriptor& font,
    const std::string& locale,
    PlatformAdapter& platform) {

    auto points = findBreakPoints(text, font, platform);

    if (!platform.supportsHyphenation(locale)) {
        return points;
    }

    std::vector<BreakPoint> allPoints = points;

    // Build word boundaries from break points
    std::vector<size_t> boundaries;
    boundaries.push_back(0);
    for (const auto& bp : points) {
        boundaries.push_back(bp.charIndex);
    }
    boundaries.push_back(text.size());

    for (size_t seg = 0; seg + 1 < boundaries.size(); ++seg) {
        size_t wordStart = boundaries[seg];
        size_t wordEnd = boundaries[seg + 1];

        // Skip leading whitespace
        while (wordStart < wordEnd &&
               (text[wordStart] == ' ' || text[wordStart] == '\t')) {
            ++wordStart;
        }

        if (wordEnd <= wordStart + 4) continue;  // Skip short segments

        std::string word = text.substr(wordStart, wordEnd - wordStart);
        auto hyphPoints = platform.findHyphenationPoints(word, locale);

        for (size_t hp : hyphPoints) {
            size_t bytePos = wordStart + charCountToByteOffset(word, hp);
            if (bytePos > wordStart && bytePos < wordEnd) {
                auto measure = platform.measureText(text.substr(0, bytePos), font);
                allPoints.push_back({bytePos, measure.width, true});
            }
        }
    }

    // Sort by position
    std::sort(allPoints.begin(), allPoints.end(),
              [](const BreakPoint& a, const BreakPoint& b) {
                  return a.charIndex < b.charIndex;
              });

    return allPoints;
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
