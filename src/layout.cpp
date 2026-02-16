#include "typesetting/layout.h"
#include "typesetting/style_resolver.h"
#include <algorithm>

namespace typesetting {

class LayoutEngine::Impl {
public:
    explicit Impl(std::shared_ptr<PlatformAdapter> platform)
        : platform_(std::move(platform)) {}

    // ---------------------------------------------------------------
    // OLD overload: Style-based (backward compatible)
    // ---------------------------------------------------------------
    LayoutResult layoutChapter(const Chapter& chapter,
                               const Style& style,
                               const PageSize& pageSize) {
        // Convert Style into per-block BlockComputedStyles using
        // a StyleResolver with an empty stylesheet (gives defaults).
        CSSStylesheet emptySheet;
        StyleResolver resolver(emptySheet);
        auto styles = resolver.resolve(chapter.blocks, style);
        return layoutChapter(chapter, styles, style, pageSize);
    }

    // ---------------------------------------------------------------
    // NEW overload: BlockComputedStyle-based
    // ---------------------------------------------------------------
    LayoutResult layoutChapter(const Chapter& chapter,
                               const std::vector<BlockComputedStyle>& styles,
                               const PageSize& pageSize) {
        // We need a user Style for page margins.
        // Extract page-level settings from the first style (or use defaults).
        Style pageStyle;
        // BlockComputedStyle doesn't carry page margins, so use defaults.
        return layoutChapter(chapter, styles, pageStyle, pageSize);
    }

    std::vector<Line> layoutBlock(const Block& block,
                                  const Style& style,
                                  float availableWidth) {
        // Build a default BlockComputedStyle for this block
        CSSStylesheet emptySheet;
        StyleResolver resolver(emptySheet);
        std::vector<Block> singleBlock = {block};
        auto computedStyles = resolver.resolve(singleBlock, style);
        return layoutBlockLines(block, computedStyles[0], availableWidth, 0);
    }

private:
    std::shared_ptr<PlatformAdapter> platform_;

    // ---------------------------------------------------------------
    // Core implementation with BlockComputedStyle + page margins from Style
    // ---------------------------------------------------------------
    LayoutResult layoutChapter(const Chapter& chapter,
                               const std::vector<BlockComputedStyle>& styles,
                               const Style& pageStyle,
                               const PageSize& pageSize) {
        LayoutResult result;
        result.chapterId = chapter.id;
        result.totalBlocks = static_cast<int>(chapter.blocks.size());

        float contentWidth = pageStyle.contentWidth(pageSize.width);
        float contentHeight = pageStyle.contentHeight(pageSize.height);
        float contentX = pageStyle.marginLeft;
        float contentY = pageStyle.marginTop;

        float cursorY = 0;
        Page currentPage;
        currentPage.pageIndex = 0;
        currentPage.width = pageSize.width;
        currentPage.height = pageSize.height;
        currentPage.contentX = contentX;
        currentPage.contentY = contentY;
        currentPage.contentWidth = contentWidth;
        currentPage.contentHeight = contentHeight;
        currentPage.firstBlockIndex = 0;

        auto startNewPage = [&](int blockIdx) {
            currentPage.lastBlockIndex = blockIdx - 1;
            result.pages.push_back(currentPage);
            currentPage = Page{};
            currentPage.pageIndex = static_cast<int>(result.pages.size());
            currentPage.width = pageSize.width;
            currentPage.height = pageSize.height;
            currentPage.contentX = contentX;
            currentPage.contentY = contentY;
            currentPage.contentWidth = contentWidth;
            currentPage.contentHeight = contentHeight;
            currentPage.firstBlockIndex = blockIdx;
            cursorY = 0;
        };

        for (int blockIdx = 0; blockIdx < static_cast<int>(chapter.blocks.size()); ++blockIdx) {
            const auto& block = chapter.blocks[blockIdx];
            const auto& bstyle = styles[blockIdx];

            // Hidden blocks (display: none) — skip entirely
            if (bstyle.hidden) {
                continue;
            }

            // Handle horizontal rule
            if (block.type == BlockType::HorizontalRule) {
                float hrMarginTop = 0;
                float hrMarginBottom = 0;
                float hrBorderWidth = 1.0f;
                float hrWidthPercent = 25.0f;

                if (bstyle.hrStyle.has_value()) {
                    const auto& hr = bstyle.hrStyle.value();
                    hrMarginTop = hr.marginTopEm * bstyle.font.size;
                    hrMarginBottom = hr.marginBottomEm * bstyle.font.size;
                    hrBorderWidth = hr.borderWidth;
                    hrWidthPercent = hr.widthPercent;
                } else {
                    hrMarginTop = bstyle.font.size * 1.5f;
                    hrMarginBottom = bstyle.font.size * 1.5f;
                }

                float hrTotalHeight = hrMarginTop + hrBorderWidth + hrMarginBottom;

                if (cursorY + hrTotalHeight > contentHeight && !currentPage.lines.empty()) {
                    startNewPage(blockIdx);
                }

                // Create decoration
                float hrWidth = hrWidthPercent / 100.0f * contentWidth;
                float hrX = contentX + (contentWidth - hrWidth) / 2.0f;
                float hrY = contentY + cursorY + hrMarginTop;

                Decoration decoration;
                decoration.type = DecorationType::HorizontalRule;
                decoration.x = hrX;
                decoration.y = hrY;
                decoration.width = hrWidth;
                decoration.height = hrBorderWidth;
                currentPage.decorations.push_back(decoration);

                cursorY += hrTotalHeight;
                continue;
            }

            // Handle image blocks
            if (block.type == BlockType::Image) {
                float imageHeight = contentWidth * 0.6f;
                if (cursorY + imageHeight > contentHeight) {
                    startNewPage(blockIdx);
                }
                cursorY += imageHeight + bstyle.paragraphSpacingAfter;
                continue;
            }

            // Text blocks
            // Spacing before
            if (blockIdx > 0 && bstyle.marginTop > 0) {
                cursorY += bstyle.marginTop;
            }

            // Available width accounting for block margins
            float availableWidth = contentWidth - bstyle.marginLeft - bstyle.marginRight;
            float blockOffsetX = bstyle.marginLeft;

            // Layout text into lines
            std::vector<Line> lines = layoutBlockLines(block, bstyle, availableWidth, blockIdx);

            // Place lines on pages
            for (size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
                auto& line = lines[lineIdx];
                float lineHeight = line.height;

                // Check if line fits on current page
                if (cursorY + lineHeight > contentHeight && !currentPage.lines.empty()) {
                    startNewPage(blockIdx);
                }

                // Position the line
                line.y = contentY + cursorY + line.ascent;
                line.x = contentX + blockOffsetX;

                // Apply text alignment
                applyAlignment(line, bstyle.alignment, availableWidth);

                // Shift all runs by block offset (margins)
                for (auto& run : line.runs) {
                    run.x += contentX + blockOffsetX;
                    run.y = line.y;
                }

                currentPage.lines.push_back(line);
                cursorY += lineHeight;
            }

            // Spacing after block
            float spacingAfter = std::max(bstyle.marginBottom, bstyle.paragraphSpacingAfter);
            cursorY += spacingAfter;
        }

        // Add the last page if it has content
        if (!currentPage.lines.empty() || !currentPage.decorations.empty()) {
            currentPage.lastBlockIndex = static_cast<int>(chapter.blocks.size()) - 1;
            result.pages.push_back(currentPage);
        }

        return result;
    }

    // ---------------------------------------------------------------
    // Multi-font inline line layout
    // ---------------------------------------------------------------
    std::vector<Line> layoutBlockLines(const Block& block,
                                        const BlockComputedStyle& bstyle,
                                        float availableWidth,
                                        int blockIndex) {
        std::vector<Line> lines;

        if (block.inlines.empty()) return lines;

        float lineHeight = bstyle.font.size * bstyle.lineSpacingMultiplier;
        FontMetrics baseMetrics = platform_->resolveFontMetrics(bstyle.font);
        float maxAscent = baseMetrics.ascent;
        float maxDescent = baseMetrics.descent;

        // Line building state
        Line currentLine;
        float lineX = 0;
        bool isFirstLine = true;

        // Apply text indent on first line
        float effectiveWidth = availableWidth;
        if (isFirstLine && bstyle.textIndent > 0) {
            lineX = bstyle.textIndent;
            effectiveWidth = availableWidth - bstyle.textIndent;
        }

        auto completeLine = [&](bool isLastOfParagraph) {
            currentLine.isLastLineOfParagraph = isLastOfParagraph;
            currentLine.width = lineX;
            currentLine.height = lineHeight;
            currentLine.ascent = maxAscent;
            currentLine.descent = maxDescent;
            lines.push_back(std::move(currentLine));
            currentLine = Line{};
            lineX = 0;
            isFirstLine = false;
            effectiveWidth = availableWidth;
            // Reset per-line max metrics
            maxAscent = baseMetrics.ascent;
            maxDescent = baseMetrics.descent;
        };

        for (int inIdx = 0; inIdx < static_cast<int>(block.inlines.size()); ++inIdx) {
            const auto& inl = block.inlines[inIdx];

            // Determine font for this inline element
            FontDescriptor inlineFont = bstyle.font;
            bool runSmallCaps = bstyle.smallCaps;
            bool runIsLink = false;
            std::string runHref;

            switch (inl.type) {
                case InlineType::Text:
                    break;
                case InlineType::Bold:
                    inlineFont.weight = FontWeight::Bold;
                    break;
                case InlineType::Italic:
                    inlineFont.style = FontStyle::Italic;
                    break;
                case InlineType::BoldItalic:
                    inlineFont.weight = FontWeight::Bold;
                    inlineFont.style = FontStyle::Italic;
                    break;
                case InlineType::Code:
                    inlineFont.family = "monospace";
                    inlineFont.size = bstyle.font.size * 0.9f;
                    break;
                case InlineType::Link:
                    runIsLink = true;
                    runHref = inl.href;
                    break;
            }

            // Get metrics for this inline's font
            FontMetrics inlineMetrics = platform_->resolveFontMetrics(inlineFont);
            if (inlineMetrics.ascent > maxAscent) maxAscent = inlineMetrics.ascent;
            if (inlineMetrics.descent > maxDescent) maxDescent = inlineMetrics.descent;

            // Process the text, breaking into lines as needed
            std::string remaining = inl.text;
            int charOffset = 0;

            while (!remaining.empty()) {
                // Skip leading spaces at the beginning of a line
                if (lineX == 0 || (isFirstLine && lineX == bstyle.textIndent && currentLine.runs.empty())) {
                    size_t firstNonSpace = remaining.find_first_not_of(' ');
                    if (firstNonSpace == std::string::npos) {
                        // Entire remaining is spaces — skip
                        charOffset += static_cast<int>(remaining.size());
                        remaining.clear();
                        break;
                    }
                    if (firstNonSpace > 0) {
                        charOffset += static_cast<int>(firstNonSpace);
                        remaining = remaining.substr(firstNonSpace);
                    }
                }

                float spaceLeft = effectiveWidth - lineX;
                if (isFirstLine && bstyle.textIndent > 0 && currentLine.runs.empty()) {
                    spaceLeft = effectiveWidth - lineX + bstyle.textIndent;
                    // Recalculate: effectiveWidth already accounts for indent
                    spaceLeft = effectiveWidth - (lineX - bstyle.textIndent);
                    // Actually: effectiveWidth = availableWidth - textIndent, lineX starts at textIndent
                    // so spaceLeft = (availableWidth - textIndent) - (lineX - textIndent)
                    //              = availableWidth - lineX
                    // But lineX starts at textIndent. So:
                    // spaceLeft = availableWidth - lineX
                    spaceLeft = availableWidth - lineX;
                }

                // Measure remaining text width
                auto measurement = platform_->measureText(remaining, inlineFont);

                if (measurement.width <= spaceLeft) {
                    // Entire remaining text fits on this line
                    TextRun run;
                    run.text = remaining;
                    run.font = inlineFont;
                    run.x = lineX;
                    run.width = measurement.width;
                    run.blockIndex = blockIndex;
                    run.inlineIndex = inIdx;
                    run.charOffset = charOffset;
                    run.charLength = static_cast<int>(remaining.size());
                    run.smallCaps = runSmallCaps;
                    run.isLink = runIsLink;
                    run.href = runHref;

                    currentLine.runs.push_back(run);
                    lineX += measurement.width;
                    charOffset += static_cast<int>(remaining.size());
                    remaining.clear();
                } else {
                    // Text doesn't fit — find break point
                    size_t breakPos = platform_->findLineBreak(remaining, inlineFont, spaceLeft);

                    if (breakPos == 0) {
                        // Nothing fits
                        if (currentLine.runs.empty() && lineX <= bstyle.textIndent + 0.001f) {
                            // Line is empty — force at least one character
                            breakPos = 1;
                        } else {
                            // Complete current line and start a new one
                            completeLine(false);
                            continue; // Retry on the new line
                        }
                    }

                    std::string segment = remaining.substr(0, breakPos);

                    // Trim trailing spaces from segment
                    std::string trimmedSegment = segment;
                    while (!trimmedSegment.empty() && trimmedSegment.back() == ' ') {
                        trimmedSegment.pop_back();
                    }

                    auto segMeasurement = platform_->measureText(trimmedSegment, inlineFont);

                    TextRun run;
                    run.text = trimmedSegment;
                    run.font = inlineFont;
                    run.x = lineX;
                    run.width = segMeasurement.width;
                    run.blockIndex = blockIndex;
                    run.inlineIndex = inIdx;
                    run.charOffset = charOffset;
                    run.charLength = static_cast<int>(trimmedSegment.size());
                    run.smallCaps = runSmallCaps;
                    run.isLink = runIsLink;
                    run.href = runHref;

                    currentLine.runs.push_back(run);
                    lineX += segMeasurement.width;

                    // Complete the current line
                    completeLine(false);

                    charOffset += static_cast<int>(breakPos);
                    remaining = remaining.substr(breakPos);
                }
            }
        }

        // Finish the last line if it has runs
        if (!currentLine.runs.empty()) {
            completeLine(true);
        }

        // Mark the very last line as last-of-paragraph
        if (!lines.empty()) {
            lines.back().isLastLineOfParagraph = true;
        }

        return lines;
    }

    // ---------------------------------------------------------------
    // Text alignment
    // ---------------------------------------------------------------
    void applyAlignment(Line& line, TextAlignment alignment, float contentWidth) {
        float extraSpace = contentWidth - line.width;
        if (extraSpace <= 0) return;

        switch (alignment) {
            case TextAlignment::Left:
                break;
            case TextAlignment::Center: {
                float offset = extraSpace / 2.0f;
                line.x += offset;
                for (auto& run : line.runs) {
                    run.x += offset;
                }
                break;
            }
            case TextAlignment::Right:
                line.x += extraSpace;
                for (auto& run : line.runs) {
                    run.x += extraSpace;
                }
                break;
            case TextAlignment::Justified:
                if (!line.isLastLineOfParagraph) {
                    justifyLine(line, contentWidth);
                }
                break;
        }
    }

    // ---------------------------------------------------------------
    // Justification
    // ---------------------------------------------------------------
    void justifyLine(Line& line, float contentWidth) {
        if (line.runs.empty()) return;

        // Count total spaces across all runs
        int spaceCount = 0;
        for (const auto& run : line.runs) {
            for (char c : run.text) {
                if (c == ' ') ++spaceCount;
            }
        }
        if (spaceCount == 0) return;

        float extraSpace = contentWidth - line.width;
        if (extraSpace <= 0) return;
        float extraPerSpace = extraSpace / static_cast<float>(spaceCount);

        // Redistribute: walk through runs, adding extra width per space
        float xCursor = line.runs[0].x;
        for (auto& run : line.runs) {
            run.x = xCursor;
            int spacesInRun = 0;
            for (char c : run.text) {
                if (c == ' ') ++spacesInRun;
            }
            run.width += spacesInRun * extraPerSpace;
            xCursor += run.width;
        }
        line.width = contentWidth;
    }
};

LayoutEngine::LayoutEngine(std::shared_ptr<PlatformAdapter> platform)
    : impl_(std::make_unique<Impl>(std::move(platform))) {}

LayoutEngine::~LayoutEngine() = default;

LayoutResult LayoutEngine::layoutChapter(const Chapter& chapter,
                                          const Style& style,
                                          const PageSize& pageSize) {
    return impl_->layoutChapter(chapter, style, pageSize);
}

LayoutResult LayoutEngine::layoutChapter(const Chapter& chapter,
                                          const std::vector<BlockComputedStyle>& styles,
                                          const PageSize& pageSize) {
    return impl_->layoutChapter(chapter, styles, pageSize);
}

std::vector<Line> LayoutEngine::layoutBlock(const Block& block,
                                             const Style& style,
                                             float availableWidth) {
    return impl_->layoutBlock(block, style, availableWidth);
}

} // namespace typesetting
