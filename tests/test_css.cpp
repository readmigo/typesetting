#include <gtest/gtest.h>
#include "typesetting/css.h"

using namespace typesetting;

TEST(CSSTest, ParseElementSelector) {
    auto sheet = CSSStylesheet::parse("p { text-indent: 1em; margin: 0; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.element, "p");
    EXPECT_TRUE(rule.properties.textIndent.has_value());
    EXPECT_FLOAT_EQ(rule.properties.textIndent.value(), 1.0f);
    EXPECT_TRUE(rule.properties.marginTop.has_value());
    EXPECT_FLOAT_EQ(rule.properties.marginTop.value(), 0.0f);
}

TEST(CSSTest, ParseClassSelector) {
    auto sheet = CSSStylesheet::parse(
        ".epub-type-contains-word-z3998-song p { font-style: italic; text-indent: 0; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::Descendant);
    EXPECT_EQ(rule.selector.element, "p");
    ASSERT_NE(rule.selector.parent, nullptr);
    EXPECT_EQ(rule.selector.parent->className, "epub-type-contains-word-z3998-song");
    EXPECT_TRUE(rule.properties.fontStyle.has_value());
    EXPECT_EQ(rule.properties.fontStyle.value(), FontStyle::Italic);
}

TEST(CSSTest, ParseAdjacentSiblingSelector) {
    auto sheet = CSSStylesheet::parse("h2 + p { text-indent: 0; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::AdjacentSibling);
    EXPECT_EQ(rule.selector.element, "p");
    ASSERT_NE(rule.selector.adjacentSibling, nullptr);
    EXPECT_EQ(rule.selector.adjacentSibling->element, "h2");
}

TEST(CSSTest, ParseCommaSelectors) {
    auto sheet = CSSStylesheet::parse("b, strong { font-variant: small-caps; font-weight: normal; }");
    ASSERT_GE(sheet.rules.size(), 2);
    // Both rules should have same properties
    for (auto& rule : sheet.rules) {
        EXPECT_TRUE(rule.properties.fontVariant.has_value());
        EXPECT_EQ(rule.properties.fontVariant.value(), FontVariant::SmallCaps);
        EXPECT_TRUE(rule.properties.fontWeight.has_value());
        EXPECT_EQ(rule.properties.fontWeight.value(), FontWeight::Regular);
    }
}

TEST(CSSTest, ParseMarginShorthand) {
    auto sheet = CSSStylesheet::parse("blockquote { margin: 1em 2.5em; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_FLOAT_EQ(props.marginTop.value_or(-1), 1.0f);
    EXPECT_FLOAT_EQ(props.marginRight.value_or(-1), 2.5f);
    EXPECT_FLOAT_EQ(props.marginBottom.value_or(-1), 1.0f);
    EXPECT_FLOAT_EQ(props.marginLeft.value_or(-1), 2.5f);
}

TEST(CSSTest, ParseDisplayNone) {
    auto sheet = CSSStylesheet::parse("h1 { display: none; }");
    ASSERT_GE(sheet.rules.size(), 1);
    EXPECT_TRUE(sheet.rules[0].properties.display.has_value());
    EXPECT_EQ(sheet.rules[0].properties.display.value(), "none");
}

TEST(CSSTest, SkipAtRules) {
    auto sheet = CSSStylesheet::parse(
        "@media (prefers-color-scheme: dark) { body { color: white; } }\n"
        "p { text-indent: 1em; }");
    // Should only have the p rule, @media skipped
    ASSERT_GE(sheet.rules.size(), 1);
    EXPECT_EQ(sheet.rules[0].selector.element, "p");
}

TEST(CSSTest, ParsePseudoFirstChild) {
    auto sheet = CSSStylesheet::parse("p:first-child { text-indent: 0; }");
    ASSERT_GE(sheet.rules.size(), 1);
    EXPECT_EQ(sheet.rules[0].selector.type, SelectorType::PseudoFirstChild);
    EXPECT_EQ(sheet.rules[0].selector.element, "p");
}

TEST(CSSTest, SpecificityOrder) {
    auto sheet = CSSStylesheet::parse(
        "p { text-indent: 1em; }\n"
        ".song p { text-indent: 0; }\n"
        "h2 + p { text-indent: 0; }");
    ASSERT_GE(sheet.rules.size(), 3);
    // Class selector should have higher specificity than element
    int elemSpec = -1, classSpec = -1;
    for (auto& rule : sheet.rules) {
        if (rule.selector.type == SelectorType::Element) {
            elemSpec = rule.selector.specificity();
        }
        if (rule.selector.type == SelectorType::Descendant && rule.selector.parent) {
            classSpec = rule.selector.specificity();
        }
    }
    EXPECT_GT(classSpec, elemSpec);
}

TEST(CSSTest, StripComments) {
    auto sheet = CSSStylesheet::parse(
        "/* This is a comment */\n"
        "p { text-indent: 1em; /* inline comment */ }");
    ASSERT_GE(sheet.rules.size(), 1);
    EXPECT_TRUE(sheet.rules[0].properties.textIndent.has_value());
}

TEST(CSSTest, ParseHRStyles) {
    auto sheet = CSSStylesheet::parse("hr { border-top: 1px solid; width: 25%; margin: 1.5em auto; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_FLOAT_EQ(props.borderTopWidth.value_or(0), 1.0f);
    EXPECT_FLOAT_EQ(props.widthPercent.value_or(0), 25.0f);
}

TEST(CSSTest, ParseEmptyCSS) {
    auto sheet = CSSStylesheet::parse("");
    EXPECT_TRUE(sheet.rules.empty());
}

TEST(CSSTest, ParseMarginShorthandSingleValue) {
    auto sheet = CSSStylesheet::parse("p { margin: 2em; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_FLOAT_EQ(props.marginTop.value_or(-1), 2.0f);
    EXPECT_FLOAT_EQ(props.marginRight.value_or(-1), 2.0f);
    EXPECT_FLOAT_EQ(props.marginBottom.value_or(-1), 2.0f);
    EXPECT_FLOAT_EQ(props.marginLeft.value_or(-1), 2.0f);
}

TEST(CSSTest, MergeProperties) {
    CSSProperties base;
    base.textIndent = 1.0f;
    base.marginTop = 0.5f;

    CSSProperties override;
    override.textIndent = 0.0f;
    override.fontStyle = FontStyle::Italic;

    base.merge(override);
    EXPECT_FLOAT_EQ(base.textIndent.value(), 0.0f);
    EXPECT_FLOAT_EQ(base.marginTop.value(), 0.5f);
    EXPECT_EQ(base.fontStyle.value(), FontStyle::Italic);
}

TEST(CSSTest, ParseHyphens) {
    auto sheet = CSSStylesheet::parse("p { hyphens: auto; }");
    ASSERT_GE(sheet.rules.size(), 1);
    EXPECT_TRUE(sheet.rules[0].properties.hyphens.value_or(false));

    auto sheet2 = CSSStylesheet::parse("h2 { hyphens: none; }");
    ASSERT_GE(sheet2.rules.size(), 1);
    EXPECT_FALSE(sheet2.rules[0].properties.hyphens.value_or(true));
}

TEST(CSSTest, ParseTextAlign) {
    auto sheet = CSSStylesheet::parse("p { text-align: center; }");
    ASSERT_GE(sheet.rules.size(), 1);
    EXPECT_EQ(sheet.rules[0].properties.textAlign.value(), TextAlignment::Center);
}

TEST(CSSTest, ParseMultipleRules) {
    auto sheet = CSSStylesheet::parse(
        "p { text-indent: 1em; }\n"
        "blockquote { margin-left: 2em; }\n"
        "h2 { text-align: center; }");
    ASSERT_EQ(sheet.rules.size(), 3);
    EXPECT_EQ(sheet.rules[0].selector.element, "p");
    EXPECT_EQ(sheet.rules[1].selector.element, "blockquote");
    EXPECT_EQ(sheet.rules[2].selector.element, "h2");
}

TEST(CSSTest, SkipNamespaceAtRule) {
    auto sheet = CSSStylesheet::parse(
        "@namespace epub \"http://www.idpf.org/2007/ops\";\n"
        "p { text-indent: 1em; }");
    ASSERT_GE(sheet.rules.size(), 1);
    EXPECT_EQ(sheet.rules[0].selector.element, "p");
}

TEST(CSSTest, ParseMarginAuto) {
    auto sheet = CSSStylesheet::parse("hr { margin: 1.5em auto; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_FLOAT_EQ(props.marginTop.value_or(-1), 1.5f);
    EXPECT_FLOAT_EQ(props.marginBottom.value_or(-1), 1.5f);
}

// =============================================================================
// MARK: - Compound Selector Tests (Task 1)
// =============================================================================

TEST(CSSTest, ParseCompoundElementClass) {
    auto sheet = CSSStylesheet::parse(
        "p.epub-type-contains-word-z3998-salutation { font-variant: small-caps; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::Element);
    EXPECT_EQ(rule.selector.element, "p");
    EXPECT_EQ(rule.selector.className, "epub-type-contains-word-z3998-salutation");
    EXPECT_TRUE(rule.properties.fontVariant.has_value());
    EXPECT_EQ(rule.properties.fontVariant.value(), FontVariant::SmallCaps);
}

TEST(CSSTest, ParseCompoundElementClassPseudo) {
    auto sheet = CSSStylesheet::parse(
        "p.special:first-child { text-indent: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::PseudoFirstChild);
    EXPECT_EQ(rule.selector.element, "p");
    EXPECT_EQ(rule.selector.className, "special");
    EXPECT_EQ(rule.selector.pseudoClass, "first-child");
}

TEST(CSSTest, CompoundSelectorSpecificity) {
    auto sheet = CSSStylesheet::parse(
        "p { text-indent: 1em; }\n"
        "p.special { text-indent: 0; }");
    ASSERT_EQ(sheet.rules.size(), 2);
    // p = specificity 1, p.special = specificity 11 (element + class)
    EXPECT_EQ(sheet.rules[0].selector.specificity(), 1);
    EXPECT_EQ(sheet.rules[1].selector.specificity(), 11);
}

TEST(CSSTest, ParseCompoundInDescendant) {
    auto sheet = CSSStylesheet::parse(
        "section.dedication p { text-align: center; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::Descendant);
    EXPECT_EQ(rule.selector.element, "p");
    ASSERT_NE(rule.selector.parent, nullptr);
    EXPECT_EQ(rule.selector.parent->element, "section");
    EXPECT_EQ(rule.selector.parent->className, "dedication");
}

// =============================================================================
// MARK: - Font-size Parsing Tests (Task 3)
// =============================================================================

TEST(CSSTest, ParseFontSizeEm) {
    auto sheet = CSSStylesheet::parse("p { font-size: 1.17em; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    EXPECT_TRUE(sheet.rules[0].properties.fontSize.has_value());
    EXPECT_FLOAT_EQ(sheet.rules[0].properties.fontSize.value(), 1.17f);
}

TEST(CSSTest, ParseFontSizeSmaller) {
    auto sheet = CSSStylesheet::parse("a { font-size: smaller; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    EXPECT_TRUE(sheet.rules[0].properties.fontSize.has_value());
    EXPECT_FLOAT_EQ(sheet.rules[0].properties.fontSize.value(), 0.833f);
}

TEST(CSSTest, ParseFontSizeLarger) {
    auto sheet = CSSStylesheet::parse("p { font-size: larger; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    EXPECT_TRUE(sheet.rules[0].properties.fontSize.has_value());
    EXPECT_FLOAT_EQ(sheet.rules[0].properties.fontSize.value(), 1.2f);
}

TEST(CSSTest, ParseFontSizePercent) {
    auto sheet = CSSStylesheet::parse("p { font-size: 83%; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    EXPECT_TRUE(sheet.rules[0].properties.fontSize.has_value());
    EXPECT_FLOAT_EQ(sheet.rules[0].properties.fontSize.value(), 0.83f);
}

TEST(CSSTest, FontSizeMerge) {
    CSSProperties base;
    base.fontSize = 1.0f;

    CSSProperties override_props;
    override_props.fontSize = 1.17f;

    base.merge(override_props);
    EXPECT_FLOAT_EQ(base.fontSize.value(), 1.17f);
}

// =============================================================================
// MARK: - Child Combinator Tests (Task 6)
// =============================================================================

TEST(CSSTest, ParseChildCombinator) {
    auto sheet = CSSStylesheet::parse("hgroup > * { font-weight: normal; margin: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::Descendant);
    EXPECT_EQ(rule.selector.element, "*");
    ASSERT_NE(rule.selector.parent, nullptr);
    EXPECT_EQ(rule.selector.parent->element, "hgroup");
    EXPECT_TRUE(rule.selector.isChildCombinator);
}

TEST(CSSTest, ParseChildCombinatorWithClass) {
    auto sheet = CSSStylesheet::parse(
        "section.dedication > * { display: inline-block; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::Descendant);
    EXPECT_EQ(rule.selector.element, "*");
    ASSERT_NE(rule.selector.parent, nullptr);
    EXPECT_EQ(rule.selector.parent->element, "section");
    EXPECT_EQ(rule.selector.parent->className, "dedication");
    EXPECT_TRUE(rule.selector.isChildCombinator);
}

TEST(CSSTest, ParseChildCombinatorElement) {
    auto sheet = CSSStylesheet::parse("section > p { text-indent: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::Descendant);
    EXPECT_EQ(rule.selector.element, "p");
    ASSERT_NE(rule.selector.parent, nullptr);
    EXPECT_EQ(rule.selector.parent->element, "section");
    EXPECT_TRUE(rule.selector.isChildCombinator);
}

// =============================================================================
// MARK: - Multi-level Adjacent Sibling Tests (Task 7)
// =============================================================================

TEST(CSSTest, ParseMultiLevelAdjacentSibling) {
    auto sheet = CSSStylesheet::parse("h2 + p + p { font-size: 1em; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::AdjacentSibling);
    EXPECT_EQ(rule.selector.element, "p");
    ASSERT_NE(rule.selector.adjacentSibling, nullptr);
    EXPECT_EQ(rule.selector.adjacentSibling->element, "p");
    ASSERT_NE(rule.selector.adjacentSibling->adjacentSibling, nullptr);
    EXPECT_EQ(rule.selector.adjacentSibling->adjacentSibling->element, "h2");
}

TEST(CSSTest, ParseTripleLevelAdjacentSibling) {
    auto sheet = CSSStylesheet::parse("h2 + p + p + p { font-size: .83em; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::AdjacentSibling);
    EXPECT_EQ(rule.selector.element, "p");
    // Chain: p -> p -> h2
    auto* sib1 = rule.selector.adjacentSibling.get();
    ASSERT_NE(sib1, nullptr);
    EXPECT_EQ(sib1->element, "p");
    auto* sib2 = sib1->adjacentSibling.get();
    ASSERT_NE(sib2, nullptr);
    EXPECT_EQ(sib2->element, "p");
    auto* sib3 = sib2->adjacentSibling.get();
    ASSERT_NE(sib3, nullptr);
    EXPECT_EQ(sib3->element, "h2");
}

TEST(CSSTest, ParseDescendantWithAdjacentSibling) {
    auto sheet = CSSStylesheet::parse("hgroup h2 + p { font-size: 1.17em; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::AdjacentSibling);
    EXPECT_EQ(rule.selector.element, "p");
    ASSERT_NE(rule.selector.adjacentSibling, nullptr);
    EXPECT_EQ(rule.selector.adjacentSibling->element, "h2");
    // Descendant context
    ASSERT_NE(rule.selector.parent, nullptr);
    EXPECT_EQ(rule.selector.parent->element, "hgroup");
}

TEST(CSSTest, ParseDescendantWithMultiLevelSibling) {
    auto sheet = CSSStylesheet::parse("hgroup h2 + p + p { font-size: 1em; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::AdjacentSibling);
    EXPECT_EQ(rule.selector.element, "p");
    // Sibling chain: p -> h2
    ASSERT_NE(rule.selector.adjacentSibling, nullptr);
    EXPECT_EQ(rule.selector.adjacentSibling->element, "p");
    ASSERT_NE(rule.selector.adjacentSibling->adjacentSibling, nullptr);
    EXPECT_EQ(rule.selector.adjacentSibling->adjacentSibling->element, "h2");
    // Parent context
    ASSERT_NE(rule.selector.parent, nullptr);
    EXPECT_EQ(rule.selector.parent->element, "hgroup");
}

// =============================================================================
// MARK: - ID Selector Tests (Task 8)
// =============================================================================

TEST(CSSTest, ParseIdSelector) {
    auto sheet = CSSStylesheet::parse("#chapter-19 { text-align: center; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::Id);
    EXPECT_EQ(rule.selector.id, "chapter-19");
}

TEST(CSSTest, ParseIdDescendant) {
    auto sheet = CSSStylesheet::parse("#chapter-19 blockquote { margin: 2em; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::Descendant);
    EXPECT_EQ(rule.selector.element, "blockquote");
    ASSERT_NE(rule.selector.parent, nullptr);
    EXPECT_EQ(rule.selector.parent->type, SelectorType::Id);
    EXPECT_EQ(rule.selector.parent->id, "chapter-19");
}

TEST(CSSTest, ParseCompoundElementId) {
    auto sheet = CSSStylesheet::parse("section#intro { margin-top: 2em; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    auto& rule = sheet.rules[0];
    EXPECT_EQ(rule.selector.type, SelectorType::Element);
    EXPECT_EQ(rule.selector.element, "section");
    EXPECT_EQ(rule.selector.id, "intro");
}

TEST(CSSTest, IdSpecificity) {
    auto sheet = CSSStylesheet::parse(
        "p { text-indent: 1em; }\n"
        "#special { text-indent: 0; }");
    ASSERT_EQ(sheet.rules.size(), 2);
    EXPECT_EQ(sheet.rules[0].selector.specificity(), 1);   // element
    EXPECT_EQ(sheet.rules[1].selector.specificity(), 100);  // id
}

// --- Task 3: text-transform ---

TEST(CSSTest, ParseTextTransform) {
    auto sheet = CSSStylesheet::parse("p { text-transform: uppercase; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    ASSERT_TRUE(sheet.rules[0].properties.textTransform.has_value());
    EXPECT_EQ(sheet.rules[0].properties.textTransform.value(), TextTransform::Uppercase);
}

TEST(CSSTest, ParseTextTransformLowercase) {
    auto sheet = CSSStylesheet::parse("blockquote { text-transform: lowercase; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    ASSERT_TRUE(sheet.rules[0].properties.textTransform.has_value());
    EXPECT_EQ(sheet.rules[0].properties.textTransform.value(), TextTransform::Lowercase);
}

TEST(CSSTest, ParseTextTransformCapitalize) {
    auto sheet = CSSStylesheet::parse("h1 { text-transform: capitalize; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    ASSERT_TRUE(sheet.rules[0].properties.textTransform.has_value());
    EXPECT_EQ(sheet.rules[0].properties.textTransform.value(), TextTransform::Capitalize);
}

// --- Task 4: vertical-align ---

TEST(CSSTest, ParseVerticalAlign) {
    auto sheet = CSSStylesheet::parse("a { vertical-align: super; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    ASSERT_TRUE(sheet.rules[0].properties.verticalAlign.has_value());
    EXPECT_EQ(sheet.rules[0].properties.verticalAlign.value(), "super");
}

TEST(CSSTest, ParseVerticalAlignSub) {
    auto sheet = CSSStylesheet::parse("span { vertical-align: sub; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    ASSERT_TRUE(sheet.rules[0].properties.verticalAlign.has_value());
    EXPECT_EQ(sheet.rules[0].properties.verticalAlign.value(), "sub");
}

// --- Task 5: white-space ---

TEST(CSSTest, ParseWhiteSpace) {
    auto sheet = CSSStylesheet::parse("abbr { white-space: nowrap; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    ASSERT_TRUE(sheet.rules[0].properties.whiteSpace.has_value());
    EXPECT_EQ(sheet.rules[0].properties.whiteSpace.value(), "nowrap");
}

// --- Task 6: :last-child ---

TEST(CSSTest, ParseLastChildSelector) {
    auto sheet = CSSStylesheet::parse("p:last-child { text-indent: 0; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    EXPECT_EQ(sheet.rules[0].selector.type, SelectorType::PseudoFirstChild);
    EXPECT_EQ(sheet.rules[0].selector.element, "p");
    EXPECT_EQ(sheet.rules[0].selector.pseudoClass, "last-child");
}

// --- Task 7: font-variant-numeric ---

TEST(CSSTest, ParseFontVariantNumeric) {
    auto sheet = CSSStylesheet::parse("body { font-variant-numeric: oldstyle-nums; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    ASSERT_TRUE(sheet.rules[0].properties.fontVariantNumeric.has_value());
    EXPECT_TRUE(sheet.rules[0].properties.fontVariantNumeric.value());
}

TEST(CSSTest, ParseFontVariantNumericNormal) {
    auto sheet = CSSStylesheet::parse("body { font-variant-numeric: normal; }");
    ASSERT_EQ(sheet.rules.size(), 1);
    ASSERT_TRUE(sheet.rules[0].properties.fontVariantNumeric.has_value());
    EXPECT_FALSE(sheet.rules[0].properties.fontVariantNumeric.value());
}

// =============================================================================
// MARK: - Phase 3: max-width and margin auto
// =============================================================================

TEST(CSSTest, ParseMaxWidthPercent) {
    auto sheet = CSSStylesheet::parse("section { max-width: 70%; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_TRUE(props.maxWidthPercent.has_value());
    EXPECT_FLOAT_EQ(props.maxWidthPercent.value(), 70.0f);
}

TEST(CSSTest, ParseMarginAutoShorthand) {
    auto sheet = CSSStylesheet::parse("p { margin: 1em auto; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_FLOAT_EQ(props.marginTop.value_or(-1), 1.0f);
    EXPECT_FLOAT_EQ(props.marginBottom.value_or(-1), 1.0f);
    EXPECT_TRUE(props.marginLeftAuto.value_or(false));
    EXPECT_TRUE(props.marginRightAuto.value_or(false));
}

TEST(CSSTest, ParseMarginLeftAuto) {
    auto sheet = CSSStylesheet::parse("p { margin-left: auto; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_TRUE(props.marginLeftAuto.value_or(false));
    EXPECT_FALSE(props.marginLeft.has_value());
}

TEST(CSSTest, ParseMarginRightAuto) {
    auto sheet = CSSStylesheet::parse("p { margin-right: auto; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_TRUE(props.marginRightAuto.value_or(false));
    EXPECT_FALSE(props.marginRight.has_value());
}

TEST(CSSTest, ParseImportantFlag) {
    auto sheet = CSSStylesheet::parse(
        "a { font-style: normal !important; font-size: smaller; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_TRUE(props.fontStyle.has_value());
    EXPECT_EQ(props.fontStyle.value(), FontStyle::Normal);
    EXPECT_NE(props.importantFlags & typesetting::kImpFontStyle, 0u);
    EXPECT_EQ(props.importantFlags & typesetting::kImpFontSize, 0u);
    EXPECT_TRUE(props.fontSize.has_value());
}

TEST(CSSTest, ParseImportantHangingPunctuation) {
    auto sheet = CSSStylesheet::parse(
        "p { hanging-punctuation: none !important; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_TRUE(props.hangingPunctuation.has_value());
    EXPECT_FALSE(props.hangingPunctuation.value());
    EXPECT_NE(props.importantFlags & typesetting::kImpHangingPunct, 0u);
}

TEST(CSSTest, ParseLineHeightNumber) {
    auto sheet = CSSStylesheet::parse("p { line-height: 1.5; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_TRUE(props.lineHeight.has_value());
    EXPECT_FLOAT_EQ(props.lineHeight.value(), 1.5f);
}

TEST(CSSTest, ParseLineHeightEm) {
    auto sheet = CSSStylesheet::parse("p { line-height: 2em; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_TRUE(props.lineHeight.has_value());
    EXPECT_FLOAT_EQ(props.lineHeight.value(), 2.0f);
}

TEST(CSSTest, ParseLineHeightZero) {
    auto sheet = CSSStylesheet::parse("p { line-height: 0; }");
    ASSERT_GE(sheet.rules.size(), 1);
    auto& props = sheet.rules[0].properties;
    EXPECT_TRUE(props.lineHeight.has_value());
    EXPECT_FLOAT_EQ(props.lineHeight.value(), 0.0f);
}

// --- @supports expansion ---

TEST(CSSTest, SupportsExpansionFontSize) {
    // @supports(font-size: 0) should be treated as true, inner rules extracted
    auto sheet = CSSStylesheet::parse(
        "p { color: red; } "
        "@supports(font-size: 0) { span { display: block; } } "
        "div { color: blue; }");
    // Should have 3 rules: p, span (from @supports), div
    ASSERT_GE(sheet.rules.size(), 3);
    EXPECT_EQ(sheet.rules[1].selector.element, "span");
    EXPECT_TRUE(sheet.rules[1].properties.display.has_value());
    EXPECT_EQ(sheet.rules[1].properties.display.value(), "block");
}

TEST(CSSTest, SupportsExpansionDisplayFlex) {
    // @supports(display: flex) should also be expanded
    auto sheet = CSSStylesheet::parse(
        "@supports(display: flex) { "
        "  section { max-width: 70%; } "
        "  section > p { margin-left: auto; } "
        "}");
    ASSERT_GE(sheet.rules.size(), 2);
    EXPECT_TRUE(sheet.rules[0].properties.maxWidthPercent.has_value());
    EXPECT_FLOAT_EQ(sheet.rules[0].properties.maxWidthPercent.value(), 70.0f);
}

TEST(CSSTest, MediaRuleStillSkipped) {
    // @media rules should still be skipped
    auto sheet = CSSStylesheet::parse(
        "p { font-size: 1em; } "
        "@media (prefers-color-scheme: dark) { p { color: white; } } "
        "div { font-size: 2em; }");
    // Should have 2 rules: p, div (media block skipped)
    ASSERT_EQ(sheet.rules.size(), 2);
    EXPECT_EQ(sheet.rules[0].selector.element, "p");
    EXPECT_EQ(sheet.rules[1].selector.element, "div");
}
