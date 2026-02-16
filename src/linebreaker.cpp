#include "typesetting/layout.h"
#include "typesetting/platform.h"
#include <vector>
#include <string>
#include <cmath>

namespace typesetting {

namespace linebreaker {

/// A candidate break point in the text
struct BreakPoint {
    size_t charIndex = 0;     // Character index in the text
    float widthBefore = 0;    // Total width up to this point
    bool isHyphen = false;    // Whether this break requires a hyphen
};

/// Find all candidate break points (spaces and hyphens) in text
std::vector<BreakPoint> findBreakPoints(const std::string& text,
                                        const FontDescriptor& font,
                                        PlatformAdapter& platform) {
    std::vector<BreakPoint> points;
    float currentWidth = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        auto measurement = platform.measureText(text.substr(0, i + 1), font);
        currentWidth = measurement.width;

        // Space is always a valid break point
        if (c == ' ' || c == '\t') {
            points.push_back({i + 1, currentWidth, false});
        }
        // Existing hyphen is a valid break point
        else if (c == '-' && i > 0 && i < text.size() - 1) {
            points.push_back({i + 1, currentWidth, false});
        }
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
