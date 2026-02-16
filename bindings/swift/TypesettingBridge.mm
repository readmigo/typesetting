#import "TypesettingBridge.h"

#include "typesetting/engine.h"
#include "typesetting/platform.h"

#import <CoreText/CoreText.h>
#import <UIKit/UIKit.h>

// MARK: - CoreText Platform Adapter

class CoreTextAdapter : public typesetting::PlatformAdapter {
public:
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
}
@end

@implementation TypesettingBridge

- (instancetype)init {
    self = [super init];
    if (self) {
        auto platform = std::make_shared<CoreTextAdapter>();
        _engine = std::make_unique<typesetting::Engine>(platform);
    }
    return self;
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
            [decorations addObject:tsDeco];
        }
        tsPage.decorations = decorations;

        [pages addObject:tsPage];
    }
    tsResult.pages = pages;
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

@end
