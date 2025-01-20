package com.gyso.player

import android.graphics.Bitmap
import android.media.MediaCodec
import android.text.TextUtils
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.camera.core.ImageProxy
import androidx.lifecycle.LifecycleOwner
import com.gyso.player.yuv.BitmapUtils
import pan.lib.camera_record.media.video.CameraPreviewInterface
import java.nio.ByteBuffer

class GySoPlayer(surfaceView: SurfaceView) : SurfaceHolder.Callback {
    private var onStatCallback: OnStatCallback? = null
    private var surfaceHolder: SurfaceHolder? = null
    private var isSurfaceReady = false
    private var streamManager: StreamManager? = null
    private var surfaceView:SurfaceView ? = null
    private var lastDataSource:String? = null
    init {
        setSurfaceView(surfaceView)
        SimpleH264TcpIpServer.instance.init()
    }

    fun addCameraControl(lifecycleOwner: LifecycleOwner){
        surfaceView?.let {
            val cameraPreviewInterface = createCameraPreviewInterface(this@GySoPlayer, it, onBitmapReady = { _ -> {}})
            val context = it.context
            streamManager = context?.let { it1 ->
                StreamManager(
                    it1,
                    lifecycleOwner = lifecycleOwner,
                    cameraPreviewInterface = cameraPreviewInterface
                )
            }
        }
    }

    fun play(dataSource: String?): GySoPlayer {
        stop()
        if (!TextUtils.isEmpty(dataSource)) {
            if (dataSource == CAMERA_BACK || dataSource == CAMERA_FRONT){
                streamManager?.start()
            }
            surfaceView?.let {
                prepareNative(dataSource)
                isPacketCallbackNative(true)
                setViewport(it.width, it.height)
                Log.i(TAG, "play--------------->: ${it.width} ${it.height}")
            }
        }
        lastDataSource = dataSource
        return this
    }

    fun syncH264FrameToServer(ip: String, port: Int) {

    }

    private fun stop() {
        streamManager?.stop()
        isPacketCallbackNative(false)
        stopNative()
    }

    private fun setSurfaceView(surfaceView: SurfaceView) {
        this.surfaceView = surfaceView
        if (null != surfaceHolder) {
            surfaceHolder!!.removeCallback(this)
        }
        surfaceHolder = surfaceView.holder
        surfaceHolder?.addCallback(this)
    }

    /**
     * jni反射调用接口
     */
    fun onPrepared() {
        if (null != onStatCallback) {
            onStatCallback!!.onPrepared()
        }
        delayMs(500)
        Thread {
            while (!isSurfaceReady) {
                delayMs(100)
            }
            startNative()
        }.start()
    }

    private fun delayMs(delayTime: Int) {
        try {
            Thread.sleep(delayTime.toLong())
        } catch (e: InterruptedException) {
            throw RuntimeException(e)
        }
    }

    /**
     * 底层解码回调
     * @param errorCode 错误码
     */
    fun onError(errorCode: Int) {
        Log.e(TAG, "onError: errorCode[$errorCode]")
        if (null != onStatCallback) {
            onStatCallback!!.onError(errorCode)
        }
    }

    /**
     * 获取
     * @param currentPlayTime 当前播放时间
     */
    fun onProgress(currentPlayTime: Int) {
//        Log.e(TAG, "onProgress:currentPlayTime["+currentPlayTime+"]");
        if (onStatCallback != null) {
            onStatCallback!!.onProgress(currentPlayTime)
        }
    }

    //如果设置了isPacketCallbackNative(true)就会统一回调播放的每帧H264 packet(NAL)
    @OptIn(ExperimentalStdlibApi::class)
    private fun onPacketCallback(packet:ByteArray){
        Log.i(TAG, "onPacketCallback: "+packet.size+"  "+packet.toHexString(0,packet.size,HexFormat.Default))
        SimpleH264TcpIpServer.instance.send(packet)
    }

    fun setOnStatCallback(onStatCallback: OnStatCallback?) {
        this.onStatCallback = onStatCallback
    }


    override fun surfaceCreated(holder: SurfaceHolder) {

    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.i(TAG, "surfaceChanged: setSurfaceNative")
        isSurfaceReady = true
        setSurfaceNative(surfaceHolder!!.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
    }

    val duration: Int
        /**
         * 获取视频时长
         * @return int
         */
        get() = durationNative

    /**
     * 进度设置
     */
    fun seek(playProgress: Int) {
        seekNative(playProgress)
    }

    interface OnStatCallback {
        //每次新播放都会回调
        fun onPrepared()
        //错误回调
        fun onError(errorCode: Int)
        fun onProgress(currentPlayTime: Int)
    }

    fun showCameraFrame(data:ByteArray){
        onPacketCallback(data)
        playCameraFrame(data)
    }

    /**
     * 本地方法接口
     */
    private external fun prepareNative(videoPath: String?)
    private external fun startNative()
    private external fun stopNative()
    private external fun releaseNative()
    private external fun setSurfaceNative(surface: Surface)
    private external fun seekNative(playProgress: Int)
    private external fun isPacketCallbackNative(isCallback: Boolean)
    private external fun playCameraFrame(data:ByteArray)
    private external fun setViewport(vpWidth:Int, vpHeight:Int)
    external fun yuvToNV21(width: Int, height: Int, byteBufferY: ByteBuffer,byteBufferYLength: Int,
        byteBufferU: ByteBuffer,byteBufferULength: Int,byteBufferV: ByteBuffer,byteBufferVLength: Int,): ByteArray

    private val durationNative: Int
        external get

    companion object {
        private val TAG: String = GySoPlayer::class.java.simpleName
        val CAMERA_FRONT: String = "CAMERA_FRONT"
        val CAMERA_BACK: String = "CAMERA_BACK"

        /* TODO 3.2.5 新增 --- start */ //准备过程错误码
        const val ERROR_CODE_FFMPEG_PREPARE: Int = 1000

        //播放过程错误码
        const val ERROR_CODE_FFMPEG_PLAY: Int = 2000

        //打不开视频
        const val FFMPEG_CAN_NOT_OPEN_URL: Int = (ERROR_CODE_FFMPEG_PREPARE - 1)

        //找不到媒体流信息
        const val FFMPEG_CAN_NOT_FIND_STREAMS: Int = (ERROR_CODE_FFMPEG_PREPARE - 2)

        //找不到解码器
        const val FFMPEG_FIND_DECODER_FAIL: Int = (ERROR_CODE_FFMPEG_PREPARE - 3)

        //无法根据解码器创建上下文
        const val FFMPEG_ALLOC_CODEC_CONTEXT_FAIL: Int = (ERROR_CODE_FFMPEG_PREPARE - 4)

        //根据流信息 配置上下文参数失败
        const val FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL: Int = (ERROR_CODE_FFMPEG_PREPARE - 5)

        //打开解码器失败
        const val FFMPEG_OPEN_DECODER_FAIL: Int = (ERROR_CODE_FFMPEG_PREPARE - 6)

        //没有音视频
        const val FFMPEG_NOMEDIA: Int = (ERROR_CODE_FFMPEG_PREPARE - 7)

        //读取媒体数据包失败
        const val FFMPEG_READ_PACKETS_FAIL: Int = (ERROR_CODE_FFMPEG_PLAY - 1)

        init {
            System.loadLibrary("gysotools")
        }
    }
}

private fun createCameraPreviewInterface(player: GySoPlayer, sreviewView: SurfaceView, onBitmapReady: (Bitmap) -> Unit): CameraPreviewInterface {
    return object : CameraPreviewInterface {
        val needSaveH264ToLocal = true // 是否保存h264到本地
        var width:Int = 0
        var height:Int = 0
        override fun getSurfaceView(): SurfaceView = sreviewView
        override fun onGetWidthAndHeight(width: Int, height: Int) {
            Log.i("CameraPreviewScreen", "onGetWidthAndHeight: $width, $height")
            this.width=width
            this.height=height
            sendWH()
        }

        override fun onNv21Frame(nv21: ByteArray, imageProxy: ImageProxy) {
            val bitmap = BitmapUtils.getBitmap(
                ByteBuffer.wrap(nv21),
                imageProxy.width,
                imageProxy.height,
                imageProxy.imageInfo.rotationDegrees
            )
            if (bitmap != null) {
                onBitmapReady(bitmap)
            }
//            Log.i("CameraPreviewScreen", "onNv21Frame: width=${imageProxy.width} height=${imageProxy.height}")
        }

        fun sendWH(){
            //发送 宽高
            val bf =ByteBuffer.allocate(4 + 1 + 4 + 4) //0x00 0x00 0x00 0x01 0xff int1_bytes int2_bytes
            bf.put(byteArrayOf(0x00, 0x00, 0x00, 0x01, 0xFF.toByte()))
            bf.putInt(width)
            bf.putInt(height)
            Log.i(
                "CameraPreviewScreen",
                "sendWH: width=$width, height=$height"
            )
//            VideoStreamServer.sendByteArray(bf.array())
        }

        override fun onSpsPpsVps(sps: ByteBuffer, pps: ByteBuffer?, vps: ByteBuffer?) {
            Log.i("CameraPreviewScreen", "onSpsPpsVps: ")
            "CameraPreviewScreen".logByteBufferContent("SPS", sps)
            "CameraPreviewScreen".logByteBufferContent("PPS", pps)
            "CameraPreviewScreen".logByteBufferContent("VPS", vps)//only for H265
            if (pps?.array() != null && vps?.array() != null) {
                player.showCameraFrame(pps.array())
                player.showCameraFrame(vps.array())
            }
        }

        override fun onVideoBuffer(h264Buffer: ByteBuffer, info: MediaCodec.BufferInfo) {
            // 处理 H264 缓冲区（如果需要）
            //Log.d("CameraPreviewScreen", "Received H264 Buffer")
            val data: ByteArray
            if (h264Buffer.hasArray()) {
                data = h264Buffer.array()
            } else {
                data = ByteArray(h264Buffer.remaining())
                h264Buffer.get(data)
            }
            player.showCameraFrame(data)
//            Log.i("ddd", "onVideoBuffer: len"+data.size)
//            h264Buffer.rewind()
//            "onVideoBuffer".logByteBufferContent("NAL", h264Buffer)
            if (needSaveH264ToLocal) {
                //FileUtil.writeBytesToFile(context = context, data, "test.h264")
            }
        }
    }
}

private fun String.logByteBufferContent(name: String, byteBuffer: ByteBuffer?) {
    if (byteBuffer == null) {
        Log.d(this, "$name is null")
        return
    }

    val byteData = ByteArray(byteBuffer.remaining())
    byteBuffer.rewind()
    byteBuffer.get(byteData)

    val hexString = byteData.joinToString(separator = " ") { "%02X".format(it) }
    Log.d(this, "$name: $hexString")
}