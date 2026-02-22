#pragma once

#include "typesetting/platform.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace typesetting {

/// Inline element types within a paragraph
enum class InlineType {
    Text,
    Bold,
    Italic,
    BoldItalic,
    Code,
    Link,
};

/// An inline run of text with uniform styling
struct InlineElement {
    InlineType type = InlineType::Text;
    std::string text;
    std::string href;       // For Link type only
    std::string lang;       // <i lang="lt"> language attribute
    std::string className;  // inline element's class
    std::string epubType;   // <abbr epub:type="z3998:name-title">
    std::string htmlTag;    // original HTML tag name ("a", "abbr", "span", etc.)
    bool isFootnoteRef = false;   // This inline is a footnote reference marker
    std::string footnoteId;        // Target footnote ID

    static InlineElement plain(const std::string& t) {
        return {InlineType::Text, t, {}, {}, {}, {}};
    }
    static InlineElement bold(const std::string& t) {
        return {InlineType::Bold, t, {}, {}, {}, {}};
    }
    static InlineElement italic(const std::string& t) {
        return {InlineType::Italic, t, {}, {}, {}, {}};
    }
    static InlineElement code(const std::string& t) {
        return {InlineType::Code, t, {}, {}, {}, {}};
    }
    static InlineElement link(const std::string& t, const std::string& url) {
        return {InlineType::Link, t, url, {}, {}, {}};
    }
};

/// Block-level element types
enum class BlockType {
    Paragraph,
    Heading1,
    Heading2,
    Heading3,
    Heading4,
    Blockquote,
    CodeBlock,
    Image,
    HorizontalRule,
    ListItem,
    Figcaption,
    Table,
};

/// A single cell in a table
struct TableCell {
    std::vector<InlineElement> inlines;
    int colspan = 1;
    bool isHeader = false;
};

/// A row in a table
struct TableRow {
    std::vector<TableCell> cells;
};

/// A block-level element in the document
struct Block {
    BlockType type = BlockType::Paragraph;
    std::vector<InlineElement> inlines;  // For text blocks
    std::string src;                      // For Image: source URL
    std::string alt;                      // For Image: alt text
    std::string caption;                  // For Image: caption
    int listIndex = -1;                   // For ListItem: ordered list index (-1 = unordered)
    std::vector<TableRow> tableRows;      // For Table type

    // Metadata for CSS selector matching
    std::string className;                // HTML class attribute value
    std::string epubType;                 // epub:type attribute value
    std::string htmlTag;                  // Original HTML tag name (e.g., "p", "h2", "blockquote")
    std::string parentTag;                // Parent element's tag name
    std::string parentClassName;          // Parent element's class
    std::string parentEpubType;           // Parent element's epub:type
    std::string parentId;                 // Parent element's id attribute
    bool isFirstChild = true;             // Is this the first block-child of its parent?
    std::vector<std::string> previousSiblingTags;  // Previous sibling tags [0]=immediate, [1]=before that, etc.
    std::string id;                       // HTML id attribute

    /// Helper: get concatenated plain text from all inlines
    std::string plainText() const {
        std::string result;
        for (const auto& el : inlines) {
            result += el.text;
        }
        return result;
    }
};

/// A chapter in the document
struct Chapter {
    std::string id;
    std::string title;
    int orderIndex = 0;
    std::vector<Block> blocks;
};

/// The full document model
struct Document {
    std::string bookId;
    std::string title;
    std::vector<Chapter> chapters;
};

/// Parse an HTML string into a list of Blocks.
/// This handles the Standard Ebooks HTML format used by Readmigo.
std::vector<Block> parseHTML(const std::string& html);

} // namespace typesetting
