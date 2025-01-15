//
// Created by GuaishouN on 2020/4/25.
//

#ifndef GYSOFFMPEGAPPLICATION_GYSOPLAYER_H
#define GYSOFFMPEGAPPLICATION_GYSOPLAYER_H

#include "AudioChannel.h"
#include "VideoChannel.h"
#include "CallbackHelper.h"
#include <android/log.h>
#include <android/native_window_jni.h>
#include <zconf.h>
#include "macro.h"
#include <pthread.h>

extern "C"{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
}


class GySoPlayer {
   friend void *task_stop(void *args);
public:
    GySoPlayer(const char *string,CallbackHelper *callbackHelper);
    ~GySoPlayer();

    void prepare();

    void _prepare();
    void show_frame(AVCodecContext *pContext, AVFrame *frame);

    void start();

    void _start();

    void setRenderCallback(RenderCallback renderCallback);

    void seek(int progress);

    int getDuration();

    void stop();

    int deal_picture_file();

    void setWinWidthAndHeight(int _winWidth, int _winHeight);

    int winWidth = 500;
    int winHeight = 500;

    int playCameraFrame(uint8_t *data, size_t data_size);

    int prepareForCamera();
    bool isPacketCallbackEnabled = false;
    CallbackHelper *callbackHelper = nullptr;
private:
    char * videoPath = nullptr;
    bool isPlaying;
    VideoChannel *videoChannel = nullptr;
    AudioChannel *audioChannel = nullptr;
    pthread_t pThread_prepare;
    pthread_t pThread_start;
    AVFormatContext *avFormatContext = nullptr;
    RenderCallback renderCallback;
    int duration;
    pthread_mutex_t seek_mutex;
    pthread_t pid_stop;
};


#endif //GYSOFFMPEGAPPLICATION_GYSOPLAYER_H
