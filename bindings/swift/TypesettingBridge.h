#pragma once

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

NS_ASSUME_NONNULL_BEGIN

/// A positioned text run for rendering
@interface TSTextRun : NSObject
@property (nonatomic, copy) NSString *text;
@property (nonatomic, copy) NSString *fontFamily;
@property (nonatomic, assign) CGFloat fontSize;
@property (nonatomic, assign) NSInteger fontWeight;
@property (nonatomic, assign) BOOL isItalic;
@property (nonatomic, assign) CGFloat x;
@property (nonatomic, assign) CGFloat y;
@property (nonatomic, assign) CGFloat width;
@property (nonatomic, assign) NSInteger blockIndex;
@property (nonatomic, assign) NSInteger charOffset;
@property (nonatomic, assign) NSInteger charLength;
@property (nonatomic, assign) BOOL smallCaps;
@property (nonatomic, assign) BOOL isLink;
@property (nonatomic, copy, nullable) NSString *href;
@property (nonatomic, assign) BOOL isSuperscript;
@end

/// A laid-out line
@interface TSLine : NSObject
@property (nonatomic, strong) NSArray<TSTextRun *> *runs;
@property (nonatomic, assign) CGFloat x;
@property (nonatomic, assign) CGFloat y;
@property (nonatomic, assign) CGFloat width;
@property (nonatomic, assign) CGFloat height;
@property (nonatomic, assign) BOOL isLastLineOfParagraph;
@end

/// A visual decoration element (e.g., horizontal rule, image placeholder, table border)
@interface TSDecoration : NSObject
@property (nonatomic, assign) NSInteger type;  // 0 = HorizontalRule, 1 = ImagePlaceholder, 2 = TableBorder
@property (nonatomic, assign) CGFloat x;
@property (nonatomic, assign) CGFloat y;
@property (nonatomic, assign) CGFloat width;
@property (nonatomic, assign) CGFloat height;
@property (nonatomic, copy, nullable) NSString *imageSrc;  // For ImagePlaceholder
@property (nonatomic, copy, nullable) NSString *imageAlt;  // For ImagePlaceholder
@end

/// A laid-out page
@interface TSPage : NSObject
@property (nonatomic, assign) NSInteger pageIndex;
@property (nonatomic, strong) NSArray<TSLine *> *lines;
@property (nonatomic, assign) CGFloat pageWidth;
@property (nonatomic, assign) CGFloat pageHeight;
@property (nonatomic, assign) NSInteger firstBlockIndex;
@property (nonatomic, assign) NSInteger lastBlockIndex;
@property (nonatomic, strong) NSArray<TSDecoration *> *decorations;
@end

/// Layout result for a chapter
@interface TSLayoutResult : NSObject
@property (nonatomic, copy) NSString *chapterId;
@property (nonatomic, strong) NSArray<TSPage *> *pages;
@property (nonatomic, assign) NSInteger totalBlocks;
@property (nonatomic, strong) NSArray<NSNumber *> *warnings;  // LayoutWarning values (0=None, 1=EmptyContent, 2=ParseError, 3=LayoutOverflow)
@end

/// Style configuration
@interface TSStyle : NSObject
@property (nonatomic, copy) NSString *fontFamily;
@property (nonatomic, assign) CGFloat fontSize;
@property (nonatomic, assign) NSInteger fontWeight;
@property (nonatomic, assign) CGFloat lineSpacingMultiplier;
@property (nonatomic, assign) CGFloat letterSpacing;
@property (nonatomic, assign) CGFloat wordSpacing;
@property (nonatomic, assign) CGFloat paragraphSpacing;
@property (nonatomic, assign) NSInteger textAlignment; // 0=left, 1=center, 2=right, 3=justified
@property (nonatomic, assign) BOOL hyphenation;
@property (nonatomic, copy, nullable) NSString *cssString;
@property (nonatomic, assign) CGFloat marginTop;
@property (nonatomic, assign) CGFloat marginBottom;
@property (nonatomic, assign) CGFloat marginLeft;
@property (nonatomic, assign) CGFloat marginRight;
@end

/// Hit test result
@interface TSHitTestResult : NSObject
@property (nonatomic, assign) NSInteger blockIndex;
@property (nonatomic, assign) NSInteger lineIndex;
@property (nonatomic, assign) NSInteger runIndex;
@property (nonatomic, assign) NSInteger charOffset;
@property (nonatomic, assign) BOOL found;
@end

/// Word range
@interface TSWordRange : NSObject
@property (nonatomic, assign) NSInteger blockIndex;
@property (nonatomic, assign) NSInteger charOffset;
@property (nonatomic, assign) NSInteger charLength;
@property (nonatomic, copy) NSString *text;
@end

/// Sentence range
@interface TSSentenceRange : NSObject
@property (nonatomic, assign) NSInteger blockIndex;
@property (nonatomic, assign) NSInteger charOffset;
@property (nonatomic, assign) NSInteger charLength;
@property (nonatomic, copy) NSString *text;
@end

/// Text rectangle in page coordinates
@interface TSTextRect : NSObject
@property (nonatomic, assign) CGFloat x;
@property (nonatomic, assign) CGFloat y;
@property (nonatomic, assign) CGFloat width;
@property (nonatomic, assign) CGFloat height;
@end

/// Image hit test result
@interface TSImageHitResult : NSObject
@property (nonatomic, copy, nullable) NSString *imageSrc;
@property (nonatomic, copy, nullable) NSString *imageAlt;
@property (nonatomic, assign) CGFloat x;
@property (nonatomic, assign) CGFloat y;
@property (nonatomic, assign) CGFloat width;
@property (nonatomic, assign) CGFloat height;
@property (nonatomic, assign) BOOL found;
@end

/// Page metadata for header/footer rendering
@interface TSPageInfo : NSObject
@property (nonatomic, copy) NSString *chapterTitle;
@property (nonatomic, assign) NSInteger currentPage;
@property (nonatomic, assign) NSInteger totalPages;
@property (nonatomic, assign) CGFloat progress;
@property (nonatomic, assign) NSInteger firstBlockIndex;
@property (nonatomic, assign) NSInteger lastBlockIndex;
@end

/// Protocol for providing image dimensions to the typesetting engine
@protocol TSImageSizeProvider <NSObject>
/// Return the natural size of an image, or nil if unknown.
/// The CGSize should be wrapped in an NSValue.
- (nullable NSValue *)imageSizeForSource:(NSString *)src;
@end

/// Main bridge to the C++ typesetting engine
@interface TypesettingBridge : NSObject

@property (nonatomic, weak, nullable) id<TSImageSizeProvider> imageSizeProvider;

- (instancetype)init;

/// Layout HTML content into pages
- (TSLayoutResult *)layoutHTML:(NSString *)html
                     chapterId:(NSString *)chapterId
                         style:(TSStyle *)style
                     pageWidth:(CGFloat)pageWidth
                    pageHeight:(CGFloat)pageHeight;

/// Layout HTML content with CSS into pages
- (TSLayoutResult *)layoutHTML:(NSString *)html
                           css:(nullable NSString *)css
                     chapterId:(NSString *)chapterId
                         style:(TSStyle *)style
                     pageWidth:(CGFloat)pageWidth
                    pageHeight:(CGFloat)pageHeight;

/// Re-layout with new style (uses cached content)
- (TSLayoutResult *)relayoutWithStyle:(TSStyle *)style
                            pageWidth:(CGFloat)pageWidth
                           pageHeight:(CGFloat)pageHeight;

/// Set chapter title for page info queries
- (void)setChapterTitle:(NSString *)title;

/// Layout a cover page (full-bleed image)
- (TSLayoutResult *)layoutCover:(NSString *)imageSrc
                      pageWidth:(CGFloat)pageWidth
                     pageHeight:(CGFloat)pageHeight;

// -- Interaction queries --

/// Hit test: coordinate → character-level mapping
- (TSHitTestResult *)hitTest:(NSInteger)pageIndex x:(CGFloat)x y:(CGFloat)y;

/// Get the word at a given point
- (TSWordRange *)wordAtPoint:(NSInteger)pageIndex x:(CGFloat)x y:(CGFloat)y;

/// Get all sentences on a page
- (NSArray<TSSentenceRange *> *)getSentences:(NSInteger)pageIndex;

/// Get all sentences across all pages
- (NSArray<TSSentenceRange *> *)getAllSentences;

/// Character range → visual rectangles
- (NSArray<TSTextRect *> *)getRectsForRange:(NSInteger)pageIndex
                                 blockIndex:(NSInteger)blockIndex
                                 charOffset:(NSInteger)charOffset
                                 charLength:(NSInteger)charLength;

/// Bounding rect of an entire block on a page
- (TSTextRect *)getBlockRect:(NSInteger)pageIndex blockIndex:(NSInteger)blockIndex;

/// Image hit test
- (TSImageHitResult *)hitTestImage:(NSInteger)pageIndex x:(CGFloat)x y:(CGFloat)y;

/// Page metadata (chapter title, progress, etc.)
- (TSPageInfo *)getPageInfo:(NSInteger)pageIndex;

@end

NS_ASSUME_NONNULL_END
