//
// Created by GuaishouN on 2020/4/25.
//

#ifndef GYSOFFMPEGAPPLICATION_GYSOPLAYER_H
#define GYSOFFMPEGAPPLICATION_GYSOPLAYER_H

#include "VideoChannel.h"
#include "CallbackHelper.h"
#include <android/log.h>
#include <android/native_window_jni.h>
#include <zconf.h>
#include "macro.h"
#include <pthread.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>
}


class GySoPlayer {
    friend void *task_stop(void *args);

public:
    GySoPlayer(const char *string, CallbackHelper *callbackHelper);

    ~GySoPlayer();

    CallbackHelper *callbackHelper;

    void prepare();

    void _prepare();

    void start();

    void _start();

    void setRenderCallback(RenderCallback renderCallback);

    void seek(int progress);

    int getDuration();

    void stop();

    void setFrameNum(int fNum);

private:
    char *videoPath;
    bool isPlaying;
    VideoChannel *videoChannel;
    pthread_t pThread_prepare;
    pthread_t pThread_start;
    AVFormatContext *avFormatContext;
    RenderCallback renderCallback;
    int duration;
    pthread_mutex_t seek_mutex;
    pthread_t pid_stop;
};


#endif //GYSOFFMPEGAPPLICATION_GYSOPLAYER_H
