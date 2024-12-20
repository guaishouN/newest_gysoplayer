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
//        Thread {
//            val assetsVideoStreamer =
//                AssetsVideoStreamPusher(
//                    applicationContext,
//                    "demo.mp4",
//                    10002
//                )
//            assetsVideoStreamer.startServer()
//        }.start()

        prepareVideo()
    }

    /**
     * 准备video
     */
    private fun prepareVideo() {
        val filePath = Environment.getExternalStorageDirectory().absolutePath + File.separator + "demo.mp4";
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
        gySoPlayer = GySoPlayer(file.absolutePath)
//        gySoPlayer = GySoPlayer("tcp://127.0.0.1:10002")
        gySoPlayer.setSurfaceView(binding.surfaceview)
        gySoPlayer.prepare()
        gySoPlayer.setOnStatCallback(object : OnStatCallback {
            override fun onPrepared() {
                runOnUiThread {
                    Toast.makeText(
                        this@MainActivity,
                        "准备播放完毕",
                        Toast.LENGTH_SHORT
                    ).show()
                }
                gySoPlayer.start()
            }

            override fun onError(errorCode: Int) {
                runOnUiThread {
                    Toast.makeText(
                        this@MainActivity,
                        "播放视频出错了!",
                        Toast.LENGTH_SHORT
                    ).show()
                }
            }

            @SuppressLint("SetTextI18n")
            override fun onProgress(currentPlayTime: Int) {
                //底层返回进度更新

            }
        })
    }

    /**
     * 点击播放
     * @param view view
     */
    fun playVideo(view: View?) {
        gySoPlayer.start()
    }

    @SuppressLint("SetTextI18n")
    fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {

    }

    private fun getMinutes(duration: Int): String {
        val minutes = duration / 60
        if (minutes <= 9) {
            return "0$minutes"
        }
        return "" + minutes
    }

    private fun getSeconds(duration: Int): String {
        val seconds = duration % 60
        if (seconds <= 9) {
            return "0$seconds"
        }
        return "" + seconds
    }
}