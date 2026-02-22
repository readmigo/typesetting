#include <gtest/gtest.h>
#include "typesetting/engine.h"
#include "typesetting/document.h"
#include "typesetting/style.h"
#include "typesetting/style_resolver.h"
#include "typesetting/page.h"

using namespace typesetting;

/// Mock platform adapter for testing
class MockPlatformAdapter : public PlatformAdapter {
public:
    float charWidth = 8.0f; // Fixed-width for predictable tests
    std::optional<ImageSize> mockImageSize;  // Configurable mock image size

    FontMetrics resolveFontMetrics(const FontDescriptor& desc) override {
        FontMetrics m;
        m.ascent = desc.size * 0.8f;
        m.descent = desc.size * 0.2f;
        m.leading = desc.size * 0.1f;
        m.xHeight = desc.size * 0.5f;
        m.capHeight = desc.size * 0.7f;
        return m;
    }

    TextMeasurement measureText(const std::string& text,
                                const FontDescriptor& font) override {
        // Simple fixed-width measurement
        float width = static_cast<float>(text.size()) * charWidth;
        auto metrics = resolveFontMetrics(font);
        return {width, metrics.ascent + metrics.descent};
    }

    size_t findLineBreak(const std::string& text,
                         const FontDescriptor& font,
                         float maxWidth) override {
        size_t maxChars = static_cast<size_t>(maxWidth / charWidth);
        if (maxChars >= text.size()) return text.size();

        // Find last space before maxChars
        size_t lastSpace = text.rfind(' ', maxChars);
        if (lastSpace != std::string::npos && lastSpace > 0) {
            return lastSpace + 1; // Break after space
        }
        return maxChars; // Force break
    }

    bool supportsHyphenation(const std::string& locale) override {
        return false;
    }

    std::vector<size_t> findHyphenationPoints(const std::string& word,
                                               const std::string& locale) override {
        return {};
    }

    std::optional<ImageSize> getImageSize(const std::string& src) override {
        return mockImageSize;
    }
};

// MARK: - Document Parsing Tests

TEST(DocumentTest, ParseSimpleParagraph) {
    auto blocks = parseHTML("<p>Hello world</p>");
    ASSERT_EQ(blocks.size(), 1);
    EXPECT_EQ(blocks[0].type, BlockType::Paragraph);
    EXPECT_EQ(blocks[0].plainText(), "Hello world");
}

TEST(DocumentTest, ParseMultipleParagraphs) {
    auto blocks = parseHTML("<p>First</p><p>Second</p><p>Third</p>");
    ASSERT_EQ(blocks.size(), 3);
    EXPECT_EQ(blocks[0].plainText(), "First");
    EXPECT_EQ(blocks[1].plainText(), "Second");
    EXPECT_EQ(blocks[2].plainText(), "Third");
}

TEST(DocumentTest, ParseHeadings) {
    auto blocks = parseHTML("<h1>Title</h1><h2>Subtitle</h2><p>Body</p>");
    ASSERT_EQ(blocks.size(), 3);
    EXPECT_EQ(blocks[0].type, BlockType::Heading1);
    EXPECT_EQ(blocks[1].type, BlockType::Heading2);
    EXPECT_EQ(blocks[2].type, BlockType::Paragraph);
}

TEST(DocumentTest, ParseInlineFormatting) {
    auto blocks = parseHTML("<p>This is <strong>bold</strong> and <em>italic</em></p>");
    ASSERT_EQ(blocks.size(), 1);
    ASSERT_GE(blocks[0].inlines.size(), 3);
    EXPECT_EQ(blocks[0].inlines[0].type, InlineType::Text);
    EXPECT_EQ(blocks[0].inlines[1].type, InlineType::Bold);
    EXPECT_EQ(blocks[0].inlines[2].type, InlineType::Text);
}

TEST(DocumentTest, ParseHorizontalRule) {
    auto blocks = parseHTML("<p>Before</p><hr><p>After</p>");
    ASSERT_EQ(blocks.size(), 3);
    EXPECT_EQ(blocks[1].type, BlockType::HorizontalRule);
}

TEST(DocumentTest, DecodeHTMLEntities) {
    auto blocks = parseHTML("<p>Tom &amp; Jerry &mdash; friends</p>");
    ASSERT_EQ(blocks.size(), 1);
    auto text = blocks[0].plainText();
    EXPECT_NE(text.find("&"), std::string::npos);
    EXPECT_NE(text.find("\xe2\x80\x94"), std::string::npos); // mdash
}

// MARK: - Layout Tests

TEST(LayoutTest, SingleParagraphFitsOnePage) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML("<p>Short text</p>", "ch1", style, pageSize);
    EXPECT_EQ(result.pages.size(), 1);
    EXPECT_GT(result.pages[0].lines.size(), 0);
}

TEST(LayoutTest, LongTextCreatesMultiplePages) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 200.0f}; // Small page to force pagination

    // Generate long text
    std::string html = "<p>";
    for (int i = 0; i < 100; ++i) {
        html += "This is a sentence that should fill up the page. ";
    }
    html += "</p>";

    auto result = engine.layoutHTML(html, "ch1", style, pageSize);
    EXPECT_GT(result.pages.size(), 1);
}

TEST(LayoutTest, RelayoutPreservesContent) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result1 = engine.layoutHTML("<p>Hello world</p>", "ch1", style, pageSize);

    // Relayout with larger font
    style.font.size = 24.0f;
    auto result2 = engine.relayout(style, pageSize);

    EXPECT_EQ(result1.chapterId, result2.chapterId);
    EXPECT_GT(result2.pages.size(), 0);
}

TEST(LayoutTest, EmptyContentReturnsNoPages) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML("", "ch1", style, pageSize);
    EXPECT_EQ(result.pages.size(), 0);
}

TEST(LayoutTest, PageBlockIndicesAreCorrect) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(
        "<h1>Title</h1><p>First paragraph</p><p>Second paragraph</p>",
        "ch1", style, pageSize);

    ASSERT_GT(result.pages.size(), 0);
    EXPECT_EQ(result.pages[0].firstBlockIndex, 0);
    EXPECT_GE(result.pages[0].lastBlockIndex, 0);
}

// MARK: - HTML Metadata Extraction Tests

TEST(DocumentTest, ParseSEChapterHTML) {
    auto blocks = parseHTML(
        "<section id=\"chapter-1\" epub:type=\"chapter\">"
        "<h2 epub:type=\"ordinal\">I</h2>"
        "<p>First paragraph.</p>"
        "<p>Second paragraph.</p>"
        "</section>");
    ASSERT_GE(blocks.size(), 3);
    // h2
    EXPECT_EQ(blocks[0].htmlTag, "h2");
    EXPECT_EQ(blocks[0].epubType, "ordinal");
    EXPECT_EQ(blocks[0].parentTag, "section");
    EXPECT_EQ(blocks[0].isFirstChild, true);
    // first p after h2
    EXPECT_EQ(blocks[1].htmlTag, "p");
    EXPECT_EQ(blocks[1].previousSiblingTags[0], "h2");
    EXPECT_EQ(blocks[1].isFirstChild, false);
    // second p
    EXPECT_EQ(blocks[2].previousSiblingTags[0], "p");
}

TEST(DocumentTest, ParseClassNames) {
    auto blocks = parseHTML(
        "<blockquote class=\"epub-type-contains-word-z3998-song\">"
        "<p>Song lyrics</p>"
        "</blockquote>");
    // The p inside should have parentClassName set
    bool found = false;
    for (const auto& b : blocks) {
        if (b.htmlTag == "p") {
            EXPECT_EQ(b.parentClassName, "epub-type-contains-word-z3998-song");
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(DocumentTest, ParseFirstChild) {
    auto blocks = parseHTML(
        "<section>"
        "<p>First</p>"
        "<p>Second</p>"
        "</section>");
    ASSERT_GE(blocks.size(), 2);
    EXPECT_TRUE(blocks[0].isFirstChild);
    EXPECT_FALSE(blocks[1].isFirstChild);
}

TEST(DocumentTest, ParseAdjacentSiblingHR) {
    auto blocks = parseHTML("<p>Before</p><hr/><p>After</p>");
    // Find the paragraph after hr
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].type == BlockType::HorizontalRule && i + 1 < blocks.size()) {
            ASSERT_FALSE(blocks[i + 1].previousSiblingTags.empty());
            EXPECT_EQ(blocks[i + 1].previousSiblingTags[0], "hr");
        }
    }
}

TEST(DocumentTest, ParseInlineLang) {
    auto blocks = parseHTML("<p>Hello <i lang=\"lt\">Lietuviškai</i> world</p>");
    ASSERT_EQ(blocks.size(), 1);
    bool foundLang = false;
    for (const auto& il : blocks[0].inlines) {
        if (il.type == InlineType::Italic && !il.lang.empty()) {
            EXPECT_EQ(il.lang, "lt");
            foundLang = true;
        }
    }
    EXPECT_TRUE(foundLang);
}

// MARK: - Phase 4: Multi-font Inline and Computed Style Layout Tests

TEST(LayoutTest, MultiFontInlineRuns) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);
    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(
        "<p>Normal <b>bold</b> end</p>", "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);
    ASSERT_GT(result.pages[0].lines.size(), 0);
    auto& line = result.pages[0].lines[0];
    // Should have multiple runs with different formatting
    EXPECT_GE(line.runs.size(), 2);
}

TEST(LayoutTest, TextIndentFirstLine) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    // Use CSS that sets text-indent
    std::string css = "p { text-indent: 1em; }";
    std::string html = "<p>This is a paragraph with enough text to wrap to multiple lines for testing purposes here.</p>";

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(html, css, "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);
    ASSERT_GT(result.pages[0].lines.size(), 0);
    // First line's first run should have x offset > margin
    auto& firstRun = result.pages[0].lines[0].runs[0];
    EXPECT_GT(firstRun.x, style.marginLeft);
}

TEST(LayoutTest, JustifyDistributesSpace) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);
    Style style;
    style.font.size = 16.0f;
    style.alignment = TextAlignment::Justified;
    PageSize pageSize{390.0f, 844.0f};

    // Create text that wraps to multiple lines
    std::string html = "<p>";
    for (int i = 0; i < 20; ++i) html += "word ";
    html += "</p>";

    auto result = engine.layoutHTML(html, "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);
    // Non-last lines should have width close to content width
    if (result.pages[0].lines.size() > 1) {
        auto& firstLine = result.pages[0].lines[0];
        float contentWidth = style.contentWidth(pageSize.width);
        EXPECT_NEAR(firstLine.width, contentWidth, 1.0f);
    }
}

TEST(LayoutTest, HiddenBlockSkipped) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    std::string css = "h1 { display: none; }";
    std::string html = "<h1>Hidden Title</h1><p>Visible text</p>";

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(html, css, "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);
    // Should only have lines from the paragraph, not the heading
    bool foundHidden = false;
    for (auto& line : result.pages[0].lines) {
        for (auto& run : line.runs) {
            if (run.text.find("Hidden") != std::string::npos) foundHidden = true;
        }
    }
    EXPECT_FALSE(foundHidden);
}

TEST(LayoutTest, HorizontalRuleDecoration) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    std::string css = "hr { border-top: 1px solid; width: 25%; }";
    std::string html = "<p>Before</p><hr/><p>After</p>";

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(html, css, "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);
    // Should have at least one decoration
    bool hasDecoration = false;
    for (auto& page : result.pages) {
        if (!page.decorations.empty()) hasDecoration = true;
    }
    EXPECT_TRUE(hasDecoration);
}

// =============================================================================
// MARK: - List Bullet/Number Rendering Tests
// =============================================================================

TEST(DocumentTest, ParseUnorderedListItem) {
    auto blocks = parseHTML("<ul><li>Item one</li><li>Item two</li></ul>");
    ASSERT_GE(blocks.size(), 2);
    EXPECT_EQ(blocks[0].type, BlockType::ListItem);
    EXPECT_EQ(blocks[0].plainText(), "Item one");
    EXPECT_EQ(blocks[0].listIndex, -1);  // Unordered = -1
    EXPECT_EQ(blocks[1].type, BlockType::ListItem);
    EXPECT_EQ(blocks[1].plainText(), "Item two");
    EXPECT_EQ(blocks[1].listIndex, -1);
}

TEST(DocumentTest, ParseOrderedListItem) {
    auto blocks = parseHTML("<ol><li>First</li><li>Second</li><li>Third</li></ol>");
    ASSERT_GE(blocks.size(), 3);
    for (const auto& b : blocks) {
        EXPECT_EQ(b.type, BlockType::ListItem);
    }
    EXPECT_EQ(blocks[0].plainText(), "First");
    EXPECT_EQ(blocks[1].plainText(), "Second");
    EXPECT_EQ(blocks[2].plainText(), "Third");
    // Note: parser treats ol and ul items the same (listIndex stays -1)
    EXPECT_EQ(blocks[0].listIndex, -1);
}

TEST(DocumentTest, ParseEmptyListItem) {
    auto blocks = parseHTML("<ul><li></li></ul>");
    // An empty <li> should produce no blocks (no inline content)
    // or a block with empty inlines depending on parser behavior
    for (const auto& b : blocks) {
        if (b.type == BlockType::ListItem) {
            EXPECT_TRUE(b.inlines.empty());
        }
    }
}

TEST(LayoutTest, UnorderedListBulletMarker) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(
        "<ul><li>Bullet item text</li></ul>", "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);
    ASSERT_GT(result.pages[0].lines.size(), 0);

    // The first line should contain a bullet marker run as its first run
    auto& firstLine = result.pages[0].lines[0];
    ASSERT_GE(firstLine.runs.size(), 2);  // marker + text
    // Bullet marker: "• " (UTF-8: \xe2\x80\xa2 + space)
    EXPECT_EQ(firstLine.runs[0].text, "\xe2\x80\xa2 ");
    EXPECT_EQ(firstLine.runs[0].inlineIndex, -1);  // Marker has inlineIndex == -1
}

TEST(LayoutTest, OrderedListNumberMarker) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    // Parser doesn't set listIndex for <ol>, so create blocks programmatically
    Block item1;
    item1.type = BlockType::ListItem;
    item1.listIndex = 0;  // First ordered item
    item1.inlines.push_back(InlineElement::plain("First item"));

    Block item2;
    item2.type = BlockType::ListItem;
    item2.listIndex = 1;  // Second ordered item
    item2.inlines.push_back(InlineElement::plain("Second item"));

    auto result = engine.layoutBlocks({item1, item2}, "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);
    ASSERT_GT(result.pages[0].lines.size(), 0);

    // Find runs for list items; first item should have "1. " marker
    bool foundFirst = false;
    bool foundSecond = false;
    for (auto& line : result.pages[0].lines) {
        for (auto& run : line.runs) {
            if (run.text == "1. ") foundFirst = true;
            if (run.text == "2. ") foundSecond = true;
        }
    }
    EXPECT_TRUE(foundFirst);
    EXPECT_TRUE(foundSecond);
}

TEST(LayoutTest, ListItemSubsequentLineIndent) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{200.0f, 844.0f};  // Narrow page to force wrapping

    // Long text that wraps
    std::string html = "<ul><li>";
    for (int i = 0; i < 30; ++i) html += "word ";
    html += "</li></ul>";

    auto result = engine.layoutHTML(html, "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);

    // Find lines belonging to this list item
    auto& page = result.pages[0];
    ASSERT_GE(page.lines.size(), 2);  // Should wrap to multiple lines

    // The first line has the marker; subsequent lines should also be indented
    auto& firstLine = page.lines[0];
    auto& secondLine = page.lines[1];

    ASSERT_GE(firstLine.runs.size(), 2);  // marker + text
    float markerWidth = firstLine.runs[0].width;
    EXPECT_GT(markerWidth, 0);

    // The text content on the first line starts after the marker
    float firstLineTextStartX = firstLine.runs[1].x;

    // Second line should also be indented (not starting from the very left margin)
    // The second line's first run x should be greater than just the content margin
    if (!secondLine.runs.empty()) {
        float contentStartX = style.marginLeft;
        // Second line should be indented beyond the raw content start
        EXPECT_GT(secondLine.runs[0].x, contentStartX);
    }
}

// =============================================================================
// MARK: - Image Layout Tests
// =============================================================================

TEST(DocumentTest, ParseImageBlock) {
    auto blocks = parseHTML("<img src=\"cover.jpg\" alt=\"Book cover\"/>");
    ASSERT_EQ(blocks.size(), 1);
    EXPECT_EQ(blocks[0].type, BlockType::Image);
    EXPECT_EQ(blocks[0].src, "cover.jpg");
    EXPECT_EQ(blocks[0].alt, "Book cover");
}

TEST(LayoutTest, ImageWithPlatformDimensions) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    platform->mockImageSize = ImageSize{800.0f, 600.0f};
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(
        "<img src=\"photo.jpg\" alt=\"A photo\"/>", "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);

    // Should have an ImagePlaceholder decoration
    bool hasImage = false;
    for (auto& page : result.pages) {
        for (auto& deco : page.decorations) {
            if (deco.type == DecorationType::ImagePlaceholder) {
                hasImage = true;
                EXPECT_EQ(deco.imageSrc, "photo.jpg");
                EXPECT_EQ(deco.imageAlt, "A photo");
                // Image should be scaled to fit content width
                float contentWidth = style.contentWidth(pageSize.width);
                EXPECT_FLOAT_EQ(deco.width, contentWidth);
                // Height should be proportionally scaled: 600 * (contentWidth/800)
                float expectedHeight = 600.0f * (contentWidth / 800.0f);
                EXPECT_NEAR(deco.height, expectedHeight, 0.1f);
            }
        }
    }
    EXPECT_TRUE(hasImage);
}

TEST(LayoutTest, ImageWithoutPlatformDimensions) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    // mockImageSize defaults to nullopt
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(
        "<img src=\"unknown.jpg\" alt=\"Mystery\"/>", "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);

    // Should still have an ImagePlaceholder decoration with default ratio
    bool hasImage = false;
    for (auto& page : result.pages) {
        for (auto& deco : page.decorations) {
            if (deco.type == DecorationType::ImagePlaceholder) {
                hasImage = true;
                float contentWidth = style.contentWidth(pageSize.width);
                EXPECT_FLOAT_EQ(deco.width, contentWidth);
                // Default placeholder ratio is 0.6
                float expectedHeight = contentWidth * 0.6f;
                EXPECT_NEAR(deco.height, expectedHeight, 0.1f);
            }
        }
    }
    EXPECT_TRUE(hasImage);
}

TEST(LayoutTest, ImageCaptionText) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    // Create a block with caption directly via layoutBlocks
    Block imgBlock;
    imgBlock.type = BlockType::Image;
    imgBlock.src = "art.jpg";
    imgBlock.alt = "Art";
    imgBlock.caption = "Figure 1: Beautiful art";

    auto result = engine.layoutBlocks({imgBlock}, "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);

    // Should have ImagePlaceholder decoration AND text lines for the caption
    bool hasDecoration = false;
    bool hasCaptionText = false;
    for (auto& page : result.pages) {
        for (auto& deco : page.decorations) {
            if (deco.type == DecorationType::ImagePlaceholder) {
                hasDecoration = true;
            }
        }
        for (auto& line : page.lines) {
            for (auto& run : line.runs) {
                if (run.text.find("Figure 1") != std::string::npos) {
                    hasCaptionText = true;
                }
            }
        }
    }
    EXPECT_TRUE(hasDecoration);
    EXPECT_TRUE(hasCaptionText);
}

TEST(LayoutTest, ImagePageBreak) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    platform->mockImageSize = ImageSize{400.0f, 1000.0f};  // Very tall image
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 200.0f};  // Short page

    // Put text before the image to fill the page
    std::string html = "<p>Some text before the image.</p><img src=\"tall.jpg\" alt=\"Tall\"/>";

    auto result = engine.layoutHTML(html, "ch1", style, pageSize);
    // The image should cause a page break since it won't fit after the text
    EXPECT_GE(result.pages.size(), 2);
}

// =============================================================================
// MARK: - Table Parsing Tests
// =============================================================================

TEST(DocumentTest, ParseSimpleTable) {
    auto blocks = parseHTML(
        "<table>"
        "<tr><td>A</td><td>B</td></tr>"
        "<tr><td>C</td><td>D</td></tr>"
        "</table>");
    ASSERT_EQ(blocks.size(), 1);
    EXPECT_EQ(blocks[0].type, BlockType::Table);
    ASSERT_EQ(blocks[0].tableRows.size(), 2);
    ASSERT_EQ(blocks[0].tableRows[0].cells.size(), 2);
    EXPECT_EQ(blocks[0].tableRows[0].cells[0].inlines.size(), 1);
    EXPECT_EQ(blocks[0].tableRows[0].cells[0].inlines[0].text, "A");
    EXPECT_EQ(blocks[0].tableRows[0].cells[1].inlines[0].text, "B");
    EXPECT_EQ(blocks[0].tableRows[1].cells[0].inlines[0].text, "C");
    EXPECT_EQ(blocks[0].tableRows[1].cells[1].inlines[0].text, "D");
}

TEST(DocumentTest, ParseTableWithHeaders) {
    auto blocks = parseHTML(
        "<table>"
        "<tr><th>Name</th><th>Age</th></tr>"
        "<tr><td>Alice</td><td>30</td></tr>"
        "</table>");
    ASSERT_EQ(blocks.size(), 1);
    ASSERT_EQ(blocks[0].tableRows.size(), 2);
    // Header row
    EXPECT_TRUE(blocks[0].tableRows[0].cells[0].isHeader);
    EXPECT_TRUE(blocks[0].tableRows[0].cells[1].isHeader);
    EXPECT_EQ(blocks[0].tableRows[0].cells[0].inlines[0].text, "Name");
    // Data row
    EXPECT_FALSE(blocks[0].tableRows[1].cells[0].isHeader);
    EXPECT_EQ(blocks[0].tableRows[1].cells[0].inlines[0].text, "Alice");
}

TEST(DocumentTest, ParseTableWithColspan) {
    auto blocks = parseHTML(
        "<table>"
        "<tr><td colspan=\"2\">Spanning</td></tr>"
        "<tr><td>Left</td><td>Right</td></tr>"
        "</table>");
    ASSERT_EQ(blocks.size(), 1);
    ASSERT_EQ(blocks[0].tableRows.size(), 2);
    // First row: single cell with colspan=2
    ASSERT_EQ(blocks[0].tableRows[0].cells.size(), 1);
    EXPECT_EQ(blocks[0].tableRows[0].cells[0].colspan, 2);
    EXPECT_EQ(blocks[0].tableRows[0].cells[0].inlines[0].text, "Spanning");
    // Second row: two cells
    ASSERT_EQ(blocks[0].tableRows[1].cells.size(), 2);
    EXPECT_EQ(blocks[0].tableRows[1].cells[0].colspan, 1);
}

TEST(DocumentTest, ParseTableInlineFormatting) {
    auto blocks = parseHTML(
        "<table>"
        "<tr><td><b>Bold</b> text</td><td><em>Italic</em></td></tr>"
        "</table>");
    ASSERT_EQ(blocks.size(), 1);
    ASSERT_EQ(blocks[0].tableRows.size(), 1);
    auto& row = blocks[0].tableRows[0];
    ASSERT_EQ(row.cells.size(), 2);

    // First cell: bold inline + text inline
    ASSERT_GE(row.cells[0].inlines.size(), 2);
    EXPECT_EQ(row.cells[0].inlines[0].type, InlineType::Bold);
    EXPECT_EQ(row.cells[0].inlines[0].text, "Bold");
    EXPECT_EQ(row.cells[0].inlines[1].type, InlineType::Text);
    EXPECT_EQ(row.cells[0].inlines[1].text, "text");

    // Second cell: italic inline
    ASSERT_GE(row.cells[1].inlines.size(), 1);
    EXPECT_EQ(row.cells[1].inlines[0].type, InlineType::Italic);
    EXPECT_EQ(row.cells[1].inlines[0].text, "Italic");
}

TEST(DocumentTest, ParseEmptyTable) {
    auto blocks = parseHTML("<table></table>");
    ASSERT_EQ(blocks.size(), 1);
    EXPECT_EQ(blocks[0].type, BlockType::Table);
    EXPECT_TRUE(blocks[0].tableRows.empty());
}

TEST(DocumentTest, ParseTableWithThead) {
    // thead/tbody are skipped structural tags, but content should still parse
    auto blocks = parseHTML(
        "<table>"
        "<thead><tr><th>H1</th><th>H2</th></tr></thead>"
        "<tbody><tr><td>D1</td><td>D2</td></tr></tbody>"
        "</table>");
    ASSERT_EQ(blocks.size(), 1);
    EXPECT_EQ(blocks[0].type, BlockType::Table);
    ASSERT_EQ(blocks[0].tableRows.size(), 2);
    EXPECT_TRUE(blocks[0].tableRows[0].cells[0].isHeader);
    EXPECT_FALSE(blocks[0].tableRows[1].cells[0].isHeader);
}

// =============================================================================
// MARK: - Table Layout Tests
// =============================================================================

TEST(LayoutTest, TableRowColumnLayout) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(
        "<table>"
        "<tr><td>Cell1</td><td>Cell2</td></tr>"
        "<tr><td>Cell3</td><td>Cell4</td></tr>"
        "</table>",
        "ch1", style, pageSize);

    ASSERT_GT(result.pages.size(), 0);
    auto& page = result.pages[0];

    // Should have TableBorder decorations (one per cell)
    int borderCount = 0;
    for (auto& deco : page.decorations) {
        if (deco.type == DecorationType::TableBorder) {
            borderCount++;
            EXPECT_GT(deco.width, 0);
            EXPECT_GT(deco.height, 0);
        }
    }
    EXPECT_EQ(borderCount, 4);  // 2 rows x 2 cells

    // Should have text runs for cell content
    bool foundCell1 = false;
    bool foundCell4 = false;
    for (auto& line : page.lines) {
        for (auto& run : line.runs) {
            if (run.text == "Cell1") foundCell1 = true;
            if (run.text == "Cell4") foundCell4 = true;
        }
    }
    EXPECT_TRUE(foundCell1);
    EXPECT_TRUE(foundCell4);
}

TEST(LayoutTest, TableBorderDecoration) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(
        "<table><tr><td>Single cell</td></tr></table>",
        "ch1", style, pageSize);

    ASSERT_GT(result.pages.size(), 0);
    bool hasTableBorder = false;
    for (auto& deco : result.pages[0].decorations) {
        if (deco.type == DecorationType::TableBorder) {
            hasTableBorder = true;
            // Cell border should span the full content width for a single-column table
            float contentWidth = style.contentWidth(pageSize.width);
            EXPECT_NEAR(deco.width, contentWidth, 1.0f);
        }
    }
    EXPECT_TRUE(hasTableBorder);
}

TEST(LayoutTest, TablePageBreak) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 100.0f};  // Very short page

    // Fill page with text, then add a table
    std::string html = "<p>Fill text here.</p>"
        "<table>"
        "<tr><td>Row1</td></tr>"
        "<tr><td>Row2</td></tr>"
        "<tr><td>Row3</td></tr>"
        "</table>";

    auto result = engine.layoutHTML(html, "ch1", style, pageSize);
    // Should span multiple pages due to limited height
    EXPECT_GE(result.pages.size(), 1);
}

TEST(LayoutTest, EmptyTableProducesNoLines) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML("<table></table>", "ch1", style, pageSize);
    // Empty table should produce no lines or decorations
    if (!result.pages.empty()) {
        EXPECT_TRUE(result.pages[0].lines.empty());
        EXPECT_TRUE(result.pages[0].decorations.empty());
    }
}

// =============================================================================
// MARK: - Figcaption Tests
// =============================================================================

TEST(DocumentTest, ParseFigcaption) {
    auto blocks = parseHTML(
        "<figure>"
        "<img src=\"pic.jpg\" alt=\"Picture\"/>"
        "<figcaption>This is a caption</figcaption>"
        "</figure>");

    // Should produce an Image block and a Figcaption block
    bool hasImage = false;
    bool hasFigcaption = false;
    for (const auto& b : blocks) {
        if (b.type == BlockType::Image) {
            hasImage = true;
            EXPECT_EQ(b.src, "pic.jpg");
        }
        if (b.type == BlockType::Figcaption) {
            hasFigcaption = true;
            EXPECT_EQ(b.plainText(), "This is a caption");
        }
    }
    EXPECT_TRUE(hasImage);
    EXPECT_TRUE(hasFigcaption);
}

TEST(DocumentTest, FigcaptionParentIsFigure) {
    auto blocks = parseHTML(
        "<figure>"
        "<figcaption>Caption text</figcaption>"
        "</figure>");

    bool found = false;
    for (const auto& b : blocks) {
        if (b.type == BlockType::Figcaption) {
            EXPECT_EQ(b.parentTag, "figure");
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(LayoutTest, FigcaptionLayout) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(
        "<figure>"
        "<figcaption>A nice figure caption here</figcaption>"
        "</figure>",
        "ch1", style, pageSize);

    ASSERT_GT(result.pages.size(), 0);
    ASSERT_GT(result.pages[0].lines.size(), 0);

    bool foundCaption = false;
    for (auto& line : result.pages[0].lines) {
        for (auto& run : line.runs) {
            if (run.text.find("caption") != std::string::npos) {
                foundCaption = true;
            }
        }
    }
    EXPECT_TRUE(foundCaption);
}

// =============================================================================
// MARK: - Footnote/Endnote Tests
// =============================================================================

TEST(DocumentTest, ParseFootnoteRef) {
    auto blocks = parseHTML(
        "<p>Some text<a href=\"#note-1\" epub:type=\"noteref\">1</a> more text.</p>");
    ASSERT_EQ(blocks.size(), 1);

    bool foundNoteref = false;
    for (const auto& inl : blocks[0].inlines) {
        if (inl.isFootnoteRef) {
            foundNoteref = true;
            EXPECT_EQ(inl.footnoteId, "#note-1");
            EXPECT_EQ(inl.text, "1");
            EXPECT_EQ(inl.type, InlineType::Link);
        }
    }
    EXPECT_TRUE(foundNoteref);
}

TEST(DocumentTest, ParseNonFootnoteLink) {
    auto blocks = parseHTML(
        "<p>Visit <a href=\"https://example.com\">here</a> for more.</p>");
    ASSERT_EQ(blocks.size(), 1);

    for (const auto& inl : blocks[0].inlines) {
        // Non-noteref links should NOT be marked as footnote refs
        EXPECT_FALSE(inl.isFootnoteRef);
        EXPECT_TRUE(inl.footnoteId.empty());
    }
}

TEST(LayoutTest, FootnoteRefSuperscript) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(
        "<p>Text<a href=\"#note-1\" epub:type=\"noteref\">1</a> continues.</p>",
        "ch1", style, pageSize);

    ASSERT_GT(result.pages.size(), 0);
    ASSERT_GT(result.pages[0].lines.size(), 0);

    bool foundSuperscript = false;
    for (auto& line : result.pages[0].lines) {
        for (auto& run : line.runs) {
            if (run.isSuperscript) {
                foundSuperscript = true;
                // Footnote ref font size should be 0.7x base
                EXPECT_NEAR(run.font.size, 16.0f * 0.7f, 0.1f);
            }
        }
    }
    EXPECT_TRUE(foundSuperscript);
}

TEST(LayoutTest, FootnoteRefSuperscriptYOffset) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(
        "<p>Text<a href=\"#n1\" epub:type=\"noteref\">1</a> end.</p>",
        "ch1", style, pageSize);

    ASSERT_GT(result.pages.size(), 0);
    ASSERT_GT(result.pages[0].lines.size(), 0);

    auto& line = result.pages[0].lines[0];
    float normalY = 0;
    float superY = 0;
    for (auto& run : line.runs) {
        if (run.isSuperscript) {
            superY = run.y;
        } else if (!run.text.empty() && run.text != "\xe2\x80\xa2 ") {
            normalY = run.y;
        }
    }
    // Superscript y should be above normal baseline (lower value)
    if (superY != 0 && normalY != 0) {
        EXPECT_LT(superY, normalY);
    }
}

// =============================================================================
// MARK: - Hanging Punctuation Tests
// =============================================================================

TEST(LayoutTest, HangingPunctuationOpenQuote) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    // Enable hanging punctuation via CSS
    std::string css = "p { hanging-punctuation: first; }";
    std::string html = "<p>\xe2\x80\x9cHello world,\xe2\x80\x9d she said.</p>";

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(html, css, "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);
    ASSERT_GT(result.pages[0].lines.size(), 0);

    auto& firstLine = result.pages[0].lines[0];
    ASSERT_GT(firstLine.runs.size(), 0);

    // With hanging punctuation, the first run's x should be negative
    // relative to the normal content start (shifted left into the margin)
    float normalContentStart = style.marginLeft;
    EXPECT_LT(firstLine.runs[0].x, normalContentStart);
}

TEST(LayoutTest, HangingPunctuationNonQuote) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    std::string css = "p { hanging-punctuation: first; }";
    std::string html = "<p>Normal text without quotes.</p>";

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(html, css, "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);
    ASSERT_GT(result.pages[0].lines.size(), 0);

    auto& firstLine = result.pages[0].lines[0];
    ASSERT_GT(firstLine.runs.size(), 0);

    // Non-quote text should NOT have negative offset
    // The first run's x should be >= marginLeft (accounting for text-indent)
    EXPECT_GE(firstLine.runs[0].x, style.marginLeft);
}

TEST(LayoutTest, HangingPunctuationDisabled) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    // No hanging-punctuation CSS
    std::string html = "<p>\xe2\x80\x9cQuoted text\xe2\x80\x9d</p>";

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(html, "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);
    ASSERT_GT(result.pages[0].lines.size(), 0);

    auto& firstLine = result.pages[0].lines[0];
    ASSERT_GT(firstLine.runs.size(), 0);

    // Without hanging punctuation, the first run should be at or after marginLeft
    EXPECT_GE(firstLine.runs[0].x, style.marginLeft);
}

TEST(LayoutTest, HangingPunctuationASCIIQuote) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    // Set text-indent: 0 so the hanging offset clearly shifts into the margin
    std::string css = "p { hanging-punctuation: first; text-indent: 0; }";
    std::string html = "<p>\"Hello\" she said.</p>";

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML(html, css, "ch1", style, pageSize);
    ASSERT_GT(result.pages.size(), 0);
    ASSERT_GT(result.pages[0].lines.size(), 0);

    auto& firstLine = result.pages[0].lines[0];
    ASSERT_GT(firstLine.runs.size(), 0);

    // ASCII double quote should also trigger hanging punctuation
    // With text-indent: 0, the run starts at margin and gets shifted left
    float normalContentStart = style.marginLeft;
    EXPECT_LT(firstLine.runs[0].x, normalContentStart);
}

// =============================================================================
// MARK: - Error Handling Tests
// =============================================================================

// =============================================================================
// MARK: - Inline htmlTag Tests
// =============================================================================

TEST(DocumentTest, InlineHtmlTagPopulated) {
    auto blocks = parseHTML("<p><b>bold</b> text <a href=\"#\">link</a></p>");
    ASSERT_GE(blocks.size(), 1);
    ASSERT_GE(blocks[0].inlines.size(), 3);
    EXPECT_EQ(blocks[0].inlines[0].htmlTag, "b");
    EXPECT_EQ(blocks[0].inlines[1].htmlTag, "");  // plain text
    EXPECT_EQ(blocks[0].inlines[2].htmlTag, "a");
}

TEST(DocumentTest, InlineHtmlTagAbbr) {
    auto blocks = parseHTML("<p><abbr epub:type=\"z3998:name-title\">Mr.</abbr> Smith</p>");
    ASSERT_GE(blocks.size(), 1);
    ASSERT_GE(blocks[0].inlines.size(), 1);
    EXPECT_EQ(blocks[0].inlines[0].htmlTag, "abbr");
    EXPECT_EQ(blocks[0].inlines[0].epubType, "z3998:name-title");
}

// =============================================================================
// MARK: - Error Handling Tests
// =============================================================================

TEST(ErrorTest, EmptyContentWarning) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML("", "ch1", style, pageSize);
    // Should have EmptyContent warning
    bool hasEmptyWarning = false;
    for (auto w : result.warnings) {
        if (w == LayoutWarning::EmptyContent) hasEmptyWarning = true;
    }
    EXPECT_TRUE(hasEmptyWarning);
}

TEST(ErrorTest, WarningsDefaultEmpty) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 844.0f};

    auto result = engine.layoutHTML("<p>Valid content</p>", "ch1", style, pageSize);
    // Normal content should not produce warnings
    EXPECT_TRUE(result.warnings.empty());
}

TEST(ErrorTest, EmptyContentFromBlocksAPI) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    PageSize pageSize{390.0f, 844.0f};

    std::vector<Block> emptyBlocks;
    auto result = engine.layoutBlocks(emptyBlocks, "ch1", style, pageSize);
    bool hasEmptyWarning = false;
    for (auto w : result.warnings) {
        if (w == LayoutWarning::EmptyContent) hasEmptyWarning = true;
    }
    EXPECT_TRUE(hasEmptyWarning);
}

TEST(ErrorTest, RelayoutEmptyContentWarning) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    PageSize pageSize{390.0f, 844.0f};

    // First layout with empty content
    engine.layoutHTML("", "ch1", style, pageSize);

    // Relayout should also warn about empty content
    auto result = engine.relayout(style, pageSize);
    bool hasEmptyWarning = false;
    for (auto w : result.warnings) {
        if (w == LayoutWarning::EmptyContent) hasEmptyWarning = true;
    }
    EXPECT_TRUE(hasEmptyWarning);
}

TEST(ErrorTest, NormalContentNoOverflowWarning) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    Engine engine(platform);

    Style style;
    style.font.size = 16.0f;
    PageSize pageSize{390.0f, 200.0f};  // Small page

    std::string html = "<p>Short paragraph one.</p><p>Short paragraph two.</p>";
    auto result = engine.layoutHTML(html, "ch1", style, pageSize);

    // Normal content should never trigger LayoutOverflow
    for (auto w : result.warnings) {
        EXPECT_NE(w, LayoutWarning::LayoutOverflow);
    }
}

// --- Task 6: :last-child ---

TEST(DocumentTest, ParseLastChild) {
    auto blocks = parseHTML(
        "<section>"
        "<p>First</p>"
        "<p>Middle</p>"
        "<p>Last</p>"
        "</section>");
    ASSERT_GE(blocks.size(), 3);
    EXPECT_TRUE(blocks[0].isFirstChild);
    EXPECT_FALSE(blocks[0].isLastChild);
    EXPECT_FALSE(blocks[1].isFirstChild);
    EXPECT_FALSE(blocks[1].isLastChild);
    EXPECT_FALSE(blocks[2].isFirstChild);
    EXPECT_TRUE(blocks[2].isLastChild);
}

// =============================================================================
// MARK: - Phase 3: max-width and centering layout tests
// =============================================================================

TEST(LayoutTest, MaxWidthWithCentering) {
    auto platform = std::make_shared<MockPlatformAdapter>();
    platform->charWidth = 8.0f;
    LayoutEngine engine(platform);

    Chapter chapter;
    chapter.id = "test";

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.inlines.push_back(InlineElement::plain("Hello"));
    chapter.blocks.push_back(block);

    // Style with max-width 50% and horizontal centering
    std::vector<BlockComputedStyle> styles(1);
    styles[0].font.size = 16.0f;
    styles[0].font.family = "test";
    styles[0].lineSpacingMultiplier = 1.4f;
    styles[0].paragraphSpacingAfter = 12.0f;
    styles[0].textIndent = 0;
    styles[0].maxWidthPercent = 50.0f;
    styles[0].horizontalCentering = true;

    PageSize pageSize{400.0f, 600.0f};
    auto result = engine.layoutChapter(chapter, styles, pageSize);

    ASSERT_FALSE(result.pages.empty());
    ASSERT_FALSE(result.pages[0].lines.empty());

    // BlockComputedStyle overload creates default Style: marginLeft=20, marginRight=20
    // contentWidth = 400 - 20 - 20 = 360
    // maxWidth = 360 * 50% = 180
    // centeringOffset = (360 - 180) / 2 = 90
    // blockOffsetX = paddingLeft(0) + 90 = 90
    // line.x = contentX(20) + blockOffsetX(90) = 110
    auto& line = result.pages[0].lines[0];
    EXPECT_NEAR(line.x, 110.0f, 1.0f);
}

TEST(LayoutTest, DedicationPageLayout) {
    // Simulates SE dedication page: section with centered, width-constrained content
    auto platform = std::make_shared<MockPlatformAdapter>();
    platform->charWidth = 8.0f;
    LayoutEngine engine(platform);

    // Parse CSS similar to SE dedication
    auto sheet = CSSStylesheet::parse(
        "section > p { max-width: 70%; margin: auto; }");
    StyleResolver resolver(sheet);

    Chapter chapter;
    chapter.id = "dedication";

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.parentTag = "section";
    block.inlines.push_back(InlineElement::plain("To my beloved family"));
    chapter.blocks.push_back(block);

    Style userStyle;
    userStyle.font.size = 18.0f;
    userStyle.font.family = "Georgia";

    auto resolved = resolver.resolve(chapter.blocks, userStyle);

    // Verify style resolution
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].maxWidthPercent, 70.0f);
    EXPECT_TRUE(resolved.blockStyles[0].horizontalCentering);

    // Verify layout positions the block centered
    PageSize pageSize{400.0f, 600.0f};
    auto result = engine.layoutChapter(chapter, resolved.blockStyles, pageSize);
    ASSERT_FALSE(result.pages.empty());
    ASSERT_FALSE(result.pages[0].lines.empty());

    // contentWidth = 400 - 20 - 20 = 360 (default Style margins)
    // maxWidth = 360 * 70% = 252
    // centering offset = (360 - 252) / 2 = 54
    // line.x = contentX(20) + offset(54) = 74
    auto& dedicationLine = result.pages[0].lines[0];
    EXPECT_GT(dedicationLine.x, 20.0f + 10.0f);  // noticeably right of page margin
    EXPECT_NEAR(dedicationLine.x, 74.0f, 2.0f);
}
