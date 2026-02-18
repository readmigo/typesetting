#include <gtest/gtest.h>
#include "typesetting/style_resolver.h"
#include "typesetting/css.h"

using namespace typesetting;

TEST(StyleResolverTest, DefaultParagraphStyle) {
    // No CSS rules, just defaults
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    userStyle.font.size = 18.0f;
    userStyle.font.family = "Georgia";

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_FLOAT_EQ(styles[0].textIndent, 18.0f); // 1em = fontSize
    EXPECT_EQ(styles[0].alignment, TextAlignment::Justified);
    EXPECT_TRUE(styles[0].hyphens);
}

TEST(StyleResolverTest, HeadingDefaultStyle) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Heading2;
    block.htmlTag = "h2";

    Style userStyle;
    userStyle.font.size = 18.0f;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_TRUE(styles[0].smallCaps);
    EXPECT_EQ(styles[0].alignment, TextAlignment::Center);
    EXPECT_FALSE(styles[0].hyphens);
    EXPECT_GT(styles[0].font.size, 18.0f); // Should be 1.3x
}

TEST(StyleResolverTest, CSSOverridesDefault) {
    // h2 + p { text-indent: 0; }
    auto sheet = CSSStylesheet::parse("h2 + p { text-indent: 0; }");
    StyleResolver resolver(sheet);

    Block h2block;
    h2block.type = BlockType::Heading2;
    h2block.htmlTag = "h2";

    Block pblock;
    pblock.type = BlockType::Paragraph;
    pblock.htmlTag = "p";
    pblock.previousSiblingTag = "h2";

    Style userStyle;
    userStyle.font.size = 18.0f;

    auto styles = resolver.resolve({h2block, pblock}, userStyle);
    ASSERT_EQ(styles.size(), 2);
    EXPECT_FLOAT_EQ(styles[1].textIndent, 0.0f); // CSS override
}

TEST(StyleResolverTest, SmallCapsForBold) {
    // SE: b, strong { font-variant: small-caps; font-weight: normal; }
    auto sheet = CSSStylesheet::parse("b, strong { font-variant: small-caps; font-weight: normal; }");
    StyleResolver resolver(sheet);
    // This affects inline styling, not block-level -- but we can verify
    // the resolver correctly applies font-variant at block level for elements
    // that match, and doesn't crash for non-matching blocks
    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    Style userStyle;
    auto styles = resolver.resolve({block}, userStyle);
    EXPECT_EQ(styles.size(), 1);
}

TEST(StyleResolverTest, DisplayNone) {
    auto sheet = CSSStylesheet::parse(
        ".epub-type-contains-word-titlepage h1 { display: none; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Heading1;
    block.htmlTag = "h1";
    block.parentTag = "section";
    block.parentClassName = "epub-type-contains-word-titlepage";

    Style userStyle;
    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_TRUE(styles[0].hidden);
}

TEST(StyleResolverTest, UserFontFamilyOverrides) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    userStyle.font.family = "Palatino";
    userStyle.font.size = 20.0f;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_EQ(styles[0].font.family, "Palatino");
}

TEST(StyleResolverTest, HeadingAlignmentPreserved) {
    // Even if user sets Left alignment, headings should stay centered
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Heading2;
    block.htmlTag = "h2";

    Style userStyle;
    userStyle.alignment = TextAlignment::Left;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_EQ(styles[0].alignment, TextAlignment::Center); // Preserved
}

TEST(StyleResolverTest, BlockquoteMargins) {
    auto sheet = CSSStylesheet::parse("blockquote { margin: 1em 2.5em; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Blockquote;
    block.htmlTag = "blockquote";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_FLOAT_EQ(styles[0].marginLeft, 40.0f);  // 2.5 * 16
    EXPECT_FLOAT_EQ(styles[0].marginRight, 40.0f); // 2.5 * 16
}

TEST(StyleResolverTest, FirstChildNoIndent) {
    auto sheet = CSSStylesheet::parse("p:first-child { text-indent: 0; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.isFirstChild = true;

    Style userStyle;
    userStyle.font.size = 18.0f;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_FLOAT_EQ(styles[0].textIndent, 0.0f);
}

TEST(StyleResolverTest, DescendantClassSelector) {
    auto sheet = CSSStylesheet::parse(
        ".epub-type-contains-word-z3998-song p { font-style: italic; text-indent: 0; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.parentClassName = "epub-type-contains-word-z3998-song";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_FLOAT_EQ(styles[0].textIndent, 0.0f);
    EXPECT_EQ(styles[0].font.style, FontStyle::Italic);
}

TEST(StyleResolverTest, EmptyStylesheet) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    userStyle.font.size = 16.0f;
    userStyle.font.family = "Helvetica";

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    // Should use defaults + user style
    EXPECT_EQ(styles[0].font.family, "Helvetica");
    EXPECT_FLOAT_EQ(styles[0].textIndent, 16.0f);
}

TEST(StyleResolverTest, EmptyBlocksVector) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);
    Style userStyle;

    auto styles = resolver.resolve({}, userStyle);
    EXPECT_TRUE(styles.empty());
}

TEST(StyleResolverTest, HorizontalRuleDefaults) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::HorizontalRule;
    block.htmlTag = "hr";

    Style userStyle;
    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    ASSERT_TRUE(styles[0].hrStyle.has_value());
    EXPECT_FLOAT_EQ(styles[0].hrStyle->borderWidth, 1.0f);
    EXPECT_FLOAT_EQ(styles[0].hrStyle->widthPercent, 25.0f);
}

TEST(StyleResolverTest, CSSHyphensDisabledPreserved) {
    // If CSS sets hyphens: none, user override should NOT re-enable
    auto sheet = CSSStylesheet::parse("blockquote { hyphens: none; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Blockquote;
    block.htmlTag = "blockquote";

    Style userStyle;
    userStyle.hyphenation = true;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_FALSE(styles[0].hyphens); // CSS disabled, stays disabled
}

TEST(StyleResolverTest, UniversalSelectorMatches) {
    auto sheet = CSSStylesheet::parse("* { hanging-punctuation: first; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_TRUE(styles[0].hangingPunctuation);
}

TEST(StyleResolverTest, SpecificityOrdering) {
    // More specific rule should override less specific
    auto sheet = CSSStylesheet::parse(
        "p { text-indent: 2em; }\n"
        ".special p { text-indent: 0; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.parentClassName = "special";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    // .special p (specificity 11) > p (specificity 1), so text-indent should be 0
    EXPECT_FLOAT_EQ(styles[0].textIndent, 0.0f);
}

TEST(StyleResolverTest, BlockTypeToTagFallback) {
    // Block with empty htmlTag should fall back to BlockType mapping
    CSSStylesheet sheet;
    auto parsed = CSSStylesheet::parse("h1 { text-align: center; }");
    StyleResolver resolver(parsed);

    Block block;
    block.type = BlockType::Heading1;
    // htmlTag intentionally left empty

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_EQ(styles[0].alignment, TextAlignment::Center);
}

TEST(StyleResolverTest, CodeBlockDefaults) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::CodeBlock;
    block.htmlTag = "pre";

    Style userStyle;
    userStyle.font.size = 16.0f;
    userStyle.font.family = "Georgia";

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    // Code block should still get user font family override
    EXPECT_EQ(styles[0].font.family, "Georgia");
    // But the computed size should be 0.9x
    EXPECT_FLOAT_EQ(styles[0].font.size, 16.0f * 0.9f);
    EXPECT_FALSE(styles[0].hyphens);
}

TEST(StyleResolverTest, HeadingFontSizePreserved) {
    // User font size should not override heading proportional size
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Heading1;
    block.htmlTag = "h1";

    Style userStyle;
    userStyle.font.size = 20.0f;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    // Heading1 = 1.5x user font size
    EXPECT_FLOAT_EQ(styles[0].font.size, 30.0f);
}

// =============================================================================
// MARK: - Figcaption Default Style Tests
// =============================================================================

TEST(StyleResolverTest, FigcaptionDefaultStyle) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Figcaption;
    block.htmlTag = "figcaption";

    Style userStyle;
    userStyle.font.size = 16.0f;
    // User alignment overrides figcaption's center; verify other properties
    userStyle.alignment = TextAlignment::Justified;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);

    // Figcaption: small text (0.85x), italic, no indent
    EXPECT_FLOAT_EQ(styles[0].font.size, 16.0f * 0.85f);
    EXPECT_EQ(styles[0].font.style, FontStyle::Italic);
    EXPECT_FLOAT_EQ(styles[0].textIndent, 0.0f);
    EXPECT_FALSE(styles[0].hyphens);
    // Note: user alignment overrides the default center alignment
    // (only headings preserve center alignment through user overrides)
    EXPECT_EQ(styles[0].alignment, TextAlignment::Justified);
}

TEST(StyleResolverTest, FigcaptionFontSizePreserved) {
    // Figcaption font size should NOT be overridden by user font size
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Figcaption;
    block.htmlTag = "figcaption";

    Style userStyle;
    userStyle.font.size = 20.0f;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    // Should be 0.85 * 20 = 17, not overridden to 20
    EXPECT_FLOAT_EQ(styles[0].font.size, 20.0f * 0.85f);
}

// =============================================================================
// MARK: - Table Default Style Tests
// =============================================================================

TEST(StyleResolverTest, TableDefaultStyle) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Table;
    block.htmlTag = "table";

    Style userStyle;
    userStyle.font.size = 16.0f;
    userStyle.alignment = TextAlignment::Justified;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);

    // Table: no indent, no hyphens
    EXPECT_FLOAT_EQ(styles[0].textIndent, 0.0f);
    EXPECT_FALSE(styles[0].hyphens);
    // Table margins: 1em top and bottom
    EXPECT_FLOAT_EQ(styles[0].marginTop, 16.0f);
    EXPECT_FLOAT_EQ(styles[0].marginBottom, 16.0f);
    // Note: user alignment overrides the default left alignment
    // (only headings preserve alignment through user overrides)
    EXPECT_EQ(styles[0].alignment, TextAlignment::Justified);
}

// =============================================================================
// MARK: - ListItem Default Style Tests
// =============================================================================

TEST(StyleResolverTest, ListItemDefaultStyle) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::ListItem;
    block.htmlTag = "li";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);

    // ListItem: left margin 2em, justified, hyphens on
    EXPECT_FLOAT_EQ(styles[0].marginLeft, 32.0f);  // 2.0 * 16
    EXPECT_EQ(styles[0].alignment, TextAlignment::Justified);
    EXPECT_TRUE(styles[0].hyphens);
}

// =============================================================================
// MARK: - Hanging Punctuation CSS Tests
// =============================================================================

TEST(StyleResolverTest, HangingPunctuationFromCSS) {
    auto sheet = CSSStylesheet::parse("p { hanging-punctuation: first; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_TRUE(styles[0].hangingPunctuation);
}

TEST(StyleResolverTest, HangingPunctuationDisabledByDefault) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    auto styles = resolver.resolve({block}, userStyle);
    ASSERT_EQ(styles.size(), 1);
    EXPECT_FALSE(styles[0].hangingPunctuation);
}
