#ifndef D_PLAYER_BASECHANNEL_H
#define D_PLAYER_BASECHANNEL_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
}

#include "safe_queue.h"
#include "log4c.h"

class BaseChannel {
public:
    int stream_index;  //音频 or 视频的下标
    SafeQueue<AVPacket *> packets; //压缩数据包
    SafeQueue<AVFrame *> frames;   //原始数据包
    bool isPlaying;       //音频 和 视频 都会有的标记 是否播放
    AVCodecContext *codecContext = 0;   //音频 和 视频都需要的 解码器上下文

    BaseChannel(int streamIndex, AVCodecContext *codecContext) : stream_index(streamIndex),
                                                                 codecContext(codecContext) {
        packets.setReleaseCallback(releaseAVPacket);  //给队列设置Callback，Callback释放队列里面的数据
        frames.setReleaseCallback(releaseAVFrame);
    }

    //父类析构一定要加virtual
    virtual ~BaseChannel() {
        packets.clear();
        frames.clear();
    }

    /**
     * 释放队列中所有的 AVPacket *
     * @param p 这里是要传 （AVPacket * 的地址），所以用 AVPacket ** 接收
     * */
    // typedef void (*ReleaseCallback)(T *);
    //FixMe:为什么要static？
    static void releaseAVPacket(AVPacket **p) {
        if (p) {
            av_packet_free(p);  // 释放队列里面的 T == AVPacket
            *p = 0;
        }
    }

    static void releaseAVFrame(AVFrame **f) {
        if (f) {
            av_frame_free(f);  // 释放队列里面的 T == AVFrame
            *f = 0;
        }
    }
};

#endif //D_PLAYER_BASECHANNEL_H
