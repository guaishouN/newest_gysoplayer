package com.gyso.player

class NativeLib {

    /**
     * A native method that is implemented by the 'player' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    companion object {
        // Used to load the 'player' library on application startup.
        init {
            System.loadLibrary("player")
        }
    }
}