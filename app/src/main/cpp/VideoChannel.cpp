//
// Created by Administrator on 2023/3/28.
//

#include "VideoChannel.h"

VideoChannel::VideoChannel(int stream_index, AVCodecContext *codecContext) : BaseChannel(
        stream_index, codecContext) {

}

VideoChannel::~VideoChannel() {

}

void VideoChannel::stop() {

}

void *task_video_decode(void *args) {
    auto *video_channel = static_cast<VideoChannel *>(args);
    video_channel->video_decode();
    return 0;
}

void *task_video_play(void *args) {
    auto *video_channel = static_cast<VideoChannel *>(args);
    video_channel->video_play();
    return 0;
}

// 视频：1.解码    2.播放
// 1.把队列里面的压缩包(AVPacket *)取出来，然后解码成（AVFrame * ）原始包 ----> 保存队列
// 2.把队列里面的原始包(AVFrame *)取出来， 播放
void VideoChannel::start() {
    isPlaying = 1;

    //队列开始工作了
    packets.setWork(1);
    frames.setWork(1);

    //第一个线程：视频，取出队列的压缩包，进行编码，编码后的原始包再push到队列中
    pthread_create(&pid_video_decode,0,task_video_decode, this);

    //第二个线程：视频，取出队列的原始包，播放
    pthread_create(&pid_video_play,0,task_video_play, this);
}

/**
 * 解码：压缩包->原始包
 * 1）从队列取压缩包
 * 2) 发送压缩包到缓存区
 * 3）从缓存区获取原始包
 * 4）原始包放入队列
 *
 * TODO:第五节课 - 内存泄漏关键点（控制frames队列大小，等待队列中的数据被消费） - 2
 * */
void VideoChannel::video_decode() {
    AVPacket *pkt = 0;
    while (isPlaying){
        //2.1 内存泄漏点 原始包队列>100时，暂停放入压缩包队列
        if (isPlaying && frames.size() > 100){
            av_usleep(10 * 1000);
            continue;
        }
        //Step1：
        int ret = packets.getQueueAndDel(pkt);  //阻塞式函数
        if (!isPlaying){
            break;   // 如果关闭了播放，跳出循环，releaseAVPacket(&pkt);
        }
        if (!ret){   //ret == 0
            continue;  // 哪怕是没有成功，也要继续（假设：你生产太慢(压缩包加入队列)，我消费就等一下你）
        }

        //Step2：
        //1.发送pkt（压缩包）到缓冲区   2.从缓冲区拿出（原始包）
        ret = avcodec_send_packet(codecContext,pkt);
        //FFmpeg源码缓存一份pkt，所以我们的pkt可以在这释放了
        //releaseAVPacket(&pkt);
        if (ret){
            break;  // avcodec_send_packet 出现了错误
        }

        //Step3:从缓冲区获取原始包
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext,frame);
        if (ret == AVERROR(EAGAIN)){
            //B帧，B帧参考前面成功，B帧参考后面失败，可能是P帧没有出来，再拿一次即可。
            continue;
        } else if (ret !=0){
            // TODO 第五节课 解码视频的frame出错，马上释放，防止你在堆区开辟了空间 2.1 内存泄漏点
            if (frame){
                releaseAVFrame(&frame);
            }
            break;  //错误了
        }

        //step4:原始包放入队列
        frames.insertToQueue(frame);

        // TODO 第五节课 内存泄漏点 4
        // 安心释放pkt本身空间释放 和 pkt成员指向的空间释放
        av_packet_unref(pkt); // 减1 = 0 释放成员指向的堆区
        releaseAVPacket(&pkt); // 释放AVPacket * 本身的堆区空间
    }
    // TODO 第五节课 内存泄漏点 4.1
    av_packet_unref(pkt); // 减1 = 0 释放成员指向的堆区
    releaseAVPacket(&pkt); // 释放AVPacket * 本身的堆区空间
}

// 2.把队列里面的原始包(AVFrame *)取出来， 播放 【真正干活的就是他】
void VideoChannel::video_play() {
    // SWS_FAST_BILINEAR == 很快 可能会模糊
    // SWS_BILINEAR 适中算法

    AVFrame *frame = 0;
    uint8_t *dst_data[4];   //RGBA
    int dst_linesize[4];    //RGBA  FIXME：这是一个数组吗？
    //原始包(YUV数据)   ---->   [libswascale] Android屏幕(RGBA数据)

    //给 dst_data 申请内存   width * height * 4 xxxx
    av_image_alloc(dst_data, dst_linesize,
                   codecContext->width, codecContext->height, AV_PIX_FMT_RGBA, 1);

    SwsContext *sws_ctx = sws_getContext(
            // 下面是输入环节
            codecContext->width,
            codecContext->height,
            codecContext->pix_fmt, // 自动获取 xxx.mp4 的像素格式  AV_PIX_FMT_YUV420P // 写死的

            // 下面是输出环节
            codecContext->width,
            codecContext->height,
            AV_PIX_FMT_RGBA,
            SWS_BILINEAR, NULL, NULL, NULL);

    while (isPlaying){
        int ret = frames.getQueueAndDel(frame);
        if (!isPlaying){
            break;    //如果关闭了播放，跳出循环
        }
        if (!ret) { // ret == 0
            continue; // 哪怕是没有成功，也要继续（假设：你生产太慢(原始包加入队列)，我消费就等一下你）
        }

        // 格式转换 yuv ---> rgba
        sws_scale(sws_ctx,
                // 下面是输入环节 YUV的数据
                  frame->data, frame->linesize,0, codecContext->height,

                // 下面是输出环节  成果：RGBA数据
                  dst_data,dst_linesize
        );

        // ANativeWindows 渲染工作
        // SurfaceView ----- ANativeWindows
        // 如何渲染一贞图像？
        // 答：宽、高、数据 -------> 函数指针的声明
        // 拿不到Surface，只能回调给native-lib.cpp

        //基础：数组被传递会退化成指针，默认就是第一个元素  Fixme：为什么最后一个参数是int而不是int*？
        renderCallback(dst_data[0],codecContext->width,codecContext->height,dst_linesize[0]);
        //releaseAVFrame(&frame);  //释放原始包，此时已经被渲染完，没用了

        // TODO 第五节课 内存泄漏点 6
        av_frame_unref(frame); // 减1 = 0 释放成员指向的堆区
        releaseAVFrame(&frame); // 释放AVFrame * 本身的堆区空间
    }
    // 简单的释放
    // releaseAVFrame(&frame); // 出现错误，所退出的循环，都要释放frame
    // TODO 第五节课 内存泄漏点 6.1
    av_frame_unref(frame);
    releaseAVFrame(&frame);
    isPlaying = false;
    av_free(&dst_data);
    sws_freeContext(sws_ctx);  // free(sws_ctx); FFmpeg必须使用人家的函数释放，直接崩溃
}

void VideoChannel::setRenderCallback(RenderCallback callback) {
    this->renderCallback = callback;
}




