package com.gyso.gysoplayerapplication

import android.annotation.SuppressLint
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.View
import android.widget.SeekBar
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.gyso.gysoplayerapplication.GySoPlayer.OnStatCallback
import com.gyso.gysoplayerapplication.databinding.ActivityMainBinding
import java.io.File


class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    var permissionGrander: Boolean = false
    lateinit var permissionUtil: PermissionUtil
    lateinit var gySoPlayer: GySoPlayer

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
        val filePath = Environment.getExternalStorageDirectory().absolutePath + File.separator + "final1.mp4";
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
        gySoPlayer.play(file.absolutePath)
//        gySoPlayer.play("tcp://127.0.0.1:10002")
    }
}