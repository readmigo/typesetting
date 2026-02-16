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

/// A visual decoration element (e.g., horizontal rule)
@interface TSDecoration : NSObject
@property (nonatomic, assign) NSInteger type;  // 0 = HorizontalRule
@property (nonatomic, assign) CGFloat x;
@property (nonatomic, assign) CGFloat y;
@property (nonatomic, assign) CGFloat width;
@property (nonatomic, assign) CGFloat height;
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

/// Main bridge to the C++ typesetting engine
@interface TypesettingBridge : NSObject

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

@end

NS_ASSUME_NONNULL_END
