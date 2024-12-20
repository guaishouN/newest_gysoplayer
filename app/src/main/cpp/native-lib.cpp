#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <zconf.h>
#include "GySoPlayer.h"

#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"GysoPlayer",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"GysoPlayer",FORMAT,##__VA_ARGS__);
extern "C"{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

JavaVM *javaVM = 0;
GySoPlayer *gysoplayer = 0;
ANativeWindow *window = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
jint JNI_OnLoad(JavaVM *vm, void *unused) {
    javaVM = vm;
    return JNI_VERSION_1_6;
}

/**
 * 渲染图像到屏幕
 * @param src_data
 * @param width
 * @param height
 * @param src_lineSize
 */
void renderFrame(uint8_t *src_data, int width, int height, int src_lineSize){
    LOGI("renderFrame width=%d, height=%d src_lineSize=%d", width, height, src_lineSize)
//    pthread_mutex_lock(&mutex);
//    if(!window){
//        pthread_mutex_unlock(&mutex);
//    }
//    //设置窗口属性
//    ANativeWindow_setBuffersGeometry(
//            window,
//            width,
//            height,
//            WINDOW_FORMAT_RGBA_8888
//            );
//    ANativeWindow_Buffer windowBuffer;
//    if(ANativeWindow_lock(window,&windowBuffer,0)){
//        ANativeWindow_release(window);
//        window = 0;
//        pthread_mutex_unlock(&mutex);
//        return;
//    }
//    //填充buffer
//    uint8_t *dst_data = static_cast<uint8_t *>(windowBuffer.bits);
//    int dst_lineSize = windowBuffer.stride*4;//RGBA
//    for (int i = 0; i < windowBuffer.height; ++i) {
//        //拷贝一行
//        memcpy(dst_data+i*dst_lineSize,
//                src_data+i*src_lineSize,
//                dst_lineSize
//                );
//    }
//    ANativeWindow_unlockAndPost(window);
//    pthread_mutex_unlock(&mutex);
}


extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_gysoplayerapplication_GySoPlayer_prepareNative(JNIEnv *env, jobject thiz,jstring path) {
    const char * filePath = env->GetStringUTFChars(path,0);
    CallbackHelper *callbackHelper =new CallbackHelper(javaVM,env,thiz);
    gysoplayer = new GySoPlayer(filePath, callbackHelper);
    gysoplayer->setRenderCallback(renderFrame);
    gysoplayer->prepare();
    env->ReleaseStringUTFChars(path,filePath);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_gysoplayerapplication_GySoPlayer_startNative(JNIEnv *env, jobject thiz) {
    if(gysoplayer){
        gysoplayer->start();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_gysoplayerapplication_GySoPlayer_stopNative(JNIEnv *env, jobject thiz) {
    if(gysoplayer){
        gysoplayer->stop();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_gysoplayerapplication_GySoPlayer_releaseNative(JNIEnv *env, jobject thiz) {
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
Java_com_gyso_gysoplayerapplication_GySoPlayer_setSurfaceNative(JNIEnv *env, jobject thiz,
                                                                 jobject surface) {
    pthread_mutex_lock(&mutex);
    //释放之前的显示窗口
    if(window){
        ANativeWindow_release(window);
        window =0;
    }
    //创建新window
    window = ANativeWindow_fromSurface(env,surface);
    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_gysoplayerapplication_GySoPlayer_seekNative(JNIEnv *env,
        jobject thiz,
        jint play_progress) {

    if(gysoplayer){
        gysoplayer->seek(play_progress);
    }
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_gyso_gysoplayerapplication_GySoPlayer_getDurationNative(JNIEnv *env,
        jobject thiz) {
    if(gysoplayer){
        return gysoplayer->getDuration();
    }
    return 0;
}
