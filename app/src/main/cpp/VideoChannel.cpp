//
// Created by GuaishouN on 2020/4/25.
//

#include "VideoChannel.h"
/**
 * 丢packet包
 */
void dropAVPacket(queue<AVPacket *> &q){
    while(!q.empty()){
        AVPacket *packet =q.front();
        //AV_PKT_FLAG_KEY 关键帧
        if(packet->flags != AV_PKT_FLAG_KEY){
            BaseChannel::releaseAVPacket(&packet);
            q.pop();
        }else{
            break;
        }
    }
}

/**
 * 丢frame包
 */
void dropAVFrame(queue<AVFrame *> &q){
    if(!q.front()){
        AVFrame *frame = q.front();
        BaseChannel::releaseAVFrame(&frame);
        q.pop();
    }
}

VideoChannel::VideoChannel(int stream_index, AVCodecContext *pContext, AVRational time_base, int fps):
BaseChannel( stream_index,pContext,time_base) {
    this->fps = fps;
    packets.setSyncCallback(dropAVPacket);
    frames.setSyncCallback(dropAVFrame);
}
VideoChannel::~VideoChannel() {

}
void VideoChannel::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}
void *threadVideoDecode(void* agrs){
    VideoChannel *videoChannel = static_cast<VideoChannel *>(agrs);
    videoChannel->video_decode();
    return 0;
}

void *threadVideoPlay(void* agrs){
    VideoChannel *videoChannel = static_cast<VideoChannel *>(agrs);
    videoChannel->video_play();
    return 0;
}

void VideoChannel::start() {
    isPlaying = 1;
    packets.setWork(1);
    frames.setWork(1);
    //解码线程和播放线程
    pthread_create(&pid_video_decode,0,threadVideoDecode,this);
    pthread_create(&pid_video_play,0,threadVideoPlay,this);
}
void VideoChannel::stop() {
    isPlaying = 0;
    packets.setWork(0);
    frames.setWork(0);
    pthread_join(pid_video_decode,0);
    pthread_join(pid_video_play,0);
}
void VideoChannel::video_decode() {
    AVPacket *packet = 0;
    while(isPlaying){
        /**
         * 队列控制
         */
        if(isPlaying && frames.size()>100){
            av_usleep(10*1000);
            continue;
        }
        int ret = packets.pop(packet);
        if(!isPlaying){
            break;
        }
        if(!ret){
            continue;
        }
        ret = avcodec_send_packet(pContext,packet);
        if(ret){
            break;
        }
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(pContext,frame);
        if(ret == AVERROR(EAGAIN)){
            continue;
        }else if(ret!=0){
            releaseAVFrame(&frame);
            break;
        }
        frames.push(frame);
        if(packet){
            releaseAVPacket(&packet);
        }
    }
    releaseAVPacket(&packet);
}
void VideoChannel::video_play(){
    //播放
    uint8_t *dst_data[4];
    int dst_linesize[4];
    AVFrame *frame = 0;
    SwsContext *swsContext = sws_getContext(
            pContext->width,
            pContext->height,
            pContext->pix_fmt,
            pContext->width,
            pContext->height,
            AV_PIX_FMT_RGBA,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
            );
    //给显示缓存申请内存
    av_image_alloc(
            dst_data,
            dst_linesize,
            pContext->width,
            pContext->height,
            AV_PIX_FMT_RGBA,
            1
            );
    while(isPlaying){
        int ret = frames.pop(frame);
        if(!isPlaying){
            break;
        }
        if(!ret){
            continue;
        }
//        //yuv ->rgba
        sws_scale(
                swsContext,
                frame->data,
                frame->linesize,
                0,
                pContext->height,
                dst_data,
                dst_linesize
                );
        /**
         * 音视频同步 单位微秒us
         */
         if(!frame){
             continue;
         }
         //当前帧的额外延时
        double extra_delay = frame->repeat_pict / (2*fps);
        //根据frame得到的延时
        double base_delay = 1.0 /fps;
        //当前帧的实际的延时时间
        double real_delay = base_delay + extra_delay;
        double video_time =frame->best_effort_timestamp*av_q2d(time_base);
        if(!audioChannel){
            av_usleep(static_cast<unsigned int>(real_delay * 1000000));
            //没有音频则使用视频时间回调
            if(callbackHelper){
                callbackHelper->onProgress(THREAD_CHILD,video_time);
            }
        }else{
            double audio_time = audioChannel->audio_time;
            double time_diff = video_time -audio_time;
            if(time_diff>0){
                //视频时间比音频时间大，等音频
                if(time_diff>1){
                    av_usleep((real_delay *2)*1000000);
                }else{
                    av_usleep((real_delay +time_diff)*1000000);
                }
            }else if(time_diff<0){
                //视频时间比音频时间小，frames和packets中丢帧追音频
                //在0.05的时间差内，人的肉眼几乎没有延迟感
                if(fabs(time_diff)>=0.05){
                    //丢掉一帧
                    frames.sync();
                    continue;
                }
            }
        }
//      //宽高数据
        renderCallback(
                dst_data[0],
                pContext->width,
                pContext->height,
                dst_linesize[0]
                );
        releaseAVFrame(&frame);
    }
    releaseAVFrame(&frame);
    isPlaying = 0;
    av_freep(&dst_data[0]);
    sws_freeContext(swsContext);
}

void VideoChannel::setAudioChannel(AudioChannel *pChannel) {
    this->audioChannel = pChannel;
}
