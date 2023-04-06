//
// Created by Administrator on 2023/3/28.
//

#ifndef D_PLAYER_VIDEOCHANNEL_H
#define D_PLAYER_VIDEOCHANNEL_H

#include "BaseChannel.h"
#include "AudioChannel.h"

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
};

typedef void(*RenderCallback)(uint8_t *,int,int,int);  // 函数指针声明定义

class VideoChannel: public BaseChannel{
private:
    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderCallback renderCallback;

    int fps;  //fps是视频通道独有的，fps（一秒钟多少帧）
    AudioChannel *audioChannel = 0;

public:
    VideoChannel(int stream_index,AVCodecContext *codecContext, AVRational rational, int i);
    ~VideoChannel();
    void stop();
    void start();
    void video_decode();
    void video_play();
    void setRenderCallback(RenderCallback callback);
    void setAudioChannel(AudioChannel *audio_channel);
};


#endif //D_PLAYER_VIDEOCHANNEL_H
