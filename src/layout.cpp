#include "typesetting/layout.h"
#include "typesetting/style_resolver.h"
#include "typesetting/log.h"
#include <algorithm>
#include <cctype>

namespace typesetting {

namespace {

std::string applyTextTransform(const std::string& text, TextTransform transform) {
    if (transform == TextTransform::None) return text;
    std::string result = text;
    if (transform == TextTransform::Uppercase) {
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::toupper(c); });
    } else if (transform == TextTransform::Lowercase) {
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
    } else if (transform == TextTransform::Capitalize) {
        bool newWord = true;
        for (size_t i = 0; i < result.size(); ++i) {
            if (std::isspace(static_cast<unsigned char>(result[i]))) {
                newWord = true;
            } else if (newWord) {
                result[i] = std::toupper(static_cast<unsigned char>(result[i]));
                newWord = false;
            }
        }
    }
    return result;
}

} // anonymous namespace

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
        CSSStylesheet emptySheet;
        StyleResolver resolver(emptySheet);
        auto resolved = resolver.resolve(chapter.blocks, style);
        return layoutChapter(chapter, resolved, style, pageSize);
    }

    // ---------------------------------------------------------------
    // NEW overload: BlockComputedStyle-based
    // ---------------------------------------------------------------
    LayoutResult layoutChapter(const Chapter& chapter,
                               const std::vector<BlockComputedStyle>& styles,
                               const PageSize& pageSize) {
        Style pageStyle;
        ResolvedStyles resolved;
        resolved.blockStyles = styles;
        resolved.inlineStyles.resize(styles.size()); // empty inline styles per block
        return layoutChapter(chapter, resolved, pageStyle, pageSize);
    }

    std::vector<Line> layoutBlock(const Block& block,
                                  const Style& style,
                                  float availableWidth) {
        CSSStylesheet emptySheet;
        StyleResolver resolver(emptySheet);
        std::vector<Block> singleBlock = {block};
        auto resolved = resolver.resolve(singleBlock, style);
        std::vector<InlineComputedStyle> emptyInlineStyles;
        return layoutBlockLines(block, resolved.blockStyles[0],
                                resolved.inlineStyles.empty() ? emptyInlineStyles : resolved.inlineStyles[0],
                                availableWidth, 0);
    }

private:
    std::shared_ptr<PlatformAdapter> platform_;

    // ---------------------------------------------------------------
    // Core implementation with ResolvedStyles + page margins from Style
    // ---------------------------------------------------------------
    LayoutResult layoutChapter(const Chapter& chapter,
                               const ResolvedStyles& resolved,
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
            TS_LOGD("layout: newPage pageIndex=%d blockIdx=%d", currentPage.pageIndex, blockIdx);
        };

        for (int blockIdx = 0; blockIdx < static_cast<int>(chapter.blocks.size()); ++blockIdx) {
            const auto& block = chapter.blocks[blockIdx];
            const auto& bstyle = resolved.blockStyles[blockIdx];

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
                float imageWidth = contentWidth;
                float imageHeight = contentWidth * 0.6f;  // Default placeholder ratio

                // Try to get actual image dimensions from platform
                auto imgSize = platform_->getImageSize(block.src);
                if (imgSize.has_value() && imgSize->width > 0 && imgSize->height > 0) {
                    float scale = contentWidth / imgSize->width;
                    imageWidth = contentWidth;
                    imageHeight = imgSize->height * scale;
                    TS_LOGD("layout: image src='%s' native=%.0fx%.0f scaled=%.0fx%.0f",
                            block.src.c_str(), imgSize->width, imgSize->height,
                            imageWidth, imageHeight);
                } else {
                    TS_LOGD("layout: image src='%s' no dimensions, placeholder=%.0fx%.0f",
                            block.src.c_str(), imageWidth, imageHeight);
                }

                if (cursorY + imageHeight > contentHeight && !currentPage.lines.empty()) {
                    startNewPage(blockIdx);
                }

                // Create image placeholder decoration
                Decoration imgDeco;
                imgDeco.type = DecorationType::ImagePlaceholder;
                imgDeco.x = contentX;
                imgDeco.y = contentY + cursorY;
                imgDeco.width = imageWidth;
                imgDeco.height = imageHeight;
                imgDeco.imageSrc = block.src;
                imgDeco.imageAlt = block.alt;
                currentPage.decorations.push_back(imgDeco);

                cursorY += imageHeight;

                // If there's a caption, lay it out as text below the image
                if (!block.caption.empty()) {
                    float captionSpacing = bstyle.font.size * 0.5f;
                    cursorY += captionSpacing;

                    // Create a temporary block for the caption
                    Block captionBlock;
                    captionBlock.type = BlockType::Figcaption;
                    captionBlock.inlines.push_back(InlineElement::plain(block.caption));

                    BlockComputedStyle captionStyle = bstyle;
                    captionStyle.font.size = bstyle.font.size * 0.85f;
                    captionStyle.font.style = FontStyle::Italic;
                    captionStyle.alignment = TextAlignment::Center;
                    captionStyle.textIndent = 0;

                    auto captionLines = layoutBlockLines(captionBlock, captionStyle, {}, contentWidth, blockIdx);
                    for (auto& line : captionLines) {
                        if (cursorY + line.height > contentHeight && !currentPage.lines.empty()) {
                            startNewPage(blockIdx);
                        }
                        line.y = contentY + cursorY + line.ascent;
                        line.x = contentX;
                        applyAlignment(line, captionStyle.alignment, contentWidth);
                        for (auto& run : line.runs) {
                            run.x += contentX;
                            run.y = line.y;
                        }
                        currentPage.lines.push_back(line);
                        cursorY += line.height;
                    }
                }

                cursorY += bstyle.paragraphSpacingAfter;
                continue;
            }

            // Handle table blocks
            if (block.type == BlockType::Table) {
                if (!block.tableRows.empty()) {
                    float tableMarginTop = bstyle.font.size;
                    float tableMarginBottom = bstyle.font.size;
                    cursorY += tableMarginTop;

                    // Determine number of columns
                    int maxCols = 0;
                    for (const auto& row : block.tableRows) {
                        int rowCols = 0;
                        for (const auto& cell : row.cells) {
                            rowCols += cell.colspan;
                        }
                        if (rowCols > maxCols) maxCols = rowCols;
                    }
                    if (maxCols == 0) maxCols = 1;
                    TS_LOGD("layout: table rows=%zu cols=%d cellWidth=%.1f",
                            block.tableRows.size(), maxCols,
                            contentWidth / static_cast<float>(maxCols));

                    float cellWidth = contentWidth / static_cast<float>(maxCols);
                    float cellPadding = bstyle.font.size * 0.3f;
                    float tableBorderWidth = 1.0f;

                    // Draw outer table border
                    float tableStartY = contentY + cursorY;

                    // Layout each row
                    for (const auto& row : block.tableRows) {
                        float rowHeight = 0;
                        float cellX = 0;

                        // First pass: calculate row height
                        for (const auto& cell : row.cells) {
                            float thisCellWidth = cellWidth * cell.colspan - cellPadding * 2;
                            if (thisCellWidth < 1) thisCellWidth = 1;

                            Block cellBlock;
                            cellBlock.type = BlockType::Paragraph;
                            cellBlock.inlines = cell.inlines;

                            BlockComputedStyle cellStyle = bstyle;
                            cellStyle.textIndent = 0;
                            cellStyle.alignment = TextAlignment::Left;
                            if (cell.isHeader) {
                                cellStyle.font.weight = FontWeight::Bold;
                            }

                            auto cellLines = layoutBlockLines(cellBlock, cellStyle, {}, thisCellWidth, blockIdx);
                            float cellHeight = cellPadding * 2;
                            for (const auto& cl : cellLines) {
                                cellHeight += cl.height;
                            }
                            if (cellHeight > rowHeight) rowHeight = cellHeight;
                        }

                        // Check page break
                        if (cursorY + rowHeight > contentHeight && !currentPage.lines.empty()) {
                            startNewPage(blockIdx);
                        }

                        // Second pass: place cell content
                        cellX = 0;
                        for (const auto& cell : row.cells) {
                            float thisCellWidth = cellWidth * cell.colspan - cellPadding * 2;
                            if (thisCellWidth < 1) thisCellWidth = 1;

                            Block cellBlock;
                            cellBlock.type = BlockType::Paragraph;
                            cellBlock.inlines = cell.inlines;

                            BlockComputedStyle cellStyle = bstyle;
                            cellStyle.textIndent = 0;
                            cellStyle.alignment = TextAlignment::Left;
                            if (cell.isHeader) {
                                cellStyle.font.weight = FontWeight::Bold;
                            }

                            auto cellLines = layoutBlockLines(cellBlock, cellStyle, {}, thisCellWidth, blockIdx);
                            float cellCursorY = cursorY + cellPadding;

                            for (auto& line : cellLines) {
                                line.y = contentY + cellCursorY + line.ascent;
                                line.x = contentX + cellX + cellPadding;
                                applyAlignment(line, cellStyle.alignment, thisCellWidth);
                                for (auto& run : line.runs) {
                                    run.x += contentX + cellX + cellPadding;
                                    run.y = line.y;
                                }
                                currentPage.lines.push_back(line);
                                cellCursorY += line.height;
                            }

                            // Cell border decoration
                            Decoration cellBorder;
                            cellBorder.type = DecorationType::TableBorder;
                            cellBorder.x = contentX + cellX;
                            cellBorder.y = contentY + cursorY;
                            cellBorder.width = cellWidth * cell.colspan;
                            cellBorder.height = rowHeight;
                            currentPage.decorations.push_back(cellBorder);

                            cellX += cellWidth * cell.colspan;
                        }

                        cursorY += rowHeight;
                    }

                    cursorY += tableMarginBottom;
                }
                continue;
            }

            // Text blocks
            // Spacing before
            if (blockIdx > 0 && bstyle.marginTop > 0) {
                cursorY += bstyle.marginTop;
            }

            // Available width accounting for block margins and padding
            float availableWidth = contentWidth - bstyle.marginLeft - bstyle.marginRight - bstyle.paddingLeft;
            float blockOffsetX = bstyle.marginLeft + bstyle.paddingLeft;

            // Layout text into lines
            static const std::vector<InlineComputedStyle> emptyInlineStyles;
            const auto& blockInlineStyles = (blockIdx < static_cast<int>(resolved.inlineStyles.size()))
                ? resolved.inlineStyles[blockIdx] : emptyInlineStyles;
            std::vector<Line> lines = layoutBlockLines(block, bstyle, blockInlineStyles, availableWidth, blockIdx);

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
                    // Superscript: shift baseline up by 30% of ascent
                    if (run.isSuperscript) {
                        run.y -= line.ascent * 0.3f;
                    }
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

        // Detect layout overflow: if page count is unreasonably high relative to content
        if (result.totalBlocks > 0 && static_cast<int>(result.pages.size()) > result.totalBlocks * 50) {
            TS_LOGW("layout: overflow detected pages=%zu blocks=%d ratio=%d",
                    result.pages.size(), result.totalBlocks,
                    static_cast<int>(result.pages.size()) / result.totalBlocks);
            result.warnings.push_back(LayoutWarning::LayoutOverflow);
        }

        TS_LOGI("layoutChapter: pages=%zu blocks=%d", result.pages.size(), result.totalBlocks);
        return result;
    }

    // ---------------------------------------------------------------
    // Multi-font inline line layout
    // ---------------------------------------------------------------
    std::vector<Line> layoutBlockLines(const Block& block,
                                        const BlockComputedStyle& bstyle,
                                        const std::vector<InlineComputedStyle>& inlineStyles,
                                        float availableWidth,
                                        int blockIndex) {
        std::vector<Line> lines;

        if (block.inlines.empty()) return lines;

        float lineHeight = bstyle.font.size * bstyle.lineSpacingMultiplier;
        FontMetrics baseMetrics = platform_->resolveFontMetrics(bstyle.font);
        float maxAscent = baseMetrics.ascent;
        float maxDescent = baseMetrics.descent;

        // List bullet/number marker
        float markerWidth = 0;
        std::string markerText;
        if (block.type == BlockType::ListItem) {
            if (block.listIndex < 0) {
                // Unordered list: bullet
                markerText = "\xe2\x80\xa2 ";  // "• "
            } else {
                // Ordered list: number
                markerText = std::to_string(block.listIndex + 1) + ". ";
            }
            auto markerMeasure = platform_->measureText(markerText, bstyle.font);
            markerWidth = markerMeasure.width;
        }

        // Line building state
        Line currentLine;
        float lineX = 0;
        bool isFirstLine = true;

        // Apply text indent on first line
        float effectiveWidth = availableWidth;
        if (isFirstLine && bstyle.textIndent != 0) {
            lineX = bstyle.textIndent;
            effectiveWidth = availableWidth - bstyle.textIndent;
        }

        // For list items, place the marker on the first line and indent subsequent text
        if (block.type == BlockType::ListItem && markerWidth > 0) {
            TextRun markerRun;
            markerRun.text = markerText;
            markerRun.font = bstyle.font;
            markerRun.x = lineX;
            markerRun.width = markerWidth;
            markerRun.blockIndex = blockIndex;
            markerRun.inlineIndex = -1;
            markerRun.charOffset = 0;
            markerRun.charLength = static_cast<int>(markerText.size());
            currentLine.runs.push_back(markerRun);
            lineX += markerWidth;
        }

        auto completeLine = [&](bool isLastOfParagraph) {
            currentLine.isLastLineOfParagraph = isLastOfParagraph;
            currentLine.width = lineX;
            currentLine.height = lineHeight;
            currentLine.ascent = maxAscent;
            currentLine.descent = maxDescent;
            lines.push_back(std::move(currentLine));
            currentLine = Line{};
            isFirstLine = false;
            // For list items, subsequent lines indent to align with text after marker
            if (block.type == BlockType::ListItem && markerWidth > 0) {
                lineX = markerWidth;
                effectiveWidth = availableWidth - markerWidth;
            } else {
                lineX = 0;
                effectiveWidth = availableWidth;
            }
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
            bool runIsSuperscript = false;
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

            // Footnote reference: use smaller font and superscript positioning
            if (inl.isFootnoteRef) {
                inlineFont.size = bstyle.font.size * 0.7f;
                runIsSuperscript = true;
            }

            // CSS overrides from InlineComputedStyle
            if (inIdx < static_cast<int>(inlineStyles.size())) {
                const auto& istyle = inlineStyles[inIdx];
                if (istyle.fontSizeMultiplier) {
                    inlineFont.size = bstyle.font.size * *istyle.fontSizeMultiplier;
                }
                if (istyle.fontStyle) {
                    inlineFont.style = *istyle.fontStyle;
                }
                if (istyle.fontWeight) {
                    inlineFont.weight = *istyle.fontWeight;
                }
                if (istyle.smallCaps.has_value()) {
                    runSmallCaps = *istyle.smallCaps;
                }
                if (istyle.isSuperscript) {
                    runIsSuperscript = true;
                    if (!istyle.fontSizeMultiplier) {
                        inlineFont.size = bstyle.font.size * 0.7f;
                    }
                }
            }

            // Get metrics for this inline's font
            FontMetrics inlineMetrics = platform_->resolveFontMetrics(inlineFont);
            if (inlineMetrics.ascent > maxAscent) maxAscent = inlineMetrics.ascent;
            if (inlineMetrics.descent > maxDescent) maxDescent = inlineMetrics.descent;

            // Apply text-transform
            TextTransform effectiveTransform = bstyle.textTransform;
            if (inIdx < static_cast<int>(inlineStyles.size()) &&
                inlineStyles[inIdx].textTransform.has_value()) {
                effectiveTransform = *inlineStyles[inIdx].textTransform;
            }

            // Process the text, breaking into lines as needed
            std::string remaining = applyTextTransform(inl.text, effectiveTransform);
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
                    run.isSuperscript = runIsSuperscript;

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
                            // Line is empty — force at least one UTF-8 character
                            unsigned char lead = static_cast<unsigned char>(remaining[0]);
                            if (lead < 0x80) breakPos = 1;
                            else if ((lead & 0xE0) == 0xC0) breakPos = 2;
                            else if ((lead & 0xF0) == 0xE0) breakPos = 3;
                            else if ((lead & 0xF8) == 0xF0) breakPos = 4;
                            else breakPos = 1;
                            if (breakPos > remaining.size()) breakPos = remaining.size();
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
                    run.isSuperscript = runIsSuperscript;

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

        // Hanging punctuation: if enabled, check the first text run of the first line
        // for an opening quote character and shift it into the left margin
        if (bstyle.hangingPunctuation && !lines.empty()) {
            auto& firstLine = lines[0];
            // Find the first run that has actual text content (skip marker runs with inlineIndex == -1)
            for (auto& run : firstLine.runs) {
                if (run.text.empty() || run.inlineIndex < 0) continue;
                // Check first character for opening quotes
                // UTF-8 sequences: \xe2\x80\x9c = left double quote
                //                  \xe2\x80\x9d = right double quote (rare at start)
                //                  \xe2\x80\x98 = left single quote
                //                  \xe2\x80\x99 = right single quote
                //                  \xc2\xab     = left guillemet
                //                  \xe2\x80\x9e = double low-9 quote
                //                  " and '      = ASCII quotes
                std::string firstChar;
                unsigned char lead = static_cast<unsigned char>(run.text[0]);
                if (lead == '"' || lead == '\'') {
                    firstChar = run.text.substr(0, 1);
                } else if (lead == 0xC2 && run.text.size() >= 2) {
                    firstChar = run.text.substr(0, 2);  // guillemet
                } else if (lead == 0xE2 && run.text.size() >= 3) {
                    firstChar = run.text.substr(0, 3);  // unicode quotes
                }
                if (!firstChar.empty()) {
                    bool isOpenQuote = (firstChar == "\"" || firstChar == "'" ||
                                        firstChar == "\xe2\x80\x9c" ||  // left double quote
                                        firstChar == "\xe2\x80\x98" ||  // left single quote
                                        firstChar == "\xc2\xab"     ||  // left guillemet
                                        firstChar == "\xe2\x80\x9e");   // double low-9 quote
                    if (isOpenQuote) {
                        auto charMeasure = platform_->measureText(firstChar, run.font);
                        float hangOffset = charMeasure.width;
                        // Shift this run and all subsequent runs left
                        for (auto& r : firstLine.runs) {
                            r.x -= hangOffset;
                        }
                        firstLine.x -= hangOffset;
                    }
                }
                break;  // Only check the first real text run
            }
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
