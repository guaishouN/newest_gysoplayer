package pan.lib.camera_record.media.video

import android.annotation.SuppressLint
import android.content.Context
import android.util.Log
import android.util.Size
import android.view.Surface
import androidx.camera.core.CameraControl
import androidx.camera.core.CameraSelector
import androidx.camera.core.ExperimentalGetImage
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.Preview
import androidx.camera.core.resolutionselector.ResolutionSelector
import androidx.camera.core.resolutionselector.ResolutionStrategy
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import com.google.common.util.concurrent.ListenableFuture
import com.gyso.gysoplayerapplication.yuv.YuvUtil
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

@SuppressLint("MissingPermission")
class CameraRecorder(
    private val context: Context,
    private val lifecycleOwner: LifecycleOwner,
    private val cameraPreviewInterface: CameraPreviewInterface
) {

    private var lensFacing: Int = CameraSelector.LENS_FACING_FRONT
    private lateinit var cameraProviderFuture: ListenableFuture<ProcessCameraProvider>
    private val isRecording = AtomicBoolean(false)
    private var isEncoderInitialized = false


    private val videoEncoder = VideoEncoder(cameraPreviewInterface)

    fun startRecording() {
        if (isRecording.get()) return // 已经在录制中

        isRecording.set(true)
        startCameraPreview()
    }

    fun stopRecording() {
        if (!isRecording.get()) return // 未在录制中
        isRecording.set(false)
        videoEncoder.stop()
    }

    fun switchCamera() {
        lensFacing = CameraSelector.LENS_FACING_FRONT
        startCameraPreview()
    }

    private fun bindPreview(cameraProvider: ProcessCameraProvider, imageAnalysis: ImageAnalysis) {
        val preview: Preview = Preview.Builder().build()
        val cameraSelector: CameraSelector = CameraSelector.Builder()
            .requireLensFacing(lensFacing)
            .build()

//        val surfaceHolder = cameraPreviewInterface.getSurfaceView().holder
//        val ctx = cameraPreviewInterface.getSurfaceView().context
//        preview.setSurfaceProvider { request ->
//            val surface: Surface = surfaceHolder.surface
//            request.provideSurface(surface, ContextCompat.getMainExecutor(ctx)) {
//                Log.e("sssssssssssssssssssssssss", "sssssssssssssssssssssssss")
//            }
//        }

        cameraProvider.unbindAll()
        cameraProvider.bindToLifecycle(
            lifecycleOwner,
            cameraSelector,
            imageAnalysis,
            preview
        )
    }

    @androidx.annotation.OptIn(ExperimentalGetImage::class)
    private fun startCameraPreview() {
        val resolutionSelector = ResolutionSelector.Builder().setResolutionStrategy(
            ResolutionStrategy(
                Size(720, 1080),
                ResolutionStrategy.FALLBACK_RULE_CLOSEST_LOWER
            )
        ).build()

        val imageAnalysis = ImageAnalysis.Builder()
            .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_YUV_420_888)
            .setResolutionSelector(resolutionSelector)
            .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST) // 非阻塞式，保留最新的图像
            .build()

        imageAnalysis.setAnalyzer(Executors.newSingleThreadExecutor()) { imageProxy ->
            if (!isRecording.get()) {
                imageProxy.close()
                return@setAnalyzer
            }
            if (!isEncoderInitialized) {
//                cameraPreviewInterface.onGetWidthAndHeight(imageProxy.height, imageProxy.width)
                videoEncoder.init(context, imageProxy.width, imageProxy.height)
                videoEncoder.start()
                isEncoderInitialized = true
            }
            val nv21 = YuvUtil.YUV_420_888toNV21(imageProxy.image)
            val nv12 = ByteArray(imageProxy.width * imageProxy.height * 3 / 2)

            YuvUtil.NV21ToNV12(
                nv21,
                nv12,
                imageProxy.width,
                imageProxy.height
            )

//            cameraPreviewInterface.onNv21Frame(nv21, imageProxy)
            videoEncoder.encode(nv12)
            imageProxy.close()
        }

        cameraProviderFuture = ProcessCameraProvider.getInstance(context)
        cameraProviderFuture.addListener({
            val cameraProvider = cameraProviderFuture.get()
            bindPreview(cameraProvider, imageAnalysis)
        }, ContextCompat.getMainExecutor(context))
    }
}
