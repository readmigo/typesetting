#include "typesetting/document.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace typesetting {

namespace {

/// Simple HTML tag info
struct Tag {
    std::string name;
    bool isClosing = false;
    bool isSelfClosing = false;
    std::string raw;  // Raw tag content (between < and >) for attribute extraction
    std::string getAttribute(const std::string& rawStr, const std::string& attr) const;
    std::string getAttribute(const std::string& attr) const { return getAttribute(raw, attr); }
};

/// Parent element tracking for CSS selector matching
struct ParentInfo {
    std::string tag;
    std::string className;
    std::string epubType;
    int childBlockCount = 0;  // count of block-level children seen
};

/// Trim whitespace
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

/// Decode basic HTML entities
std::string decodeEntities(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '&') {
            auto semi = text.find(';', i);
            if (semi != std::string::npos && semi - i < 10) {
                auto entity = text.substr(i + 1, semi - i - 1);
                if (entity == "amp") result += '&';
                else if (entity == "lt") result += '<';
                else if (entity == "gt") result += '>';
                else if (entity == "quot") result += '"';
                else if (entity == "apos") result += '\'';
                else if (entity == "nbsp") result += ' ';
                else if (entity == "mdash") result += "\xe2\x80\x94";
                else if (entity == "ndash") result += "\xe2\x80\x93";
                else if (entity == "hellip") result += "\xe2\x80\xa6";
                else if (entity == "lsquo") result += "\xe2\x80\x98";
                else if (entity == "rsquo") result += "\xe2\x80\x99";
                else if (entity == "ldquo") result += "\xe2\x80\x9c";
                else if (entity == "rdquo") result += "\xe2\x80\x9d";
                else {
                    result += text.substr(i, semi - i + 1);
                }
                i = semi;
                continue;
            }
        }
        result += text[i];
    }
    return result;
}

/// Parse the next tag from position, returns end position
size_t parseTag(const std::string& html, size_t pos, Tag& tag) {
    if (pos >= html.size() || html[pos] != '<') return pos;

    auto end = html.find('>', pos);
    if (end == std::string::npos) return pos;

    std::string raw = html.substr(pos + 1, end - pos - 1);
    tag.raw = raw;  // Store raw content for attribute extraction
    tag.isClosing = (!raw.empty() && raw[0] == '/');
    tag.isSelfClosing = (!raw.empty() && raw.back() == '/');

    // Extract tag name
    size_t nameStart = tag.isClosing ? 1 : 0;
    size_t nameEnd = raw.find_first_of(" \t\n\r/", nameStart);
    if (nameEnd == std::string::npos) nameEnd = raw.size();
    tag.name = raw.substr(nameStart, nameEnd - nameStart);

    // Lowercase
    std::transform(tag.name.begin(), tag.name.end(), tag.name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return end + 1;
}

/// Extract attribute value from raw tag content
std::string Tag::getAttribute(const std::string& raw, const std::string& attr) const {
    auto pos = raw.find(attr + "=\"");
    if (pos == std::string::npos) {
        pos = raw.find(attr + "='");
        if (pos == std::string::npos) return "";
    }
    auto valStart = raw.find_first_of("\"'", pos + attr.size() + 1);
    if (valStart == std::string::npos) return "";
    char quote = raw[valStart];
    auto valEnd = raw.find(quote, valStart + 1);
    if (valEnd == std::string::npos) return "";
    return raw.substr(valStart + 1, valEnd - valStart - 1);
}

/// Map tag name to block type
BlockType tagToBlockType(const std::string& tagName) {
    if (tagName == "h1") return BlockType::Heading1;
    if (tagName == "h2") return BlockType::Heading2;
    if (tagName == "h3") return BlockType::Heading3;
    if (tagName == "h4") return BlockType::Heading4;
    if (tagName == "blockquote") return BlockType::Blockquote;
    if (tagName == "pre") return BlockType::CodeBlock;
    if (tagName == "hr") return BlockType::HorizontalRule;
    if (tagName == "li") return BlockType::ListItem;
    if (tagName == "figcaption") return BlockType::Figcaption;
    return BlockType::Paragraph;
}

/// Check if tag is a container-only element (not a block itself)
bool isContainerTag(const std::string& name) {
    return name == "section" || name == "div" || name == "article" ||
           name == "figure" || name == "ul" || name == "ol";
}

/// Check if tag is a block-level element
bool isBlockTag(const std::string& name) {
    return name == "p" || name == "h1" || name == "h2" || name == "h3" ||
           name == "h4" || name == "h5" || name == "h6" ||
           name == "blockquote" || name == "pre" || name == "li" ||
           name == "div" || name == "section" || name == "article" ||
           name == "figure" || name == "figcaption" || name == "hr";
}

/// Check if tag is an inline formatting element
InlineType tagToInlineType(const std::string& name) {
    if (name == "b" || name == "strong") return InlineType::Bold;
    if (name == "i" || name == "em" || name == "cite") return InlineType::Italic;
    if (name == "code") return InlineType::Code;
    if (name == "a") return InlineType::Link;
    return InlineType::Text;
}

} // anonymous namespace

std::vector<Block> parseHTML(const std::string& html) {
    std::vector<Block> blocks;
    size_t pos = 0;
    Block currentBlock;
    currentBlock.type = BlockType::Paragraph;
    bool inBlock = false;
    InlineType currentInline = InlineType::Text;
    std::string currentInlineRaw;  // Raw tag content for current inline element

    // Parent tracking for CSS selector matching
    std::vector<ParentInfo> parentStack;
    // Track the last block's htmlTag at each nesting level for previousSiblingTag
    // Key: parentStack depth, Value: last htmlTag at that depth
    std::vector<std::string> lastSiblingTagAtDepth;

    // Helper: populate block metadata from parent stack and sibling tracking
    auto populateBlockMeta = [&](Block& block, const std::string& tagName, const Tag& tag) {
        block.htmlTag = tagName;
        block.className = tag.getAttribute("class");
        block.epubType = tag.getAttribute("epub:type");

        if (!parentStack.empty()) {
            auto& parent = parentStack.back();
            block.parentTag = parent.tag;
            block.parentClassName = parent.className;
            block.parentEpubType = parent.epubType;
            block.isFirstChild = (parent.childBlockCount == 0);
            parent.childBlockCount++;
        }

        size_t depth = parentStack.size();
        if (depth < lastSiblingTagAtDepth.size()) {
            block.previousSiblingTag = lastSiblingTagAtDepth[depth];
        }
        // Update sibling tracking - ensure vector is large enough
        if (depth >= lastSiblingTagAtDepth.size()) {
            lastSiblingTagAtDepth.resize(depth + 1);
        }
        lastSiblingTagAtDepth[depth] = tagName;
    };

    while (pos < html.size()) {
        if (html[pos] == '<') {
            Tag tag;
            size_t nextPos = parseTag(html, pos, tag);

            // Skip non-visible elements: <style>, <script>, <head>
            // Their content should not be rendered as text
            if ((tag.name == "style" || tag.name == "script" || tag.name == "head") && !tag.isClosing) {
                // Find the matching closing tag and skip everything
                std::string closingTag = "</" + tag.name;
                auto closePos = html.find(closingTag, nextPos);
                if (closePos != std::string::npos) {
                    auto closeEnd = html.find('>', closePos);
                    pos = (closeEnd != std::string::npos) ? closeEnd + 1 : closePos + closingTag.size();
                } else {
                    pos = nextPos;
                }
                continue;
            }

            // Handle <table>: parse into a Table block
            if (tag.name == "table" && !tag.isClosing) {
                if (inBlock && !currentBlock.inlines.empty()) {
                    blocks.push_back(currentBlock);
                    currentBlock = Block{};
                    inBlock = false;
                }

                // Find the closing </table> and extract inner HTML
                std::string closeTag = "</table>";
                auto tableEnd = html.find(closeTag, nextPos);
                if (tableEnd == std::string::npos) {
                    tableEnd = html.size();
                }
                std::string tableHTML = html.substr(nextPos, tableEnd - nextPos);

                // Parse table content into rows and cells
                Block tableBlock;
                tableBlock.type = BlockType::Table;
                populateBlockMeta(tableBlock, "table", tag);

                // Simple table parser: scan for tr/td/th tags
                size_t tpos = 0;
                TableRow currentRow;
                TableCell currentCell;
                bool inRow = false;
                bool inCell = false;
                InlineType cellInline = InlineType::Text;
                std::string cellInlineRaw;

                while (tpos < tableHTML.size()) {
                    if (tableHTML[tpos] == '<') {
                        Tag ttag;
                        size_t tnextPos = parseTag(tableHTML, tpos, ttag);

                        if (ttag.name == "tr" && !ttag.isClosing) {
                            currentRow = TableRow{};
                            inRow = true;
                        } else if (ttag.name == "tr" && ttag.isClosing) {
                            if (inCell) {
                                currentRow.cells.push_back(currentCell);
                                currentCell = TableCell{};
                                inCell = false;
                            }
                            if (inRow) {
                                tableBlock.tableRows.push_back(currentRow);
                                currentRow = TableRow{};
                                inRow = false;
                            }
                        } else if ((ttag.name == "td" || ttag.name == "th") && !ttag.isClosing) {
                            if (inCell) {
                                currentRow.cells.push_back(currentCell);
                            }
                            currentCell = TableCell{};
                            currentCell.isHeader = (ttag.name == "th");
                            std::string colspanStr = ttag.getAttribute("colspan");
                            if (!colspanStr.empty()) {
                                try { currentCell.colspan = std::stoi(colspanStr); }
                                catch (...) { currentCell.colspan = 1; }
                            }
                            inCell = true;
                            cellInline = InlineType::Text;
                            cellInlineRaw.clear();
                        } else if ((ttag.name == "td" || ttag.name == "th") && ttag.isClosing) {
                            if (inCell) {
                                currentRow.cells.push_back(currentCell);
                                currentCell = TableCell{};
                                inCell = false;
                            }
                            cellInline = InlineType::Text;
                            cellInlineRaw.clear();
                        } else if (inCell) {
                            // Inline formatting within cells
                            if (!ttag.isClosing) {
                                cellInline = tagToInlineType(ttag.name);
                                cellInlineRaw = ttag.raw;
                            } else {
                                cellInline = InlineType::Text;
                                cellInlineRaw.clear();
                            }
                        }
                        tpos = tnextPos;
                    } else {
                        // Text content
                        auto nextTag = tableHTML.find('<', tpos);
                        if (nextTag == std::string::npos) nextTag = tableHTML.size();
                        std::string text = tableHTML.substr(tpos, nextTag - tpos);
                        text = decodeEntities(text);
                        std::string trimmed = trim(text);
                        if (!trimmed.empty() && inCell) {
                            InlineElement el;
                            el.type = cellInline;
                            el.text = trimmed;
                            currentCell.inlines.push_back(el);
                        }
                        tpos = nextTag;
                    }
                }
                // Flush any remaining cell/row
                if (inCell) {
                    currentRow.cells.push_back(currentCell);
                }
                if (inRow && !currentRow.cells.empty()) {
                    tableBlock.tableRows.push_back(currentRow);
                }

                blocks.push_back(tableBlock);

                // Skip past </table>
                pos = (tableEnd < html.size()) ? tableEnd + closeTag.size() : html.size();
                continue;
            }

            // Skip structural/metadata tags that don't produce content
            if (tag.name == "html" || tag.name == "body" || tag.name == "meta" ||
                tag.name == "link" || tag.name == "title" || tag.name == "!doctype" ||
                tag.name == "span" || tag.name == "abbr" || tag.name == "sup" ||
                tag.name == "sub" || tag.name == "small" || tag.name == "ruby" ||
                tag.name == "rt" || tag.name == "rp" ||
                tag.name == "thead" || tag.name == "tbody" ||
                tag.name == "header" ||
                tag.name == "footer" || tag.name == "nav" || tag.name == "aside") {
                // For container-like structural tags, treat like containers
                if (!tag.isClosing && (tag.name == "header" || tag.name == "footer" ||
                    tag.name == "nav" || tag.name == "aside")) {
                    if (inBlock && !currentBlock.inlines.empty()) {
                        blocks.push_back(currentBlock);
                        currentBlock = Block{};
                        inBlock = false;
                    }
                }
                pos = nextPos;
                continue;
            }

            // Self-closing / void tags
            if (tag.name == "hr") {
                if (inBlock && !currentBlock.inlines.empty()) {
                    blocks.push_back(currentBlock);
                    currentBlock = Block{};
                    inBlock = false;
                }
                Block hrBlock;
                hrBlock.type = BlockType::HorizontalRule;
                populateBlockMeta(hrBlock, "hr", tag);
                blocks.push_back(hrBlock);
                pos = nextPos;
                continue;
            }

            if (tag.name == "br") {
                if (inBlock) {
                    currentBlock.inlines.push_back(InlineElement::plain("\n"));
                }
                pos = nextPos;
                continue;
            }

            if (tag.name == "img" && !tag.isClosing) {
                if (inBlock && !currentBlock.inlines.empty()) {
                    blocks.push_back(currentBlock);
                    currentBlock = Block{};
                    inBlock = false;
                }
                Block imgBlock;
                imgBlock.type = BlockType::Image;
                imgBlock.src = tag.getAttribute("src");
                imgBlock.alt = tag.getAttribute("alt");
                populateBlockMeta(imgBlock, "img", tag);
                blocks.push_back(imgBlock);
                pos = nextPos;
                continue;
            }

            // Container-only tags (section, div, article, figure, ul, ol)
            if (isContainerTag(tag.name)) {
                if (!tag.isClosing) {
                    // Flush any open block before entering a container
                    if (inBlock && !currentBlock.inlines.empty()) {
                        blocks.push_back(currentBlock);
                        currentBlock = Block{};
                        inBlock = false;
                    }
                    ParentInfo pi;
                    pi.tag = tag.name;
                    pi.className = tag.getAttribute("class");
                    pi.epubType = tag.getAttribute("epub:type");
                    parentStack.push_back(pi);
                    // Reset sibling tracking for this new depth
                    if (parentStack.size() >= lastSiblingTagAtDepth.size()) {
                        lastSiblingTagAtDepth.resize(parentStack.size() + 1);
                    }
                    lastSiblingTagAtDepth[parentStack.size()] = "";
                } else {
                    // Flush any open block before leaving a container
                    if (inBlock && !currentBlock.inlines.empty()) {
                        blocks.push_back(currentBlock);
                        currentBlock = Block{};
                        inBlock = false;
                    }
                    if (!parentStack.empty()) {
                        parentStack.pop_back();
                    }
                }
                pos = nextPos;
                continue;
            }

            // Block-level opening tag (blockquote is both container AND block)
            if (isBlockTag(tag.name) && !tag.isClosing) {
                if (inBlock && !currentBlock.inlines.empty()) {
                    blocks.push_back(currentBlock);
                }
                currentBlock = Block{};
                currentBlock.type = tagToBlockType(tag.name);
                populateBlockMeta(currentBlock, tag.name, tag);
                inBlock = true;

                // blockquote is also a container
                if (tag.name == "blockquote") {
                    ParentInfo pi;
                    pi.tag = tag.name;
                    pi.className = tag.getAttribute("class");
                    pi.epubType = tag.getAttribute("epub:type");
                    parentStack.push_back(pi);
                    if (parentStack.size() >= lastSiblingTagAtDepth.size()) {
                        lastSiblingTagAtDepth.resize(parentStack.size() + 1);
                    }
                    lastSiblingTagAtDepth[parentStack.size()] = "";
                }

                pos = nextPos;
                continue;
            }

            // Block-level closing tag
            if (isBlockTag(tag.name) && tag.isClosing) {
                if (inBlock && !currentBlock.inlines.empty()) {
                    blocks.push_back(currentBlock);
                }
                currentBlock = Block{};
                inBlock = false;
                currentInline = InlineType::Text;
                currentInlineRaw.clear();

                // blockquote is also a container - pop on close
                if (tag.name == "blockquote") {
                    if (!parentStack.empty()) {
                        parentStack.pop_back();
                    }
                }

                pos = nextPos;
                continue;
            }

            // Inline formatting tags
            if (!tag.isClosing) {
                currentInline = tagToInlineType(tag.name);
                currentInlineRaw = tag.raw;
            } else {
                currentInline = InlineType::Text;
                currentInlineRaw.clear();
            }

            pos = nextPos;
        } else {
            // Text content
            auto nextTag = html.find('<', pos);
            if (nextTag == std::string::npos) nextTag = html.size();

            std::string text = html.substr(pos, nextTag - pos);
            text = decodeEntities(text);
            std::string trimmed = trim(text);

            if (!trimmed.empty()) {
                if (!inBlock) {
                    currentBlock = Block{};
                    currentBlock.type = BlockType::Paragraph;
                    inBlock = true;
                }
                InlineElement el;
                el.type = currentInline;
                el.text = trimmed;

                // Extract inline metadata from the raw tag
                if (currentInline != InlineType::Text && !currentInlineRaw.empty()) {
                    Tag dummyTag;
                    dummyTag.raw = currentInlineRaw;
                    el.lang = dummyTag.getAttribute("lang");
                    el.className = dummyTag.getAttribute("class");
                    el.epubType = dummyTag.getAttribute("epub:type");

                    // Detect footnote references: <a epub:type="noteref" href="#...">
                    if (currentInline == InlineType::Link &&
                        el.epubType.find("noteref") != std::string::npos) {
                        el.isFootnoteRef = true;
                        el.footnoteId = dummyTag.getAttribute("href");
                    }
                }

                currentBlock.inlines.push_back(el);
            }

            pos = nextTag;
        }
    }

    // Flush remaining block
    if (inBlock && !currentBlock.inlines.empty()) {
        blocks.push_back(currentBlock);
    }

    return blocks;
}

} // namespace typesetting
