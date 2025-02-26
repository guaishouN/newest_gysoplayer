#include <jni.h>
#include <string>
#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <zconf.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
#include <stdint.h>
#include <libavutil/log.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>
#include <cstdint>
#include <cstring>
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <stdio.h>
#include <stdlib.h>
}

#include "GySoPlayer.h"
#include "macro.h"
#include "GysoTools.h"


JavaVM *javaVM = 0;
GySoPlayer *gysoplayer = 0;
ANativeWindow *window = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

jint JNI_OnLoad(JavaVM *vm, void *unused) {
    javaVM = vm;
    return JNI_VERSION_1_6;
}

GysoTools *ff = new GysoTools();

int play_h264_stream(const char *url) {
    int count = 0;
    AVFormatContext *format_ctx = NULL;
    int video_stream_index = -1;
    AVCodecContext *codec_ctx = NULL;
    AVPacket packet;
    AVFrame *frame = NULL;
    struct SwsContext *sws_ctx = NULL;
    int ret;

    // Register all formats and codecs
    avformat_network_init();

    // Open the input stream
    if (avformat_open_input(&format_ctx, url, NULL, NULL) < 0) {
        LOGD("Could not open input stream\n");
        return -1;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        LOGD("Could not find stream information\n");
        return -1;
    }

    // Find the first video stream
    for (int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_id == AV_CODEC_ID_H264) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        LOGD("Could not find H.264 video stream\n");
        return -1;
    }

    // Get the codec context
    codec_ctx = avcodec_alloc_context3(NULL);
    if (!codec_ctx) {
        LOGD("Could not allocate codec context\n");
        return -1;
    }

    // Copy codec parameters from the stream to the codec context
    ret = avcodec_parameters_to_context(codec_ctx,
                                        format_ctx->streams[video_stream_index]->codecpar);
    if (ret < 0) {
        LOGD("Could not copy codec parameters to context\n");
        return -1;
    }

    // Find the decoder for H.264
    const AVCodec *codec = avcodec_find_decoder(codec_ctx->codec_id);
    if (!codec) {
        LOGD("Codec not found\n");
        return -1;
    }

    // Open the decoder
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        LOGD("Could not open codec\n");
        return -1;
    }

    // Allocate frame for storing decoded data
    frame = av_frame_alloc();
    if (!frame) {
        LOGD("Could not allocate frame\n");
        return -1;
    }

    // Initialize packet
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    // Read and decode video frames
    while (av_read_frame(format_ctx, &packet) >= 0) {
        if (packet.stream_index == video_stream_index) {
            ret = avcodec_send_packet(codec_ctx, &packet);
            if (ret < 0) {
                LOGD("Error sending packet to decoder\n");
                return -1;
            }

            // Receive decoded frame
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                continue;  // Need more data or end of stream
            } else if (ret < 0) {
                LOGD("Error receiving frame from decoder\n");
                return -1;
            }
            LOGD("Decoded frame with count:%d  width: %d, height: %d\n", count, frame->width,
                 frame->height);
//            LOGD("PTS: %lld, DTS: %lld", frame->pts, frame->pkt_dts);
//            LOGD("Keyframe: %s", frame->key_frame ? "Yes" : "No");
            LOGD("Frame type: %c", av_get_picture_type_char(frame->pict_type));
            count++;
        }

        // Free the packet
        av_packet_unref(&packet);
    }

    // Clean up
    av_frame_free(&frame);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    return 0;
}

int parse_sps(uint8_t *sps_data, size_t sps_size, int *width, int *height) {
    LOGD("parse_sps: Start parsing SPS data %d", (int) sps_size);
    if (!sps_data || sps_size == 0 || !width || !height) {
        LOGD("Invalid input parameters");
        return -1;
    }

    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    AVCodecParserContext *parser;
    if (!codec) {
        LOGD("Failed to find H.264 decoder");
        return -1;
    }
    parser = av_parser_init(codec->id);
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        LOGD("Failed to allocate codec context");
        return -1;
    }

    // 打开解码器
    if (avcodec_open2(ctx, codec, NULL) < 0) {
        LOGD("Failed to open codec");
        avcodec_free_context(&ctx);
        return -1;
    }

    AVPacket *pPacket = av_packet_alloc();
    if (!pPacket) {
        LOGD("Failed to allocate AVPacket");
        avcodec_free_context(&ctx);
        return -1;
    }
    pPacket->data = sps_data;
    pPacket->size = sps_size; // 使用正确的 sps_size

    // 分配 AVFrame 以接收解码后的帧
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        LOGD("Failed to allocate AVFrame");
        av_packet_free(&pPacket);
        avcodec_free_context(&ctx);
        return -1;
    }
    int ret1 = av_parser_parse2(parser, ctx, &pPacket->data, &pPacket->size, sps_data, sps_size,
                                AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    // 发送数据包给解码器
    int ret = avcodec_send_packet(ctx, pPacket);
    if (ret < 0) {
        LOGD("Error sending packet to decoder");
        av_frame_free(&frame);
        av_packet_free(&pPacket);
        avcodec_free_context(&ctx);
        return -1;
    }

    // 接收解码帧
    LOGD("Waiting for decoded frames...");
    ret = avcodec_receive_frame(ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        LOGD("Need more data or end of stream");
        av_frame_free(&frame);
        av_packet_free(&pPacket);
        avcodec_free_context(&ctx);
        return -1;
    } else if (ret < 0) {
        LOGD("Error receiving frame from decoder");
        av_frame_free(&frame);
        av_packet_free(&pPacket);
        avcodec_free_context(&ctx);
        return -1;
    }

    // 打印解码后的帧的宽高信息
    LOGD("Decoded frame with width: %d, height: %d", frame->width, frame->height);
    LOGD(
            "Frame %c (%d) pts %d dts %d",
            av_get_picture_type_char(frame->pict_type),
            (int) (ctx->frame_num),
            (int) (frame->pts),
            (int) (frame->pkt_dts)
    );

    // 返回解析结果
    *width = frame->width;
    *height = frame->height;

    // 清理资源
    av_frame_free(&frame);
    av_packet_free(&pPacket);
    avcodec_free_context(&ctx);

    return 0;
}

extern "C"
JNIEXPORT jint

JNICALL
Java_com_gyso_videohandlerlib_GysoFfmpegTools_parseSPS(JNIEnv *env, jobject thiz,
                                                           jbyteArray sps_data,
                                                           jintArray dimensions) {
    LOGD("Java_com_gyso_videohandlerlib_GysoFfmpegTools_parseSPS 1 %s", "");
    jsize sps_size = env->GetArrayLength(sps_data);
    jbyte *sps_data_ptr = env->GetByteArrayElements(sps_data, nullptr);
    jsize dim_size = env->GetArrayLength(dimensions);
    if (dim_size < 2) {
        env->ReleaseByteArrayElements(sps_data, sps_data_ptr, 0);
        return -1;
    }
    LOGD("Java_com_gyso_videohandlerlib_GysoFfmpegTools_parseSPS 2 %d", dim_size);
    jint *dim_ptr = env->GetIntArrayElements(dimensions, nullptr);
    int width = 0, height = 0;
    int result = parse_sps(reinterpret_cast<uint8_t *>(sps_data_ptr), sps_size, &width, &height);
    dim_ptr[0] = width;
    dim_ptr[1] = height;
    env->ReleaseByteArrayElements(sps_data, sps_data_ptr, 0);
    env->ReleaseIntArrayElements(dimensions, dim_ptr, 0);
    return result;
}

extern "C"
JNIEXPORT jstring

JNICALL
Java_com_gyso_videohandlerlib_GysoFfmpegTools_mainTest(JNIEnv *env, jobject thiz) {
    std::string hello = "Hello from C++ main test";
    int rs = ff->sum(3);
    const char *avInfo = av_version_info();
    hello += ", av_version_info: ";
    hello += avInfo;
    hello += ", sum result: ";
    hello += std::to_string(rs);
    LOGD("Generated string: %s", hello.c_str());
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT jstring

JNICALL
Java_com_gyso_videohandlerlib_GysoFfmpegTools_mainStart(JNIEnv *env, jobject thiz) {
    // TODO: implement mainStart()
    const char *url = "tcp://172.26.4.37:8999";
    if (play_h264_stream(url) < 0) {
        fprintf(stderr, "Error playing H.264 stream\n");
    }
}

/**
 * 渲染图像到屏幕
 * @param src_data
 * @param width
 * @param height
 * @param src_lineSize
 */
void renderFrame(uint8_t *src_data, int width, int height, int src_lineSize, uint8_t *nv21_buffer,
                 int nv21_buffer_size) {
//    pthread_mutex_lock(&mutex);
//    LOGD("renderCallback  width=%d, height=%d, src_lineSize=%d, nv21_buffer_size=%d",
//         width,
//         height,
//         src_lineSize,
//         nv21_buffer_size);
    if (gysoplayer) {
        gysoplayer->callbackHelper->onYuv(THREAD_CHILD, width, height, nv21_buffer,
                                          nv21_buffer_size);
    }
//    if(!window){
//        pthread_mutex_unlock(&mutex);
//    }
//    //设置窗口属性
//    ANativeWindow_setBuffersGeometry(
//            window,
//            width,
//            height,
//            WINDOW_FORMAT_RGBA_8888
//    );
//    ANativeWindow_Buffer windowBuffer;
//    if(ANativeWindow_lock(window,&windowBuffer,nullptr)){
//        ANativeWindow_release(window);
//        window = nullptr;
//        pthread_mutex_unlock(&mutex);
//        return;
//    }
//    //填充buffer
//    auto *dst_data = static_cast<uint8_t *>(windowBuffer.bits);
//    int dst_lineSize = windowBuffer.stride*4;//RGBA
//    for (int i = 0; i < windowBuffer.height; ++i) {
//        //拷贝一行
//        memcpy(dst_data+i*dst_lineSize,
//               src_data+i*src_lineSize,
//               dst_lineSize
//        );
//    }
//    ANativeWindow_unlockAndPost(window);
//    pthread_mutex_unlock(&mutex);
}


extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_videohandlerlib_GysoFfmpegTools_prepareNative(JNIEnv
                                                                *env,
                                                                jobject thiz, jstring
                                                                path) {
    const char *filePath = env->GetStringUTFChars(path, 0);
    CallbackHelper *callbackHelper = new CallbackHelper(javaVM, env, thiz);
    gysoplayer = new GySoPlayer(filePath, callbackHelper);
    gysoplayer->
            setRenderCallback(renderFrame);
    gysoplayer->

            prepare();

    env->
            ReleaseStringUTFChars(path, filePath
    );
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_videohandlerlib_GysoFfmpegTools_startNative(JNIEnv
                                                              *env,
                                                              jobject thiz
) {
    if (gysoplayer) {
        gysoplayer->

                start();

    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_videohandlerlib_GysoFfmpegTools_stopNative(JNIEnv
                                                             *env,
                                                             jobject thiz
) {
    if (gysoplayer) {
        gysoplayer->

                stop();

    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_videohandlerlib_GysoFfmpegTools_releaseNative(JNIEnv
                                                                *env,
                                                                jobject thiz
) {
    pthread_mutex_lock(&mutex);
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
    pthread_mutex_unlock(&mutex);
    DELETE(gysoplayer);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_videohandlerlib_GysoFfmpegTools_setSurfaceNative(JNIEnv
                                                                   *env,
                                                                   jobject thiz,
                                                                   jobject
                                                                   surface) {
//    pthread_mutex_lock(&mutex);
//    //释放之前的显示窗口
//    if(window){
//        ANativeWindow_release(window);
//        window =0;
//    }
//    //创建新window
//    window = ANativeWindow_fromSurface(env,surface);
//    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_videohandlerlib_GysoFfmpegTools_seekNative(JNIEnv
                                                             *env,
                                                             jobject thiz,
                                                             jint
                                                             play_progress) {

    if (gysoplayer) {
        gysoplayer->
                seek(play_progress);
    }
}
extern "C"
JNIEXPORT jint
JNICALL
Java_com_gyso_videohandlerlib_GysoFfmpegTools_getDurationNative(JNIEnv *env,
                                                                    jobject
                                                                    thiz) {
    if (gysoplayer) {
        return gysoplayer->

                getDuration();

    }
    return 0;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_videohandlerlib_GysoFfmpegTools_setFrameCounter(JNIEnv
                                                                  *env,
                                                                  jobject thiz,
                                                                  jint
                                                                  framenum) {
    LOGD("setFrameCounter %d", (int) framenum);
    int tmp = (int) framenum;
    if (gysoplayer) {
        gysoplayer->setFrameNum(tmp);
    }
}