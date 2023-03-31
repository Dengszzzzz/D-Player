//
// Created by Administrator on 2023/3/28.
//

#ifndef D_PLAYER_DPLAYER_H
#define D_PLAYER_DPLAYER_H

#include <cstring>
#include <pthread.h>
#include "AudioChannel.h"
#include "VideoChannel.h"
#include "JNICallbackHelper.h"
#include "util.h"

// ffmpeg是纯c写的，必须采用c的编译方式，否则奔溃
extern "C" {
#include "libavformat/avformat.h"
};


class DPlayer {

private:
    char *data_source = 0; //指针，记得赋初始值
    pthread_t pid_prepare;
    pthread_t pid_start;
    AVFormatContext *formatContext = 0;
    AudioChannel * audio_channel = 0;
    VideoChannel * video_channel = 0;
    JNICallbackHelper *helper = 0;
    bool isPlaying;  //是否播放
    RenderCallback renderCallback;

public:
    DPlayer(const char *data_source,JNICallbackHelper *helper);

    ~DPlayer();

    void prepare();
    void prepare_();

    void start();
    void start_();

    void setRenderCallback(RenderCallback callback);
};


#endif //D_PLAYER_DPLAYER_H
