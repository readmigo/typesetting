# Typesetting Engine — 设计文档

## 目录

1. [项目概述](#1-项目概述)
2. [设计原则](#2-设计原则)
3. [系统架构](#3-系统架构)
4. [核心数据模型](#4-核心数据模型)
5. [模块详细设计](#5-模块详细设计)
   - 5.1 [平台抽象层 (PlatformAdapter)](#51-平台抽象层-platformadapter)
   - 5.2 [HTML 解析器 (Document)](#52-html-解析器-document)
   - 5.3 [CSS 解析器 (CSSStylesheet)](#53-css-解析器-cssstylesheet)
   - 5.4 [样式解析器 (StyleResolver)](#54-样式解析器-styleresolver)
   - 5.5 [排版引擎 (LayoutEngine)](#55-排版引擎-layoutengine)
   - 5.6 [引擎门面 (Engine)](#56-引擎门面-engine)
6. [平台绑定层](#6-平台绑定层)
   - 6.1 [iOS 绑定 (TypesettingBridge)](#61-ios-绑定-typesettingbridge)
   - 6.2 [Android 绑定 (TypesettingJNI)](#62-android-绑定-typesettingjni)
7. [数据流](#7-数据流)
8. [关键算法](#8-关键算法)
9. [样式层叠与优先级](#9-样式层叠与优先级)
10. [SE CSS 支持](#10-se-css-支持)
11. [构建与测试](#11-构建与测试)
12. [文件清单](#12-文件清单)
13. [向后兼容性](#13-向后兼容性)

---

## 1. 项目概述

Typesetting Engine 是一个高性能的 C++17 排版引擎，专为电子书阅读场景设计，服务于 iOS 和 Android 平台。引擎接收 HTML 内容和 CSS 样式，结合用户阅读偏好，输出精确定位的排版结果（页面、行、文本片段），由平台原生渲染层负责最终绘制。

### 核心能力

| 能力 | 说明 |
|------|------|
| HTML 解析 | 解析 Standard Ebooks 格式的 HTML，提取块级元素和行内元素 |
| CSS 样式 | 完整支持 SE CSS 选择器匹配和属性解析 |
| 样式层叠 | 三层优先级：引擎默认值 → CSS 规则 → 用户偏好 |
| 多字体行内排版 | 同一行内支持不同字体的 TextRun（粗体、斜体、等宽等） |
| 文本排版 | 两端对齐、断词、首行缩进、小型大写 |
| 跨平台 | 平台无关的 C++ 核心 + 原生绑定（CoreText / JNI） |

### 技术栈

| 组件 | 技术 |
|------|------|
| 核心引擎 | C++17, CMake 3.20+ |
| 测试框架 | GoogleTest v1.14.0 |
| iOS 绑定 | Objective-C++, CoreText / CoreFoundation / CoreGraphics |
| Android 绑定 | C++/JNI, Skia / HarfBuzz |
| 编译器要求 | Clang 10+ / GCC 9+ / MSVC 19.14+ |

---

## 2. 设计原则

| 原则 | 说明 |
|------|------|
| 管线架构 | 数据沿 HTML → Block[] → BlockComputedStyle[] → Line[] → Page[] 管线单向流动 |
| 平台无关核心 | 所有排版逻辑在 C++ 层完成，通过 `PlatformAdapter` 抽象接口获取字体度量 |
| 关注点分离 | 解析、样式计算、排版、输出各自独立，单一职责 |
| 渐进扩展 | 新增模块（css、style_resolver）基于现有架构扩展，不重写核心数据结构 |
| 接口兼容 | 保留无 CSS 的旧接口，新增带 CSS 参数的重载 |
| 数据驱动排版 | 排版引擎由 `BlockComputedStyle` 驱动，不硬编码块类型与样式的映射 |
| 最小平台依赖 | 核心引擎仅使用 C++ 标准库（`vector`, `string`, `optional`, `shared_ptr`），无第三方依赖 |

---

## 3. 系统架构

### 3.1 整体架构

```mermaid
graph TB
    subgraph "输入"
        HTML["HTML 字符串"]
        CSS["CSS 字符串"]
        UserStyle["Style（用户偏好）"]
        PS["PageSize"]
    end

    subgraph "Core Engine (C++17)"
        HTMLParser["HTML 解析器<br/>document.h/cpp"]
        CSSParser["CSS 解析器<br/>css.h/cpp"]
        SR["样式解析器<br/>style_resolver.h/cpp"]
        LE["排版引擎<br/>layout.h/cpp"]
        Facade["引擎门面<br/>engine.h/cpp"]
    end

    subgraph "Platform Adapter（抽象接口）"
        PA["PlatformAdapter<br/>platform.h"]
    end

    subgraph "平台实现"
        CT["CoreText 实现<br/>（iOS）"]
        Android["Skia/HarfBuzz 实现<br/>（Android）"]
    end

    subgraph "绑定层"
        Swift["TypesettingBridge<br/>Objective-C++"]
        JNI["TypesettingJNI<br/>C++/JNI"]
    end

    subgraph "输出"
        Result["LayoutResult<br/>Page[] → Line[] → TextRun[]"]
    end

    HTML --> Facade
    CSS --> Facade
    UserStyle --> Facade
    PS --> Facade

    Facade --> HTMLParser
    Facade --> CSSParser
    Facade --> SR
    Facade --> LE

    HTMLParser --> SR
    CSSParser --> SR
    UserStyle --> SR
    SR --> LE
    PA --> LE

    CT --> PA
    Android --> PA

    LE --> Result

    Swift --> Facade
    JNI --> Facade
```

### 3.2 设计模式

| 模式 | 应用位置 | 说明 |
|------|---------|------|
| **门面模式 (Facade)** | `Engine` | 统一入口，协调 HTML 解析、CSS 解析、样式计算、排版 |
| **Pimpl 模式** | `LayoutEngine` | 隐藏排版实现细节，减少头文件依赖，保持 ABI 稳定 |
| **抽象工厂 (Abstract Interface)** | `PlatformAdapter` | 定义字体度量和文本测量的平台无关接口 |
| **工厂方法 (Factory)** | `InlineElement` | 静态工厂方法创建不同类型的行内元素 |
| **策略模式 (Strategy)** | `StyleResolver` | 可替换的样式解析策略（默认值 → CSS → 用户覆盖） |

---

## 4. 核心数据模型

### 4.1 文档模型

```mermaid
classDiagram
    class Document {
        +string bookId
        +string title
        +vector~Chapter~ chapters
    }

    class Chapter {
        +string id
        +string title
        +int orderIndex
        +vector~Block~ blocks
    }

    class Block {
        +BlockType type
        +vector~InlineElement~ inlines
        +string src
        +string alt
        +string caption
        +int listIndex
        +string className
        +string epubType
        +string htmlTag
        +string parentTag
        +string parentClassName
        +string parentEpubType
        +bool isFirstChild
        +string previousSiblingTag
        +plainText() string
    }

    class InlineElement {
        +InlineType type
        +string text
        +string href
        +string lang
        +string className
        +string epubType
    }

    class BlockType {
        <<enumeration>>
        Paragraph
        Heading1~4
        Blockquote
        CodeBlock
        Image
        HorizontalRule
        ListItem
    }

    class InlineType {
        <<enumeration>>
        Text
        Bold
        Italic
        BoldItalic
        Code
        Link
    }

    Document --> Chapter
    Chapter --> Block
    Block --> InlineElement
    Block --> BlockType
    InlineElement --> InlineType
```

### 4.2 样式模型

```mermaid
classDiagram
    class Style {
        +FontDescriptor font
        +float lineSpacingMultiplier
        +float letterSpacing
        +float wordSpacing
        +float paragraphSpacing
        +TextAlignment alignment
        +bool hyphenation
        +string locale
        +float textIndent
        +float marginTop/Bottom/Left/Right
        +lineHeight() float
        +contentWidth(pageWidth) float
        +contentHeight(pageHeight) float
    }

    class BlockComputedStyle {
        +FontDescriptor font
        +float textIndent
        +TextAlignment alignment
        +bool hyphens
        +bool smallCaps
        +bool hidden
        +float lineSpacingMultiplier
        +float letterSpacing
        +float wordSpacing
        +float paragraphSpacingAfter
        +float marginTop/Bottom/Left/Right
        +bool oldstyleNums
        +bool hangingPunctuation
        +optional~HRStyle~ hrStyle
    }

    class HRStyle {
        +float borderWidth
        +float widthPercent
        +float marginTopEm
        +float marginBottomEm
    }

    class FontDescriptor {
        +string family
        +float size
        +FontWeight weight
        +FontStyle style
    }

    class TextAlignment {
        <<enumeration>>
        Left
        Center
        Right
        Justified
    }

    Style --> FontDescriptor
    Style --> TextAlignment
    BlockComputedStyle --> FontDescriptor
    BlockComputedStyle --> TextAlignment
    BlockComputedStyle --> HRStyle
```

`Style` 表示用户的全局阅读偏好（字体、字号、行距等）。`BlockComputedStyle` 表示每个块级元素的最终计算样式，由引擎默认值 + CSS 规则 + 用户偏好三层合并而成。

### 4.3 输出模型

```mermaid
classDiagram
    class LayoutResult {
        +string chapterId
        +vector~Page~ pages
        +int totalBlocks
    }

    class Page {
        +int pageIndex
        +vector~Line~ lines
        +vector~Decoration~ decorations
        +float width / height
        +float contentX / contentY
        +float contentWidth / contentHeight
        +int firstBlockIndex
        +int lastBlockIndex
    }

    class Line {
        +vector~TextRun~ runs
        +float x / y
        +float width / height
        +float ascent / descent
        +bool isLastLineOfParagraph
        +bool endsWithHyphen
    }

    class TextRun {
        +string text
        +FontDescriptor font
        +float x / y / width
        +int blockIndex
        +int inlineIndex
        +int charOffset / charLength
        +bool smallCaps
        +bool isLink
        +string href
    }

    class Decoration {
        +DecorationType type
        +float x / y / width / height
    }

    LayoutResult --> Page
    Page --> Line
    Page --> Decoration
    Line --> TextRun
```

`TextRun` 是最小渲染单元，携带文本内容、精确坐标（以页面左上角为原点）、字体描述以及回溯源文档的索引信息（用于 TTS 高亮、文本选择等场景）。

### 4.4 CSS 模型

```mermaid
classDiagram
    class CSSStylesheet {
        +vector~CSSRule~ rules
        +parse(css) CSSStylesheet
    }

    class CSSRule {
        +CSSSelector selector
        +CSSProperties properties
    }

    class CSSSelector {
        +SelectorType type
        +string element
        +string className
        +string pseudoClass
        +string attribute
        +string attributeValue
        +shared_ptr~CSSSelector~ parent
        +shared_ptr~CSSSelector~ adjacentSibling
        +specificity() int
    }

    class CSSProperties {
        +optional~float~ textIndent
        +optional~float~ marginTop/Bottom/Left/Right
        +optional~TextAlignment~ textAlign
        +optional~FontStyle~ fontStyle
        +optional~FontWeight~ fontWeight
        +optional~FontVariant~ fontVariant
        +optional~bool~ hyphens
        +optional~bool~ displayNone
        +optional~bool~ hangingPunctuation
        +optional~float~ borderTopWidth
        +optional~float~ widthPercent
        +merge(other) void
    }

    class SelectorType {
        <<enumeration>>
        Element
        Class
        Descendant
        AdjacentSibling
        PseudoFirstChild
        Attribute
        Universal
    }

    CSSStylesheet --> CSSRule
    CSSRule --> CSSSelector
    CSSRule --> CSSProperties
    CSSSelector --> SelectorType
```

### 4.5 平台抽象模型

```mermaid
classDiagram
    class PlatformAdapter {
        <<abstract>>
        +resolveFontMetrics(FontDescriptor) FontMetrics
        +measureText(string, FontDescriptor) TextMeasurement
        +findLineBreak(string, FontDescriptor, float) size_t
        +supportsHyphenation(string) bool
        +findHyphenationPoints(string, string) vector~size_t~
    }

    class FontMetrics {
        +float ascent
        +float descent
        +float leading
        +float xHeight
        +float capHeight
        +lineHeight() float
    }

    class TextMeasurement {
        +float width
        +float height
    }

    class FontWeight {
        <<enumeration>>
        Thin = 100
        Light = 300
        Regular = 400
        Medium = 500
        Semibold = 600
        Bold = 700
        Heavy = 900
    }

    class FontStyle {
        <<enumeration>>
        Normal
        Italic
    }

    PlatformAdapter --> FontMetrics
    PlatformAdapter --> TextMeasurement
```

---

## 5. 模块详细设计

### 5.1 平台抽象层 (PlatformAdapter)

**文件**：`include/typesetting/platform.h`

**职责**：定义字体度量和文本测量的抽象接口，使排版核心与平台实现解耦。

**接口说明**：

| 方法 | 输入 | 输出 | 说明 |
|------|------|------|------|
| `resolveFontMetrics` | `FontDescriptor` | `FontMetrics` | 解析字体描述符，返回 ascent/descent/leading 等度量 |
| `measureText` | `text`, `FontDescriptor` | `TextMeasurement` | 测量文本宽度和高度 |
| `findLineBreak` | `text`, `FontDescriptor`, `maxWidth` | `size_t` | 查找不超过 maxWidth 的最佳断行位置（字节索引） |
| `supportsHyphenation` | `locale` | `bool` | 检查给定语言是否支持断词 |
| `findHyphenationPoints` | `word`, `locale` | `vector<size_t>` | 返回单词的合法断词位置 |

**不变性保证**：PlatformAdapter 接口在整个项目演进过程中保持不变。现有的 `FontDescriptor`（family/size/weight/style）足以覆盖所有 CSS 字体需求。小型大写（small-caps）等高级排版特性由引擎层或平台渲染层处理，不需要扩展 PlatformAdapter。

### 5.2 HTML 解析器 (Document)

**文件**：`include/typesetting/document.h`, `src/document.cpp`

**职责**：将 HTML 字符串解析为结构化的 Block/InlineElement 文档树。

#### 解析流程

```mermaid
flowchart TD
    A["HTML 字符串"] --> B["逐字符扫描"]
    B --> C{是否为标签?}
    C -- 是 --> D["parseTag() 提取标签信息"]
    D --> E{标签类型?}
    E -- "不可见标签<br/>(style/script/head)" --> F["跳过标签及内容"]
    E -- "容器标签<br/>(section/div/article)" --> G["入栈/出栈 ParentInfo"]
    E -- "块级标签<br/>(p/h1-h4/blockquote)" --> H["创建新 Block<br/>填充元数据"]
    E -- "行内标签<br/>(b/i/a/code)" --> I["设置 currentInline 类型"]
    E -- "自闭合标签<br/>(hr/br/img)" --> J["创建特殊 Block"]
    C -- 否 --> K["提取文本内容"]
    K --> L["decodeEntities() 解码"]
    L --> M["创建 InlineElement<br/>加入当前 Block"]

    H --> N["populateBlockMeta()"]
    N --> O["从 parentStack 读取父元素信息"]
    N --> P["从 lastSiblingTagAtDepth 读取兄弟信息"]
```

#### 父元素追踪

HTML 解析器维护一个 `parentStack`（类型 `vector<ParentInfo>`），用于追踪当前元素的祖先信息，为 CSS 后代选择器（如 `blockquote p`）提供匹配数据：

```
<section class="chapter">     → parentStack.push({tag:"section", class:"chapter"})
  <p>Text</p>                 → block.parentTag="section", block.parentClassName="chapter"
</section>                     → parentStack.pop()
```

#### 兄弟元素追踪

使用 `lastSiblingTagAtDepth` 数组追踪每个嵌套深度上最后一个块级元素的标签名，为 CSS 相邻兄弟选择器（如 `h2 + p`）提供匹配数据。

#### HTML 实体解码

支持的 HTML 实体：

| 实体 | 字符 | 实体 | 字符 |
|------|------|------|------|
| `&amp;` | & | `&mdash;` | — |
| `&lt;` | < | `&ndash;` | – |
| `&gt;` | > | `&hellip;` | … |
| `&quot;` | " | `&lsquo;` / `&rsquo;` | ' / ' |
| `&nbsp;` | (空格) | `&ldquo;` / `&rdquo;` | " / " |

### 5.3 CSS 解析器 (CSSStylesheet)

**文件**：`include/typesetting/css.h`, `src/css.cpp`

**职责**：将 CSS 字符串解析为结构化的 `CSSStylesheet`（规则列表）。

#### 解析流程

```mermaid
flowchart TD
    A["CSS 字符串"] --> B["stripComments()<br/>移除 /* ... */"]
    B --> C["逐字符扫描"]
    C --> D{字符类型?}
    D -- "@" --> E["跳过 @-rules<br/>(@media, @namespace 等)"]
    D -- "其他" --> F["查找 { 和 }"]
    F --> G["提取选择器部分"]
    F --> H["提取声明块"]
    G --> I["处理逗号分隔<br/>的多选择器"]
    I --> J["parseSelector()<br/>解析每个选择器"]
    H --> K["parseProperties()<br/>解析属性声明"]
    J --> L["创建 CSSRule"]
    K --> L
    L --> M["添加到 stylesheet.rules"]
```

#### 选择器解析

选择器解析器通过 `parseSelector()` 函数将选择器字符串解析为 `CSSSelector` 结构。解析逻辑：

1. **检查 `+` 符号**：若包含，解析为 `AdjacentSibling` 类型
2. **按空白分词**：
   - 单个 token → 判断为 Element / Class / PseudoFirstChild / Attribute / Universal
   - 多个 token → 解析为 `Descendant` 类型，构建 parent 链
3. **特殊前缀**：`[` 开头为 Attribute，`.` 开头为 Class，`*` 为 Universal
4. **伪类**：通过 `:` 分割 element 和 pseudo-class 部分

#### 支持的选择器类型

| 类型 | 语法示例 | CSSSelector 字段 |
|------|---------|-----------------|
| 元素选择器 | `p`, `h2` | `type=Element, element="p"` |
| 类选择器 | `.classname` | `type=Class, className="classname"` |
| 后代选择器 | `blockquote p` | `type=Descendant, element="p", parent→element="blockquote"` |
| 相邻兄弟选择器 | `h2 + p` | `type=AdjacentSibling, element="p", adjacentSibling→element="h2"` |
| 伪类选择器 | `p:first-child` | `type=PseudoFirstChild, element="p", pseudoClass="first-child"` |
| 属性选择器 | `[epub\|type~="dedication"]` | `type=Attribute, attribute="epub:type", attributeValue="dedication"` |
| 通配选择器 | `*` | `type=Universal` |

#### 属性解析

`parseProperties()` 解析 CSS 声明块，支持以下属性：

| CSS 属性 | 解析逻辑 |
|---------|---------|
| `text-indent` | 解析数值 + 单位，存为 em 值 |
| `text-align` | 映射到 `TextAlignment` 枚举 |
| `font-style` | `italic` / `normal` |
| `font-weight` | `bold` / `normal` / 数值 |
| `font-variant` | `small-caps` / `normal` |
| `hyphens` | `auto` → true, `none` → false |
| `display` | `none` → displayNone=true |
| `margin` | 支持 1-4 值简写（含 `auto` 关键字） |
| `margin-top/bottom/left/right` | 独立边距属性 |
| `border-top` | 提取像素宽度值 |
| `width` | 百分比值（如 `25%`） |
| `hanging-punctuation` | `first` / `last` / `first last` / `none` |

#### Specificity 计算

选择器优先级按 CSS 标准的 (id, class, element) 三元组计算，编码为 `ids * 100 + classes * 10 + elements`：

| 选择器 | 计算 | Specificity |
|--------|------|------------|
| `p` | (0,0,1) | 1 |
| `.class` | (0,1,0) | 10 |
| `p:first-child` | (0,1,1) | 11 |
| `h2 + p` | (0,0,2) | 2 |
| `blockquote p` | (0,0,2) | 2 |
| `.parent p` | (0,1,1) | 11 |

对于复合选择器，递归累加 parent 和 adjacentSibling 的 specificity。

### 5.4 样式解析器 (StyleResolver)

**文件**：`include/typesetting/style_resolver.h`, `src/style_resolver.cpp`

**职责**：将 CSS 规则 + 用户 Style + Block 元数据合并为每个 Block 的最终计算样式。

#### 解析流程

```mermaid
flowchart TD
    A["输入: Block[], CSSStylesheet, Style"] --> B["遍历每个 Block"]

    B --> C["1. defaultStyleForBlock()<br/>根据 BlockType 生成默认样式"]
    C --> D["2. selectorMatches()<br/>收集所有匹配的 CSS 规则"]
    D --> E["3. 按 specificity 升序排序"]
    E --> F["4. applyProperties()<br/>依次应用 CSS 属性"]
    F --> G["5. applyUserOverrides()<br/>应用用户偏好覆盖"]
    G --> H["输出: BlockComputedStyle"]
```

#### 默认样式映射

引擎为每种 `BlockType` 预设合理的默认样式：

| BlockType | 字号比例 | 对齐方式 | 缩进 | 特殊属性 |
|-----------|---------|---------|------|---------|
| Paragraph | 1.0x | Justified | 1em | hyphens=true |
| Heading1 | 1.5x | Center | 0 | smallCaps, hyphens=false |
| Heading2 | 1.3x | Center | 0 | smallCaps, hyphens=false |
| Heading3 | 1.1x | Center | 0 | smallCaps, hyphens=false |
| Heading4 | 1.0x | Center | 0 | smallCaps, hyphens=false |
| Blockquote | 1.0x | Justified | 0 | marginLeft/Right=2.5em |
| CodeBlock | 0.9x (monospace) | Left | 0 | hyphens=false |
| HorizontalRule | — | — | — | hrStyle 默认值 |
| ListItem | 1.0x | Justified | 0 | marginLeft=2em |

#### 选择器匹配

`selectorMatches()` 方法针对每种 `SelectorType` 执行不同的匹配逻辑：

| SelectorType | 匹配逻辑 |
|-------------|---------|
| Element | `block.htmlTag` (或 `blockTypeToTag(block.type)`) == selector.element |
| Class | `block.className` 的空格分隔列表中包含 selector.className |
| Descendant | 主元素匹配当前 block + parent 选择器匹配 block.parentTag/parentClassName/parentEpubType |
| AdjacentSibling | 主元素匹配当前 block + adjacentSibling 匹配 block.previousSiblingTag |
| PseudoFirstChild | 元素匹配 + block.isFirstChild == true |
| Attribute | block.epubType 或 block.parentEpubType 包含 attributeValue |
| Universal | 始终匹配 |

#### 用户偏好覆盖规则

| 用户 Style 属性 | 覆盖行为 |
|----------------|---------|
| `font.family` | **始终覆盖**所有 Block |
| `font.size` | 覆盖正文 Block；标题和代码块保持相对比例 |
| `lineSpacingMultiplier` | **始终覆盖** |
| `letterSpacing` / `wordSpacing` | **始终覆盖** |
| `paragraphSpacing` | **始终覆盖** |
| `alignment` | 覆盖，**除非** Block 是标题且 CSS 设为 Center |
| `hyphenation` | 覆盖，**除非** CSS 显式设置 `hyphens: none` |
| `textIndent` | **不覆盖**（由 CSS 控制） |
| `margin*` | 作为**页面边距**，不影响 Block 级别的 CSS margin |

### 5.5 排版引擎 (LayoutEngine)

**文件**：`include/typesetting/layout.h`, `src/layout.cpp`

**职责**：将文档块 + 计算样式 + 页面尺寸转换为精确定位的排版结果。

#### 排版流程

```mermaid
flowchart TD
    A["输入: Chapter, BlockComputedStyle[], PageSize"] --> B["初始化页面状态"]

    B --> C["遍历每个 Block"]
    C --> D{Block 类型?}

    D -- "hidden" --> E["跳过"]
    D -- "HorizontalRule" --> F["生成 Decoration<br/>计算居中位置"]
    D -- "Image" --> G["预留高度"]
    D -- "文本块" --> H["layoutBlockLines()<br/>多字体行内排版"]

    H --> I["遍历每个 InlineElement"]
    I --> J["确定行内字体<br/>（Bold/Italic/Code/Link）"]
    J --> K["measureText() 测量宽度"]
    K --> L{适合当前行?}
    L -- 是 --> M["创建 TextRun<br/>添加到当前行"]
    L -- 否 --> N["findLineBreak() 断行"]
    N --> O["完成当前行<br/>开始新行"]

    O --> P["applyAlignment()<br/>应用对齐方式"]
    P --> Q{页面剩余空间足够?}
    Q -- 是 --> R["放置行到当前页"]
    Q -- 否 --> S["startNewPage()<br/>开始新页"]
```

#### 行内排版算法 (layoutBlockLines)

核心行内排版逻辑：

1. **逐 InlineElement 处理**：不再拼接全文，而是按 InlineElement 逐段处理
2. **字体选择**：根据 InlineType（Bold→FontWeight::Bold, Italic→FontStyle::Italic, Code→monospace）确定字体
3. **宽度测量**：使用 `platform_->measureText()` 测量每段文本
4. **断行**：当累积宽度超过可用宽度时，使用 `platform_->findLineBreak()` 查找断行位置
5. **首行缩进**：第一行 `lineX` 初始值为 `textIndent`，`effectiveWidth` 减去缩进量
6. **行首空格跳过**：新行开始时跳过前导空格
7. **强制推进**：当完全无法容纳时，至少推进一个 UTF-8 字符（防止无限循环）
8. **基线对齐**：同一行内不同字体的 TextRun 按最大 ascent 值对齐基线

#### 文本对齐 (applyAlignment)

| 对齐方式 | 实现 |
|---------|------|
| Left | 默认，不做处理 |
| Center | 所有 TextRun 的 x 坐标加上 `extraSpace / 2` |
| Right | 所有 TextRun 的 x 坐标加上 `extraSpace` |
| Justified | 调用 `justifyLine()`（段落最后一行除外） |

#### 两端对齐算法 (justifyLine)

```
1. 统计行内所有 TextRun 中的空格总数 spaceCount
2. 计算 extraPerSpace = (contentWidth - lineWidth) / spaceCount
3. 从第一个 run 开始，逐个 run 重新计算：
   - run.x = xCursor
   - run.width += spacesInRun * extraPerSpace
   - xCursor += run.width
4. 设置 line.width = contentWidth
```

#### 分页算法

- 维护 `cursorY` 追踪当前页面已使用的垂直空间
- 当 `cursorY + lineHeight > contentHeight` 且当前页不为空时，触发分页
- `startNewPage()` 保存当前页，创建新页，重置 `cursorY`
- 块级元素的 `marginTop` 和 `marginBottom` / `paragraphSpacingAfter` 参与垂直空间计算

#### 特殊 Block 处理

| Block 类型 | 处理方式 |
|-----------|---------|
| HorizontalRule | 根据 `hrStyle` 计算 margin、border 宽度、居中位置，生成 `Decoration` 对象 |
| Image | 预留 `contentWidth * 0.6` 高度 |
| Hidden (display:none) | 完全跳过，不参与排版 |

### 5.6 引擎门面 (Engine)

**文件**：`include/typesetting/engine.h`, `src/engine.cpp`

**职责**：提供统一的排版 API，协调各模块的调用顺序。

#### 公开接口

| 方法 | 说明 |
|------|------|
| `layoutHTML(html, chapterId, style, pageSize)` | 基础排版：HTML 解析 → 默认样式 → 排版 |
| `layoutHTML(html, css, chapterId, style, pageSize)` | CSS 排版：HTML 解析 + CSS 解析 → 样式计算 → 排版 |
| `layoutBlocks(blocks, chapterId, style, pageSize)` | 直接排版预解析的 Block 列表 |
| `relayout(style, pageSize)` | 使用缓存的 Block 和 CSS 重新排版（字体大小变更场景） |
| `platform()` | 获取平台适配器实例 |

#### 缓存机制

Engine 缓存上次排版的中间结果，支持 `relayout()` 时快速重排：

| 缓存字段 | 用途 |
|---------|------|
| `lastBlocks_` | 上次解析的 Block 列表 |
| `lastChapterId_` | 上次排版的章节 ID |
| `lastStylesheet_` | 上次解析的 CSS 样式表 |
| `hasStylesheet_` | 是否有 CSS 样式表 |
| `lastStyles_` | 上次计算的 BlockComputedStyle 列表 |

`relayout()` 逻辑：

```
if (有缓存的 stylesheet) {
    重新解析样式（因为 userStyle 可能变了）
    使用 BlockComputedStyle 重排
} else {
    使用 Style 直接重排
}
```

#### 辅助模块：LineBreaker

**文件**：`src/linebreaker.cpp`

**职责**：提供行断裂相关的工具函数（当前作为备用模块保留）。

| 函数 | 说明 |
|------|------|
| `utf8CharLen()` | 返回 UTF-8 字符的字节长度 |
| `charCountToByteOffset()` | UTF-16 字符计数 → UTF-8 字节偏移转换 |
| `findBreakPoints()` | 查找文本中所有候选断行位置（空格、连字符） |
| `breakGreedy()` | 贪心断行算法 |

---

## 6. 平台绑定层

### 6.1 iOS 绑定 (TypesettingBridge)

**文件**：`bindings/swift/TypesettingBridge.h`, `bindings/swift/TypesettingBridge.mm`

**职责**：将 C++ 排版引擎封装为 Objective-C 接口，供 iOS/macOS 应用调用。

#### 平台适配器实现

`CoreTextAdapter` 实现 `PlatformAdapter` 接口，使用 iOS CoreText 框架：

| PlatformAdapter 方法 | CoreText 实现 |
|---------------------|-------------|
| `resolveFontMetrics()` | `CTFontCreateWithName` → `CTFontGetAscent/Descent/Leading/XHeight/CapHeight` |
| `measureText()` | `CTLineCreateWithAttributedString` → `CTLineGetTypographicBounds` |
| `findLineBreak()` | `CTTypesetterCreateWithAttributedString` → `CTTypesetterSuggestLineBreak` |
| `supportsHyphenation()` | `CFLocaleCopyCurrent` → 语言代码匹配 |
| `findHyphenationPoints()` | 返回空列表（TODO: 使用 `CFStringGetHyphenationLocationBeforeIndex`） |

#### 数据类型映射

| C++ 类型 | Objective-C 类型 |
|---------|-----------------|
| `LayoutResult` | `TSLayoutResult` |
| `Page` | `TSPage` |
| `Line` | `TSLine` |
| `TextRun` | `TSTextRun` |
| `Decoration` | `TSDecoration` |
| `Style` | `TSStyle` |

#### API

```objc
@interface TypesettingBridge : NSObject
- (TSLayoutResult *)layoutHTML:(NSString *)html
                     chapterId:(NSString *)chapterId
                         style:(TSStyle *)style
                     pageWidth:(CGFloat)pageWidth
                    pageHeight:(CGFloat)pageHeight;

- (TSLayoutResult *)layoutHTML:(NSString *)html
                           css:(NSString *)css
                     chapterId:(NSString *)chapterId
                         style:(TSStyle *)style
                     pageWidth:(CGFloat)pageWidth
                    pageHeight:(CGFloat)pageHeight;

- (TSLayoutResult *)relayoutWithStyle:(TSStyle *)style
                            pageWidth:(CGFloat)pageWidth
                           pageHeight:(CGFloat)pageHeight;
@end
```

### 6.2 Android 绑定 (TypesettingJNI)

**文件**：`bindings/jni/TypesettingJNI.h`, `bindings/jni/TypesettingJNI.cpp`

**职责**：通过 JNI 将 C++ 排版引擎暴露给 Android/Kotlin 应用。

#### 平台适配器实现

`AndroidPlatformAdapter` 实现 `PlatformAdapter` 接口，通过 JNI 回调 Java 层的 `MeasureHelper`：

| PlatformAdapter 方法 | JNI 实现 |
|---------------------|---------|
| `resolveFontMetrics()` | JNI 调用 `measureHelper.getFontMetrics(family, size, weight, isItalic)` |
| `measureText()` | JNI 调用 `measureHelper.measureText(text, family, size, weight, isItalic)` |
| `findLineBreak()` | JNI 调用 `measureHelper.findLineBreak(...)` + UTF-16↔UTF-8 转换 |
| `supportsHyphenation()` | 返回 false（TODO） |
| `findHyphenationPoints()` | 返回空列表（TODO） |

#### UTF-8 安全处理

JNI 层包含 `safeNewStringUTF()` 函数，在创建 JNI 字符串前验证 UTF-8 编码有效性，防止在多字节字符中间截断导致的崩溃。

#### JNI 函数

| JNI 函数 | 说明 |
|---------|------|
| `nativeCreate(measureHelper)` | 创建 Engine 实例，持有 AndroidPlatformAdapter |
| `nativeDestroy(ptr)` | 释放 Engine 实例 |
| `nativeLayoutHTML(ptr, html, css, chapterId, style, pageWidth, pageHeight)` | 排版入口 |
| `nativeRelayout(ptr, style, pageWidth, pageHeight)` | 重新排版 |

#### 内存管理

- `AndroidPlatformAdapter` 使用 `GlobalRef` 持有 `measureHelper` 对象引用
- `nativeCreate` 返回 `jlong` 指针，`nativeDestroy` 释放
- 嵌套循环中使用 `DeleteLocalRef` 避免局部引用表溢出

---

## 7. 数据流

### 7.1 完整排版流程

```mermaid
sequenceDiagram
    participant App as iOS/Android App
    participant Bridge as TypesettingBridge / JNI
    participant Engine
    participant CSSParser as CSS 解析器
    participant HTMLParser as HTML 解析器
    participant SR as 样式解析器
    participant LE as 排版引擎
    participant PA as PlatformAdapter

    App->>Bridge: layoutHTML(html, css, style, pageSize)
    Bridge->>Engine: layoutHTML(html, css, chapterId, style, pageSize)

    Engine->>CSSParser: CSSStylesheet::parse(css)
    CSSParser-->>Engine: CSSStylesheet

    Engine->>HTMLParser: parseHTML(html)
    HTMLParser-->>Engine: vector<Block>

    Engine->>SR: resolve(blocks, userStyle)
    SR-->>Engine: vector<BlockComputedStyle>

    Engine->>LE: layoutChapter(chapter, styles, pageSize)

    loop 每个 Block
        LE->>LE: 检查 hidden → 跳过
        LE->>LE: 应用 margin / textIndent
        loop 每个 InlineElement
            LE->>PA: measureText(text, font)
            PA-->>LE: TextMeasurement
            LE->>PA: findLineBreak(text, font, maxWidth)
            PA-->>LE: breakPos
        end
        LE->>LE: 生成 Line + TextRun[]
        LE->>LE: applyAlignment / justifyLine
        LE->>LE: 分页检查
    end

    LE-->>Engine: LayoutResult
    Engine-->>Bridge: LayoutResult
    Bridge-->>App: TSLayoutResult / Java Object
```

### 7.2 重新排版流程

```mermaid
sequenceDiagram
    participant App
    participant Engine
    participant SR as 样式解析器
    participant LE as 排版引擎

    App->>Engine: relayout(newStyle, newPageSize)

    alt 有缓存的 CSS
        Engine->>SR: resolve(cachedBlocks, newStyle)
        SR-->>Engine: vector<BlockComputedStyle>
        Engine->>LE: layoutChapter(cachedChapter, newStyles, newPageSize)
    else 无 CSS
        Engine->>LE: layoutChapter(cachedChapter, newStyle, newPageSize)
    end

    LE-->>Engine: LayoutResult
    Engine-->>App: LayoutResult
```

---

## 8. 关键算法

### 8.1 多字体行内排版

同一行可包含多种字体的 TextRun（如正文中嵌入粗体、斜体、代码片段）。算法要点：

```
对于 Block 中的每个 InlineElement:
  1. 根据 InlineType 确定 FontDescriptor:
     - Text → block 基础字体
     - Bold → weight=Bold
     - Italic → style=Italic
     - BoldItalic → weight=Bold + style=Italic
     - Code → family="monospace", size=0.9x
     - Link → 标记 isLink=true

  2. 获取该字体的 FontMetrics, 更新行内最大 ascent/descent

  3. 处理文本:
     while (remaining 不为空):
       - 跳过行首空格
       - 测量 remaining 宽度
       - 若适合 → 创建 TextRun, 推进 lineX
       - 若不适合 → findLineBreak() 断行
         - 断行位置 > 0 → 创建 TextRun + 完成当前行
         - 断行位置 == 0 → 行为空时强制推进; 否则完成行后重试
```

### 8.2 UTF-8 安全处理

引擎在多处确保 UTF-8 编码的正确处理：

| 场景 | 处理方式 |
|------|---------|
| 强制断行 | 根据 lead byte (0x80-0xF8) 计算字符字节数，确保不在字符中间断开 |
| JNI 字符串 | `safeNewStringUTF()` 验证编码有效性后再传递给 JNI |
| UTF-16↔UTF-8 | `charCountToByteOffset()` 正确处理 surrogate pair（4 字节 UTF-8 = 2 UTF-16 字符） |

### 8.3 CSS Specificity 排序

样式计算时，匹配到同一 Block 的多条 CSS 规则按 specificity 升序排列，低优先级规则先应用，高优先级规则后覆盖，确保 CSS 级联规则的正确性。

---

## 9. 样式层叠与优先级

排版引擎实现三层样式优先级，从低到高：

```mermaid
flowchart LR
    A["层级 1<br/>引擎默认值<br/>（BlockType → 样式）"] --> B["层级 2<br/>CSS 规则<br/>（按 specificity 排序）"]
    B --> C["层级 3<br/>用户偏好<br/>（字体/字号/间距）"]
```

| 层级 | 来源 | 示例 |
|------|------|------|
| 1 | 引擎默认值 | Paragraph → textIndent=1em, alignment=Justified |
| 2 | CSS 规则 | `h2 + p { text-indent: 0 }` 覆盖段落默认缩进 |
| 3 | 用户偏好 | 用户设置 font.size=20 覆盖基础字号 |

**设计要点**：用户偏好不应完全覆盖所有 CSS 属性。例如：
- 标题应保持居中（即使用户设置 alignment=Justified）
- CSS 明确设置 `hyphens: none` 的标题不应被用户的 hyphenation=true 覆盖
- `textIndent` 由 CSS 控制，用户不参与覆盖

---

## 10. SE CSS 支持

引擎专门针对 [Standard Ebooks](https://standardebooks.org/) 格式的 CSS 进行了优化。以下是需要支持的关键 CSS 规则：

| SE CSS 规则 | 引擎处理方式 |
|------------|-------------|
| `b, strong { font-variant: small-caps; font-weight: normal; }` | Bold InlineType → smallCaps=true, weight=Regular |
| `h1-h6 { font-variant: small-caps; text-align: center; }` | 标题 Block → smallCaps + Center |
| `p { text-indent: 1em; }` | 段落默认缩进 = 1 × 用户字号 |
| `h2 + p { text-indent: 0; }` | 通过 previousSiblingTag=="h2" 匹配 |
| `hr + p { text-indent: 0; }` | 通过 previousSiblingTag=="hr" 匹配 |
| `p:first-child { text-indent: 0; }` | 通过 isFirstChild 匹配 |
| `.class p { font-style: italic; }` | 后代选择器匹配 parentClassName |
| `section[epub\|type~="dedication"] p { ... }` | 属性选择器匹配 parentEpubType |
| `blockquote { margin: 1em 2.5em; }` | em → px 转换后应用 margin |
| `hr { border-top: 1px solid; width: 25%; }` | 生成 Decoration，居中绘制 |
| `display: none` | BlockComputedStyle.hidden = true |
| `hyphens: none` | 标题等元素禁用断词 |

**@-rules 处理**：`@media`, `@supports`, `@namespace`, `@font-face` 等 @-rules 在解析阶段被跳过，不参与样式计算。

---

## 11. 构建与测试

### 11.1 构建配置

项目使用 CMake 构建，提供三个预设：

| 预设 | 用途 | 测试 | 优化 |
|------|------|------|------|
| `debug` | 开发调试 | ✅ 开启 | Debug |
| `release` | 发布构建 | ❌ 关闭 | Release |
| `ci` | 持续集成 | ✅ 开启 | Release |

构建命令：

```bash
# Debug 构建（含测试）
cmake --preset debug
cmake --build build/debug

# Release 构建
cmake --preset release
cmake --build build/release

# 或手动指定
cmake -B build -DCMAKE_BUILD_TYPE=Release -DTYPESETTING_BUILD_TESTS=ON
cmake --build build
```

### 11.2 测试策略

项目使用 GoogleTest v1.14.0 框架，通过 CMake FetchContent 自动下载。

#### 测试文件

| 文件 | 覆盖范围 | 测试数量 |
|------|---------|---------|
| `test_css.cpp` | CSS 选择器解析、属性解析、specificity、注释处理 | 14 个测试 |
| `test_layout.cpp` | HTML 解析、分页排版、重排版、文本缩进、对齐、多字体 | 18 个测试 |
| `test_style_resolver.cpp` | 默认样式、CSS 覆盖、用户偏好覆盖、选择器匹配 | 20+ 个测试 |

#### 测试用 Mock

`MockPlatformAdapter` 使用固定宽度字体度量（每字符 8px），确保排版结果可预测：

```cpp
class MockPlatformAdapter : public PlatformAdapter {
    FontMetrics resolveFontMetrics(const FontDescriptor& desc) override {
        return {desc.size * 0.8f,  // ascent
                desc.size * 0.2f,  // descent
                desc.size * 0.1f,  // leading
                desc.size * 0.5f,  // xHeight
                desc.size * 0.7f}; // capHeight
    }
    TextMeasurement measureText(const std::string& text, const FontDescriptor& font) override {
        return {static_cast<float>(text.size()) * font.size * 0.5f, font.size};
    }
    size_t findLineBreak(const std::string& text, const FontDescriptor& font, float maxWidth) override {
        float charWidth = font.size * 0.5f;
        // 寻找不超过 maxWidth 的最后一个空格位置
        ...
    }
};
```

运行测试：

```bash
cmake -B build -DTYPESETTING_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## 12. 文件清单

### 公共头文件 (`include/typesetting/`)

| 文件 | 模块 | 主要定义 |
|------|------|---------|
| `platform.h` | 平台抽象 | `PlatformAdapter`, `FontDescriptor`, `FontMetrics`, `TextMeasurement`, `FontWeight`, `FontStyle` |
| `document.h` | 文档模型 | `Block`, `InlineElement`, `Chapter`, `Document`, `BlockType`, `InlineType`, `parseHTML()` |
| `css.h` | CSS 解析 | `CSSStylesheet`, `CSSRule`, `CSSSelector`, `CSSProperties`, `SelectorType`, `FontVariant` |
| `style.h` | 样式模型 | `Style`, `BlockComputedStyle`, `HRStyle`, `TextAlignment` |
| `style_resolver.h` | 样式计算 | `StyleResolver` |
| `layout.h` | 排版引擎 | `LayoutEngine`, `PageSize` |
| `page.h` | 输出模型 | `Page`, `Line`, `TextRun`, `Decoration`, `LayoutResult`, `DecorationType` |
| `engine.h` | 引擎门面 | `Engine` |

### 实现文件 (`src/`)

| 文件 | 职责 | 代码量 |
|------|------|-------|
| `document.cpp` | HTML 解析实现 | ~400 行 |
| `css.cpp` | CSS 解析实现 | ~550 行 |
| `style_resolver.cpp` | 样式计算实现 | ~370 行 |
| `layout.cpp` | 排版引擎实现（含 Pimpl） | ~500 行 |
| `engine.cpp` | 引擎门面实现 | ~70 行 |
| `linebreaker.cpp` | 断行工具函数（备用） | ~110 行 |
| `style.cpp` | 样式扩展点（占位） | ~10 行 |
| `page.cpp` | 页面扩展点（占位） | ~10 行 |

### 绑定层

| 文件 | 平台 | 职责 |
|------|------|------|
| `bindings/swift/TypesettingBridge.h` | iOS | Objective-C 接口定义 |
| `bindings/swift/TypesettingBridge.mm` | iOS | CoreText 适配器 + 桥接实现 |
| `bindings/jni/TypesettingJNI.h` | Android | JNI 函数声明 |
| `bindings/jni/TypesettingJNI.cpp` | Android | Android 适配器 + JNI 实现 |

### 测试文件 (`tests/`)

| 文件 | 覆盖模块 |
|------|---------|
| `test_css.cpp` | CSS 解析 |
| `test_layout.cpp` | HTML 解析 + 排版 |
| `test_style_resolver.cpp` | 样式计算 |
| `CMakeLists.txt` | 测试构建配置 |

---

## 13. 向后兼容性

| 场景 | 保证 |
|------|------|
| 无 CSS 调用 | `layoutHTML(html, chapterId, style, pageSize)` 签名保留不变，行为等同原始版本 |
| Block 新增字段 | 所有新增字段有默认值，现有构造方式不受影响 |
| TextRun 新增字段 | `smallCaps=false`, `isLink=false`, `href=""`——现有渲染代码可忽略 |
| Page 新增 decorations | 默认为空向量，现有渲染代码不受影响 |
| LayoutEngine 双重载 | `layoutChapter(Style)` 和 `layoutChapter(BlockComputedStyle[])` 并存 |
