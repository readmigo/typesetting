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

#ifdef __cplusplus
}
#endif
