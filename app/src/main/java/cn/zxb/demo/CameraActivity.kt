package cn.zxb.demo

import android.os.Bundle
import android.os.Environment
import androidx.appcompat.app.AppCompatActivity
import cn.zxb.media.recorder.CameraRecorder

class CameraActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_camera)
        var recorder = CameraRecorder()
        recorder.recorder(Environment.getExternalStorageDirectory().absolutePath + "/tt.mp4")
    }
}
