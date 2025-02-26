//
// Created by GuaishouN on 2020/4/25.
//
#include "GySoPlayer.h"

GySoPlayer::GySoPlayer(const char *string,CallbackHelper *callbackHelper) {
    //获取视频文件路径
    videoPath = new char[strlen(string)+1];
    strcpy(videoPath,string);
    this->callbackHelper = callbackHelper;
    isPlaying = 0;
    pthread_mutex_init(&seek_mutex, 0);
}


GySoPlayer::~GySoPlayer() {
    DELETE(videoPath);
    DELETE(callbackHelper);
    pthread_mutex_destroy(&seek_mutex);
    avformat_network_deinit();
}

void *prepareInChildThread(void * args){
    GySoPlayer *gySoPlayer = static_cast<GySoPlayer *>(args);
    gySoPlayer->_prepare();
    return 0;
}

void GySoPlayer::prepare() {
    LOGD("prepare --------------------------------------");
    pthread_create(&pThread_prepare, 0, prepareInChildThread, this);
}

void checkFile(const char* filePath) {
    if (access(filePath, F_OK) == 0) {
        LOGD("File exists: %s\n", filePath);
    } else {
        LOGE("File not found or cannot access");
    }
}

void GySoPlayer::_prepare() {
    LOGD("prepare %s",videoPath);
//    checkFile(videoPath);
    avformat_network_init();
    //文件上下文，打开文件
//    avFormatContext = avformat_alloc_context();
    AVFormatContext *format_ctx = NULL;
    int ret = avformat_open_input(&format_ctx,videoPath,NULL,NULL);
    if(ret<0){
        LOGE("prepare open ERROR %d",ret);
        if(callbackHelper){
            callbackHelper->onError(THREAD_CHILD,FFMPEG_CAN_NOT_OPEN_URL);
        }
        return;
    }
    avFormatContext = format_ctx;
    //判断文件有没流
    ret = avformat_find_stream_info(format_ctx,0);
    if(ret<0){
        LOGE("prepare find stream[0] ERROR %d",ret);
        if(callbackHelper){
            callbackHelper->onError(THREAD_CHILD,FFMPEG_CAN_NOT_FIND_STREAMS);
        }
        return;
    }
//    //获取的 duration 单位是：秒
    duration = static_cast<int>(format_ctx->duration / AV_TIME_BASE);
    LOGI("prepare get duration %d",duration);
    //查找视频流和音频流
    for(int i=0;i<format_ctx->nb_streams;i++){
        AVStream *stream = format_ctx->streams[i];
        AVCodecParameters *avCodecParameters = stream->codecpar;
        const AVCodec *avCodec =avcodec_find_decoder(avCodecParameters->codec_id);
        if(!avCodec){
            LOGE("prepare get avCodec ERROR %d",ret);
            if(callbackHelper){
                callbackHelper->onError(THREAD_CHILD,FFMPEG_FIND_DECODER_FAIL);
            }
            return;
        }
        AVCodecContext *avCodecContext = avcodec_alloc_context3(avCodec);
        if(!avCodecContext){
            LOGE("prepare get avCodecContext ERROR %d",ret);
            if(callbackHelper){
                callbackHelper->onError(THREAD_CHILD,FFMPEG_ALLOC_CODEC_CONTEXT_FAIL);
            }
            return;
        }
        ret=avcodec_parameters_to_context(avCodecContext,avCodecParameters);
        if(ret<0){
            LOGE("prepare parameters_to_context ERROR %d",ret);
            if(callbackHelper){
                callbackHelper->onError(THREAD_CHILD,FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL);
            }
            return;
        }
        ret=avcodec_open2(avCodecContext,avCodec,NULL);
        if(ret<0){
            LOGE("prepare open avCodec ERROR %d",ret);
            if(callbackHelper){
                callbackHelper->onError(THREAD_CHILD,FFMPEG_OPEN_DECODER_FAIL);
            }
            return;
        }
        AVMediaType type = avCodecParameters->codec_type;
        AVRational time_base = stream->time_base;
        if(type==AVMEDIA_TYPE_VIDEO){
            //如果是封面 跳过
            if(stream->disposition & AV_DISPOSITION_ATTACHED_PIC){
                LOGI("prepare get video");
                continue;
            }
            LOGI("prepare get video1");
            AVRational frame_rate = stream->avg_frame_rate;
            int fps = static_cast<int>(av_q2d(frame_rate));
            videoChannel = new VideoChannel(i,avCodecContext,time_base,fps);
            videoChannel->setRenderCallback(renderCallback);
            //如果没有音频就在视频中回调进度给UI
            if (!duration) {
                //直播 不需要回调进度给Java
                videoChannel->setCallbackHelper(callbackHelper);
            }
            LOGI("prepare get video2");
        }
    }
    LOGI("prepare  finish1");
    if(!videoChannel){
        if (callbackHelper) {
            callbackHelper->onError(THREAD_CHILD, FFMPEG_NOMEDIA);
        }
        return;
    }
    if(callbackHelper){
        callbackHelper->onPrepared(THREAD_CHILD);
    }
    LOGI("prepare finish!!")
}



void *startInChildThread(void * args){
    LOGI("startInChildThread!!")
    GySoPlayer *gySoPlayer = static_cast<GySoPlayer *>(args);
    gySoPlayer->_start();
    return 0;
}
/**
 * 开始播放
 */
void GySoPlayer::start() {
    isPlaying=1;
    if(videoChannel){
        videoChannel->start();
    }
    pthread_create(&pThread_start, 0, startInChildThread, this);
}

/**
 * 在子线程中解码
 * 开始播放
 */
void GySoPlayer::_start() {
    LOGI("_start");
    while(isPlaying){
        /**
         * 控制队列大小，等待队列中的数据被消费(音频视频都要等)
         */
         if(videoChannel && videoChannel->packets.size()>100){
             av_usleep(10*1000);
             continue;
         }
        //avpacket 可能是音频或视频
        pthread_mutex_lock(&seek_mutex);
        AVPacket *packet = av_packet_alloc();
        pthread_mutex_unlock(&seek_mutex);
        int ret = av_read_frame(avFormatContext,packet);
        if(!ret){
            //视频
            if(videoChannel && videoChannel->stream_index == packet->stream_index){
                videoChannel->packets.push(packet);
            //音频
            }
        } else if(ret == AVERROR_EOF){
            //表示读取完毕
        }else{
            break;
        }
    }
    //释放
    isPlaying = 0;
    videoChannel->stop();
}

void GySoPlayer::setRenderCallback(RenderCallback renderCallback){
    this->renderCallback=renderCallback;
}

void GySoPlayer::seek(int progress) {
    if(progress<0 || progress>duration){
        return;
    }
    if(!videoChannel){
        return;
    }
    if(!avFormatContext){
        return;
    }
    //用户可能拖来拖去，同步问题
    pthread_mutex_lock(&seek_mutex);
    int ret  = av_seek_frame(
            avFormatContext,
            -1,
            progress * AV_TIME_BASE,
            AVSEEK_FLAG_BACKWARD
            );
    if(ret<0){
        if(callbackHelper){
            callbackHelper->onError(THREAD_CHILD,ret);
        }
        return;
    }
    if(videoChannel){
        //avcodec_flush_buffers(videoChannel->pContext);
        videoChannel->frames.setWork(0);
        videoChannel->packets.setWork(0);
        videoChannel->packets.clear();
        videoChannel->frames.clear();
        videoChannel->frames.setWork(1);
        videoChannel->packets.setWork(1);
    }
    pthread_mutex_unlock(&seek_mutex);
}

int GySoPlayer::getDuration() {
    return duration;
}

/**
 * 设置为友元函数
 * @param args
 * @return
 */
void *task_stop(void *args){
    GySoPlayer * gySoPlayer = static_cast<GySoPlayer *>(args);
    gySoPlayer->isPlaying = 0;
    pthread_join(gySoPlayer->pThread_prepare,0);
    pthread_join(gySoPlayer->pThread_start,0);
    if(gySoPlayer->avFormatContext){
        avformat_close_input(&gySoPlayer->avFormatContext);
        avformat_free_context(gySoPlayer->avFormatContext);
        gySoPlayer->avFormatContext = 0;
    }
    DELETE(gySoPlayer->videoChannel);
    DELETE(gySoPlayer);
    return 0;
}

void GySoPlayer::stop() {
    callbackHelper = 0 ;
    if(videoChannel){
        videoChannel->callbackHelper = 0;
    }
    pthread_create(&pid_stop, 0, task_stop, this);
}

void GySoPlayer::setFrameNum(int fNum){

}
