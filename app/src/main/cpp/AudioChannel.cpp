//
// Created by GuaishouN on 2020/4/25.
//

#include <android/log.h>
#include "AudioChannel.h"


AudioChannel::AudioChannel(int stream_index, AVCodecContext *pContext, AVRational time_base)
        : BaseChannel(stream_index, pContext, time_base) {
    //TODO
//    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);//stereo 双声道
    AVChannelLayout ch_layout;
    av_channel_layout_default(&ch_layout, 2); // 为 2 个通道创建默认布局
    out_channels = ch_layout.nb_channels;     // 获取通道数量
    //每个 sample 是16 bits = 2字节
    //TODO
    out_sample_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    out_sample_rate = 44100;
    out_buffers_size = out_sample_rate * out_sample_size * out_channels;
    out_buffers = static_cast<uint8_t *>(malloc(out_buffers_size));
    memset(out_buffers, 0, out_buffers_size);
    AVSampleFormat outSampleFormat = AV_SAMPLE_FMT_NONE;
    AVChannelLayout inChannelLayout  = pContext->ch_layout;   //输入通道
    int             inSampleRate     = pContext->sample_rate; //输入采样率
    AVSampleFormat  inSampleFormat   = pContext->sample_fmt;  //输入采样位数
    AVChannelLayout outChannelLayout = pContext->ch_layout;   //输出通道
    int             outSampleRate    = pContext->sample_rate; //输出采样率
    switch (pContext->sample_fmt) {
        case AV_SAMPLE_FMT_U8P:
        case AV_SAMPLE_FMT_S16P:
        case AV_SAMPLE_FMT_S32P:
        case AV_SAMPLE_FMT_FLTP:
            outSampleFormat = (AVSampleFormat)(pContext->sample_fmt - 5);
            break;
        case AV_SAMPLE_FMT_DBLP:
            outSampleFormat = AV_SAMPLE_FMT_FLT;
            break;
        case AV_SAMPLE_FMT_S64P:
            outSampleFormat = AV_SAMPLE_FMT_S32;
            break;
        default:
            outSampleFormat = AV_SAMPLE_FMT_U8P;
            break;
    }
    swr_ctx = swr_alloc();
    int ret2 = swr_alloc_set_opts2(&swr_ctx,
                                   &outChannelLayout,
                                   outSampleFormat,
                                   outSampleRate,
                                   &inChannelLayout,
                                   inSampleFormat,
                                   inSampleRate,
                                   0,
                                   nullptr);
    LOGI("swr_alloc_set_opts2 %d", ret2)
    swr_init(swr_ctx);
}

AudioChannel::~AudioChannel() {
    if (swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = 0;
    }
    DELETE(out_buffers);
}

void *task_audio_decode(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->audio_decode();
    return 0;
}

void *task_audio_play(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->audio_play();
    return 0;
}

void AudioChannel::start() {
    isPlaying = 1;
    packets.setWork(1);
    frames.setWork(1);
    pthread_create(&pid_audio_decode, 0, task_audio_decode, this);
    pthread_create(&pid_audio_play, 0, task_audio_play, this);
}

void AudioChannel::stop() {
    isPlaying = 0;
    packets.setWork(0);
    frames.setWork(0);
    pthread_join(pid_audio_decode, 0);
    pthread_join(pid_audio_play, 0);
    //7.1 设置停止状态
    if (bqPlayerPlay) {
        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
        bqPlayerPlay = 0;
    }
    //7.2 销毁播放器
    if (bqPlayerObject) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = 0;
        bpAndroidPlayerBufferQueue = 0;
    }
    //7.3 销毁混音器
    if (outputMixObject) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = 0;
    }
    //7.4 销毁引擎
    if (engineObject) {
        (*engineObject)->Destroy(engineObject);
        engineObject = 0;
        engineItf = 0;
    }
}

/**
 * 创建回调函数
 */
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    int pcm_size = audioChannel->getPCM();
    (*bq)->Enqueue(bq, audioChannel->out_buffers, static_cast<SLuint32>(pcm_size));
}

void AudioChannel::audio_decode() {
    AVPacket *packet = 0;
    while (isPlaying) {
        /**
         * 队列控制
         */
        if (isPlaying && frames.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        int ret = packets.pop(packet);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        ret = avcodec_send_packet(pContext, packet);
        if (ret) {
            break;
        }
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(pContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret != 0) {
            break;
        }
        if (frame) {
            frames.push(frame);
        }
        if (packet) {
            releaseAVPacket(&packet);
        }
    }
    releaseAVPacket(&packet);
}

void AudioChannel::audio_play() {
    /**
     *   1  创建引擎并获取接口
     */
    SLresult result;
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    result = (*engineObject)->Realize(engineObject,
                                      SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    result = (*engineObject)->GetInterface(
            engineObject,
            SL_IID_ENGINE,
            &engineItf
    );
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    /**
     * 2 设置混声器
     */
    result = (*engineItf)->CreateOutputMix(engineItf,
                                           &outputMixObject, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    result = (*outputMixObject)->Realize(outputMixObject,
                                         SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    /**
     * 3 创建播放器
     */
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    //pcm数据格式
    //SL_DATAFORMAT_PCM：数据格式为pcm格式
    //2：双声道
    //SL_SAMPLINGRATE_44_1：采样率为44100，应用最广的兼容性最好的（b/s）
    //SL_PCMSAMPLEFORMAT_FIXED_16：采样格式为16bit (2字节)
    //SL_PCMSAMPLEFORMAT_FIXED_16：数据大小为16bit
    //SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT：左右声道（双声道）
    //SL_BYTEORDER_LITTLEENDIAN：小端模式
    SLDataFormat_PCM formatPcm = {
            SL_DATAFORMAT_PCM,
            2,
            SL_SAMPLINGRATE_44_1,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource audioSrc = {&loc_bufq, &formatPcm};
    //配置输出
    SLDataLocator_OutputMix locatorOutputMix = {
            SL_DATALOCATOR_OUTPUTMIX,
            outputMixObject
    };
    SLDataSink audioSink = {&locatorOutputMix, NULL};
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    result = (*engineItf)->CreateAudioPlayer(
            engineItf,
            &bqPlayerObject,
            &audioSrc,
            &audioSink,
            1,
            ids,
            req
    );
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    result = (*bqPlayerObject)->Realize(bqPlayerObject,
                                        SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject,
                                             SL_IID_PLAY, &bqPlayerPlay);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    /**
     * 4 设置播放回调
     */
    (*bqPlayerObject)->GetInterface(
            bqPlayerObject,
            SL_IID_BUFFERQUEUE,
            &bpAndroidPlayerBufferQueue
    );
    (*bpAndroidPlayerBufferQueue)->RegisterCallback(
            bpAndroidPlayerBufferQueue,
            bqPlayerCallback,
            this
    );
    /**
     * 5 设置播放状态
     */
    (*bqPlayerPlay)->SetPlayState(
            bqPlayerPlay,
            SL_PLAYSTATE_PLAYING
    );
    /**
     * 6 手动激活回调函数
     */
    bqPlayerCallback(bpAndroidPlayerBufferQueue, this);
}

int AudioChannel::getPCM() {
    int pcm_data_size = 0;
    AVFrame *frame = 0;
    while (isPlaying) {
        int ret = frames.pop(frame);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        /**
         * pcm的处理逻辑
         * 音频播放器的数据格式是我们自己在下面定义的
         *  而原始数据（待播放的音频pcm数据）重采样
         */
        int dst_nb_samples = static_cast<int>(av_rescale_rnd(
                swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples,
                out_sample_rate,
                frame->sample_rate,
                AV_ROUND_UP
        ));
        int samples_per_channel = swr_convert(
                swr_ctx,
                &out_buffers,
                dst_nb_samples,
                (const uint8_t **) frame->data,
                frame->nb_samples
        );
        pcm_data_size = samples_per_channel *
                        out_sample_size *
                        out_channels;
        //每一帧音频的时间
        audio_time = frame->best_effort_timestamp * av_q2d(time_base);
        //回调播放时间到UI
        if (callbackHelper) {
            callbackHelper->onProgress(THREAD_CHILD, audio_time);
        }
        break;
    }
    releaseAVFrame(&frame);
    return pcm_data_size;
}

/**
    不启用混响可以不用获取混音器接口
     获得混音器接口
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                             &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
    设置混响 ： 默认。
    SL_I3DL2_ENVIRONMENT_PRESET_ROOM: 室内
    SL_I3DL2_ENVIRONMENT_PRESET_AUDITORIUM : 礼堂 等
    const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
           outputMixEnvironmentalReverb, &settings);
    }
 */
