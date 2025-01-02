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
    AVPacket *packet = nullptr;
    while(isPlaying){
        /**
         * 队列控制
         */
        if(frames.size() > 100){
            av_usleep(10*1000);
            continue;
        }
        int ret = packets.pop(packet);
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
//        LOGI("VideoChannel::video_decode frames.size()=%d packets.size()=%d", frames.size(), packets.size())
    }
    releaseAVPacket(&packet);
}

void VideoChannel::video_play(){
    //播放
    int count = 0;
    uint8_t *dst_data[4];
    int dst_linesize[4];
    AVFrame *frame = nullptr;
    float scaleX = (float)pContext->width / (float) winWidth;
    float scaleY = (float)pContext->height / (float) winHeight;
    float scale = fmax(scaleX, scaleY);
    int newWidth = (int)(pContext->width/scale);
    int newHeight = (int)(pContext->height/scale);
    int offset_width = (winWidth-newWidth)/2;
    int offset_height = (winHeight-newHeight)/2;
    SwsContext *swsContext = sws_getContext(
            pContext->width,
            pContext->height,
            pContext->pix_fmt,
            newWidth,
            newHeight,
            AV_PIX_FMT_RGBA,
            SWS_BILINEAR,
            nullptr,
            nullptr,
            nullptr
            );
    //给显示缓存申请内存
    av_image_alloc(
            dst_data,
            dst_linesize,
            newWidth,
            newHeight,
            AV_PIX_FMT_RGBA,
            1
            );
    int ret = 0;
    while(isPlaying){
        //如果不是本地视频文件则最低延时播放，完全没缓存
        if(!isMp4LocalSource){
            while(frames.size()>0){
                if(frame){
                    releaseAVFrame(&frame);
                }
                ret = frames.pop(frame);
            }
        }else{
            ret = frames.pop(frame);
        }
        if(!ret){
            continue;
        }
//        LOGI("VideoChannel::video_decode render frame start count=%d", count)
        /**
         * 音视频同步 单位微秒us
         */
         if(!frame){
             continue;
         }
        //yuv ->rgba
        sws_scale(
                swsContext,
                frame->data,
                frame->linesize,
                0,
                pContext->height,
                dst_data,
                dst_linesize
        );
        //播放本地文件要控制延时
        if(isMp4LocalSource){
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
                double time_diff = video_time - audio_time;
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
        }}

         count++;
//        LOGI("VideoChannel::video_decode render frame mid count=%d", count)
//      //宽高数据
        renderCallback(
                dst_data[0],
                newWidth,
                newHeight,
                dst_linesize[0],
                offset_width,
                offset_height
                );
//        LOGI("VideoChannel::video_decode render frame end count=%d", count)
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

void VideoChannel::setWinWidthAndHeight(int winWidth, int winHeight) {
    LOGI("VideoChannel setWinWidthAndheight winWidth=%d winHeight=%d", winWidth, winHeight)
    this->winWidth = winWidth;
    this->winHeight = winHeight;
}
