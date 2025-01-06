package com.gyso.gysoplayerapplication

import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.gyso.gysoplayerapplication.databinding.ActivityMainBinding
import com.gyso.player.AssetsVideoStreamPusher
import com.gyso.player.GySoPlayer
import java.io.File


class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    var permissionGrander: Boolean = false
    lateinit var permissionUtil: PermissionUtil
    lateinit var gySoPlayer: GySoPlayer
    lateinit var handlerThread: HandlerThread
    lateinit var handler: Handler
    companion object {
        const val TAG = "MainActivity"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        permissionUtil = PermissionUtil()
        permissionUtil.init(context = this, {
            permissionGrander = true
        }, {
            permissionGrander = false
        })
        handlerThread = HandlerThread("gysoplayer handler")
        handlerThread.start()
        handler = Handler(handlerThread.looper)
        if (Build.VERSION.SDK_INT >= 33) {
            if (permissionUtil.hasStoragePermission(this, PermissionUtil.TOTAL_PERMISSION_13)) {
                permissionGrander = true
            }
        } else {
            if (permissionUtil.hasStoragePermission(this, PermissionUtil.TOTAL_PERMISSION)) {
                permissionGrander = true
            }
        }
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
        binding.sampleText.text = "video play develop"
        Thread {
            val assetsVideoStreamer =
                AssetsVideoStreamPusher(
                    applicationContext,
                    "demo.mp4",
                    10002
                )
            assetsVideoStreamer.startServer()
        }.start()

        prepareVideo()
    }

    /**
     * 准备video
     */
    private fun prepareVideo() {
        val filePath = Environment.getExternalStorageDirectory().absolutePath + File.separator + "/jpgfiles/output_020.jpg";
        Log.i(TAG, "prepareVideo: videofile filePath=" + filePath)
        val file = File(filePath)
        if (!file.exists()) {
            Log.e(TAG, "playVideo: file not exist")
            Toast.makeText(
                this@MainActivity,
                "文件不存在",
                Toast.LENGTH_SHORT
            ).show()
            return
        }
        Log.i(TAG, "prepareVideo: videofile len=" + file.length())
        gySoPlayer = GySoPlayer(binding.surfaceview)
//        gySoPlayer.addCameraControl(lifecycleOwner = this)
//        gySoPlayer.play("tcp://127.0.0.1:10002")
//        gySoPlayer.play("tcp://172.26.4.25:8997")
//        gySoPlayer.play(GySoPlayer.CAMERA_FRONT)
        gySoPlayer.play(file.absolutePath)
//        handler.post{
//            Thread.sleep(5*1000)
//            gySoPlayer.play(GySoPlayer.CAMERA_FRONT)
////            Thread.sleep(5*1000)
////            gySoPlayer.play(file.absolutePath)
//            Thread.sleep(5*1000)
////            gySoPlayer.stop()
////            gySoPlayer = GySoPlayer(binding.surfaceview)
////            gySoPlayer.addCameraControl(lifecycleOwner = this)
//            Log.i(TAG, "prepareVideo: ----------------last")
//            var filePath = Environment.getExternalStorageDirectory().absolutePath + File.separator + "final1.mp4";
//            var file = File(filePath)
//            gySoPlayer.play(file.absolutePath)
//            Thread.sleep(5*1000)
//            filePath = Environment.getExternalStorageDirectory().absolutePath + File.separator + "/jpgfiles/output_020.jpg";
//            file = File(filePath)
//            gySoPlayer.play(file.absolutePath)
//            Thread.sleep(5*1000)
//            filePath = Environment.getExternalStorageDirectory().absolutePath + File.separator + "final2.mp4";
//            file = File(filePath)
//            gySoPlayer.play(file.absolutePath)
//            Thread.sleep(5*1000)
//            filePath = Environment.getExternalStorageDirectory().absolutePath + File.separator + "demo.mp4";
//            file = File(filePath)
//            gySoPlayer.play(file.absolutePath)
//            Thread.sleep(5*1000)
//            filePath = Environment.getExternalStorageDirectory().absolutePath + File.separator + "output.mp4";
//            file = File(filePath)
//            gySoPlayer.play(file.absolutePath)
//        }
    }
}