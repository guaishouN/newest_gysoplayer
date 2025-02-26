//
// Created by GuaishouN on 2020/4/25.
//

#include "CallbackHelper.h"
#include "macro.h"

CallbackHelper::CallbackHelper(JavaVM *pVm, JNIEnv *pEnv, jobject pJobject){
    this->javaVM = pVm;
    this->pEnv = pEnv;
    this->instance = pEnv->NewGlobalRef(pJobject);
    //找出MethodId
    jclass clazz = pEnv->GetObjectClass(pJobject);
    jmd_prepared_methodId = pEnv->GetMethodID(clazz, "onPrepared", "()V");
    jmd_error_methodId = pEnv->GetMethodID(clazz,"onError","(I)V");
    jmd_progress_methodId = pEnv->GetMethodID(clazz,"onProgress","(I)V");
    jmd_yuv_methodId = pEnv->GetMethodID(clazz,"onYuv","([BIII)V");
}
CallbackHelper::~CallbackHelper(){
    javaVM = 0;
    pEnv = 0;
    pEnv->DeleteGlobalRef(instance);
    jmd_prepared_methodId = 0;
}
void CallbackHelper::onPrepared(int threadType) {
    if(threadType==THREAD_MAIN){
        //主线程
        pEnv->CallVoidMethod(instance,jmd_prepared_methodId);
    }else{
        //子线程
        JNIEnv *env_child;
        javaVM->AttachCurrentThread(&env_child,0);
        env_child->CallVoidMethod(instance,jmd_prepared_methodId);
        javaVM->DetachCurrentThread();
    }
}

void CallbackHelper::onError(int threadType, int errorCode) {
    if(threadType==THREAD_MAIN){
        //主线程
        pEnv->CallVoidMethod(instance,jmd_error_methodId,errorCode);
    }else{
        //子线程
        JNIEnv *env_child;
        javaVM->AttachCurrentThread(&env_child,0);
        env_child->CallVoidMethod(instance,jmd_error_methodId,errorCode);
        javaVM->DetachCurrentThread();
    }
}

void CallbackHelper::onProgress(int threadType, int currentPlayTime) {
    if(jmd_progress_methodId){
        if(threadType==THREAD_MAIN){
            //主线程
            pEnv->CallVoidMethod(instance,jmd_progress_methodId,currentPlayTime);
        }else{
            //子线程
            JNIEnv *env_child;
            javaVM->AttachCurrentThread(&env_child,0);
            env_child->CallVoidMethod(instance,jmd_progress_methodId,currentPlayTime);
            javaVM->DetachCurrentThread();
        }
    }
}


void CallbackHelper::onYuv(int threadType, int width, int height, uint8_t *nv21_buffer, int nv21_buffer_size) {
    if(threadType==THREAD_MAIN){
        jbyteArray nv21ByteArray = pEnv->NewByteArray(nv21_buffer_size);
        pEnv->SetByteArrayRegion(nv21ByteArray, 0, nv21_buffer_size, (jbyte *)nv21_buffer);
        //主线程
        pEnv->CallVoidMethod(instance,jmd_yuv_methodId, nv21ByteArray, width,  height, nv21_buffer_size);
    }else{
        //子线程
        JNIEnv *env_child;
        javaVM->AttachCurrentThread(&env_child,0);
        jbyteArray nv21ByteArray = env_child->NewByteArray(nv21_buffer_size);
        env_child->SetByteArrayRegion(nv21ByteArray, 0, nv21_buffer_size, (jbyte *)nv21_buffer);
        env_child->CallVoidMethod(instance, jmd_yuv_methodId, nv21ByteArray, width,  height, nv21_buffer_size);
        javaVM->DetachCurrentThread();
    }
}

