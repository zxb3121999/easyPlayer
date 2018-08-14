package cn.jx.easyplayer

import android.Manifest
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Environment
import android.support.v4.app.ActivityCompat
import android.support.v7.app.AppCompatActivity
import android.view.KeyEvent
import kotlinx.android.synthetic.main.activity_main.easy_video_view
import kotlinx.android.synthetic.main.activity_main.record
import java.io.File

class MainActivity : AppCompatActivity() {
  private var isRecorder = false
  private var isRunning = false

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.activity_main)
    //videoView.setVideoPath("mms://video.fjtv.net/setv");
    //videoView.setVideoPath("http://clips.vorwaerts-gmbh.de/big_buck_bunny.mp4");
    if (ActivityCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
      ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE), 0)
    } else {
      //videoView.setVideoPath("http://replay.live.tianya.cn/live-151574941100022906--20180112173015.mp4");
      easy_video_view.setVideoPath(Environment.getExternalStorageDirectory().absolutePath + "/ffmpeg/" + "test.mp4")
      //videoView.setVideoPath("/storage/emulated/0/tencent/MicroMsg/Weixin/test.mp4");
      //videoView.setVideoPath("rtmp://live.hkstv.hk.lxdns.com/live/hks");
      //videoView.setVideoPath("http://hdl3a.douyucdn.cn/live/64609rdkw7Kej3bb_900.flv?wsAuth=0dbce31573d7c2a42efc9d3b1e2efe11&token=app-android1-0-64609-b8ea9667eba374a3133e415a4fe74082&logo=0&expire=0&did=10786b63-0b2b-4204-a149-1181f7cb3480&ver=v3.7.1&channel=31&pt=1");
    }
    record.setOnClickListener {
      if (isRecorder) {
        isRecorder = false
        easy_video_view.stopRecorder()
      } else {
        isRecorder = true
        val root = Environment.getExternalStorageDirectory().absolutePath + "/ffmpeg/"
        val dir = File(root)
        if (!dir.exists()) {
          dir.mkdirs()
        }
        easy_video_view.startRecorder(Environment.getExternalStorageDirectory().absolutePath + "/ffmpeg/" + System.currentTimeMillis() + ".mp4")
      }
    }
  }

  override fun onPause() {
    super.onPause()
    easy_video_view.onActivityPause()
  }

  override fun onResume() {
    super.onResume()
    easy_video_view.onActivityResume()
  }

  override fun onDestroy() {
    easy_video_view.onDestory()
    super.onDestroy()
  }

  override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
    if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
      easy_video_view.setVideoPath("http://replay.live.tianya.cn/live-151574941100022906--20180112173015.mp4")
    }
  }

  override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent) {
    if (requestCode == 0 && resultCode == Activity.RESULT_OK) {

    }
  }

  override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
    return super.onKeyDown(keyCode, event)
  }

  companion object {
    init {
      System.loadLibrary("player-lib")
    }

    private val TAG = MainActivity::class.java.simpleName
  }
}
