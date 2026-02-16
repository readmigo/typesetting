#include <gtest/gtest.h>
#include "typesetting/engine.h"
#include "typesetting/document.h"
#include "typesetting/style.h"
#include "typesetting/page.h"

using namespace typesetting;

/// Mock platform adapter for testing
class MockPlatformAdapter : public PlatformAdapter {
public:
    float charWidth = 8.0f; // Fixed-width for predictable tests

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
    EXPECT_EQ(blocks[1].previousSiblingTag, "h2");
    EXPECT_EQ(blocks[1].isFirstChild, false);
    // second p
    EXPECT_EQ(blocks[2].previousSiblingTag, "p");
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
            EXPECT_EQ(blocks[i + 1].previousSiblingTag, "hr");
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
