#import "TypesettingBridge.h"

#include "typesetting/engine.h"
#include "typesetting/platform.h"

#import <CoreText/CoreText.h>
#import <UIKit/UIKit.h>

// MARK: - CoreText Platform Adapter

class CoreTextAdapter : public typesetting::PlatformAdapter {
public:
    __weak id<TSImageSizeProvider> imageSizeProvider = nil;

    typesetting::FontMetrics resolveFontMetrics(const typesetting::FontDescriptor& desc) override {
        CTFontRef font = createCTFont(desc);
        typesetting::FontMetrics metrics;
        metrics.ascent = CTFontGetAscent(font);
        metrics.descent = CTFontGetDescent(font);
        metrics.leading = CTFontGetLeading(font);
        metrics.xHeight = CTFontGetXHeight(font);
        metrics.capHeight = CTFontGetCapHeight(font);
        CFRelease(font);
        return metrics;
    }

    typesetting::TextMeasurement measureText(const std::string& text,
                                              const typesetting::FontDescriptor& font) override {
        CTFontRef ctFont = createCTFont(font);
        NSString *nsText = [NSString stringWithUTF8String:text.c_str()];
        if (!nsText) nsText = @"";

        NSDictionary *attrs = @{(__bridge id)kCTFontAttributeName: (__bridge id)ctFont};
        NSAttributedString *attrStr = [[NSAttributedString alloc] initWithString:nsText
                                                                      attributes:attrs];
        CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)attrStr);
        CGFloat ascent, descent, leading;
        double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

        CFRelease(line);
        CFRelease(ctFont);

        return {static_cast<float>(width), static_cast<float>(ascent + descent + leading)};
    }

    size_t findLineBreak(const std::string& text,
                         const typesetting::FontDescriptor& font,
                         float maxWidth) override {
        CTFontRef ctFont = createCTFont(font);
        NSString *nsText = [NSString stringWithUTF8String:text.c_str()];
        if (!nsText || nsText.length == 0) return 0;

        NSDictionary *attrs = @{(__bridge id)kCTFontAttributeName: (__bridge id)ctFont};
        NSAttributedString *attrStr = [[NSAttributedString alloc] initWithString:nsText
                                                                      attributes:attrs];
        CTTypesetterRef typesetter = CTTypesetterCreateWithAttributedString(
            (__bridge CFAttributedStringRef)attrStr);

        CFIndex breakIndex = CTTypesetterSuggestLineBreak(typesetter, 0, maxWidth);

        CFRelease(typesetter);
        CFRelease(ctFont);

        // Convert NSString character index to UTF-8 byte index
        NSRange range = NSMakeRange(0, breakIndex);
        NSString *substring = [nsText substringWithRange:range];
        return std::string([substring UTF8String]).size();
    }

    bool supportsHyphenation(const std::string& locale) override {
        NSString *nsLocale = [NSString stringWithUTF8String:locale.c_str()];
        CFLocaleRef cfLocale = CFLocaleCreate(kCFAllocatorDefault,
                                              (__bridge CFStringRef)nsLocale);
        bool supported = (cfLocale != nullptr);
        if (cfLocale) CFRelease(cfLocale);
        return supported;
    }

    std::vector<size_t> findHyphenationPoints(const std::string& word,
                                               const std::string& locale) override {
        // TODO: Implement using CFStringGetHyphenationLocationBeforeIndex
        return {};
    }

    std::optional<typesetting::ImageSize> getImageSize(const std::string& src) override {
        id<TSImageSizeProvider> provider = imageSizeProvider;
        if (!provider) return std::nullopt;
        NSString *nsSrc = [NSString stringWithUTF8String:src.c_str()];
        NSValue *sizeValue = [provider imageSizeForSource:nsSrc];
        if (!sizeValue) return std::nullopt;
        CGSize size = [sizeValue CGSizeValue];
        return typesetting::ImageSize{static_cast<float>(size.width), static_cast<float>(size.height)};
    }

private:
    CTFontRef createCTFont(const typesetting::FontDescriptor& desc) {
        NSString *family = [NSString stringWithUTF8String:desc.family.c_str()];
        if (!family || family.length == 0) family = @"Georgia";

        UIFontWeight uiWeight;
        switch (desc.weight) {
            case typesetting::FontWeight::Thin: uiWeight = UIFontWeightThin; break;
            case typesetting::FontWeight::Light: uiWeight = UIFontWeightLight; break;
            case typesetting::FontWeight::Regular: uiWeight = UIFontWeightRegular; break;
            case typesetting::FontWeight::Medium: uiWeight = UIFontWeightMedium; break;
            case typesetting::FontWeight::Semibold: uiWeight = UIFontWeightSemibold; break;
            case typesetting::FontWeight::Bold: uiWeight = UIFontWeightBold; break;
            case typesetting::FontWeight::Heavy: uiWeight = UIFontWeightHeavy; break;
            default: uiWeight = UIFontWeightRegular; break;
        }

        UIFontDescriptor *descriptor = [UIFontDescriptor fontDescriptorWithFontAttributes:@{
            UIFontDescriptorFamilyAttribute: family,
        }];

        if (desc.style == typesetting::FontStyle::Italic) {
            descriptor = [descriptor fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitItalic];
        }

        UIFont *uiFont = [UIFont fontWithDescriptor:descriptor size:desc.size];
        if (!uiFont) {
            uiFont = [UIFont systemFontOfSize:desc.size weight:uiWeight];
        }

        return CTFontCreateWithName((__bridge CFStringRef)uiFont.fontName, desc.size, nullptr);
    }
};

// MARK: - ObjC Data Objects

@implementation TSTextRun
@end

@implementation TSLine
@end

@implementation TSDecoration
@end

@implementation TSPage
@end

@implementation TSLayoutResult
@end

@implementation TSHitTestResult
@end

@implementation TSWordRange
@end

@implementation TSSentenceRange
@end

@implementation TSTextRect
@end

@implementation TSImageHitResult
@end

@implementation TSPageInfo
@end

@implementation TSStyle
- (instancetype)init {
    self = [super init];
    if (self) {
        _fontFamily = @"Georgia";
        _fontSize = 18.0;
        _fontWeight = 400;
        _lineSpacingMultiplier = 1.4;
        _letterSpacing = 0;
        _wordSpacing = 0;
        _paragraphSpacing = 12.0;
        _textAlignment = 3; // justified
        _hyphenation = YES;
        _cssString = nil;
        _marginTop = 50.0;
        _marginBottom = 40.0;
        _marginLeft = 20.0;
        _marginRight = 20.0;
    }
    return self;
}
@end

// MARK: - Bridge Implementation

@interface TypesettingBridge () {
    std::unique_ptr<typesetting::Engine> _engine;
    std::shared_ptr<CoreTextAdapter> _adapter;
}
@end

@implementation TypesettingBridge

- (instancetype)init {
    self = [super init];
    if (self) {
        _adapter = std::make_shared<CoreTextAdapter>();
        _engine = std::make_unique<typesetting::Engine>(_adapter);
    }
    return self;
}

- (void)setImageSizeProvider:(id<TSImageSizeProvider>)imageSizeProvider {
    _imageSizeProvider = imageSizeProvider;
    _adapter->imageSizeProvider = imageSizeProvider;
}

- (typesetting::Style)convertStyle:(TSStyle *)style {
    typesetting::Style s;
    s.font.family = std::string([style.fontFamily UTF8String]);
    s.font.size = style.fontSize;
    s.font.weight = static_cast<typesetting::FontWeight>(style.fontWeight);
    s.lineSpacingMultiplier = style.lineSpacingMultiplier;
    s.letterSpacing = style.letterSpacing;
    s.wordSpacing = style.wordSpacing;
    s.paragraphSpacing = style.paragraphSpacing;
    s.alignment = static_cast<typesetting::TextAlignment>(style.textAlignment);
    s.hyphenation = style.hyphenation;
    s.marginTop = style.marginTop;
    s.marginBottom = style.marginBottom;
    s.marginLeft = style.marginLeft;
    s.marginRight = style.marginRight;
    return s;
}

- (TSLayoutResult *)convertResult:(const typesetting::LayoutResult&)result {
    TSLayoutResult *tsResult = [[TSLayoutResult alloc] init];
    tsResult.chapterId = [NSString stringWithUTF8String:result.chapterId.c_str()];
    tsResult.totalBlocks = result.totalBlocks;

    NSMutableArray<TSPage *> *pages = [NSMutableArray arrayWithCapacity:result.pages.size()];
    for (const auto& page : result.pages) {
        TSPage *tsPage = [[TSPage alloc] init];
        tsPage.pageIndex = page.pageIndex;
        tsPage.pageWidth = page.width;
        tsPage.pageHeight = page.height;
        tsPage.firstBlockIndex = page.firstBlockIndex;
        tsPage.lastBlockIndex = page.lastBlockIndex;

        NSMutableArray<TSLine *> *lines = [NSMutableArray arrayWithCapacity:page.lines.size()];
        for (const auto& line : page.lines) {
            TSLine *tsLine = [[TSLine alloc] init];
            tsLine.x = line.x;
            tsLine.y = line.y;
            tsLine.width = line.width;
            tsLine.height = line.height;
            tsLine.isLastLineOfParagraph = line.isLastLineOfParagraph;

            NSMutableArray<TSTextRun *> *runs = [NSMutableArray arrayWithCapacity:line.runs.size()];
            for (const auto& run : line.runs) {
                TSTextRun *tsRun = [[TSTextRun alloc] init];
                tsRun.text = [NSString stringWithUTF8String:run.text.c_str()];
                tsRun.fontFamily = [NSString stringWithUTF8String:run.font.family.c_str()];
                tsRun.fontSize = run.font.size;
                tsRun.fontWeight = static_cast<NSInteger>(run.font.weight);
                tsRun.isItalic = (run.font.style == typesetting::FontStyle::Italic);
                tsRun.x = run.x;
                tsRun.y = run.y;
                tsRun.width = run.width;
                tsRun.blockIndex = run.blockIndex;
                tsRun.charOffset = run.charOffset;
                tsRun.charLength = run.charLength;
                tsRun.smallCaps = run.smallCaps;
                tsRun.isLink = run.isLink;
                if (!run.href.empty()) {
                    tsRun.href = [NSString stringWithUTF8String:run.href.c_str()];
                }
                tsRun.isSuperscript = run.isSuperscript;
                [runs addObject:tsRun];
            }
            tsLine.runs = runs;
            [lines addObject:tsLine];
        }
        tsPage.lines = lines;

        NSMutableArray<TSDecoration *> *decorations = [NSMutableArray arrayWithCapacity:page.decorations.size()];
        for (const auto& deco : page.decorations) {
            TSDecoration *tsDeco = [[TSDecoration alloc] init];
            tsDeco.type = static_cast<NSInteger>(deco.type);
            tsDeco.x = deco.x;
            tsDeco.y = deco.y;
            tsDeco.width = deco.width;
            tsDeco.height = deco.height;
            if (!deco.imageSrc.empty()) {
                tsDeco.imageSrc = [NSString stringWithUTF8String:deco.imageSrc.c_str()];
            }
            if (!deco.imageAlt.empty()) {
                tsDeco.imageAlt = [NSString stringWithUTF8String:deco.imageAlt.c_str()];
            }
            [decorations addObject:tsDeco];
        }
        tsPage.decorations = decorations;

        [pages addObject:tsPage];
    }
    tsResult.pages = pages;

    NSMutableArray<NSNumber *> *warnings = [NSMutableArray arrayWithCapacity:result.warnings.size()];
    for (const auto& w : result.warnings) {
        [warnings addObject:@(static_cast<NSInteger>(w))];
    }
    tsResult.warnings = warnings;

    return tsResult;
}

- (TSLayoutResult *)layoutHTML:(NSString *)html
                     chapterId:(NSString *)chapterId
                         style:(TSStyle *)style
                     pageWidth:(CGFloat)pageWidth
                    pageHeight:(CGFloat)pageHeight {
    typesetting::Style cppStyle = [self convertStyle:style];
    typesetting::PageSize pageSize{static_cast<float>(pageWidth), static_cast<float>(pageHeight)};

    auto result = _engine->layoutHTML(
        std::string([html UTF8String]),
        std::string([chapterId UTF8String]),
        cppStyle,
        pageSize
    );

    return [self convertResult:result];
}

- (TSLayoutResult *)layoutHTML:(NSString *)html
                           css:(NSString *)css
                     chapterId:(NSString *)chapterId
                         style:(TSStyle *)style
                     pageWidth:(CGFloat)pageWidth
                    pageHeight:(CGFloat)pageHeight {
    typesetting::Style cppStyle = [self convertStyle:style];
    typesetting::PageSize pageSize{static_cast<float>(pageWidth), static_cast<float>(pageHeight)};

    std::string cppCss = css ? std::string([css UTF8String]) : "";

    auto result = _engine->layoutHTML(
        std::string([html UTF8String]),
        cppCss,
        std::string([chapterId UTF8String]),
        cppStyle,
        pageSize
    );

    return [self convertResult:result];
}

- (TSLayoutResult *)relayoutWithStyle:(TSStyle *)style
                            pageWidth:(CGFloat)pageWidth
                           pageHeight:(CGFloat)pageHeight {
    typesetting::Style cppStyle = [self convertStyle:style];
    typesetting::PageSize pageSize{static_cast<float>(pageWidth), static_cast<float>(pageHeight)};

    auto result = _engine->relayout(cppStyle, pageSize);
    return [self convertResult:result];
}

// MARK: - Chapter title & cover

- (void)setChapterTitle:(NSString *)title {
    _engine->setChapterTitle(std::string([title UTF8String]));
}

- (TSLayoutResult *)layoutCover:(NSString *)imageSrc
                      pageWidth:(CGFloat)pageWidth
                     pageHeight:(CGFloat)pageHeight {
    typesetting::PageSize pageSize{static_cast<float>(pageWidth), static_cast<float>(pageHeight)};
    auto result = _engine->layoutCover(std::string([imageSrc UTF8String]), pageSize);
    return [self convertResult:result];
}

// MARK: - Interaction queries

- (TSHitTestResult *)hitTest:(NSInteger)pageIndex x:(CGFloat)x y:(CGFloat)y {
    auto hit = _engine->hitTest(static_cast<int>(pageIndex),
                                 static_cast<float>(x), static_cast<float>(y));
    TSHitTestResult *r = [[TSHitTestResult alloc] init];
    r.blockIndex = hit.blockIndex;
    r.lineIndex = hit.lineIndex;
    r.runIndex = hit.runIndex;
    r.charOffset = hit.charOffset;
    r.found = hit.found;
    return r;
}

- (TSWordRange *)wordAtPoint:(NSInteger)pageIndex x:(CGFloat)x y:(CGFloat)y {
    auto word = _engine->wordAtPoint(static_cast<int>(pageIndex),
                                      static_cast<float>(x), static_cast<float>(y));
    TSWordRange *r = [[TSWordRange alloc] init];
    r.blockIndex = word.blockIndex;
    r.charOffset = word.charOffset;
    r.charLength = word.charLength;
    r.text = [NSString stringWithUTF8String:word.text.c_str()];
    return r;
}

- (NSArray<TSSentenceRange *> *)getSentences:(NSInteger)pageIndex {
    auto sentences = _engine->getSentences(static_cast<int>(pageIndex));
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:sentences.size()];
    for (const auto& s : sentences) {
        TSSentenceRange *sr = [[TSSentenceRange alloc] init];
        sr.blockIndex = s.blockIndex;
        sr.charOffset = s.charOffset;
        sr.charLength = s.charLength;
        sr.text = [NSString stringWithUTF8String:s.text.c_str()];
        [result addObject:sr];
    }
    return result;
}

- (NSArray<TSSentenceRange *> *)getAllSentences {
    auto sentences = _engine->getAllSentences();
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:sentences.size()];
    for (const auto& s : sentences) {
        TSSentenceRange *sr = [[TSSentenceRange alloc] init];
        sr.blockIndex = s.blockIndex;
        sr.charOffset = s.charOffset;
        sr.charLength = s.charLength;
        sr.text = [NSString stringWithUTF8String:s.text.c_str()];
        [result addObject:sr];
    }
    return result;
}

- (NSArray<TSTextRect *> *)getRectsForRange:(NSInteger)pageIndex
                                 blockIndex:(NSInteger)blockIndex
                                 charOffset:(NSInteger)charOffset
                                 charLength:(NSInteger)charLength {
    auto rects = _engine->getRectsForRange(static_cast<int>(pageIndex),
                                            static_cast<int>(blockIndex),
                                            static_cast<int>(charOffset),
                                            static_cast<int>(charLength));
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:rects.size()];
    for (const auto& rect : rects) {
        TSTextRect *tr = [[TSTextRect alloc] init];
        tr.x = rect.x;
        tr.y = rect.y;
        tr.width = rect.width;
        tr.height = rect.height;
        [result addObject:tr];
    }
    return result;
}

- (TSTextRect *)getBlockRect:(NSInteger)pageIndex blockIndex:(NSInteger)blockIndex {
    auto rect = _engine->getBlockRect(static_cast<int>(pageIndex),
                                       static_cast<int>(blockIndex));
    TSTextRect *r = [[TSTextRect alloc] init];
    r.x = rect.x;
    r.y = rect.y;
    r.width = rect.width;
    r.height = rect.height;
    return r;
}

- (TSImageHitResult *)hitTestImage:(NSInteger)pageIndex x:(CGFloat)x y:(CGFloat)y {
    auto hit = _engine->hitTestImage(static_cast<int>(pageIndex),
                                      static_cast<float>(x), static_cast<float>(y));
    TSImageHitResult *r = [[TSImageHitResult alloc] init];
    r.imageSrc = hit.imageSrc.empty() ? nil : [NSString stringWithUTF8String:hit.imageSrc.c_str()];
    r.imageAlt = hit.imageAlt.empty() ? nil : [NSString stringWithUTF8String:hit.imageAlt.c_str()];
    r.x = hit.x;
    r.y = hit.y;
    r.width = hit.width;
    r.height = hit.height;
    r.found = hit.found;
    return r;
}

- (TSPageInfo *)getPageInfo:(NSInteger)pageIndex {
    auto info = _engine->getPageInfo(static_cast<int>(pageIndex));
    TSPageInfo *r = [[TSPageInfo alloc] init];
    r.chapterTitle = [NSString stringWithUTF8String:info.chapterTitle.c_str()];
    r.currentPage = info.currentPage;
    r.totalPages = info.totalPages;
    r.progress = info.progress;
    r.firstBlockIndex = info.firstBlockIndex;
    r.lastBlockIndex = info.lastBlockIndex;
    return r;
}

@end
