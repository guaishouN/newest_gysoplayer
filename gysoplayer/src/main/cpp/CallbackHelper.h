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
    void onPacketCallback(int threadType, uint8_t *data, int data_size);
    JavaVM *javaVM = nullptr;
    JNIEnv *pEnv = nullptr;
    jobject instance;
    jmethodID jmd_prepared_methodId;
    jmethodID jmd_error_methodId;
    jmethodID jmd_progress_methodId=nullptr;
    jmethodID jmd_packet_callback_methodId=nullptr;
};


#endif //GYSOFFMPEGAPPLICATION_CALLBACKHELPER_H
