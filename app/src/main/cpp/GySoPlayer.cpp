//
// Created by GuaishouN on 2020/4/25.
//
#include "GySoPlayer.h"

GySoPlayer::GySoPlayer(const char *string, CallbackHelper *callbackHelper) {
    audioChannel = nullptr;
    videoChannel = nullptr;
    //获取视频文件路径
    videoPath = new char[strlen(string) + 1];
    strcpy(videoPath, string);
    this->callbackHelper = callbackHelper;
    isPlaying = 0;
    pthread_mutex_init(&seek_mutex, 0);
    const AVCodec *codec = nullptr;
    void *iter = nullptr;

//    LOGI("Supported codecs:\n")
//    while ((codec = av_codec_iterate(&iter))) {
//        LOGI("list ffmpeg codec--->%s  %d  %s   %s",
//             (av_codec_is_encoder(codec) ? "Encoder" : "Decoder"),
//             codec->id,
//             codec->name,
//             codec->long_name);
//    }
}


GySoPlayer::~GySoPlayer() {
    DELETE(videoPath);
    DELETE(callbackHelper);
    pthread_mutex_destroy(&seek_mutex);
    avformat_network_deinit();
}

void *prepareInChildThread(void *args) {
    GySoPlayer *gySoPlayer = static_cast<GySoPlayer *>(args);
    gySoPlayer->_prepare();
    return 0;
}

void GySoPlayer::prepare() {
    pthread_create(&pThread_prepare, 0, prepareInChildThread, this);
}

void GySoPlayer::show_img(AVCodecContext *pContext, AVFrame *frame) {
    //播放
    uint8_t *dst_data[4];
    int dst_linesize[4];
    float scaleX = (float) pContext->width / (float) winWidth;
    float scaleY = (float) pContext->height / (float) winHeight;
    float scale = fmax(scaleX, scaleY);
    int newWidth = (int) (pContext->width / scale);
    int newHeight = (int) (pContext->height / scale);
    int offset_width = (winWidth - newWidth) / 2;
    int offset_height = (winHeight - newHeight) / 2;
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
    sws_scale(
            swsContext,
            frame->data,
            frame->linesize,
            0,
            pContext->height,
            dst_data,
            dst_linesize
    );
    //宽高数据
    renderCallback(
            dst_data[0],
            newWidth,
            newHeight,
            dst_linesize[0],
            offset_width,
            offset_height
    );
    LOGI("winWidth=%d winHeight=%d  newWidth=%d newHeight=%d frame->linesize=%d offset_widht=%d offset_height=%d",
         winWidth, winHeight, newWidth, newHeight, frame->linesize, offset_width, offset_height)
    av_freep(&dst_data[0]);
    sws_freeContext(swsContext);
}

int GySoPlayer::deal_picture_file() {
    int ret = 0;
    // 查找第一条流，可能是图片
    AVStream *stream = avFormatContext->streams[0];
    AVCodecParameters *codecParams = stream->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) {
        LOGE("Image codec not found codec_id=%d", codecParams->codec_id);
        if (callbackHelper) {
            callbackHelper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL);
        }
        return ret;
    }

    // 创建解码器上下文
    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        LOGE("Failed to allocate codec context");
        return ret;
    }
    if (avcodec_parameters_to_context(codecCtx, codecParams) < 0) {
        LOGE("Failed to copy codec parameters to context");
        return ret;
    }
    if (avcodec_open2(codecCtx, codec, NULL) < 0) {
        LOGE("Failed to open codec");
        return ret;
    }

    // 读取并解码图片
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    while (av_read_frame(avFormatContext, packet) >= 0) {
        ret = avcodec_send_packet(codecCtx, packet);
        if (ret < 0) {
            LOGE("Error sending packet for decoding");
            break;
        }
        while (ret >= 0) {
            ret = avcodec_receive_frame(codecCtx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                LOGE("Error during decoding");
                break;
            }

            LOGI("Decoded image frame: width=%d, height=%d", frame->width, frame->height);
            // 将解码的图像帧传递给渲染模块
            show_img(codecCtx,frame);
            break;
        }
        av_packet_unref(packet);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&avFormatContext);
    return ret;
}


void GySoPlayer::_prepare() {
    LOGI("prepare %s", videoPath);
    avformat_network_init();
    //文件上下文，打开文件
    av_log_set_level(AV_LOG_DEBUG);
    avFormatContext = avformat_alloc_context();
//    AVDictionary * opt = NULL;
//    av_dict_set(&opt,"timeout","3000000",0);
    const char *fileExtension = strrchr(videoPath, '.');
    if (fileExtension && (strcasecmp(fileExtension, ".mp4") == 0)){
        FILE *f = fopen(videoPath, "rb");
        if (!f) {
            LOGE("Could not open %s\n", videoPath);
        } else {
            LOGE("sucesssfully open %s\n", videoPath);
        }
    }

    int ret = avformat_open_input(&avFormatContext, videoPath, nullptr, nullptr);

    if (ret < 0) {
        LOGE("prepare open ERROR %d", ret);
        if (callbackHelper) {
            callbackHelper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL);
        }
        return;
    }
    // 判断是否为图片格式
    if (fileExtension && (strcasecmp(fileExtension, ".png") == 0
                          || strcasecmp(fileExtension, ".jpg") == 0
                          || strcasecmp(fileExtension, ".jpeg") == 0
                          || strcasecmp(fileExtension, ".webp") == 0
    )) {
        LOGI("Detected image file: %s", videoPath);
        //start后直接显示
        if (callbackHelper) {
            callbackHelper->onPrepared(THREAD_CHILD);
        }
        return;
    }

    //判断文件有没流
    ret = avformat_find_stream_info(avFormatContext, nullptr);
    if (ret < 0) {
        LOGE("prepare find stream[0] ERROR %d", ret);
        if (callbackHelper) {
            callbackHelper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS);
        }
        return;
    }
    //获取的 duration 单位是：秒
    duration = static_cast<int>(avFormatContext->duration / AV_TIME_BASE);
    LOGI("prepare get duration %d", duration);
    //查找视频流和音频流
    for (int i = 0; i < avFormatContext->nb_streams; i++) {
        AVStream *stream = avFormatContext->streams[i];
        AVCodecParameters *avCodecParameters = stream->codecpar;
        const AVCodec *avCodec = avcodec_find_decoder(avCodecParameters->codec_id);
        if (!avCodec) {
            LOGE("prepare get avCodec ERROR %d", ret);
            if (callbackHelper) {
                callbackHelper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL);
            }
            return;
        }
        AVCodecContext *avCodecContext = avcodec_alloc_context3(avCodec);
        if (!avCodecContext) {
            LOGE("prepare get avCodecContext ERROR %d", ret);
            if (callbackHelper) {
                callbackHelper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL);
            }
            return;
        }
        ret = avcodec_parameters_to_context(avCodecContext, avCodecParameters);
        if (ret < 0) {
            LOGE("prepare parameters_to_context ERROR %d", ret);
            if (callbackHelper) {
                callbackHelper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL);
            }
            return;
        }
        ret = avcodec_open2(avCodecContext, avCodec, nullptr);
        if (ret < 0) {
            LOGE("prepare open avCodec ERROR %d", ret);
            if (callbackHelper) {
                callbackHelper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL);
            }
            return;
        }
        AVMediaType type = avCodecParameters->codec_type;
        AVRational time_base = stream->time_base;
        if (type == AVMEDIA_TYPE_VIDEO) {
            //如果是封面 跳过
            if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                LOGI("prepare get video");
                continue;
            }
            LOGI("prepare get video1");
            AVRational frame_rate = stream->avg_frame_rate;
            int fps = static_cast<int>(av_q2d(frame_rate));
            videoChannel = new VideoChannel(i, avCodecContext, time_base, fps);
            videoChannel->setRenderCallback(renderCallback);
            //如果没有音频就在视频中回调进度给UI
            if (!duration) {
                //直播 不需要回调进度给Java
                videoChannel->setCallbackHelper(callbackHelper);
            }
            LOGI("prepare get video2");
        } else {
            LOGI("prepare get audio1");
            audioChannel = new AudioChannel(i, avCodecContext, time_base);
            //以音频的时间为准回调
            //如果是直播则不需要回调
            if (duration) {
                audioChannel->setCallbackHelper(callbackHelper);
            }
            LOGI("prepare get audio2");
        }
    }
    LOGI("prepare  finish1");
    if (!videoChannel && !audioChannel) {
        if (callbackHelper) {
            callbackHelper->onError(THREAD_CHILD, FFMPEG_NOMEDIA);
        }
        return;
    }
    if (callbackHelper) {
        callbackHelper->onPrepared(THREAD_CHILD);
    }
    LOGI("prepare finish!!")
}


void *startInChildThread(void *args) {
    LOGI("startInChildThread!!")
    GySoPlayer *gySoPlayer = static_cast<GySoPlayer *>(args);
    gySoPlayer->_start();
    return 0;
}

/**
 * 开始播放
 */
void GySoPlayer::start() {
    isPlaying = 1;
    if (videoChannel) {
        videoChannel->setAudioChannel(audioChannel);
        videoChannel->setWinWidthAndHeight(this->winWidth, this->winHeight);
        videoChannel->start();
    }
    if (audioChannel) {
        LOGI("audioChannel->start() audioChannel=%d")
        audioChannel->start();
    }
    pthread_create(&pThread_start, 0, startInChildThread, this);
}

void GySoPlayer::setWinWidthAndHeight(int _winWidth, int _winHeight) {
    LOGI("GySoPlayer setWinWidthAndheight _winWidth=%d _winHeight=%d", _winWidth, _winHeight)
    this->winWidth = _winWidth;
    this->winHeight = _winHeight;
}

/**
 * 在子线程中解码
 * 开始播放
 */
void GySoPlayer::_start() {
    LOGI("_start");
    const char *fileExtension = strrchr(videoPath, '.');
    // 如果为显示图片，则显示后就返回
    if (fileExtension && (strcasecmp(fileExtension, ".png") == 0
                          || strcasecmp(fileExtension, ".jpg") == 0
                          || strcasecmp(fileExtension, ".jpeg") == 0
                          || strcasecmp(fileExtension, ".webp") == 0
    )) {
        LOGI("start show  image file: %s", videoPath);
        deal_picture_file();
        return;
    }
    while (isPlaying) {
        /**
         * 控制队列大小，等待队列中的数据被消费(音频视频都要等)
         */
//        LOGI("VideoChannel::video_decode videoChannel->packets.size()=%d", videoChannel->packets.size())
        if (videoChannel && videoChannel->packets.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        if (audioChannel && audioChannel->packets.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        //avpacket 可能是音频或视频
        pthread_mutex_lock(&seek_mutex);
        AVPacket *packet = av_packet_alloc();
        pthread_mutex_unlock(&seek_mutex);
        int ret = av_read_frame(avFormatContext, packet);
        if (!ret) {
            //视频
            if (videoChannel && videoChannel->stream_index == packet->stream_index) {
                videoChannel->packets.push(packet);
                //音频
            } else if (audioChannel && audioChannel->stream_index == packet->stream_index) {
                audioChannel->packets.push(packet);
            }
        } else if (ret == AVERROR_EOF) {
            //表示读取完毕
            LOGE("avFormatContext finished file reading!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!11")
            break;
        } else {
            break;
        }
    }
    //释放
    isPlaying = 0;
    if (videoChannel) {
        videoChannel->stop();
    }
    if (audioChannel) {
        audioChannel->stop();
    }
}

void GySoPlayer::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

void GySoPlayer::seek(int progress) {
    if (progress < 0 || progress > duration) {
        return;
    }
    if (!audioChannel && !videoChannel) {
        return;
    }
    if (!avFormatContext) {
        return;
    }
    //用户可能拖来拖去，同步问题
    pthread_mutex_lock(&seek_mutex);
    int ret = av_seek_frame(
            avFormatContext,
            -1,
            progress * AV_TIME_BASE,
            AVSEEK_FLAG_BACKWARD
    );
    if (ret < 0) {
        if (callbackHelper) {
            callbackHelper->onError(THREAD_CHILD, ret);
        }
        return;
    }
    //如果还在解码或播放，那么先停下在播放
    if (audioChannel) {
        //avcodec_flush_buffers(audioChannel->pContext);
        audioChannel->frames.setWork(0);
        audioChannel->packets.setWork(0);
        audioChannel->packets.clear();
        audioChannel->frames.clear();
        audioChannel->frames.setWork(1);
        audioChannel->packets.setWork(1);
    }
    if (videoChannel) {
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
void *task_stop(void *args) {
    GySoPlayer *gySoPlayer = static_cast<GySoPlayer *>(args);
    gySoPlayer->isPlaying = 0;
    pthread_join(gySoPlayer->pThread_prepare, nullptr);
    pthread_join(gySoPlayer->pThread_start, nullptr);
    if (gySoPlayer->avFormatContext) {
        avformat_close_input(&gySoPlayer->avFormatContext);
        avformat_free_context(gySoPlayer->avFormatContext);
        gySoPlayer->avFormatContext = nullptr;
    }
    if(gySoPlayer->audioChannel){
        DELETE(gySoPlayer->audioChannel);
    }
    if(gySoPlayer->videoChannel){
        DELETE(gySoPlayer->videoChannel);
    }
    DELETE(gySoPlayer);
    return nullptr;
}

void GySoPlayer::stop() {
    callbackHelper = 0;
    if (audioChannel) {
        audioChannel->callbackHelper = 0;
    }
    if (videoChannel) {
        videoChannel->callbackHelper = 0;
    }
    pthread_create(&pid_stop, 0, task_stop, this);
}
