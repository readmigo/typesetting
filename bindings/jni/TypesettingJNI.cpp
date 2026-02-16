#include "TypesettingJNI.h"
#include "typesetting/engine.h"
#include "typesetting/platform.h"

#include <android/log.h>
#include <string>
#include <memory>

#define LOG_TAG "TypesettingJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/// Create a jstring safely from a UTF-8 std::string.
/// NewStringUTF requires valid Modified UTF-8; if the C++ engine produces
/// a string truncated mid-character (e.g., 0xe2 without continuation bytes),
/// this trims to the last valid character boundary.
static jstring safeNewStringUTF(JNIEnv *env, const char *str) {
    if (!str || str[0] == '\0') return env->NewStringUTF("");

    // Validate UTF-8 and find safe length
    size_t len = 0;
    size_t safeLen = 0;
    const unsigned char *p = reinterpret_cast<const unsigned char *>(str);
    while (p[len]) {
        unsigned char c = p[len];
        size_t charBytes;
        if (c < 0x80) {
            charBytes = 1;
        } else if ((c & 0xE0) == 0xC0) {
            charBytes = 2;
        } else if ((c & 0xF0) == 0xE0) {
            charBytes = 3;
        } else if ((c & 0xF8) == 0xF0) {
            charBytes = 4;
        } else {
            // Invalid lead byte, skip
            len++;
            continue;
        }

        // Check that all continuation bytes are present
        bool valid = true;
        for (size_t i = 1; i < charBytes; i++) {
            if (p[len + i] == 0 || (p[len + i] & 0xC0) != 0x80) {
                valid = false;
                break;
            }
        }

        if (valid) {
            len += charBytes;
            safeLen = len;
        } else {
            break; // truncated multi-byte sequence at end
        }
    }

    if (safeLen == 0) return env->NewStringUTF("");

    // Use safe length
    std::string safe(str, safeLen);
    return env->NewStringUTF(safe.c_str());
}

// MARK: - Android Platform Adapter (Skia/HarfBuzz)

class AndroidPlatformAdapter : public typesetting::PlatformAdapter {
public:
    AndroidPlatformAdapter(JNIEnv *env, jobject measureHelper)
        : env_(env), measureHelper_(env->NewGlobalRef(measureHelper)) {
        jclass cls = env->GetObjectClass(measureHelper_);
        measureTextMethod_ = env->GetMethodID(cls, "measureText",
            "(Ljava/lang/String;Ljava/lang/String;FI)F");
        findLineBreakMethod_ = env->GetMethodID(cls, "findLineBreak",
            "(Ljava/lang/String;Ljava/lang/String;FIF)I");
        getFontMetricsMethod_ = env->GetMethodID(cls, "getFontMetrics",
            "(Ljava/lang/String;FI)[F");
    }

    ~AndroidPlatformAdapter() {
        if (env_ && measureHelper_) {
            env_->DeleteGlobalRef(measureHelper_);
        }
    }

    typesetting::FontMetrics resolveFontMetrics(const typesetting::FontDescriptor& desc) override {
        jstring family = safeNewStringUTF(env_, desc.family.c_str());
        jfloatArray result = (jfloatArray)env_->CallObjectMethod(
            measureHelper_, getFontMetricsMethod_,
            family, desc.size, static_cast<int>(desc.weight));
        env_->DeleteLocalRef(family);

        typesetting::FontMetrics metrics;
        if (result) {
            jfloat *data = env_->GetFloatArrayElements(result, nullptr);
            metrics.ascent = data[0];
            metrics.descent = data[1];
            metrics.leading = data[2];
            env_->ReleaseFloatArrayElements(result, data, 0);
            env_->DeleteLocalRef(result);
        }
        return metrics;
    }

    typesetting::TextMeasurement measureText(const std::string& text,
                                              const typesetting::FontDescriptor& font) override {
        jstring jtext = safeNewStringUTF(env_, text.c_str());
        jstring jfamily = safeNewStringUTF(env_, font.family.c_str());
        float width = env_->CallFloatMethod(
            measureHelper_, measureTextMethod_,
            jtext, jfamily, font.size, static_cast<int>(font.weight));
        env_->DeleteLocalRef(jtext);
        env_->DeleteLocalRef(jfamily);

        auto metrics = resolveFontMetrics(font);
        return {width, metrics.ascent + metrics.descent};
    }

    size_t findLineBreak(const std::string& text,
                         const typesetting::FontDescriptor& font,
                         float maxWidth) override {
        jstring jtext = safeNewStringUTF(env_, text.c_str());
        jstring jfamily = safeNewStringUTF(env_, font.family.c_str());
        int charCount = env_->CallIntMethod(
            measureHelper_, findLineBreakMethod_,
            jtext, jfamily, font.size, static_cast<int>(font.weight), maxWidth);
        env_->DeleteLocalRef(jtext);
        env_->DeleteLocalRef(jfamily);

        // Convert Java character count (UTF-16) to C++ byte offset (UTF-8)
        size_t byteOffset = 0;
        int chars = 0;
        const unsigned char *p = reinterpret_cast<const unsigned char *>(text.c_str());
        while (byteOffset < text.size() && chars < charCount) {
            unsigned char c = p[byteOffset];
            size_t len;
            if (c < 0x80) len = 1;
            else if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) { len = 4; chars++; } // surrogate pair = 2 UTF-16 chars
            else len = 1;
            if (byteOffset + len > text.size()) break;
            byteOffset += len;
            chars++;
        }
        return byteOffset;
    }

    bool supportsHyphenation(const std::string& locale) override {
        return true; // Android supports hyphenation via Minikin
    }

    std::vector<size_t> findHyphenationPoints(const std::string& word,
                                               const std::string& locale) override {
        // TODO: Implement via Android Hyphenator API
        return {};
    }

private:
    JNIEnv *env_;
    jobject measureHelper_;
    jmethodID measureTextMethod_;
    jmethodID findLineBreakMethod_;
    jmethodID getFontMetricsMethod_;
};

// MARK: - JNI Functions

struct EngineHandle {
    std::unique_ptr<typesetting::Engine> engine;
    std::shared_ptr<AndroidPlatformAdapter> adapter;
};

static std::string jstringToStd(JNIEnv *env, jstring jstr) {
    if (!jstr) return "";
    const char *chars = env->GetStringUTFChars(jstr, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return result;
}

static typesetting::Style extractStyle(JNIEnv *env, jobject style) {
    typesetting::Style s;
    jclass cls = env->GetObjectClass(style);

    jfieldID fontFamilyField = env->GetFieldID(cls, "fontFamily", "Ljava/lang/String;");
    jfieldID fontSizeField = env->GetFieldID(cls, "fontSize", "F");
    jfieldID fontWeightField = env->GetFieldID(cls, "fontWeight", "I");
    jfieldID lineSpacingField = env->GetFieldID(cls, "lineSpacingMultiplier", "F");
    jfieldID letterSpacingField = env->GetFieldID(cls, "letterSpacing", "F");
    jfieldID wordSpacingField = env->GetFieldID(cls, "wordSpacing", "F");
    jfieldID paragraphSpacingField = env->GetFieldID(cls, "paragraphSpacing", "F");
    jfieldID textAlignmentField = env->GetFieldID(cls, "textAlignment", "I");
    jfieldID hyphenationField = env->GetFieldID(cls, "hyphenation", "Z");
    jfieldID marginTopField = env->GetFieldID(cls, "marginTop", "F");
    jfieldID marginBottomField = env->GetFieldID(cls, "marginBottom", "F");
    jfieldID marginLeftField = env->GetFieldID(cls, "marginLeft", "F");
    jfieldID marginRightField = env->GetFieldID(cls, "marginRight", "F");

    jstring fontFamily = (jstring)env->GetObjectField(style, fontFamilyField);
    s.font.family = jstringToStd(env, fontFamily);
    if (fontFamily) env->DeleteLocalRef(fontFamily);

    s.font.size = env->GetFloatField(style, fontSizeField);
    s.font.weight = static_cast<typesetting::FontWeight>(env->GetIntField(style, fontWeightField));
    s.lineSpacingMultiplier = env->GetFloatField(style, lineSpacingField);
    s.letterSpacing = env->GetFloatField(style, letterSpacingField);
    s.wordSpacing = env->GetFloatField(style, wordSpacingField);
    s.paragraphSpacing = env->GetFloatField(style, paragraphSpacingField);
    s.alignment = static_cast<typesetting::TextAlignment>(env->GetIntField(style, textAlignmentField));
    s.hyphenation = env->GetBooleanField(style, hyphenationField);
    s.marginTop = env->GetFloatField(style, marginTopField);
    s.marginBottom = env->GetFloatField(style, marginBottomField);
    s.marginLeft = env->GetFloatField(style, marginLeftField);
    s.marginRight = env->GetFloatField(style, marginRightField);

    env->DeleteLocalRef(cls);
    return s;
}

static jobject convertLayoutResult(JNIEnv *env, const typesetting::LayoutResult& result) {
    // Find Java classes
    jclass resultClass = env->FindClass("com/readmigo/typesetting/TSLayoutResult");
    jclass pageClass = env->FindClass("com/readmigo/typesetting/TSPage");
    jclass lineClass = env->FindClass("com/readmigo/typesetting/TSLine");
    jclass runClass = env->FindClass("com/readmigo/typesetting/TSTextRun");
    jclass decoClass = env->FindClass("com/readmigo/typesetting/TSDecoration");
    jclass arrayListClass = env->FindClass("java/util/ArrayList");

    // Get constructors
    jmethodID resultInit = env->GetMethodID(resultClass, "<init>", "()V");
    jmethodID pageInit = env->GetMethodID(pageClass, "<init>", "()V");
    jmethodID lineInit = env->GetMethodID(lineClass, "<init>", "()V");
    jmethodID runInit = env->GetMethodID(runClass, "<init>", "()V");
    jmethodID decoInit = env->GetMethodID(decoClass, "<init>", "()V");
    jmethodID arrayListInit = env->GetMethodID(arrayListClass, "<init>", "(I)V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

    // TSLayoutResult fields
    jfieldID resultChapterIdField = env->GetFieldID(resultClass, "chapterId", "Ljava/lang/String;");
    jfieldID resultPagesField = env->GetFieldID(resultClass, "pages", "Ljava/util/List;");
    jfieldID resultTotalBlocksField = env->GetFieldID(resultClass, "totalBlocks", "I");

    // TSPage fields
    jfieldID pageIndexField = env->GetFieldID(pageClass, "pageIndex", "I");
    jfieldID pageLinesField = env->GetFieldID(pageClass, "lines", "Ljava/util/List;");
    jfieldID pageWidthField = env->GetFieldID(pageClass, "pageWidth", "F");
    jfieldID pageHeightField = env->GetFieldID(pageClass, "pageHeight", "F");
    jfieldID pageFirstBlockField = env->GetFieldID(pageClass, "firstBlockIndex", "I");
    jfieldID pageLastBlockField = env->GetFieldID(pageClass, "lastBlockIndex", "I");
    jfieldID pageDecorationsField = env->GetFieldID(pageClass, "decorations", "Ljava/util/List;");

    // TSLine fields
    jfieldID lineRunsField = env->GetFieldID(lineClass, "runs", "Ljava/util/List;");
    jfieldID lineXField = env->GetFieldID(lineClass, "x", "F");
    jfieldID lineYField = env->GetFieldID(lineClass, "y", "F");
    jfieldID lineWidthField = env->GetFieldID(lineClass, "width", "F");
    jfieldID lineHeightField = env->GetFieldID(lineClass, "height", "F");
    jfieldID lineIsLastField = env->GetFieldID(lineClass, "isLastLineOfParagraph", "Z");

    // TSTextRun fields
    jfieldID runTextField = env->GetFieldID(runClass, "text", "Ljava/lang/String;");
    jfieldID runFontFamilyField = env->GetFieldID(runClass, "fontFamily", "Ljava/lang/String;");
    jfieldID runFontSizeField = env->GetFieldID(runClass, "fontSize", "F");
    jfieldID runFontWeightField = env->GetFieldID(runClass, "fontWeight", "I");
    jfieldID runIsItalicField = env->GetFieldID(runClass, "isItalic", "Z");
    jfieldID runXField = env->GetFieldID(runClass, "x", "F");
    jfieldID runYField = env->GetFieldID(runClass, "y", "F");
    jfieldID runWidthField = env->GetFieldID(runClass, "width", "F");
    jfieldID runBlockIndexField = env->GetFieldID(runClass, "blockIndex", "I");
    jfieldID runCharOffsetField = env->GetFieldID(runClass, "charOffset", "I");
    jfieldID runCharLengthField = env->GetFieldID(runClass, "charLength", "I");
    jfieldID runSmallCapsField = env->GetFieldID(runClass, "smallCaps", "Z");
    jfieldID runIsLinkField = env->GetFieldID(runClass, "isLink", "Z");
    jfieldID runHrefField = env->GetFieldID(runClass, "href", "Ljava/lang/String;");

    // TSDecoration fields
    jfieldID decoTypeField = env->GetFieldID(decoClass, "type", "I");
    jfieldID decoXField = env->GetFieldID(decoClass, "x", "F");
    jfieldID decoYField = env->GetFieldID(decoClass, "y", "F");
    jfieldID decoWidthField = env->GetFieldID(decoClass, "width", "F");
    jfieldID decoHeightField = env->GetFieldID(decoClass, "height", "F");

    // Build result object
    jobject jResult = env->NewObject(resultClass, resultInit);
    jstring jChapterId = safeNewStringUTF(env, result.chapterId.c_str());
    env->SetObjectField(jResult, resultChapterIdField, jChapterId);
    env->SetIntField(jResult, resultTotalBlocksField, result.totalBlocks);
    env->DeleteLocalRef(jChapterId);

    // Build pages list
    jobject pagesList = env->NewObject(arrayListClass, arrayListInit, (jint)result.pages.size());
    for (const auto& page : result.pages) {
        jobject jPage = env->NewObject(pageClass, pageInit);
        env->SetIntField(jPage, pageIndexField, page.pageIndex);
        env->SetFloatField(jPage, pageWidthField, page.width);
        env->SetFloatField(jPage, pageHeightField, page.height);
        env->SetIntField(jPage, pageFirstBlockField, page.firstBlockIndex);
        env->SetIntField(jPage, pageLastBlockField, page.lastBlockIndex);

        // Build lines list
        jobject linesList = env->NewObject(arrayListClass, arrayListInit, (jint)page.lines.size());
        for (const auto& line : page.lines) {
            jobject jLine = env->NewObject(lineClass, lineInit);
            env->SetFloatField(jLine, lineXField, line.x);
            env->SetFloatField(jLine, lineYField, line.y);
            env->SetFloatField(jLine, lineWidthField, line.width);
            env->SetFloatField(jLine, lineHeightField, line.height);
            env->SetBooleanField(jLine, lineIsLastField, line.isLastLineOfParagraph);

            // Build runs list
            jobject runsList = env->NewObject(arrayListClass, arrayListInit, (jint)line.runs.size());
            for (const auto& run : line.runs) {
                jobject jRun = env->NewObject(runClass, runInit);

                jstring jText = safeNewStringUTF(env, run.text.c_str());
                env->SetObjectField(jRun, runTextField, jText);
                env->DeleteLocalRef(jText);

                jstring jFontFamily = safeNewStringUTF(env, run.font.family.c_str());
                env->SetObjectField(jRun, runFontFamilyField, jFontFamily);
                env->DeleteLocalRef(jFontFamily);

                env->SetFloatField(jRun, runFontSizeField, run.font.size);
                env->SetIntField(jRun, runFontWeightField, static_cast<int>(run.font.weight));
                env->SetBooleanField(jRun, runIsItalicField,
                    run.font.style == typesetting::FontStyle::Italic);
                env->SetFloatField(jRun, runXField, run.x);
                env->SetFloatField(jRun, runYField, run.y);
                env->SetFloatField(jRun, runWidthField, run.width);
                env->SetIntField(jRun, runBlockIndexField, run.blockIndex);
                env->SetIntField(jRun, runCharOffsetField, run.charOffset);
                env->SetIntField(jRun, runCharLengthField, run.charLength);
                env->SetBooleanField(jRun, runSmallCapsField, run.smallCaps);
                env->SetBooleanField(jRun, runIsLinkField, run.isLink);

                jstring jHref = safeNewStringUTF(env, run.href.c_str());
                env->SetObjectField(jRun, runHrefField, jHref);
                env->DeleteLocalRef(jHref);

                env->CallBooleanMethod(runsList, arrayListAdd, jRun);
                env->DeleteLocalRef(jRun);
            }
            env->SetObjectField(jLine, lineRunsField, runsList);
            env->DeleteLocalRef(runsList);

            env->CallBooleanMethod(linesList, arrayListAdd, jLine);
            env->DeleteLocalRef(jLine);
        }
        env->SetObjectField(jPage, pageLinesField, linesList);
        env->DeleteLocalRef(linesList);

        // Build decorations list
        jobject decosList = env->NewObject(arrayListClass, arrayListInit, (jint)page.decorations.size());
        for (const auto& deco : page.decorations) {
            jobject jDeco = env->NewObject(decoClass, decoInit);
            env->SetIntField(jDeco, decoTypeField, static_cast<int>(deco.type));
            env->SetFloatField(jDeco, decoXField, deco.x);
            env->SetFloatField(jDeco, decoYField, deco.y);
            env->SetFloatField(jDeco, decoWidthField, deco.width);
            env->SetFloatField(jDeco, decoHeightField, deco.height);
            env->CallBooleanMethod(decosList, arrayListAdd, jDeco);
            env->DeleteLocalRef(jDeco);
        }
        env->SetObjectField(jPage, pageDecorationsField, decosList);
        env->DeleteLocalRef(decosList);

        env->CallBooleanMethod(pagesList, arrayListAdd, jPage);
        env->DeleteLocalRef(jPage);
    }
    env->SetObjectField(jResult, resultPagesField, pagesList);
    env->DeleteLocalRef(pagesList);

    // Clean up class refs
    env->DeleteLocalRef(resultClass);
    env->DeleteLocalRef(pageClass);
    env->DeleteLocalRef(lineClass);
    env->DeleteLocalRef(runClass);
    env->DeleteLocalRef(decoClass);
    env->DeleteLocalRef(arrayListClass);

    return jResult;
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeCreate(
    JNIEnv *env, jobject thiz, jobject measureHelper) {
    auto adapter = std::make_shared<AndroidPlatformAdapter>(env, measureHelper);
    auto handle = new EngineHandle();
    handle->adapter = adapter;
    handle->engine = std::make_unique<typesetting::Engine>(adapter);
    LOGI("TypesettingEngine created with platform adapter");
    return reinterpret_cast<jlong>(handle);
}

JNIEXPORT void JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeDestroy(JNIEnv *env, jobject thiz, jlong ptr) {
    auto handle = reinterpret_cast<EngineHandle*>(ptr);
    delete handle;
    LOGI("TypesettingEngine destroyed");
}

JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeLayoutHTML(
    JNIEnv *env, jobject thiz, jlong ptr,
    jstring html, jstring css, jstring chapterId,
    jobject style, jfloat pageWidth, jfloat pageHeight) {
    auto handle = reinterpret_cast<EngineHandle*>(ptr);
    if (!handle || !handle->engine) {
        LOGI("nativeLayoutHTML: invalid engine handle");
        return nullptr;
    }

    std::string htmlStr = jstringToStd(env, html);
    std::string cssStr = jstringToStd(env, css);
    std::string chapterIdStr = jstringToStd(env, chapterId);
    typesetting::Style s = extractStyle(env, style);
    typesetting::PageSize pageSize{pageWidth, pageHeight};

    typesetting::LayoutResult result;
    if (cssStr.empty()) {
        result = handle->engine->layoutHTML(htmlStr, chapterIdStr, s, pageSize);
    } else {
        result = handle->engine->layoutHTML(htmlStr, cssStr, chapterIdStr, s, pageSize);
    }

    LOGI("nativeLayoutHTML: %zu pages for chapter '%s'",
         result.pages.size(), chapterIdStr.c_str());

    return convertLayoutResult(env, result);
}

JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeRelayout(
    JNIEnv *env, jobject thiz, jlong ptr,
    jobject style, jfloat pageWidth, jfloat pageHeight) {
    auto handle = reinterpret_cast<EngineHandle*>(ptr);
    if (!handle || !handle->engine) {
        LOGI("nativeRelayout: invalid engine handle");
        return nullptr;
    }

    typesetting::Style s = extractStyle(env, style);
    typesetting::PageSize pageSize{pageWidth, pageHeight};

    typesetting::LayoutResult result = handle->engine->relayout(s, pageSize);

    LOGI("nativeRelayout: %zu pages", result.pages.size());

    return convertLayoutResult(env, result);
}

} // extern "C"
