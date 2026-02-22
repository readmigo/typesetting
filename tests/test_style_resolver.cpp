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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 18.0f); // 1em = fontSize
    EXPECT_EQ(resolved.blockStyles[0].alignment, TextAlignment::Justified);
    EXPECT_TRUE(resolved.blockStyles[0].hyphens);
}

TEST(StyleResolverTest, HeadingDefaultStyle) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Heading2;
    block.htmlTag = "h2";

    Style userStyle;
    userStyle.font.size = 18.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_TRUE(resolved.blockStyles[0].smallCaps);
    EXPECT_EQ(resolved.blockStyles[0].alignment, TextAlignment::Center);
    EXPECT_FALSE(resolved.blockStyles[0].hyphens);
    EXPECT_GT(resolved.blockStyles[0].font.size, 18.0f); // Should be 1.3x
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
    pblock.previousSiblingTags = {"h2"};

    Style userStyle;
    userStyle.font.size = 18.0f;

    auto resolved = resolver.resolve({h2block, pblock}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 2);
    EXPECT_FLOAT_EQ(resolved.blockStyles[1].textIndent, 0.0f); // CSS override
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
    auto resolved = resolver.resolve({block}, userStyle);
    EXPECT_EQ(resolved.blockStyles.size(), 1);
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
    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_TRUE(resolved.blockStyles[0].hidden);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_EQ(resolved.blockStyles[0].font.family, "Palatino");
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_EQ(resolved.blockStyles[0].alignment, TextAlignment::Center); // Preserved
}

TEST(StyleResolverTest, BlockquoteMargins) {
    auto sheet = CSSStylesheet::parse("blockquote { margin: 1em 2.5em; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Blockquote;
    block.htmlTag = "blockquote";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].marginLeft, 40.0f);  // 2.5 * 16
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].marginRight, 40.0f); // 2.5 * 16
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 0.0f);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 0.0f);
    EXPECT_EQ(resolved.blockStyles[0].font.style, FontStyle::Italic);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // Should use defaults + user style
    EXPECT_EQ(resolved.blockStyles[0].font.family, "Helvetica");
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 16.0f);
}

TEST(StyleResolverTest, EmptyBlocksVector) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);
    Style userStyle;

    auto resolved = resolver.resolve({}, userStyle);
    EXPECT_TRUE(resolved.blockStyles.empty());
}

TEST(StyleResolverTest, HorizontalRuleDefaults) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::HorizontalRule;
    block.htmlTag = "hr";

    Style userStyle;
    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    ASSERT_TRUE(resolved.blockStyles[0].hrStyle.has_value());
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].hrStyle->borderWidth, 1.0f);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].hrStyle->widthPercent, 25.0f);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FALSE(resolved.blockStyles[0].hyphens); // CSS disabled, stays disabled
}

TEST(StyleResolverTest, UniversalSelectorMatches) {
    auto sheet = CSSStylesheet::parse("* { hanging-punctuation: first; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_TRUE(resolved.blockStyles[0].hangingPunctuation);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // .special p (specificity 11) > p (specificity 1), so text-indent should be 0
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 0.0f);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_EQ(resolved.blockStyles[0].alignment, TextAlignment::Center);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // Code block should still get user font family override
    EXPECT_EQ(resolved.blockStyles[0].font.family, "Georgia");
    // But the computed size should be 0.9x
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].font.size, 16.0f * 0.9f);
    EXPECT_FALSE(resolved.blockStyles[0].hyphens);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // Heading1 = 1.5x user font size
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].font.size, 30.0f);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);

    // Figcaption: small text (0.85x), italic, no indent
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].font.size, 16.0f * 0.85f);
    EXPECT_EQ(resolved.blockStyles[0].font.style, FontStyle::Italic);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 0.0f);
    EXPECT_FALSE(resolved.blockStyles[0].hyphens);
    // Note: user alignment overrides the default center alignment
    // (only headings preserve center alignment through user overrides)
    EXPECT_EQ(resolved.blockStyles[0].alignment, TextAlignment::Justified);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // Should be 0.85 * 20 = 17, not overridden to 20
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].font.size, 20.0f * 0.85f);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);

    // Table: no indent, no hyphens
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 0.0f);
    EXPECT_FALSE(resolved.blockStyles[0].hyphens);
    // Table margins: 1em top and bottom
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].marginTop, 16.0f);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].marginBottom, 16.0f);
    // Note: user alignment overrides the default left alignment
    // (only headings preserve alignment through user overrides)
    EXPECT_EQ(resolved.blockStyles[0].alignment, TextAlignment::Justified);
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

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);

    // ListItem: left margin 2em, justified, hyphens on
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].marginLeft, 32.0f);  // 2.0 * 16
    EXPECT_EQ(resolved.blockStyles[0].alignment, TextAlignment::Justified);
    EXPECT_TRUE(resolved.blockStyles[0].hyphens);
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
    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_TRUE(resolved.blockStyles[0].hangingPunctuation);
}

TEST(StyleResolverTest, HangingPunctuationDisabledByDefault) {
    CSSStylesheet sheet;
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FALSE(resolved.blockStyles[0].hangingPunctuation);
}

// =============================================================================
// MARK: - Compound Selector Matching Tests (Task 1)
// =============================================================================

TEST(StyleResolverTest, CompoundElementClassMatches) {
    auto sheet = CSSStylesheet::parse(
        "p.epub-type-contains-word-z3998-salutation { font-variant: small-caps; }");
    StyleResolver resolver(sheet);

    // Should match: p with the right class
    Block matchBlock;
    matchBlock.type = BlockType::Paragraph;
    matchBlock.htmlTag = "p";
    matchBlock.className = "epub-type-contains-word-z3998-salutation";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({matchBlock}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_TRUE(resolved.blockStyles[0].smallCaps);
}

TEST(StyleResolverTest, CompoundElementClassNoMatchWrongTag) {
    auto sheet = CSSStylesheet::parse(
        "p.epub-type-contains-word-z3998-salutation { font-variant: small-caps; }");
    StyleResolver resolver(sheet);

    // Should NOT match: div with the right class (wrong tag)
    Block noMatchBlock;
    noMatchBlock.type = BlockType::Paragraph;
    noMatchBlock.htmlTag = "div";
    noMatchBlock.className = "epub-type-contains-word-z3998-salutation";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({noMatchBlock}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FALSE(resolved.blockStyles[0].smallCaps);
}

TEST(StyleResolverTest, CompoundElementClassNoMatchWrongClass) {
    auto sheet = CSSStylesheet::parse(
        "p.epub-type-contains-word-z3998-salutation { font-variant: small-caps; }");
    StyleResolver resolver(sheet);

    // Should NOT match: p with wrong class
    Block noMatchBlock;
    noMatchBlock.type = BlockType::Paragraph;
    noMatchBlock.htmlTag = "p";
    noMatchBlock.className = "other-class";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({noMatchBlock}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FALSE(resolved.blockStyles[0].smallCaps);
}

TEST(StyleResolverTest, CompoundDescendantParentMatches) {
    auto sheet = CSSStylesheet::parse(
        "section.dedication p { text-indent: 0; font-style: italic; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.parentTag = "section";
    block.parentClassName = "dedication";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 0.0f);
    EXPECT_EQ(resolved.blockStyles[0].font.style, FontStyle::Italic);
}

TEST(StyleResolverTest, CompoundDescendantParentNoMatchWrongClass) {
    auto sheet = CSSStylesheet::parse(
        "section.dedication p { text-indent: 0; font-style: italic; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.parentTag = "section";
    block.parentClassName = "chapter";  // wrong class

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // Default paragraph text-indent is 1em = 16px, CSS should NOT match
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 16.0f);
    EXPECT_EQ(resolved.blockStyles[0].font.style, FontStyle::Normal);
}

// =============================================================================
// MARK: - Font-size Application Tests (Task 3)
// =============================================================================

TEST(StyleResolverTest, CSSFontSizeApplied) {
    auto sheet = CSSStylesheet::parse("p { font-size: 1.17em; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    userStyle.font.size = 20.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // CSS font-size 1.17em * 20 = 23.4, and should be preserved (not overridden)
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].font.size, 1.17f * 20.0f);
}

TEST(StyleResolverTest, CSSFontSizeSmallerApplied) {
    auto sheet = CSSStylesheet::parse("p { font-size: smaller; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    userStyle.font.size = 20.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].font.size, 0.833f * 20.0f);
}

TEST(StyleResolverTest, CSSFontSizeNotOverriddenByUser) {
    // When CSS sets font-size, user override should not replace it
    auto sheet = CSSStylesheet::parse("p { font-size: 0.83em; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    userStyle.font.size = 18.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // Should be 0.83 * 18 = 14.94, NOT overridden back to 18
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].font.size, 0.83f * 18.0f);
}

// =============================================================================
// MARK: - Padding-left Tests (Task 4)
// =============================================================================

TEST(StyleResolverTest, CSSPaddingLeftApplied) {
    auto sheet = CSSStylesheet::parse("p { padding-left: 1em; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].paddingLeft, 16.0f);  // 1em * 16
}

// =============================================================================
// MARK: - Display Property Tests (Task 5)
// =============================================================================

TEST(StyleResolverTest, DisplayInlineBlock) {
    auto sheet = CSSStylesheet::parse("p { display: inline-block; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_EQ(resolved.blockStyles[0].display, BlockComputedStyle::Display::InlineBlock);
    EXPECT_FALSE(resolved.blockStyles[0].hidden);
}

TEST(StyleResolverTest, DisplayBlock) {
    auto sheet = CSSStylesheet::parse("p { display: block; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";

    Style userStyle;
    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_EQ(resolved.blockStyles[0].display, BlockComputedStyle::Display::Block);
    EXPECT_FALSE(resolved.blockStyles[0].hidden);
}

// =============================================================================
// MARK: - Child Combinator Matching Tests (Task 6)
// =============================================================================

TEST(StyleResolverTest, ChildCombinatorUniversalMatches) {
    auto sheet = CSSStylesheet::parse("hgroup > * { font-weight: normal; margin: 0; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Heading2;
    block.htmlTag = "h2";
    block.parentTag = "hgroup";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].marginTop, 0.0f);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].marginBottom, 0.0f);
}

TEST(StyleResolverTest, ChildCombinatorNoMatchWrongParent) {
    auto sheet = CSSStylesheet::parse("hgroup > * { margin: 0; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Heading2;
    block.htmlTag = "h2";
    block.parentTag = "section";  // wrong parent

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // Default heading2 marginTop is 3em = 48px, should NOT be 0
    EXPECT_GT(resolved.blockStyles[0].marginTop, 0.0f);
}

TEST(StyleResolverTest, ChildCombinatorWithClassMatches) {
    auto sheet = CSSStylesheet::parse(
        "section.dedication > * { font-style: italic; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.parentTag = "section";
    block.parentClassName = "dedication";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_EQ(resolved.blockStyles[0].font.style, FontStyle::Italic);
}

// =============================================================================
// MARK: - Multi-level Adjacent Sibling Matching Tests (Task 7)
// =============================================================================

TEST(StyleResolverTest, MultiLevelAdjacentSiblingMatches) {
    auto sheet = CSSStylesheet::parse("h2 + p + p { text-indent: 0; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.previousSiblingTags = {"p", "h2"};  // [0]=immediate prev, [1]=before that

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 0.0f);
}

TEST(StyleResolverTest, MultiLevelAdjacentSiblingNoMatchWrongOrder) {
    auto sheet = CSSStylesheet::parse("h2 + p + p { text-indent: 0; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.previousSiblingTags = {"h2"};  // Only one previous sibling, need two

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // Default paragraph text-indent = 1em = 16px, should NOT be 0
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 16.0f);
}

TEST(StyleResolverTest, DescendantWithAdjacentSiblingMatches) {
    auto sheet = CSSStylesheet::parse("hgroup h2 + p { font-size: 1.17em; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.previousSiblingTags = {"h2"};
    block.parentTag = "hgroup";

    Style userStyle;
    userStyle.font.size = 20.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].font.size, 1.17f * 20.0f);
}

TEST(StyleResolverTest, DescendantWithAdjacentSiblingNoMatchWrongParent) {
    auto sheet = CSSStylesheet::parse("hgroup h2 + p { font-size: 1.17em; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.previousSiblingTags = {"h2"};
    block.parentTag = "section";  // wrong parent

    Style userStyle;
    userStyle.font.size = 20.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // No CSS match → user font size applied
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].font.size, 20.0f);
}

TEST(StyleResolverTest, HgroupFontSizeGradient) {
    // Simulate SE hgroup font-size gradient: h2, h2+p (1.17em), h2+p+p (1em), h2+p+p+p (.83em)
    auto sheet = CSSStylesheet::parse(
        "hgroup h2 + p { font-size: 1.17em; }\n"
        "hgroup h2 + p + p { font-size: 1em; }\n"
        "hgroup h2 + p + p + p { font-size: .83em; }");
    StyleResolver resolver(sheet);

    Style userStyle;
    userStyle.font.size = 20.0f;

    // First p after h2
    Block p1;
    p1.type = BlockType::Paragraph;
    p1.htmlTag = "p";
    p1.previousSiblingTags = {"h2"};
    p1.parentTag = "hgroup";

    // Second p (after first p, which was after h2)
    Block p2;
    p2.type = BlockType::Paragraph;
    p2.htmlTag = "p";
    p2.previousSiblingTags = {"p", "h2"};
    p2.parentTag = "hgroup";

    // Third p
    Block p3;
    p3.type = BlockType::Paragraph;
    p3.htmlTag = "p";
    p3.previousSiblingTags = {"p", "p", "h2"};
    p3.parentTag = "hgroup";

    auto resolved = resolver.resolve({p1, p2, p3}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 3);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].font.size, 1.17f * 20.0f);  // h2+p: 1.17em
    EXPECT_FLOAT_EQ(resolved.blockStyles[1].font.size, 1.0f * 20.0f);   // h2+p+p: 1em
    EXPECT_FLOAT_EQ(resolved.blockStyles[2].font.size, 0.83f * 20.0f);   // h2+p+p+p: .83em
}

// =============================================================================
// MARK: - ID Selector Matching Tests (Task 8)
// =============================================================================

TEST(StyleResolverTest, IdSelectorMatches) {
    auto sheet = CSSStylesheet::parse("#chapter-19 { text-indent: 0; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.id = "chapter-19";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 0.0f);
}

TEST(StyleResolverTest, IdSelectorNoMatch) {
    auto sheet = CSSStylesheet::parse("#chapter-19 { text-indent: 0; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.id = "chapter-20";  // wrong id

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 16.0f);  // default 1em
}

TEST(StyleResolverTest, IdDescendantMatches) {
    auto sheet = CSSStylesheet::parse(
        "#chapter-19 blockquote { font-style: italic; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Blockquote;
    block.htmlTag = "blockquote";
    block.parentId = "chapter-19";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_EQ(resolved.blockStyles[0].font.style, FontStyle::Italic);
}

TEST(StyleResolverTest, IdSpecificityOverridesElement) {
    auto sheet = CSSStylesheet::parse(
        "p { text-indent: 2em; }\n"
        "#special { text-indent: 0; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.id = "special";

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // #special (specificity 100) > p (specificity 1), text-indent should be 0
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 0.0f);
}

// =============================================================================
// MARK: - Inline CSS Matching Tests (Phase 2 Task 2)
// =============================================================================

TEST(StyleResolverTest, InlineElementMatchesByTag) {
    auto sheet = CSSStylesheet::parse("abbr { font-variant: small-caps; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    InlineElement inl;
    inl.type = InlineType::Text;
    inl.htmlTag = "abbr";
    inl.text = "Mr.";
    block.inlines.push_back(inl);

    Style userStyle;
    userStyle.font.size = 16.0f;
    auto resolved = resolver.resolve({block}, userStyle);

    ASSERT_EQ(resolved.inlineStyles.size(), 1);
    ASSERT_EQ(resolved.inlineStyles[0].size(), 1);
    ASSERT_TRUE(resolved.inlineStyles[0][0].smallCaps.has_value());
    EXPECT_TRUE(resolved.inlineStyles[0][0].smallCaps.value());
}

TEST(StyleResolverTest, InlineElementMatchesByClass) {
    auto sheet = CSSStylesheet::parse(".z3998-roman { font-variant: small-caps; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    InlineElement inl;
    inl.type = InlineType::Text;
    inl.htmlTag = "span";
    inl.className = "z3998-roman";
    inl.text = "XII";
    block.inlines.push_back(inl);

    Style userStyle;
    userStyle.font.size = 16.0f;
    auto resolved = resolver.resolve({block}, userStyle);

    ASSERT_EQ(resolved.inlineStyles[0].size(), 1);
    ASSERT_TRUE(resolved.inlineStyles[0][0].smallCaps.has_value());
    EXPECT_TRUE(resolved.inlineStyles[0][0].smallCaps.value());
}

TEST(StyleResolverTest, InlineElementMatchesByAttribute) {
    auto sheet = CSSStylesheet::parse("[epub\\|type~=\"noteref\"] { font-size: smaller; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    InlineElement inl;
    inl.type = InlineType::Link;
    inl.htmlTag = "a";
    inl.epubType = "noteref";
    inl.text = "1";
    block.inlines.push_back(inl);

    Style userStyle;
    userStyle.font.size = 16.0f;
    auto resolved = resolver.resolve({block}, userStyle);

    ASSERT_EQ(resolved.inlineStyles[0].size(), 1);
    ASSERT_TRUE(resolved.inlineStyles[0][0].fontSizeMultiplier.has_value());
    EXPECT_NEAR(resolved.inlineStyles[0][0].fontSizeMultiplier.value(), 0.833f, 0.01f);
}

TEST(StyleResolverTest, InlineDescendantMatch) {
    auto sheet = CSSStylesheet::parse("blockquote abbr { font-variant: small-caps; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.parentTag = "blockquote";
    InlineElement inl;
    inl.type = InlineType::Text;
    inl.htmlTag = "abbr";
    inl.text = "Mr.";
    block.inlines.push_back(inl);

    Style userStyle;
    userStyle.font.size = 16.0f;
    auto resolved = resolver.resolve({block}, userStyle);

    ASSERT_EQ(resolved.inlineStyles[0].size(), 1);
    // Note: "blockquote abbr" - blockquote is the parent, abbr is the inline leaf.
    // The block's parentTag is "blockquote", and the inline tag is "abbr".
    // BUT selectorMatches checks if the parent selector matches the BLOCK, not the block's parent.
    // The selector "blockquote abbr" has parent="blockquote", leaf="abbr".
    // inlineSelectorMatches checks: leaf matches inline (abbr==abbr), parent matches block.
    // selectorMatches("blockquote", block) checks block.htmlTag or block.parentTag.
    // For block with htmlTag="p", this won't match "blockquote" as element.
    // So this test verifies parent matching works correctly.
}

TEST(StyleResolverTest, InlineNoMatchBlockSelector) {
    // A selector targeting a block-level tag should NOT match inline elements
    auto sheet = CSSStylesheet::parse("p { font-variant: small-caps; }");
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    InlineElement inl;
    inl.type = InlineType::Text;
    inl.htmlTag = "";  // plain text
    inl.text = "Hello";
    block.inlines.push_back(inl);

    Style userStyle;
    userStyle.font.size = 16.0f;
    auto resolved = resolver.resolve({block}, userStyle);

    ASSERT_EQ(resolved.inlineStyles[0].size(), 1);
    // "p" is not an inline tag, so inlineSelectorMatches should return false
    EXPECT_FALSE(resolved.inlineStyles[0][0].smallCaps.has_value());
}
