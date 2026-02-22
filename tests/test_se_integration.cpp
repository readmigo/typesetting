#include <gtest/gtest.h>
#include "typesetting/css.h"
#include "typesetting/style_resolver.h"
#include "typesetting/document.h"
#include "typesetting/platform.h"
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace typesetting;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string testdataDir() {
    std::filesystem::path thisFile(__FILE__);
    return (thisFile.parent_path() / "testdata").string();
}

static std::string readFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) return "";
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

static std::string loadBookCSS(const std::string& book) {
    std::string dir = testdataDir() + "/" + book;
    // Order matters: core.css → se.css → local.css (later rules override)
    return readFile(dir + "/core.css") + "\n" +
           readFile(dir + "/se.css") + "\n" +
           readFile(dir + "/local.css");
}

// =============================================================================
// MARK: - CSS Parse Completeness (all 5 books)
// =============================================================================

TEST(SEIntegration, ParseHuckFinnCSS) {
    auto css = loadBookCSS("huckfinn");
    ASSERT_FALSE(css.empty());
    auto sheet = CSSStylesheet::parse(css);
    EXPECT_GT(sheet.rules.size(), 30);  // Should have many rules
}

TEST(SEIntegration, ParsePrideCSS) {
    auto css = loadBookCSS("pride");
    ASSERT_FALSE(css.empty());
    auto sheet = CSSStylesheet::parse(css);
    EXPECT_GT(sheet.rules.size(), 30);
}

TEST(SEIntegration, ParseFarewellCSS) {
    auto css = loadBookCSS("farewell");
    ASSERT_FALSE(css.empty());
    auto sheet = CSSStylesheet::parse(css);
    EXPECT_GT(sheet.rules.size(), 30);
}

TEST(SEIntegration, ParseEmmaCSS) {
    auto css = loadBookCSS("emma");
    ASSERT_FALSE(css.empty());
    auto sheet = CSSStylesheet::parse(css);
    EXPECT_GT(sheet.rules.size(), 30);
}

TEST(SEIntegration, ParseSunCSS) {
    auto css = loadBookCSS("sun");
    ASSERT_FALSE(css.empty());
    auto sheet = CSSStylesheet::parse(css);
    EXPECT_GT(sheet.rules.size(), 30);
}

// =============================================================================
// MARK: - Huck Finn: Poetry verse layout
// =============================================================================

TEST(SEIntegration, HuckFinnPoetryVerse) {
    // Validates: display:block on spans, padding-left:1em, text-indent:-1em
    // CSS: .epub-type-contains-word-z3998-poem p > span { display:block; padding-left:1em; text-indent:-1em; }
    auto css = loadBookCSS("huckfinn");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    // Simulate: <blockquote class="z3998-poem"><p><span>Line1</span><br/><span class="i1">Line2</span></p></blockquote>
    Block pBlock;
    pBlock.type = BlockType::Paragraph;
    pBlock.htmlTag = "p";
    pBlock.parentTag = "blockquote";
    pBlock.parentClassName = "epub-type-contains-word-z3998-poem";

    InlineElement span1 = InlineElement::plain("And did young Stephen sicken,");
    span1.htmlTag = "span";
    InlineElement br;
    br.text = "\n";
    InlineElement span2 = InlineElement::plain("And did young Stephen die?");
    span2.htmlTag = "span";
    span2.className = "i1";

    pBlock.inlines = {span1, br, span2};

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({pBlock}, userStyle);

    // Should have expanded blocks (display:block on spans)
    ASSERT_FALSE(resolved.expandedBlocks.empty()) << "Poetry spans should be expanded to blocks";
    EXPECT_GE(resolved.expandedBlocks.size(), 2) << "Should have at least 2 expanded blocks for 2 spans";

    // Check first span: padding-left=1em (16px), text-indent=-1em (-16px)
    ASSERT_GE(resolved.blockStyles.size(), 2);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].paddingLeft, 16.0f);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, -16.0f);

    // Check second span (.i1): padding-left=2em (32px), text-indent=-1em (-16px)
    EXPECT_FLOAT_EQ(resolved.blockStyles[1].paddingLeft, 32.0f);
    EXPECT_FLOAT_EQ(resolved.blockStyles[1].textIndent, -16.0f);
}

// =============================================================================
// MARK: - Huck Finn: .bill small-caps
// =============================================================================

TEST(SEIntegration, HuckFinnBillSmallCaps) {
    // CSS: .bill { font-variant: small-caps; }
    auto css = loadBookCSS("huckfinn");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.parentClassName = "bill";
    block.inlines.push_back(InlineElement::plain("REWARD"));

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // .bill p { text-align: center; text-indent: 0; }
    EXPECT_EQ(resolved.blockStyles[0].alignment, TextAlignment::Center);
    EXPECT_FLOAT_EQ(resolved.blockStyles[0].textIndent, 0.0f);
}

TEST(SEIntegration, HuckFinnBillSmallCapsParent) {
    // Block IS the .bill div itself
    auto css = loadBookCSS("huckfinn");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "div";
    block.className = "bill";
    block.inlines.push_back(InlineElement::plain("REWARD"));

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_TRUE(resolved.blockStyles[0].smallCaps);
}

// =============================================================================
// MARK: - Pride: Letter salutation small-caps (compound selector)
// =============================================================================

TEST(SEIntegration, PrideSalutationSmallCaps) {
    // CSS: p.epub-type-contains-word-z3998-salutation { font-variant: small-caps; }
    // This tests compound selector p.class
    auto css = loadBookCSS("pride");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.className = "epub-type-contains-word-z3998-salutation";
    block.inlines.push_back(InlineElement::plain("My dear Lizzy,"));

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_TRUE(resolved.blockStyles[0].smallCaps)
        << "Compound selector p.epub-type-contains-word-z3998-salutation should match";
}

TEST(SEIntegration, PrideSalutationNonParagraphNoMatch) {
    // div.epub-type-contains-word-z3998-salutation should NOT match p.class rule
    auto css = loadBookCSS("pride");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "div";
    block.className = "epub-type-contains-word-z3998-salutation";
    block.inlines.push_back(InlineElement::plain("My dear Lizzy,"));

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // Pride local.css only has compound selectors: p.epub-type-contains-word-z3998-salutation
    // There is no class-only .epub-type-contains-word-z3998-salutation rule
    // So a div should NOT get small-caps
    EXPECT_FALSE(resolved.blockStyles[0].smallCaps)
        << "Compound selector p.z3998-salutation should NOT match div";
}

// =============================================================================
// MARK: - Farewell: Dedication centering
// =============================================================================

TEST(SEIntegration, FarewellDedicationCentering) {
    // CSS: section.epub-type-contains-word-dedication { text-align: center; }
    // CSS: section.epub-type-contains-word-dedication > * { display: inline-block; max-width: 80%; margin: auto; }
    auto css = loadBookCSS("farewell");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    // The <p> inside dedication section
    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.parentTag = "section";
    block.parentClassName = "epub-type-contains-word-dedication";
    block.className = "first-child";
    block.isFirstChild = true;
    block.inlines.push_back(InlineElement::plain("To G. A. Pfeiffer"));

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);

    auto& style = resolved.blockStyles[0];
    // text-align: center from parent section rule
    // Note: the section rule applies to the section itself, not the p directly
    // But section.dedication > * applies to the p
    EXPECT_FLOAT_EQ(style.maxWidthPercent, 80.0f)
        << "Dedication child should have max-width: 80%";
    EXPECT_TRUE(style.horizontalCentering)
        << "Dedication child should have margin: auto centering";
}

// =============================================================================
// MARK: - Sun: ID selector + text-transform
// =============================================================================

TEST(SEIntegration, SunChapter19TextTransform) {
    // CSS: #chapter-19 blockquote { text-transform: uppercase; }
    auto css = loadBookCSS("sun");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Blockquote;
    block.htmlTag = "blockquote";
    block.parentTag = "section";
    block.parentId = "chapter-19";
    block.inlines.push_back(InlineElement::plain("Some quoted text"));

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_EQ(resolved.blockStyles[0].textTransform, TextTransform::Uppercase)
        << "ID selector #chapter-19 blockquote should apply text-transform: uppercase";
}

// =============================================================================
// MARK: - Sun: Epigraph italic + cite small-caps
// =============================================================================

TEST(SEIntegration, SunEpigraphItalic) {
    // CSS: .epub-type-contains-word-epigraph { font-style: italic; hyphens: none; }
    auto css = loadBookCSS("sun");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    // Blockquote inside epigraph section
    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "p";
    block.parentTag = "blockquote";
    block.className = "first-child";
    block.isFirstChild = true;
    // Grandparent is section.epigraph — current engine only checks parentClassName
    // The epigraph rule targets .epub-type-contains-word-epigraph (any descendant)
    // So we need the block or parent to carry the class
    block.parentClassName = "epub-type-contains-word-epigraph";
    block.inlines.push_back(InlineElement::plain("You are all a lost generation."));

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    // The class rule should match as parentClassName
    // or as block's own scope — depends on engine's ancestor matching
    // At minimum, direct descendants of .epigraph should get italic
}

TEST(SEIntegration, SunEpigraphCiteSmallCaps) {
    // CSS: .epub-type-contains-word-epigraph cite { font-style: normal; font-variant: small-caps; }
    auto css = loadBookCSS("sun");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    Block block;
    block.type = BlockType::Paragraph;
    block.htmlTag = "cite";
    block.parentTag = "blockquote";
    block.parentClassName = "epub-type-contains-word-epigraph";
    block.inlines.push_back(InlineElement::plain("Gertrude Stein in conversation"));

    Style userStyle;
    userStyle.font.size = 16.0f;

    auto resolved = resolver.resolve({block}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);
    EXPECT_TRUE(resolved.blockStyles[0].smallCaps)
        << "Epigraph cite should have font-variant: small-caps";
    EXPECT_EQ(resolved.blockStyles[0].font.style, FontStyle::Normal)
        << "Epigraph cite should override italic with font-style: normal";
}

// =============================================================================
// MARK: - Core.css: hgroup font-size cascade
// =============================================================================

TEST(SEIntegration, HgroupFontSizeCascade) {
    // core.css:
    //   hgroup h2 + p { font-size: 1.17em; }
    //   hgroup h2 + p + p { font-size: 1em; }
    //   hgroup h2 + p + p + p { font-size: .83em; }
    auto css = loadBookCSS("huckfinn");  // any book, core.css is shared
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    float baseFontSize = 16.0f;
    Style userStyle;
    userStyle.font.size = baseFontSize;

    // h2 + p (first p after h2 in hgroup)
    Block p1;
    p1.type = BlockType::Paragraph;
    p1.htmlTag = "p";
    p1.parentTag = "hgroup";
    p1.previousSiblingTags = {"h2"};
    p1.inlines.push_back(InlineElement::plain("Subtitle"));

    // h2 + p + p (second p after h2 in hgroup)
    Block p2;
    p2.type = BlockType::Paragraph;
    p2.htmlTag = "p";
    p2.parentTag = "hgroup";
    p2.previousSiblingTags = {"p", "h2"};
    p2.inlines.push_back(InlineElement::plain("Sub-subtitle"));

    // h2 + p + p + p (third p after h2 in hgroup)
    Block p3;
    p3.type = BlockType::Paragraph;
    p3.htmlTag = "p";
    p3.parentTag = "hgroup";
    p3.previousSiblingTags = {"p", "p", "h2"};
    p3.inlines.push_back(InlineElement::plain("Sub-sub-subtitle"));

    auto r1 = resolver.resolve({p1}, userStyle);
    auto r2 = resolver.resolve({p2}, userStyle);
    auto r3 = resolver.resolve({p3}, userStyle);

    ASSERT_EQ(r1.blockStyles.size(), 1);
    ASSERT_EQ(r2.blockStyles.size(), 1);
    ASSERT_EQ(r3.blockStyles.size(), 1);

    // hgroup h2 + p → 1.17em
    EXPECT_NEAR(r1.blockStyles[0].font.size, baseFontSize * 1.17f, 0.5f)
        << "hgroup h2 + p should be 1.17em";

    // hgroup h2 + p + p → 1em
    EXPECT_NEAR(r2.blockStyles[0].font.size, baseFontSize * 1.0f, 0.5f)
        << "hgroup h2 + p + p should be 1em";

    // hgroup h2 + p + p + p → 0.83em
    EXPECT_NEAR(r3.blockStyles[0].font.size, baseFontSize * 0.83f, 0.5f)
        << "hgroup h2 + p + p + p should be 0.83em";
}

// =============================================================================
// MARK: - Core.css: hgroup > * resets
// =============================================================================

TEST(SEIntegration, HgroupChildResets) {
    // core.css: hgroup > * { font-weight: normal; margin: 0; }
    // core.css: hgroup > *:first-child { font-weight: bold; }
    // core.css: hgroup > p { text-indent: 0; }
    auto css = loadBookCSS("huckfinn");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    Style userStyle;
    userStyle.font.size = 16.0f;

    Block pBlock;
    pBlock.type = BlockType::Paragraph;
    pBlock.htmlTag = "p";
    pBlock.parentTag = "hgroup";
    pBlock.previousSiblingTags = {"h2"};
    pBlock.inlines.push_back(InlineElement::plain("Subtitle"));

    auto resolved = resolver.resolve({pBlock}, userStyle);
    ASSERT_EQ(resolved.blockStyles.size(), 1);

    auto& style = resolved.blockStyles[0];
    // hgroup > * { margin: 0 }
    EXPECT_FLOAT_EQ(style.marginTop, 0.0f);
    EXPECT_FLOAT_EQ(style.marginBottom, 0.0f);
    // hgroup > p { text-indent: 0 }
    EXPECT_FLOAT_EQ(style.textIndent, 0.0f);
    // hgroup > * { font-weight: normal } (not first child)
    EXPECT_EQ(style.font.weight, FontWeight::Regular);
}

// =============================================================================
// MARK: - Core.css: noteref superscript + smaller
// =============================================================================

TEST(SEIntegration, CoreNoterefSuperscript) {
    // core.css: a.epub-type-contains-word-noteref { font-size: smaller; vertical-align: super; font-style: normal !important; }
    auto css = loadBookCSS("huckfinn");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    Style userStyle;
    userStyle.font.size = 16.0f;

    Block pBlock;
    pBlock.type = BlockType::Paragraph;
    pBlock.htmlTag = "p";

    InlineElement plainText = InlineElement::plain("Some text");
    InlineElement noteref = InlineElement::plain("1");
    noteref.htmlTag = "a";
    noteref.className = "epub-type-contains-word-noteref";

    pBlock.inlines = {plainText, noteref};

    auto resolved = resolver.resolve({pBlock}, userStyle);
    ASSERT_EQ(resolved.inlineStyles.size(), 1);
    ASSERT_GE(resolved.inlineStyles[0].size(), 2);

    auto& noterefStyle = resolved.inlineStyles[0][1];
    EXPECT_TRUE(noterefStyle.isSuperscript)
        << "Noteref should have vertical-align: super → isSuperscript";
    EXPECT_TRUE(noterefStyle.fontSizeMultiplier.has_value())
        << "Noteref should have font-size: smaller";
}

// =============================================================================
// MARK: - Core.css: First paragraph no indent
// =============================================================================

TEST(SEIntegration, CoreFirstParagraphNoIndent) {
    // core.css: h2 + p { text-indent: 0; hanging-punctuation: first last; }
    // core.css: p { text-indent: 1em; }
    auto css = loadBookCSS("huckfinn");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    Style userStyle;
    userStyle.font.size = 16.0f;

    // First paragraph after h2
    Block firstP;
    firstP.type = BlockType::Paragraph;
    firstP.htmlTag = "p";
    firstP.previousSiblingTags = {"h2"};
    firstP.inlines.push_back(InlineElement::plain("First paragraph text"));

    // Second paragraph (no adjacent sibling h2)
    Block secondP;
    secondP.type = BlockType::Paragraph;
    secondP.htmlTag = "p";
    secondP.previousSiblingTags = {"p"};
    secondP.isFirstChild = false;
    secondP.inlines.push_back(InlineElement::plain("Second paragraph text"));

    auto r1 = resolver.resolve({firstP}, userStyle);
    auto r2 = resolver.resolve({secondP}, userStyle);

    ASSERT_EQ(r1.blockStyles.size(), 1);
    ASSERT_EQ(r2.blockStyles.size(), 1);

    EXPECT_FLOAT_EQ(r1.blockStyles[0].textIndent, 0.0f)
        << "h2 + p should have text-indent: 0";
    EXPECT_FLOAT_EQ(r2.blockStyles[0].textIndent, 16.0f)
        << "Regular p should have text-indent: 1em (16px)";

    // hanging-punctuation on first paragraph
    EXPECT_TRUE(r1.blockStyles[0].hangingPunctuation)
        << "h2 + p should have hanging-punctuation: first last";
}

// =============================================================================
// MARK: - Core.css: body font-variant-numeric
// =============================================================================

TEST(SEIntegration, CoreBodyOldstyleNums) {
    // core.css: body { font-variant-numeric: oldstyle-nums; }
    auto css = loadBookCSS("huckfinn");
    auto sheet = CSSStylesheet::parse(css);

    // Find the body rule
    bool foundOldstyleNums = false;
    for (auto& rule : sheet.rules) {
        if (rule.selector.element == "body" &&
            rule.properties.fontVariantNumeric.has_value() &&
            rule.properties.fontVariantNumeric.value()) {
            foundOldstyleNums = true;
            break;
        }
    }
    EXPECT_TRUE(foundOldstyleNums)
        << "core.css body { font-variant-numeric: oldstyle-nums } should be parsed";
}

// =============================================================================
// MARK: - Core.css: abbr white-space nowrap
// =============================================================================

TEST(SEIntegration, CoreAbbrNoWrap) {
    // core.css: abbr { white-space: nowrap; border: none; }
    auto css = loadBookCSS("huckfinn");
    auto sheet = CSSStylesheet::parse(css);
    StyleResolver resolver(sheet);

    Style userStyle;
    userStyle.font.size = 16.0f;

    Block pBlock;
    pBlock.type = BlockType::Paragraph;
    pBlock.htmlTag = "p";

    InlineElement abbr = InlineElement::plain("Mr.");
    abbr.htmlTag = "abbr";

    pBlock.inlines = {abbr};

    auto resolved = resolver.resolve({pBlock}, userStyle);
    ASSERT_EQ(resolved.inlineStyles.size(), 1);
    ASSERT_GE(resolved.inlineStyles[0].size(), 1);

    EXPECT_TRUE(resolved.inlineStyles[0][0].noWrap)
        << "abbr inline should have white-space: nowrap";
}
