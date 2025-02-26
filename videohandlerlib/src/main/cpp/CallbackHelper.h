//
// Created by GuaishouN on 2020/4/25.
//

#ifndef GYSOFFMPEGAPPLICATION_CALLBACKHELPER_H
#define GYSOFFMPEGAPPLICATION_CALLBACKHELPER_H
#include <jni.h>

class CallbackHelper {
public:
    CallbackHelper(JavaVM *pVm, JNIEnv *pEnv, jobject pJobject);
    ~CallbackHelper();
    void onPrepared(int threadType);
    void onError(int threadType, int errorCode);
    void onProgress(int threadType, int currentPlayTime);
    void onYuv(int threadType, int width, int height, uint8_t *nv21_buffer, int nv21_buffer_size);
    JavaVM *javaVM;
    JNIEnv *pEnv;
    jobject instance;
    jmethodID jmd_prepared_methodId;
    jmethodID jmd_error_methodId;
    jmethodID jmd_progress_methodId=0;
    jmethodID jmd_yuv_methodId;
};


#endif //GYSOFFMPEGAPPLICATION_CALLBACKHELPER_H
