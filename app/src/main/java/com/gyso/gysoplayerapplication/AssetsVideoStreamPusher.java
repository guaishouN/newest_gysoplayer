package com.gyso.gysoplayerapplication;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.util.Arrays;

public class AssetsVideoStreamPusher {
    private final static String TAG = "H264TEST";
    private final Context context;
    private final String videoFileName;  // assets 目录下的视频文件名
    private final int serverPort;
    private ServerSocket serverSocket;
    private Socket clientSocket;
    private OutputStream outputStream;

    public AssetsVideoStreamPusher(Context context, String videoFileName, int serverPort) {
        this.context = context;
        this.videoFileName = videoFileName;
        this.serverPort = serverPort;
    }

    public void startServer() {
        new Thread(() -> {
            try {
                // 创建服务器，监听指定端口
                serverSocket = new ServerSocket(this.serverPort);
                Log.i(TAG, "Server started, waiting for client connection...");
//                new AssetsVideoStreamDecoder("127.0.0.1", this.serverPort);
                // 等待客户端连接
                clientSocket = serverSocket.accept();
                outputStream = clientSocket.getOutputStream();
                Log.i(TAG, "Client connected, start sending video stream...");

                // 开始发送视频流
                startStreaming_mp4();
//                new ExtractMpegFramesTest().testExtractMpegFrames();
            } catch (Exception e) {
                e.printStackTrace();
            } catch (Throwable e) {
                throw new RuntimeException(e);
            }
        }).start();
    }

    public void startStreaming_h264() {
        // 从 assets 获取文件的 FileDescriptor
        new Thread(() -> {
            try {
                AssetManager assetManager = context.getAssets();
                byte[] buffer = null;
                InputStream inputStream = assetManager.open("sps_pps.h264");
                int size = inputStream.available();
                buffer = new byte[size];
                inputStream.read(buffer);

                // 发送文件数据
                outputStream.write(buffer);
                outputStream.flush();
                // 关闭连接
                outputStream.close();
                serverSocket.close();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }).start();
    }

    public void startStreaming_mp4() throws IOException, InterruptedException {
        // 从 assets 获取文件的 FileDescriptor
        AssetFileDescriptor assetFileDescriptor = context.getAssets().openFd(videoFileName);
        MediaExtractor extractor = new MediaExtractor();
        extractor.setDataSource(assetFileDescriptor.getFileDescriptor(),
                assetFileDescriptor.getStartOffset(),
                assetFileDescriptor.getLength());

        // 找到视频轨道
        int videoTrackIndex = -1;
        for (int i = 0; i < extractor.getTrackCount(); i++) {
            MediaFormat format = extractor.getTrackFormat(i);
            String mime = format.getString(MediaFormat.KEY_MIME);
            if (mime.startsWith("video/")) {
                videoTrackIndex = i;
                break;
            }
        }

        if (videoTrackIndex < 0) {
            throw new IllegalArgumentException("No video track found in " + videoFileName);
        }
        extractor.selectTrack(videoTrackIndex);
        MediaFormat inputFormat = extractor.getTrackFormat(videoTrackIndex);

        // 提取 SPS 和 PPS
        ByteBuffer spsBuffer = inputFormat.getByteBuffer("csd-0");  // SPS
        ByteBuffer ppsBuffer = inputFormat.getByteBuffer("csd-1");  // PPS

        // 发送 SPS 和 PPS 数据帧
        assert spsBuffer != null;
        byte[] sps = new byte[spsBuffer.remaining()];
        spsBuffer.get(sps);
        Log.i(TAG, "send sps len["+sps.length+"]"+Arrays.toString(sps));
        outputStream.write(sps);
        outputStream.flush();


        assert ppsBuffer != null;
        byte[] pps = new byte[ppsBuffer.remaining()];
        ppsBuffer.get(pps);
        Log.i(TAG, "send pps len["+pps.length+"]"+Arrays.toString(pps));
        outputStream.write(pps);
        outputStream.flush();

        ByteBuffer inputBuffer = ByteBuffer.allocate(1024 * 1024); // 创建 1MB 缓冲区用于读取 H.264 帧
        int count =0;
        while (true) {
            int sampleSize = extractor.readSampleData(inputBuffer, 0);
            if (sampleSize < 0) {
                break;
            }

            // 将读取的数据转换为 H.264 数据帧
            byte[] h264Frame = new byte[sampleSize];
            inputBuffer.get(h264Frame);
            inputBuffer.clear();
//            Log.i(TAG, "send count={"+count+"} nal len["+h264Frame.length +"]"+ Arrays.toString(h264Frame));
            // 发送 H.264 帧到服务器
            outputStream.write(h264Frame);
            outputStream.flush();
            count++;
            Thread.sleep(40);
            // 前进到下一帧
            extractor.advance();
        }

        // 清理和关闭资源
        outputStream.close();
        serverSocket.close();
        extractor.release();
    }
}

