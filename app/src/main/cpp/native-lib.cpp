#include <jni.h>
#include <string>
#include "DPlayer.h"
#include "JNICallbackHelper.h"
#include <android/native_window_jni.h>   //ANativeWindow用来渲染画面的 == Surface对象
#include "log4c.h"

extern "C" {
#include <libavutil/avutil.h>
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_derry_player_MainActivity_getFFmpegVersion(
        JNIEnv *env,
        jobject /* this */) {
    std::string info = "FFmpeg的版本号是:";
    info.append(av_version_info());
    return env->NewStringUTF(info.c_str());
}

DPlayer *player = 0;
JavaVM *vm = 0;
ANativeWindow *window = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;    //静态初始化锁

jint JNI_OnLoad(JavaVM *vm, void *args) {
    ::vm = vm;
    return JNI_VERSION_1_6;
}

//函数指针 实现 渲染工作
void renderFrame(uint8_t * src_data,int width,int height,int src_lineSize){
    pthread_mutex_lock(&mutex);
    if (!window) {
        pthread_mutex_unlock(&mutex); // 出现了问题后，必须考虑到，释放锁，怕出现死锁问题
    }

    // 设置窗口的大小，各个属性
    ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);

    // 他自己有个缓冲区 buffer
    ANativeWindow_Buffer window_buffer; // 目前他是指针吗？

    // 如果我在渲染的时候，是被锁住的，那我就无法渲染，我需要释放 ，防止出现死锁
    if (ANativeWindow_lock(window, &window_buffer, 0)) {
        ANativeWindow_release(window);
        window = 0;

        pthread_mutex_unlock(&mutex); // 解锁，怕出现死锁
        return;
    }

    // TODO 开始真正渲染，因为window没有被锁住了，就可以把 rgba数据 ---> 字节对齐 渲染
    // 填充window_buffer  画面就出来了  === [目标]
    uint8_t *dst_data = static_cast<uint8_t *>(window_buffer.bits);
    int dst_linesize = window_buffer.stride * 4;

    for (int i = 0; i < window_buffer.height; ++i) { // 图：一行一行显示 [高度不用管，用循环了，遍历高度]
        // 视频分辨率：426 * 240
        // 视频分辨率：宽 426
        // 426 * 4(rgba8888) = 1704
        // memcpy(dst_data + i * 1704, src_data + i * 1704, 1704); // 花屏
        // 花屏原因：1704 无法 64字节对齐，所以花屏

        // ANativeWindow_Buffer 64字节对齐的算法，  1704无法以64位字节对齐
        // memcpy(dst_data + i * 1792, src_data + i * 1704, 1792); // OK的
        // memcpy(dst_data + i * 1793, src_data + i * 1704, 1793); // 部分花屏，无法64字节对齐
        // memcpy(dst_data + i * 1728, src_data + i * 1704, 1728); // 花屏

        // ANativeWindow_Buffer 64字节对齐的算法  1728
        // 占位 占位 占位 占位 占位 占位 占位 占位
        // 数据 数据 数据 数据 数据 数据 数据 空值

        // ANativeWindow_Buffer 64字节对齐的算法  1792  空间换时间
        // 占位 占位 占位 占位 占位 占位 占位 占位 占位
        // 数据 数据 数据 数据 数据 数据 数据 空值 空值

        // FFmpeg为什么认为  1704 没有问题 ？
        // FFmpeg是默认采用8字节对齐的，他就认为没有问题， 但是ANativeWindow_Buffer他是64字节对齐的，就有问题

        // 通用的
        memcpy(dst_data + i * dst_linesize, src_data + i * src_lineSize, dst_linesize); // OK的
    }

    // 数据刷新
    ANativeWindow_unlockAndPost(window); // 解锁后 并且刷新 window_buffer的数据显示画面

    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dzh_d_1player_DPlayer_prepareNative(JNIEnv *env, jobject thiz, jstring data_source) {
    const char *data_source_ = env->GetStringUTFChars(data_source, 0);
    auto *helper = new JNICallbackHelper(vm, env, thiz);
    player = new DPlayer(data_source_, helper);
    player->setRenderCallback(renderFrame);
    player->prepare();
    env->ReleaseStringUTFChars(data_source, data_source_);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dzh_d_1player_DPlayer_startNative(JNIEnv *env, jobject thiz) {

    //模拟奔溃，在高版本Android系统中，通过Debug app 可以打断点定位到这。
/*    AudioChannel* aaa = nullptr;
    int count = aaa->frames.size();
    LOGE("位置：count = %c\n",count);*/

    if (player) {
        player->start();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dzh_d_1player_DPlayer_stopNative(JNIEnv *env, jobject thiz) {
    // TODO: implement stopNative()
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dzh_d_1player_DPlayer_releaseNative(JNIEnv *env, jobject thiz) {
    // TODO: implement releaseNative()
}

/**
 * 实例化出 window
 * */
extern "C"
JNIEXPORT void JNICALL
Java_com_dzh_d_1player_DPlayer_setSurfaceNative(JNIEnv *env, jobject thiz, jobject surface) {
    pthread_mutex_lock(&mutex);

    // 先释放之前的显示窗口
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }

    // 创建新的窗口用于视频显示
    window = ANativeWindow_fromSurface(env, surface);

    pthread_mutex_unlock(&mutex);
}