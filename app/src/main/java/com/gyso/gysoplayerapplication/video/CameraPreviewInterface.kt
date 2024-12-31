package pan.lib.camera_record.media.video

import android.media.MediaCodec
import android.view.SurfaceView
import androidx.camera.core.ImageProxy
import java.nio.ByteBuffer


interface CameraPreviewInterface {
    fun getSurfaceView(): SurfaceView

    fun onGetWidthAndHeight(width:Int, height:Int)

    fun onNv21Frame(nv21: ByteArray, imageProxy: ImageProxy) {}

    fun onSpsPpsVps(sps: ByteBuffer, pps: ByteBuffer?, vps: ByteBuffer?)

    fun onVideoBuffer(h264Buffer: ByteBuffer, info: MediaCodec.BufferInfo)
}
