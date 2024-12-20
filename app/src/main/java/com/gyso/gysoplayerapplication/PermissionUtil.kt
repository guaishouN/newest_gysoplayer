package com.gyso.gysoplayerapplication

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat

class PermissionUtil {
    companion object {
        val TOTAL_PERMISSION = listOf(
            "android.permission.WRITE_EXTERNAL_STORAGE",
            "android.permission.READ_EXTERNAL_STORAGE",
            "android.permission.CAMERA",
            Manifest.permission.RECORD_AUDIO
        )
        val TOTAL_PERMISSION_13 = listOf(
            "android.permission.READ_MEDIA_AUDIO",
            "android.permission.READ_MEDIA_IMAGES",
            "android.permission.READ_MEDIA_VIDEO",
            "android.permission.CAMERA",
            Manifest.permission.RECORD_AUDIO
        )
    }

    private var launcher: ActivityResultLauncher<Array<String>>? = null

    fun init(
        context: ComponentActivity,
        onGranter: () -> Unit,
        onRefuse: (checkFailList: ArrayList<String>) -> Unit
    ) {
        launcher = context.registerForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions()
        ) { result: Map<String, Boolean> ->
            var isGranter = true
//            var isRefuse = false
//            var listRefuse = ArrayList<String>()
            var listCheckFail = ArrayList<String>()
            result.forEach { (permission, result) ->
//                if (ActivityCompat.shouldShowRequestPermissionRationale(this, permission)) {
//                    listRefuse.add(permission)
//                    isRefuse = true
//                }
                if (!result) {
                    listCheckFail.add(permission)
                    isGranter = false
                }
            }
            if (isGranter) {
                onGranter()
                Log.e("permission", "result:success")
            }
//            else if (isRefuse) {
//                Log.e(
//                    "permission",
//                    "result:refuse refuse refuse:${listRefuse} checkFail:${listCheckFail}"
//                )
//            }
            else {
                onRefuse(listCheckFail)
                Log.e("permission", "result:check fail  checkFail:${listCheckFail}")
            }
        }
    }

    fun hasStoragePermission(
        context: Activity,
        permissions: List<String>,
    ): Boolean {
        var isGranter = true
        permissions.forEach { permission ->
            if (ContextCompat.checkSelfPermission(
                    context,
                    permission
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                isGranter = false
            }
        }
        if (!isGranter) {
            launcher?.launch(
                permissions.toTypedArray()
            )
        }
        return isGranter
    }
}