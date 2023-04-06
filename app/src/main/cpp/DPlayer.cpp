//
// Created by Administrator on 2023/3/28.
//

#include "DPlayer.h"

DPlayer::DPlayer(const char *data_source, JNICallbackHelper *helper) {
    //this->data_source = data_source
    //如果被释放，会造成悬空指针

    //深拷贝
    //this->data_source = new char[strlen(data_source)];
    //Java：demo.mp4
    //C层：demo.mp4\0  C层会自动+\0，strlen不计算\0的长度，所以我们需要手动加\0
    this->data_source = new char[strlen(data_source) + 1];
    strcpy(this->data_source, data_source);  // 把源 Copy给成员

    this->helper = helper;
}


DPlayer::~DPlayer() {
    if (data_source) {
        delete data_source;
        data_source = nullptr;
    }
    if (helper){
        delete helper;
        helper = nullptr;
    }
}

/**
 * 指针函数，子线程的任务
 * 注意：此函数和DPlayer对象没有关系，没法拿到他的私有成员
 * */
void *task_prepare(void *args) {
    auto *player = static_cast<DPlayer *>(args);
    player->prepare_();
    return nullptr; //必须返回
}


void DPlayer::prepare_() {
    // 为什么FFmpeg源码，大量使用上下文Context？
    // 答：因为FFmpeg源码是纯C的，他不像C++、Java ， 上下文的出现是为了贯彻环境，就相当于Java的this能够操作成员

    /**
     * TODO 第一步：打开媒体地址（文件路径， 直播地址rtmp）
     */
    formatContext = avformat_alloc_context();
    // 字典（键值对）
    AVDictionary *dictionary = nullptr;
    //设置超时（5秒）,单位微秒
    av_dict_set(&dictionary, "timeout", "5000000", 0);

    /**
     * 1，AVFormatContext *
     * 2，路径 url:文件路径或直播地址
     * 3，AVInputFormat *fmt  Mac、Windows 摄像头、麦克风， 我们目前安卓用不到
     * 4，各种设置：例如：Http 连接超时， 打开rtmp的超时  AVDictionary **options
     */
    int r = avformat_open_input(&formatContext, data_source, nullptr, &dictionary);
    //释放字典
    av_dict_free(&dictionary);
    if (r) {  //对于 avformat_open_input 返回0代表Success，这里C的判断是非0即true。所以这里如果r=0不进入方法里，不为0才进入。
        // 把错误信息反馈给Java，回调给Java  Toast【打开媒体格式失败，请检查代码】
        // TODO 第一节课作业：JNI 反射回调到Java方法，并提示
        if (helper){
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL);
        }
        return;
    }

    /**
     * TODO 第二步：查找媒体中的音视频流的信息
     */
    r = avformat_find_stream_info(formatContext, nullptr);
    if (r < 0) {
        // TODO 第一节课作业：JNI 反射回调到Java方法，并提示
        if (helper){
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS);
        }
        return;
    }

    /**
     * TODO 第三步：根据流信息，流的个数，用循环来找
     */
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        /**
         * TODO 第四步：获取媒体流（视频，音频）
         */
        AVStream *avStream = formatContext->streams[i];

        /**
        * TODO 第五步：从上面的流中 获取 编码解码的【参数】
        * 由于：后面的编码器 解码器 都需要参数（宽高 等等）
        */
        AVCodecParameters *parameters = avStream->codecpar;

        /**
         * TODO 第六步：（根据上面的【参数】）获取编解码器
         */
        AVCodec *codec = avcodec_find_decoder(parameters->codec_id);
        if (!codec) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL);
            }
        }

        /**
        * TODO 第七步：编解码器 上下文 （这个才是真正干活的）
        */
        AVCodecContext *codeContext = avcodec_alloc_context3(codec);
        if (!codeContext) {
            if (helper){
                helper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL);
            }
            return;
        }

        /**
         * TODO 第八步：他目前是一张白纸（parameters copy codecContext）
         */
        r = avcodec_parameters_to_context(codeContext, parameters);
        if (r < 0) {
            if (helper){
                helper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL);
            }
            return;
        }

        /**
        * TODO 第九步：打开解码器
        */
        r = avcodec_open2(codeContext, codec, nullptr);
        if (r) {
            if (helper){
                helper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL);
            }
            return;
        }

        /**
         * TODO 第十步：从编解码器参数中，获取流的类型 codec_type  ===  音频 视频
         */
        if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO) { // 音频
            audio_channel = new AudioChannel(i,codeContext);
        } else if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) { // 视频
            video_channel = new VideoChannel(i,codeContext);
            video_channel->setRenderCallback(renderCallback);
        }
    } // for end

    /**
     * TODO 第十一步: 如果流中 没有音频 也没有 视频 【健壮性校验】
     */
     if(!audio_channel && !video_channel){
         if (helper){
             helper->onError(THREAD_CHILD, FFMPEG_NOMEDIA);
         }
         return;
     }

    /**
    * TODO 第十二步：恭喜你，准备成功，我们的媒体文件 OK了，通知给上层
    */
    if (helper){
        helper->onPrepared(THREAD_CHILD);
    }
}


/**
 * 问题：当前的prepare函数，是子线程 还是 主线程 ？
 * 答：此函数是被MainActivity的onResume调用下来的（主线程）
 *
 * 问题：解封装 FFmpeg来解析  data_source 可以直接解析吗？
 * 答：data_source == 文件io流，  直播网络rtmp， 所以按道理来说，会耗时，所以必须使用子线程
 * */
void DPlayer::prepare() {
    //创建子线程来做ffmpeg的准备工作
    //这第四个参数传了DPlayer当前对象的地址进入
    pthread_create(&pid_prepare, 0,task_prepare,this);
}




// TODO >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>  下面全部都是 start

void *task_start(void *args){
    auto *player = static_cast<DPlayer*>(args);
    player->start_();
    return nullptr;
}

// TODO 第五节课 内存泄漏关键点（控制packet队列大小，等待队列中的数据被消费） 1
// 把 视频 音频 的压缩包(AVPacket *) 循环获取出来 加入到队列里面去
void DPlayer::start_() { //子线程
    while (isPlaying){
        // 解决方案：视频 我不丢弃数据，等待队列中数据 被消费 内存泄漏点1.1
        if (video_channel && video_channel->packets.size() > 100){
            av_usleep(10 * 1000);  // 单位 ：microseconds 微妙 10毫秒
            continue;
        }
        // 解决方案：音频 我不丢弃数据，等待队列中数据 被消费 内存泄漏点1.2
        if (audio_channel && audio_channel->packets.size() > 100){
            av_usleep(10 * 1000);
            continue;
        }

        //AVPacket 可能是音频，也可能是视频（压缩包）
        AVPacket * packet = av_packet_alloc();
        int ret = av_read_frame(formatContext,packet);
        if (!ret){  // ret == 0,代表成功
            // AudioChannel    队列
            // VideoChannel   队列

            // 把我们的 AVPacket* 加入队列， 音频 和 视频
            /*AudioChannel.insert(packet);
            VideoChannel.insert(packet);*/
            if (video_channel && video_channel->stream_index == packet->stream_index){
                //代表视频
                video_channel->packets.insertToQueue(packet);
            }else if (audio_channel && audio_channel->stream_index == packet->stream_index){
                //代表音频
                audio_channel->packets.insertToQueue(packet);
            }
        }else if(ret == AVERROR_EOF){  //end of dile == 读到文件末尾了 == AVERROR_EOF
            // 内存泄漏点1.3
            //TODO 表示读完了，要考虑是否播放完成。  注意：读完了，不代表播放完毕
            if (video_channel->packets.empty() && audio_channel->packets.empty()){
                break; // 队列的数据被音频 视频 全部播放完毕了，我在退出
            }
        }else{
            break;  // av_read_frame 出现了错误，结束当前循环
        }
    }
    isPlaying = false;
    if (video_channel){
        video_channel->stop();
    }
    if (audio_channel){
        audio_channel->stop();
    }
}

void DPlayer::start() {
    isPlaying = true;

    // 视频：1.解码    2.播放
    // 1.把队列里面的压缩包(AVPacket *)取出来，然后解码成（AVFrame * ）原始包 ----> 保存队列
    // 2.把队列里面的原始包(AVFrame *)取出来， 播放
    if (video_channel){
        video_channel->start();
    }
    if (audio_channel) {
        audio_channel->start();
    }

    // 把 音频和视频 压缩包 加入队列里面去
    // 创建子线程
    pthread_create(&pid_start,0,task_start,this);
}

void DPlayer::setRenderCallback(RenderCallback callback) {
    this->renderCallback = callback;
}





