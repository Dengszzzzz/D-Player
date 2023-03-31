//
// Created by Administrator on 2023/3/28.
//

#ifndef D_PLAYER_JNICALLBACKHELPER_H
#define D_PLAYER_JNICALLBACKHELPER_H

#include <jni.h>
#include "util.h"

class JNICallbackHelper {
private:
    JavaVM *vm = 0;
    JNIEnv *env = 0;
    jobject job;
    jmethodID jmd_prepared;
    jmethodID jmd_onError;
public:
    JNICallbackHelper(JavaVM *vm,JNIEnv *env,jobject job);

    ~JNICallbackHelper();  //为什么？

    void onPrepared(int thread_mode);
    void onError(int thread_mode,int error_code);
};


#endif //D_PLAYER_JNICALLBACKHELPER_H
