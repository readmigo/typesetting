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
    EXPECT_TRUE(sheet.rules[0].properties.displayNone.value_or(false));
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
