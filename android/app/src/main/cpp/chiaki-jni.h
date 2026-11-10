// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_JNI_H
#define CHIAKI_JNI_H

#include <stdbool.h>

#define JNI_VERSION JNI_VERSION_1_6

#define BASE_PACKAGE "com/metallic/chiaki/lib"
#define JNI_FCN(name) Java_com_metallic_chiaki_lib_ChiakiNative_##name

#define E (*env)

JNIEnv *attach_thread_jni();
JNIEnv *attach_thread_jni_ex(bool *out_needs_detach);
void detach_thread_jni_ex(bool needs_detach);
jobject jnistr_from_ascii(JNIEnv *env, const char *str);

extern JavaVM *global_vm;

#endif