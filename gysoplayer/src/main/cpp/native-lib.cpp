#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <zconf.h>
#include "GySoPlayer.h"
#include <jni.h>
#include <malloc.h>
#include <cstring>

#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"GysoPlayer",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"GysoPlayer",FORMAT,##__VA_ARGS__);
extern "C"{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

JavaVM *javaVM = nullptr;
GySoPlayer *gysoplayer = nullptr;
ANativeWindow *window = nullptr;
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
void renderFrame(uint8_t *src_data, int width, int height, int src_lineSize, int offset_width, int offset_height){
//    LOGI("renderFrame width=%d, height=%d src_lineSize=%d, offset_width=%d, offset_height=%d",
//         width, height, src_lineSize, offset_width, offset_height)
    pthread_mutex_lock(&mutex);
    if(!window){
        pthread_mutex_unlock(&mutex);
        LOGE("renderFrame 0&&&&&&&&")
        return;
    }
    int windowWidth = ANativeWindow_getWidth(window);
    int windowHeight = ANativeWindow_getHeight(window);

    //设置窗口属性
    int result = ANativeWindow_setBuffersGeometry(
            window,
            windowWidth,
            windowHeight,
            WINDOW_FORMAT_RGBA_8888
            );
    if (result != 0) {
        LOGE("Failed to set buffers geometry, error code: %d", result);
        pthread_mutex_unlock(&mutex);
        return;
    }
    ANativeWindow_Buffer windowBuffer;
    if(ANativeWindow_lock(window,&windowBuffer,nullptr)){
        ANativeWindow_release(window);
        window = nullptr;
        pthread_mutex_unlock(&mutex);
        LOGI("renderFrame 3")
        return;
    }
    //填充buffer
    uint8_t *dst_data = static_cast<uint8_t *>(windowBuffer.bits);
    int dst_lineSize = windowBuffer.stride*4;//RGBA
    // 清空窗口缓冲区（黑色背景）
    memset(dst_data, 0, windowBuffer.height * dst_lineSize);

    int target_size = fmin(src_lineSize,dst_lineSize);
    uint8_t *start_position = dst_data+offset_width*4;
    for (int i = 0; i < height; ++i) {
        memcpy(start_position+(i+offset_height)*dst_lineSize,
                src_data+i*src_lineSize,
               target_size
                );
    }
    ANativeWindow_unlockAndPost(window);
    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT void
JNICALL
Java_com_gyso_player_GySoPlayer_playCameraFrame(JNIEnv *env, jobject thiz,
                                                       jbyteArray data) {
//    LOGI("Java_com_gyso_player_GySoPlayer_playCameraFrame");
    jsize sps_size = env->GetArrayLength(data);
    jbyte *sps_data_ptr = env->GetByteArrayElements(data, nullptr);
    if(gysoplayer){
        gysoplayer->playCameraFrame(reinterpret_cast<uint8_t *>(sps_data_ptr), sps_size);
    }
    env->ReleaseByteArrayElements(data, sps_data_ptr, 0);
}


extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_player_GySoPlayer_prepareNative(JNIEnv *env, jobject thiz,jstring path) {
    const char * filePath = env->GetStringUTFChars(path,0);
    CallbackHelper *callbackHelper =new CallbackHelper(javaVM,env,thiz);
    gysoplayer = new GySoPlayer(filePath, callbackHelper);
    gysoplayer->setRenderCallback(renderFrame);
    gysoplayer->prepare();
    env->ReleaseStringUTFChars(path,filePath);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_player_GySoPlayer_startNative(JNIEnv *env, jobject thiz) {
    LOGI("startNative setWinWidthAndHeight")
    if(gysoplayer){
        pthread_mutex_lock(&mutex);
        if (window) {
            LOGI("startNative setWinWidthAndHeight111")
            int windowWidth = ANativeWindow_getWidth(window);
            int windowHeight = ANativeWindow_getHeight(window);
            gysoplayer->setWinWidthAndHeight(windowWidth, windowHeight);
        }
        pthread_mutex_unlock(&mutex);
        gysoplayer->start();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_player_GySoPlayer_stopNative(JNIEnv *env, jobject thiz) {
    if(gysoplayer){
        LOGI("Java_com_gyso_player_GySoPlayer_stopNative")
        gysoplayer->stop();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_player_GySoPlayer_releaseNative(JNIEnv *env, jobject thiz) {
    pthread_mutex_lock(&mutex);
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
    pthread_mutex_unlock(&mutex);
    DELETE(gysoplayer);
    gysoplayer = nullptr;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_player_GySoPlayer_setSurfaceNative(JNIEnv *env, jobject thiz,
                                                                 jobject surface) {
    LOGI("setSurfaceNative")
    pthread_mutex_lock(&mutex);
    //释放之前的显示窗口
    if(window){
        ANativeWindow_release(window);
        window =0;
    }
    //创建新window
    window = ANativeWindow_fromSurface(env,surface);
    if(gysoplayer){
        int windowWidth = ANativeWindow_getWidth(window);
        int windowHeight = ANativeWindow_getHeight(window);
        gysoplayer->setWinWidthAndHeight(windowWidth, windowHeight);
    }
    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_player_GySoPlayer_setViewport(JNIEnv *env, jobject thiz,
                                                                jint vpWidht, jint vpHeight) {

    pthread_mutex_lock(&mutex);
    if(gysoplayer && window){
        int windowWidth = ANativeWindow_getWidth(window);
        int windowHeight = ANativeWindow_getHeight(window);
        gysoplayer->setWinWidthAndHeight(windowWidth, windowHeight);
    }
    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_player_GySoPlayer_seekNative(JNIEnv *env,
        jobject thiz,
        jint play_progress) {
    if(gysoplayer){
        gysoplayer->seek(play_progress);
    }
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_gyso_player_GySoPlayer_getDurationNative(JNIEnv *env,
        jobject thiz) {
    if(gysoplayer){
        return gysoplayer->getDuration();
    }
    return 0;
}


extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_gyso_player_GySoPlayer_yuv420ToNV21(JNIEnv *env, jobject thiz, jint width,
                                                   jint height, jobject byte_buffer_y,
                                                   jint byte_buffer_y_length,
                                                   jobject byte_buffer_u,
                                                   jint byte_buffer_u_length,
                                                   jobject byte_buffer_v,
                                                   jint byte_buffer_v_length) {

    auto *y_buffer = (jbyte *) env->GetDirectBufferAddress(byte_buffer_y);
    auto *u_buffer = (jbyte *) env->GetDirectBufferAddress(byte_buffer_u);
    auto *v_buffer = (jbyte *) env->GetDirectBufferAddress(byte_buffer_v);

    if (y_buffer != nullptr && u_buffer != nullptr && v_buffer != nullptr) {

        auto *nv21Array = static_cast< jbyte *>(malloc(sizeof(jbyte) * width * height * 3 / 2));

        memcpy(nv21Array, y_buffer, byte_buffer_y_length);

        for (int i = 0; i < byte_buffer_u_length; i++) {
            nv21Array[byte_buffer_y_length + i * 2] = v_buffer[i];
            nv21Array[byte_buffer_y_length + i * 2 + 1] = u_buffer[i];
        }

        jbyteArray nv21Data = env->NewByteArray(width * height * 3 / 2);
        env->SetByteArrayRegion(nv21Data, 0, width * height * 3 / 2, nv21Array);

        free(nv21Array);

        return nv21Data;
    }
    return nullptr;
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_gyso_player_GySoPlayer_yuvToNV21(JNIEnv *env, jobject thiz, jint width,
                                                jint height, jobject byte_buffer_y,
                                                jint byte_buffer_y_length,
                                                jobject byte_buffer_u,
                                                jint byte_buffer_u_length,
                                                jobject byte_buffer_v,
                                                jint byte_buffer_v_length) {


    auto *y_buffer = (jbyte *) env->GetDirectBufferAddress(byte_buffer_y);
    auto *u_buffer = (jbyte *) env->GetDirectBufferAddress(byte_buffer_u);
    auto *v_buffer = (jbyte *) env->GetDirectBufferAddress(byte_buffer_v);
    if (y_buffer != nullptr && u_buffer != nullptr && v_buffer != nullptr) {

        auto *nv21Array = static_cast< jbyte *>(malloc(sizeof(jbyte) * width * height * 3 / 2));

        memcpy(nv21Array, y_buffer, byte_buffer_y_length);
        memcpy(nv21Array + byte_buffer_y_length, v_buffer, byte_buffer_v_length);
        nv21Array[byte_buffer_y_length + byte_buffer_v_length] = u_buffer[byte_buffer_u_length - 1];

        jbyteArray nv21Data = env->NewByteArray(width * height * 3 / 2);
        env->SetByteArrayRegion(nv21Data, 0, width * height * 3 / 2, nv21Array);

        free(nv21Array);

        return nv21Data;
    }
    return nullptr;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_gyso_player_GySoPlayer_isPacketCallbackNative(JNIEnv *env, jobject thiz,
                                                       jboolean is_callback) {
    bool isPacketCallbackEnabled = (is_callback == JNI_TRUE);
    if (gysoplayer){
        gysoplayer->isPacketCallbackEnabled = isPacketCallbackEnabled;
    }
}