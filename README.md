# Typesetting

A high-performance C++ typesetting engine with CSS styling support, designed for e-book rendering on iOS and Android.

## Features

- **HTML Parsing** - Parses structured HTML into block-level and inline elements
- **CSS Styling** - Full SE (Standard Ebooks) CSS support with selector matching and style resolution
- **Page Layout** - Precise text layout with multi-font inline rendering, justification, and hyphenation
- **Cross-Platform** - Platform-agnostic core with native bindings for iOS (CoreText) and Android (JNI)
- **Style Cascading** - Three-tier style priority: engine defaults → CSS rules → user preferences

## Architecture

```mermaid
graph TB
    subgraph Input
        HTML["HTML"]
        CSS["CSS"]
        UserStyle["User Style"]
        PS["Page Size"]
    end

    subgraph Core["Core Engine (C++17)"]
        Parser["HTML Parser"]
        CSSParser["CSS Parser"]
        SR["Style Resolver"]
        LE["Layout Engine"]
    end

    subgraph Platform["Platform Adapter"]
        CT["CoreText (iOS)"]
        SK["Skia/HarfBuzz (Android)"]
    end

    subgraph Output
        Pages["Pages → Lines → TextRuns"]
    end

    HTML --> Parser
    CSS --> CSSParser
    Parser --> SR
    CSSParser --> SR
    UserStyle --> SR
    SR --> LE
    PS --> LE
    Platform --> LE
    LE --> Pages
```

## Project Structure

```
typesetting/
├── include/typesetting/   # Public headers
│   ├── engine.h           # Main entry point (Engine class)
│   ├── document.h         # HTML parsing (Block, InlineElement)
│   ├── css.h              # CSS parsing (CSSStylesheet, CSSRule)
│   ├── style.h            # Style types (Style, BlockComputedStyle)
│   ├── style_resolver.h   # CSS → computed style resolution
│   ├── layout.h           # Layout engine (LayoutEngine)
│   ├── page.h             # Output types (Page, Line, TextRun)
│   └── platform.h         # Platform abstraction (PlatformAdapter)
├── src/                   # Implementation
├── tests/                 # GoogleTest unit tests
├── bindings/
│   ├── swift/             # iOS binding (TypesettingBridge)
│   └── jni/               # Android binding (TypesettingJNI)
└── CMakeLists.txt
```

## Requirements

- CMake 3.20+
- C++17 compiler (Clang 10+ / GCC 9+ / MSVC 19.14+)
- macOS: CoreText, CoreFoundation, CoreGraphics frameworks

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Test

```bash
cmake -B build -DTYPESETTING_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Usage

```mermaid
sequenceDiagram
    participant App
    participant Engine
    participant LayoutEngine
    participant PlatformAdapter

    App->>Engine: layoutHTML(html, css, style, pageSize)
    Engine->>Engine: Parse HTML → Blocks
    Engine->>Engine: Parse CSS → Stylesheet
    Engine->>Engine: Resolve styles
    Engine->>LayoutEngine: layoutChapter(chapter, styles, pageSize)
    LayoutEngine->>PlatformAdapter: measureText() / findLineBreak()
    PlatformAdapter-->>LayoutEngine: measurements
    LayoutEngine-->>Engine: LayoutResult (Pages)
    Engine-->>App: Pages → Lines → TextRuns
```

## License

MIT License. See [LICENSE](LICENSE) for details.
