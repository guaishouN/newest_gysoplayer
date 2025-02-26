package com.gyso.videohandlerlib;

import android.text.TextUtils;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;

public class GysoFfmpegTools implements SurfaceHolder.Callback {
    private final static String TAG = "GysoFfmpegTools";
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
    private final ExecutorService exe = Executors.newFixedThreadPool(30);
    private ServerSocket serverSocket;
    private Socket ffmpegInnerSocket = null;
    private OutputStream ffmpegOutSteam = null;
    private boolean isStop = true;
    private final AtomicInteger connectCount = new AtomicInteger(1);

    static {
        System.loadLibrary("gysotools_service");
    }

    private String dataSource;
    private OnStatCallback onStatCallback;
    private SurfaceHolder surfaceHolder;

    public GysoFfmpegTools() {
        exe.submit(this::init);
    }

    private void init() {
        this.dataSource = "tcp://127.0.0.1:8999";
        isStop = false;
        exe.submit(() -> {
            try {
                serverSocket = new ServerSocket(8999);
                Log.i(TAG, "Local Server started, waiting for client connection...");
                exe.submit(this::prepare);
                ffmpegInnerSocket = serverSocket.accept();
                ffmpegOutSteam = ffmpegInnerSocket.getOutputStream();
                while (!isStop) {
                    Socket newClient = serverSocket.accept();
                    Log.i(TAG, "Local Server got connect = " + newClient.getInetAddress() + ", connectCount="+connectCount.getAndAdd(1));
                    InputStream inputStream = newClient.getInputStream();
                    exe.submit(()->handlerDataInput(newClient, inputStream));
                }
            } catch (Exception e) {
                Log.e(TAG, "init exception: ", e);
                try {
                    stop();
                } catch (IOException ex) {
                    Log.e(TAG, "stop! on init exception: ", e);
                }
            }
        });
    }

    private void handlerDataInput(Socket outerSocket, InputStream inputStream) {
        Log.i(TAG, "handlerDataInput: begin");
        try {
            if (outerSocket == null ||
                    ffmpegInnerSocket == null ||
                    outerSocket.isClosed() ||
                    ffmpegInnerSocket.isClosed()) {
                return;
            }
            byte[] buffer = new byte[1024 * 1024];
            int bytesRead;
            while ((bytesRead = inputStream.read(buffer)) != -1) {
                ffmpegOutSteam.write(buffer, 0, bytesRead);
                ffmpegOutSteam.flush();
                if (outerSocket.isClosed() || ffmpegInnerSocket.isClosed()) {
                    Log.i(TAG, "handlerDataInput: outerSocket or ffmpegInnerSocket closed!!");
                    break;
                }
            }
            Log.i(TAG, "handlerDataInput: finish data push!!");
        } catch (Exception e) {
            Log.i(TAG, "loopReceive: finished one data channel!", e);
        }
        connectCount.getAndAdd(-1);
    }

    public void prepare() {
        Log.i(TAG, "prepare: " + dataSource);
        if (!TextUtils.isEmpty(dataSource)) {
            prepareNative(dataSource);
        }
    }

    public void start() {
        Log.i(TAG, "start");
        startNative();
    }

    public void stop() throws IOException {
        isStop = true;
        if (ffmpegInnerSocket != null) {
            ffmpegInnerSocket.close();
        }
        if (ffmpegOutSteam != null) {
            ffmpegOutSteam.close();
        }
        stopNative();
    }

    public void release() {
        releaseNative();
    }

    void setSurfaceView(SurfaceView surfaceView) {
        if (null != surfaceHolder) {
            surfaceHolder.removeCallback(this);
        }
        surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(this);
    }


    /**
     * jni反射调用接口
     */
    public void onPrepared() {
        Log.i(TAG, "onPrepared: ");
        if (null != onStatCallback) {
            onStatCallback.onPrepared();
        }
        start();
    }

    /**
     * 底层解码回调
     *
     * @param errorCode 错误码
     */
    public void onError(int errorCode) {
        Log.e(TAG, "onError: errorCode[" + errorCode + "]");
        if (null != onStatCallback) {
            onStatCallback.onError(errorCode);
        }
    }

    /**
     * 获取
     *
     * @param currentPlayTime 当前播放时间
     */
    public void onProgress(int currentPlayTime) {
        Log.e(TAG, "onProgress:currentPlayTime[" + currentPlayTime + "]");
        if (onStatCallback != null) {
            this.onStatCallback.onProgress(currentPlayTime);
        }

    }

    /**
     * 回调帧yuv数据
     *
     * @param nv21
     * @param width
     * @param height
     * @param dataSize
     */
    public void onYuv(byte[] nv21, int width, int height, int dataSize) {
        if (onStatCallback != null) {
            this.onStatCallback.onYuv(nv21, width, height, dataSize);
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
        setSurfaceNative(surfaceHolder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    /**
     * 获取视频时长
     *
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

    public interface OnStatCallback {
        default void onPrepared() {
        }

        ;

        default void onError(int errorCode) {
        }

        ;

        default void onProgress(int currentPlayTime) {
        }

        ;

        default void onYuv(byte[] nv21, int width, int height, int dataSize) {
        }

        ;
    }

    /**
     * 本地方法接口
     */
    private native String mainTest();

    private native int parseSPS(byte[] spsData, int[] dimensionsy);

    public native String mainStart();

    public native void setFrameCounter(int frameNum);

    private native void prepareNative(String videoPath);

    private native void startNative();

    private native void stopNative();

    private native void releaseNative();

    private native void setSurfaceNative(Surface surface);

    private native void seekNative(int playProgress);

    private native int getDurationNative();
}
