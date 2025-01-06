//
// Created by GuaishouN on 2020/4/25.
//

#ifndef GYSOFFMPEGAPPLICATION_BASECHANNEL_H
#define GYSOFFMPEGAPPLICATION_BASECHANNEL_H
#include "macro.h"
extern "C"{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
};

#include "safe_queue.h"
#include "CallbackHelper.h"
class BaseChannel {

public:
    BaseChannel(int stream_index, AVCodecContext *pContext,AVRational time_base) :
    stream_index(stream_index),
    pContext(pContext),
    time_base(time_base)
    {
        packets.setReleaseCallback(releaseAVPacket);
        frames.setReleaseCallback(releaseAVFrame);
    }

    //父类析构一定要加virtual
    virtual ~BaseChannel() {
        packets.clear();
        frames.clear();
        avcodec_close(pContext);
    }

    static void releaseAVPacket(AVPacket **pPacket){
        if(pPacket){
            av_packet_free(pPacket);
            *pPacket = 0;
        }
    }

    static void releaseAVFrame(AVFrame **pFrame){
        if(pFrame){
            av_frame_free(pFrame);
            *pFrame = 0;
        }
    }

    void setCallbackHelper(CallbackHelper *callbackHelper){
        this->callbackHelper = callbackHelper;
    }


    int stream_index;
    bool isPlaying;
    AVCodecContext *pContext = 0;
    SafeQueue<AVPacket *> packets;
    SafeQueue<AVFrame *> frames;
    AVRational time_base;
    CallbackHelper *callbackHelper= 0;;
    double audio_time;
};


#endif //GYSOFFMPEGAPPLICATION_BASECHANNEL_H
