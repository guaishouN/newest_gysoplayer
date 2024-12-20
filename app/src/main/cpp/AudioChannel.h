//
// Created by GuaishouN on 2020/4/25.
//

#ifndef GYSOFFMPEGAPPLICATION_AUDIOCHANNEL_H
#define GYSOFFMPEGAPPLICATION_AUDIOCHANNEL_H
#include "BaseChannel.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
extern "C"{
#include <libswresample/swresample.h>
};
class AudioChannel : public BaseChannel {
public:
    uint8_t *out_buffers = 0;
    int out_channels;
    int out_sample_size;
    int out_sample_rate;
    int out_buffers_size;
    AudioChannel(int stream_index,AVCodecContext *pContext,AVRational time_base);
    ~AudioChannel();
    void start();
    void stop();
    void audio_decode();
    void audio_play();
    int getPCM();
private:
    pthread_t pid_audio_decode;
    pthread_t pid_audio_play;
    //引擎
    SLObjectItf engineObject;
    //引擎接口
    SLEngineItf engineItf;
    //混声器
    SLObjectItf outputMixObject;
    //播放器
    SLObjectItf bqPlayerObject;
    SLPlayItf bqPlayerPlay;
    //播放器接口
    SLAndroidSimpleBufferQueueItf bpAndroidPlayerBufferQueue;
    SwrContext *swr_ctx;
};


#endif //GYSOFFMPEGAPPLICATION_AUDIOCHANNEL_H
