#pragma once

#include "typesetting/page.h"
#include "typesetting/document.h"
#include <string>
#include <vector>

namespace typesetting {

// ---------------------------------------------------------------------------
// Data structures for interaction queries
// ---------------------------------------------------------------------------

/// A rectangle in page coordinates (origin = page top-left)
struct TextRect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
};

/// Result of a hit-test query
struct HitTestResult {
    int blockIndex = -1;     // Block index in chapter
    int lineIndex = -1;      // Line index in page
    int runIndex = -1;       // TextRun index in line
    int charOffset = -1;     // Byte offset in block.plainText()
    bool found = false;
};

/// A word range within a block
struct WordRange {
    int blockIndex = -1;
    int charOffset = 0;      // Byte offset in block.plainText()
    int charLength = 0;      // Byte length
    std::string text;
};

/// A sentence range within a block
struct SentenceRange {
    int blockIndex = -1;
    int charOffset = 0;      // Byte offset in block.plainText()
    int charLength = 0;      // Byte length
    std::string text;
};

/// Result of an image hit-test query
struct ImageHitResult {
    std::string imageSrc;
    std::string imageAlt;
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    bool found = false;
};

/// Page metadata for header/footer rendering
struct PageInfo {
    std::string chapterTitle;
    int currentPage = 0;     // 1-based page number
    int totalPages = 0;
    float progress = 0;      // 0.0 ~ 1.0
    int firstBlockIndex = -1;
    int lastBlockIndex = -1;
};

// ---------------------------------------------------------------------------
// InteractionManager — read-only query layer over cached LayoutResult
// ---------------------------------------------------------------------------

class InteractionManager {
public:
    InteractionManager() = default;

    /// Update the cached layout data for subsequent queries
    void setLayoutResult(const LayoutResult& result,
                         const std::vector<Block>& blocks,
                         const std::string& chapterTitle);

    /// Update only the chapter title (for PageInfo queries)
    void setChapterTitle(const std::string& chapterTitle);

    /// Coordinate → character-level hit test
    HitTestResult hitTest(int pageIndex, float x, float y) const;

    /// Get the word at a given point
    WordRange wordAtPoint(int pageIndex, float x, float y) const;

    /// Get all sentences on a page
    std::vector<SentenceRange> getSentences(int pageIndex) const;

    /// Get all sentences across all pages
    std::vector<SentenceRange> getAllSentences() const;

    /// Character range → visual rectangles
    std::vector<TextRect> getRectsForRange(int pageIndex, int blockIndex,
                                            int charOffset, int charLength) const;

    /// Bounding rect of an entire block on a page
    TextRect getBlockRect(int pageIndex, int blockIndex) const;

    /// Image hit test
    ImageHitResult hitTestImage(int pageIndex, float x, float y) const;

    /// Page metadata (chapter title, progress, etc.)
    PageInfo getPageInfo(int pageIndex) const;

private:
    LayoutResult result_;
    std::vector<Block> blocks_;
    std::string chapterTitle_;

    const Page* getPage(int pageIndex) const;

    /// Convert (inlineIndex, charOffset-within-inline) to block plainText byte offset
    int toBlockOffset(const Block& block, int inlineIndex, int charOffsetInInline) const;

    /// Convert block plainText byte offset to (inlineIndex, charOffset-within-inline)
    void fromBlockOffset(const Block& block, int blockOffset,
                         int& outInlineIndex, int& outCharOffset) const;

    /// Split a block's text into sentences
    std::vector<SentenceRange> splitSentences(int blockIndex) const;
};

} // namespace typesetting
