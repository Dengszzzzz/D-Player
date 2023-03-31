//
// Created by Administrator on 2023/3/28.
//

#include "JNICallbackHelper.h"
#include <jni.h>

JNICallbackHelper::JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject job) {
    this->vm = vm;
    this->env = env;
    //坑： jobject不能跨越线程，不能跨越函数，必须全局引用
    //this->job = job;
    this->job = env->NewGlobalRef(job);  //提升全局引用

    jclass clazz = env->GetObjectClass(job);
    jmd_prepared = env->GetMethodID(clazz, "onPrepared", "()V");
    jmd_onError = env->GetMethodID(clazz, "onError", "(I)V");
}

void JNICallbackHelper::onPrepared(int thread_mode) {
    if(thread_mode == THREAD_MAIN){
        //主线程
        env->CallVoidMethod(job,jmd_prepared);
    }else if(thread_mode == THREAD_CHILD){
        //子线程 要为当前线程new一个env
        JNIEnv *env_child;
        vm->AttachCurrentThread(&env_child,0);
        env_child->CallVoidMethod(job,jmd_prepared);
        vm->DetachCurrentThread();
    }
}

JNICallbackHelper::~JNICallbackHelper() {
    vm = 0;
    env->DeleteGlobalRef(job);
    job = 0;
    env = 0;
}

void JNICallbackHelper::onError(int thread_mode, int error_code) {
    if(thread_mode == THREAD_MAIN){
        env->CallVoidMethod(job,jmd_onError,error_code);
    }else if(thread_mode == THREAD_CHILD){
        JNIEnv *env_child;
        vm->AttachCurrentThread(&env_child,0);
        env_child->CallVoidMethod(job,jmd_onError,error_code);
        vm->DetachCurrentThread();
    }
}
