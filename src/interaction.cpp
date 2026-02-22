#include "typesetting/interaction.h"
#include "typesetting/log.h"
#include <algorithm>
#include <cstdint>

namespace typesetting {
namespace {

// ---------------------------------------------------------------------------
// UTF-8 helpers
// ---------------------------------------------------------------------------

int utf8CharLen(const char* ptr) {
    unsigned char c = static_cast<unsigned char>(*ptr);
    if (c < 0x80) { return 1; }
    if ((c & 0xE0) == 0xC0) { return 2; }
    if ((c & 0xF0) == 0xE0) { return 3; }
    if ((c & 0xF8) == 0xF0) { return 4; }
    return 1;
}

uint32_t utf8Decode(const char* ptr, int len) {
    if (len == 1) { return static_cast<unsigned char>(ptr[0]); }
    if (len == 2) { return ((ptr[0] & 0x1F) << 6) | (ptr[1] & 0x3F); }
    if (len == 3) { return ((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F); }
    if (len == 4) {
        return ((ptr[0] & 0x07) << 18) | ((ptr[1] & 0x3F) << 12) |
               ((ptr[2] & 0x3F) << 6) | (ptr[3] & 0x3F);
    }
    return 0;
}

bool isCJK(uint32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF)
        || (cp >= 0x3400 && cp <= 0x4DBF)
        || (cp >= 0x20000 && cp <= 0x2A6DF)
        || (cp >= 0xF900 && cp <= 0xFAFF)
        || (cp >= 0x3000 && cp <= 0x303F)
        || (cp >= 0xFF00 && cp <= 0xFFEF);
}

bool isWordSeparator(uint32_t cp) {
    if (cp <= 0x7F) {
        return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r'
            || cp == '.' || cp == ',' || cp == '!' || cp == '?'
            || cp == ';' || cp == ':' || cp == '"' || cp == '\''
            || cp == '(' || cp == ')' || cp == '[' || cp == ']'
            || cp == '{' || cp == '}' || cp == '-' || cp == '/';
    }
    if (isCJK(cp)) { return true; }
    if (cp == 0x3001 || cp == 0x3002 || cp == 0xFF0C || cp == 0xFF01
        || cp == 0xFF1F || cp == 0xFF1A || cp == 0xFF1B) {
        return true;
    }
    return false;
}

bool isSentenceEnd(uint32_t cp) {
    return cp == '.' || cp == '!' || cp == '?'
        || cp == 0x3002 || cp == 0xFF01 || cp == 0xFF1F;
}

int utf8PrevCharStart(const std::string& s, int pos) {
    if (pos <= 0) { return 0; }
    pos--;
    while (pos > 0 && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) {
        pos--;
    }
    return pos;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// InteractionManager
// ---------------------------------------------------------------------------

void InteractionManager::setLayoutResult(const LayoutResult& result,
                                          const std::vector<Block>& blocks,
                                          const std::string& chapterTitle) {
    result_ = result;
    blocks_ = blocks;
    chapterTitle_ = chapterTitle;

    TS_LOGD("InteractionManager: cached %zu pages, %zu blocks, title='%s'",
            result_.pages.size(), blocks_.size(), chapterTitle_.c_str());
}

void InteractionManager::setChapterTitle(const std::string& chapterTitle) {
    chapterTitle_ = chapterTitle;
}

const Page* InteractionManager::getPage(int pageIndex) const {
    if (pageIndex < 0 || pageIndex >= static_cast<int>(result_.pages.size())) {
        return nullptr;
    }
    return &result_.pages[pageIndex];
}

int InteractionManager::toBlockOffset(const Block& block, int inlineIndex,
                                       int charOffsetInInline) const {
    int offset = 0;
    for (int i = 0; i < inlineIndex && i < static_cast<int>(block.inlines.size()); ++i) {
        offset += static_cast<int>(block.inlines[i].text.size());
    }
    return offset + charOffsetInInline;
}

void InteractionManager::fromBlockOffset(const Block& block, int blockOffset,
                                          int& outInlineIndex, int& outCharOffset) const {
    int remaining = blockOffset;
    for (int i = 0; i < static_cast<int>(block.inlines.size()); ++i) {
        int len = static_cast<int>(block.inlines[i].text.size());
        if (remaining < len) {
            outInlineIndex = i;
            outCharOffset = remaining;
            return;
        }
        remaining -= len;
    }
    if (!block.inlines.empty()) {
        outInlineIndex = static_cast<int>(block.inlines.size()) - 1;
        outCharOffset = static_cast<int>(block.inlines.back().text.size());
    } else {
        outInlineIndex = 0;
        outCharOffset = 0;
    }
}

// ---------------------------------------------------------------------------
// hitTest — coordinate → character-level mapping
// ---------------------------------------------------------------------------

HitTestResult InteractionManager::hitTest(int pageIndex, float x, float y) const {
    const Page* page = getPage(pageIndex);
    if (page == nullptr) { return {}; }

    for (int lineIdx = 0; lineIdx < static_cast<int>(page->lines.size()); ++lineIdx) {
        const auto& line = page->lines[lineIdx];
        float lineTop = line.y - line.ascent;
        float lineBottom = lineTop + line.height;

        if (y < lineTop || y > lineBottom) { continue; }

        // Check each run on this line
        for (int runIdx = 0; runIdx < static_cast<int>(line.runs.size()); ++runIdx) {
            const auto& run = line.runs[runIdx];
            if (x < run.x || x > run.x + run.width) { continue; }
            if (run.blockIndex < 0 || run.blockIndex >= static_cast<int>(blocks_.size())) { continue; }

            float fraction = (run.width > 0) ? (x - run.x) / run.width : 0;
            int localOffset = static_cast<int>(fraction * static_cast<float>(run.charLength));
            localOffset = std::max(0, std::min(localOffset, run.charLength - 1));

            int blockOffset = toBlockOffset(
                blocks_[run.blockIndex], run.inlineIndex, run.charOffset + localOffset);

            return HitTestResult{run.blockIndex, lineIdx, runIdx, blockOffset, true};
        }

        // On the line but between runs — snap to nearest run
        if (!line.runs.empty()) {
            int closestIdx = 0;
            float closestDist = std::abs(x - line.runs[0].x);
            for (int r = 1; r < static_cast<int>(line.runs.size()); ++r) {
                float dLeft = std::abs(x - line.runs[r].x);
                float dRight = std::abs(x - (line.runs[r].x + line.runs[r].width));
                float d = std::min(dLeft, dRight);
                if (d < closestDist) {
                    closestDist = d;
                    closestIdx = r;
                }
            }
            const auto& run = line.runs[closestIdx];
            if (run.blockIndex >= 0 && run.blockIndex < static_cast<int>(blocks_.size())) {
                int charPos = (x < run.x) ? run.charOffset
                                          : run.charOffset + run.charLength - 1;
                int blockOffset = toBlockOffset(blocks_[run.blockIndex], run.inlineIndex, charPos);
                return HitTestResult{run.blockIndex, lineIdx, closestIdx, blockOffset, true};
            }
        }
    }

    return {};
}

// ---------------------------------------------------------------------------
// wordAtPoint — expand hit to word boundaries
// ---------------------------------------------------------------------------

WordRange InteractionManager::wordAtPoint(int pageIndex, float x, float y) const {
    auto hit = hitTest(pageIndex, x, y);
    if (!hit.found) { return {}; }
    if (hit.blockIndex < 0 || hit.blockIndex >= static_cast<int>(blocks_.size())) { return {}; }

    std::string text = blocks_[hit.blockIndex].plainText();
    if (text.empty()) { return {}; }

    int pos = std::max(0, std::min(hit.charOffset, static_cast<int>(text.size()) - 1));
    int charLen = utf8CharLen(text.c_str() + pos);
    uint32_t cp = utf8Decode(text.c_str() + pos, charLen);

    // CJK: single character is the word
    if (isCJK(cp)) {
        WordRange wr;
        wr.blockIndex = hit.blockIndex;
        wr.charOffset = pos;
        wr.charLength = charLen;
        wr.text = text.substr(pos, charLen);
        return wr;
    }

    // Non-CJK: expand to word boundaries
    int start = pos;
    int end = pos + charLen;

    while (start > 0) {
        int prev = utf8PrevCharStart(text, start);
        int pLen = utf8CharLen(text.c_str() + prev);
        uint32_t pCp = utf8Decode(text.c_str() + prev, pLen);
        if (isWordSeparator(pCp)) { break; }
        start = prev;
    }

    while (end < static_cast<int>(text.size())) {
        int cLen = utf8CharLen(text.c_str() + end);
        uint32_t cCp = utf8Decode(text.c_str() + end, cLen);
        if (isWordSeparator(cCp)) { break; }
        end += cLen;
    }

    WordRange wr;
    wr.blockIndex = hit.blockIndex;
    wr.charOffset = start;
    wr.charLength = end - start;
    wr.text = text.substr(start, end - start);
    return wr;
}

// ---------------------------------------------------------------------------
// splitSentences — heuristic sentence boundary detection
// ---------------------------------------------------------------------------

std::vector<SentenceRange> InteractionManager::splitSentences(int blockIndex) const {
    if (blockIndex < 0 || blockIndex >= static_cast<int>(blocks_.size())) { return {}; }

    std::string text = blocks_[blockIndex].plainText();
    if (text.empty()) { return {}; }

    std::vector<SentenceRange> sentences;
    int sentStart = 0;
    int i = 0;
    int textLen = static_cast<int>(text.size());

    while (i < textLen) {
        int charLen = utf8CharLen(text.c_str() + i);
        uint32_t cp = utf8Decode(text.c_str() + i, charLen);

        if (isSentenceEnd(cp)) {
            int afterPunct = i + charLen;

            // CJK sentence ends: break immediately
            if (cp >= 0x3000) {
                int nextStart = afterPunct;
                while (nextStart < textLen &&
                       (text[nextStart] == ' ' || text[nextStart] == '\n')) {
                    nextStart++;
                }
                SentenceRange sr;
                sr.blockIndex = blockIndex;
                sr.charOffset = sentStart;
                sr.charLength = afterPunct - sentStart;
                sr.text = text.substr(sentStart, sr.charLength);
                sentences.push_back(sr);
                sentStart = nextStart;
                i = nextStart;
                continue;
            }

            // English: end of text
            if (afterPunct >= textLen) {
                SentenceRange sr;
                sr.blockIndex = blockIndex;
                sr.charOffset = sentStart;
                sr.charLength = afterPunct - sentStart;
                sr.text = text.substr(sentStart, sr.charLength);
                sentences.push_back(sr);
                sentStart = afterPunct;
                i = afterPunct;
                continue;
            }

            // English: .!? followed by whitespace then uppercase
            if (text[afterPunct] == ' ' || text[afterPunct] == '\n') {
                int nextCharPos = afterPunct;
                while (nextCharPos < textLen &&
                       (text[nextCharPos] == ' ' || text[nextCharPos] == '\n')) {
                    nextCharPos++;
                }
                if (nextCharPos >= textLen) {
                    SentenceRange sr;
                    sr.blockIndex = blockIndex;
                    sr.charOffset = sentStart;
                    sr.charLength = afterPunct - sentStart;
                    sr.text = text.substr(sentStart, sr.charLength);
                    sentences.push_back(sr);
                    sentStart = textLen;
                    i = textLen;
                    continue;
                }
                int ncLen = utf8CharLen(text.c_str() + nextCharPos);
                uint32_t ncCp = utf8Decode(text.c_str() + nextCharPos, ncLen);
                if (ncCp >= 'A' && ncCp <= 'Z') {
                    SentenceRange sr;
                    sr.blockIndex = blockIndex;
                    sr.charOffset = sentStart;
                    sr.charLength = afterPunct - sentStart;
                    sr.text = text.substr(sentStart, sr.charLength);
                    sentences.push_back(sr);
                    sentStart = nextCharPos;
                    i = nextCharPos;
                    continue;
                }
            }
        }

        i += charLen;
    }

    // Remaining text
    if (sentStart < textLen) {
        SentenceRange sr;
        sr.blockIndex = blockIndex;
        sr.charOffset = sentStart;
        sr.charLength = textLen - sentStart;
        sr.text = text.substr(sentStart, sr.charLength);
        sentences.push_back(sr);
    }

    return sentences;
}

// ---------------------------------------------------------------------------
// getSentences / getAllSentences
// ---------------------------------------------------------------------------

std::vector<SentenceRange> InteractionManager::getSentences(int pageIndex) const {
    const Page* page = getPage(pageIndex);
    if (page == nullptr) { return {}; }

    // Collect unique block indices visible on this page
    std::vector<int> blockIndices;
    for (const auto& line : page->lines) {
        for (const auto& run : line.runs) {
            if (run.blockIndex < 0) continue;
            bool exists = false;
            for (int bi : blockIndices) {
                if (bi == run.blockIndex) { exists = true; break; }
            }
            if (!exists) blockIndices.push_back(run.blockIndex);
        }
    }

    std::vector<SentenceRange> all;
    for (int bi : blockIndices) {
        auto sents = splitSentences(bi);
        all.insert(all.end(), sents.begin(), sents.end());
    }
    return all;
}

std::vector<SentenceRange> InteractionManager::getAllSentences() const {
    std::vector<SentenceRange> all;
    for (int i = 0; i < static_cast<int>(blocks_.size()); ++i) {
        if (blocks_[i].inlines.empty()) continue;
        auto sents = splitSentences(i);
        all.insert(all.end(), sents.begin(), sents.end());
    }
    return all;
}

// ---------------------------------------------------------------------------
// getRectsForRange — character range → visual rectangles
// ---------------------------------------------------------------------------

std::vector<TextRect> InteractionManager::getRectsForRange(int pageIndex, int blockIndex,
                                                            int charOffset, int charLength) const {
    const Page* page = getPage(pageIndex);
    if (page == nullptr) { return {}; }
    if (blockIndex < 0 || blockIndex >= static_cast<int>(blocks_.size())) return {};

    int rangeStart = charOffset;
    int rangeEnd = charOffset + charLength;
    std::vector<TextRect> rects;

    for (const auto& line : page->lines) {
        for (const auto& run : line.runs) {
            if (run.blockIndex != blockIndex) continue;

            int runBlockStart = toBlockOffset(blocks_[blockIndex], run.inlineIndex, run.charOffset);
            int runBlockEnd = runBlockStart + run.charLength;

            int overlapStart = std::max(rangeStart, runBlockStart);
            int overlapEnd = std::min(rangeEnd, runBlockEnd);
            if (overlapStart >= overlapEnd) continue;

            float startFrac = (run.charLength > 0)
                ? static_cast<float>(overlapStart - runBlockStart) / run.charLength : 0;
            float endFrac = (run.charLength > 0)
                ? static_cast<float>(overlapEnd - runBlockStart) / run.charLength : 1;

            TextRect rect;
            rect.x = run.x + startFrac * run.width;
            rect.y = line.y - line.ascent;
            rect.width = (endFrac - startFrac) * run.width;
            rect.height = line.height;
            rects.push_back(rect);
        }
    }

    return rects;
}

// ---------------------------------------------------------------------------
// getBlockRect — bounding box of a block on a page
// ---------------------------------------------------------------------------

TextRect InteractionManager::getBlockRect(int pageIndex, int blockIndex) const {
    const Page* page = getPage(pageIndex);
    if (page == nullptr) { return {}; }

    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    bool found = false;

    for (const auto& line : page->lines) {
        for (const auto& run : line.runs) {
            if (run.blockIndex != blockIndex) continue;
            found = true;
            float top = line.y - line.ascent;
            minX = std::min(minX, run.x);
            minY = std::min(minY, top);
            maxX = std::max(maxX, run.x + run.width);
            maxY = std::max(maxY, top + line.height);
        }
    }

    if (!found) return {};

    TextRect rect;
    rect.x = minX;
    rect.y = minY;
    rect.width = maxX - minX;
    rect.height = maxY - minY;
    return rect;
}

// ---------------------------------------------------------------------------
// hitTestImage — check image decorations
// ---------------------------------------------------------------------------

ImageHitResult InteractionManager::hitTestImage(int pageIndex, float x, float y) const {
    const Page* page = getPage(pageIndex);
    if (page == nullptr) { return {}; }

    for (const auto& deco : page->decorations) {
        if (deco.type != DecorationType::ImagePlaceholder) continue;
        if (x >= deco.x && x <= deco.x + deco.width &&
            y >= deco.y && y <= deco.y + deco.height) {
            ImageHitResult result;
            result.imageSrc = deco.imageSrc;
            result.imageAlt = deco.imageAlt;
            result.x = deco.x;
            result.y = deco.y;
            result.width = deco.width;
            result.height = deco.height;
            result.found = true;
            return result;
        }
    }

    return {};
}

// ---------------------------------------------------------------------------
// getPageInfo — page metadata for header/footer rendering
// ---------------------------------------------------------------------------

PageInfo InteractionManager::getPageInfo(int pageIndex) const {
    const Page* page = getPage(pageIndex);
    if (page == nullptr) { return {}; }

    int totalPages = static_cast<int>(result_.pages.size());

    PageInfo info;
    info.chapterTitle = chapterTitle_;
    info.currentPage = pageIndex + 1;
    info.totalPages = totalPages;
    info.progress = (totalPages > 0)
        ? static_cast<float>(pageIndex + 1) / totalPages : 0;
    info.firstBlockIndex = page->firstBlockIndex;
    info.lastBlockIndex = page->lastBlockIndex;
    return info;
}

} // namespace typesetting
