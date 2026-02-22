#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// Engine lifecycle
JNIEXPORT jlong JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeCreate(
    JNIEnv *env, jobject thiz, jobject measureHelper);

JNIEXPORT void JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeDestroy(JNIEnv *env, jobject thiz, jlong ptr);

// Layout with CSS
JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeLayoutHTML(
    JNIEnv *env, jobject thiz, jlong ptr,
    jstring html, jstring css, jstring chapterId,
    jobject style, jfloat pageWidth, jfloat pageHeight);

JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeRelayout(
    JNIEnv *env, jobject thiz, jlong ptr,
    jobject style, jfloat pageWidth, jfloat pageHeight);

// Chapter title & cover
JNIEXPORT void JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeSetChapterTitle(
    JNIEnv *env, jobject thiz, jlong ptr, jstring title);

JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeLayoutCover(
    JNIEnv *env, jobject thiz, jlong ptr,
    jstring imageSrc, jfloat pageWidth, jfloat pageHeight);

// Interaction queries
JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeHitTest(
    JNIEnv *env, jobject thiz, jlong ptr,
    jint pageIndex, jfloat x, jfloat y);

JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeWordAtPoint(
    JNIEnv *env, jobject thiz, jlong ptr,
    jint pageIndex, jfloat x, jfloat y);

JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeGetSentences(
    JNIEnv *env, jobject thiz, jlong ptr, jint pageIndex);

JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeGetAllSentences(
    JNIEnv *env, jobject thiz, jlong ptr);

JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeGetRectsForRange(
    JNIEnv *env, jobject thiz, jlong ptr,
    jint pageIndex, jint blockIndex, jint charOffset, jint charLength);

JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeGetBlockRect(
    JNIEnv *env, jobject thiz, jlong ptr,
    jint pageIndex, jint blockIndex);

JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeHitTestImage(
    JNIEnv *env, jobject thiz, jlong ptr,
    jint pageIndex, jfloat x, jfloat y);

JNIEXPORT jobject JNICALL
Java_com_readmigo_typesetting_TypesettingEngine_nativeGetPageInfo(
    JNIEnv *env, jobject thiz, jlong ptr, jint pageIndex);

#ifdef __cplusplus
}
#endif
