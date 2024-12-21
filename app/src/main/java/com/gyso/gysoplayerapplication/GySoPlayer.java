package com.gyso.gysoplayerapplication;

import android.text.TextUtils;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class GySoPlayer implements SurfaceHolder.Callback {
    private static String TAG = GySoPlayer.class.getSimpleName();
    /* TODO 3.2.5 新增 --- start */
    //准备过程错误码
    public static final int ERROR_CODE_FFMPEG_PREPARE = 1000;
    //播放过程错误码
    public static final int ERROR_CODE_FFMPEG_PLAY = 2000;
    //打不开视频
    public static final int FFMPEG_CAN_NOT_OPEN_URL = (ERROR_CODE_FFMPEG_PREPARE - 1);
    //找不到媒体流信息
    public static final int FFMPEG_CAN_NOT_FIND_STREAMS = (ERROR_CODE_FFMPEG_PREPARE - 2);
    //找不到解码器
    public static final int FFMPEG_FIND_DECODER_FAIL = (ERROR_CODE_FFMPEG_PREPARE - 3);
    //无法根据解码器创建上下文
    public static final int FFMPEG_ALLOC_CODEC_CONTEXT_FAIL = (ERROR_CODE_FFMPEG_PREPARE - 4);
    //根据流信息 配置上下文参数失败
    public static final int FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL = (ERROR_CODE_FFMPEG_PREPARE - 5);
    //打开解码器失败
    public static final int FFMPEG_OPEN_DECODER_FAIL = (ERROR_CODE_FFMPEG_PREPARE - 6);
    //没有音视频
    public static final int FFMPEG_NOMEDIA = (ERROR_CODE_FFMPEG_PREPARE - 7);
    //读取媒体数据包失败
    public static final int FFMPEG_READ_PACKETS_FAIL = (ERROR_CODE_FFMPEG_PLAY - 1);

    static {
        System.loadLibrary("gysotools");
    }
    private String dataSource;
    private OnStatCallback onStatCallback;
    private SurfaceHolder surfaceHolder;
    private boolean isSurfaceReady = false;
    public GySoPlayer(SurfaceView surfaceView){
        setSurfaceView(surfaceView);
    }


    public void play(String dataSource){
        if (!TextUtils.isEmpty(dataSource)){
            prepareNative(dataSource);
        }
    }

    public void stop(){
        stopNative();
    }
    public void release(){
        releaseNative();
    }
    private void setSurfaceView(SurfaceView surfaceView){
        if (null!=surfaceHolder){
            surfaceHolder.removeCallback(this);
        }
        surfaceHolder=surfaceView.getHolder();
        surfaceHolder.addCallback(this);
    }

    /**
     * jni反射调用接口
     */
    public void onPrepared(){
        if (null!=onStatCallback){
            onStatCallback.onPrepared();
        }
        delayMs(500);
        new Thread(()->{
            while (!isSurfaceReady){
                delayMs(100);
            }
            startNative();
        }).start();
    }

    private void delayMs(int delayTime){
        try {
            Thread.sleep(delayTime);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * 底层解码回调
     * @param errorCode 错误码
     */
    public void onError(int errorCode){
        Log.e(TAG, "onError: errorCode["+errorCode+"]");
        if (null!=onStatCallback){
            onStatCallback.onError(errorCode);
        }
    }

    /**
     * 获取
     * @param currentPlayTime 当前播放时间
     */
    public void onProgress(int currentPlayTime){
//        Log.e(TAG, "onProgress:currentPlayTime["+currentPlayTime+"]");
        if(onStatCallback!=null){
            this.onStatCallback.onProgress(currentPlayTime);
        }

    }

    public void setOnStatCallback(OnStatCallback onStatCallback) {
        this.onStatCallback = onStatCallback;
    }


    @Override
    public void surfaceCreated(SurfaceHolder holder) {

    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.i(TAG, "surfaceChanged: setSurfaceNative");
        isSurfaceReady = true;
        setSurfaceNative(surfaceHolder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    /**
     * 获取视频时长
     * @return int
     */
    public int getDuration() {
        return getDurationNative();
    }
    /**
     * 进度设置
     */
    public void seek(int playProgress) {
        seekNative(playProgress);
    }

    interface OnStatCallback{
        void onPrepared();
        void onError(int errorCode);
        void onProgress(int currentPlayTime);
        void onFinished();
    }

    /**
     * 本地方法接口
     */
    private native void prepareNative(String videoPath);
    private native void startNative();
    private native void stopNative();
    private native void releaseNative();
    private native void setSurfaceNative(Surface surface);
    private native void seekNative(int playProgress);
    private native int getDurationNative();
}
