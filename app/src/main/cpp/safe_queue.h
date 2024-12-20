//
// Created by GuaishouN on 2020/5/10.
//

#ifndef GYSOFFMPEGAPPLICATION_SAFE_QUEUE_H
#define GYSOFFMPEGAPPLICATION_SAFE_QUEUE_H

#include <queue>
#include <pthread.h>
using namespace std;
template <typename T>
class SafeQueue{
public:
    typedef void (*ReleaseCallback)(T *);
    typedef void (*SyncCallback)(queue<T> &);
    SafeQueue(){
        pthread_mutex_init(&mutex,0);
        pthread_cond_init(&cond,0);
    }
    ~SafeQueue(){
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }
    /**
     * 入队
     * @param value
     */
    void push(T value){
        pthread_mutex_lock(&mutex);
        if(work){
            q.push(value);
            pthread_cond_signal(&cond);
        } else {
            if(releaseCallback){
                releaseCallback(&value);
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    int pop(T &value){
        pthread_mutex_lock(&mutex);
        int ret = 0;
        while(work && q.empty()){
            pthread_cond_wait(&cond,&mutex);
        }
        if(!q.empty()){
            value = q.front();
            q.pop();
            ret = 1;
        }
        pthread_mutex_unlock(&mutex);
        return ret;
    }

    void setWork(int work){
        pthread_mutex_lock(&mutex);
        this->work= work;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }
    int empty(){
        return q.empty();
    }
    int size(){
        return q.size();
    }

    void clear(){
        pthread_mutex_lock(&mutex);
        unsigned int size = q.size();
        for (int i = 0; i < size; ++i) {
            T value = q.front();
            if(releaseCallback){
                releaseCallback(&value);
            }
            q.pop();
        }
        pthread_mutex_unlock(&mutex);
    }
    /**
     * 同步操作
     */
    void sync(){
        pthread_mutex_lock(&mutex);
        syncCallback(q);
        pthread_mutex_unlock(&mutex);
    }

    void setReleaseCallback(ReleaseCallback releaseCallback){
        this->releaseCallback = releaseCallback;
    }

    void setSyncCallback(SyncCallback syncCallback){
        this->syncCallback = syncCallback;
    }

private:
    queue<T> q;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int work;
    ReleaseCallback releaseCallback;
    SyncCallback syncCallback;
};
#endif
