package cn.jx.easyplayer

import android.os.Bundle
import android.support.design.widget.Snackbar
import android.support.v7.app.AppCompatActivity
import cn.jx.easyplayerlib.transform.TransformUtils
import cn.jx.easyplayerlib.util.RawAssetsUtil
import kotlinx.android.synthetic.main.activity_transform.progressBar
import kotlinx.android.synthetic.main.activity_transform.start

class TransformActivity : AppCompatActivity() {
  private var isRunning = false
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.activity_transform)
    start.setOnClickListener {
      if (!isRunning){
        isRunning = true
        val transformUtils = TransformUtils()
        transformUtils.setOnProgressLintener { progress ->
          progressBar.progress = progress
        }
        transformUtils.setCompletedListener { isRunning = false }
        transformUtils.setOnErrorListener { errorCode, ffmpegCode, msg ->
          isRunning = false
          Snackbar.make(it,String.format("%s:errorCode:%d",msg,errorCode),Snackbar.LENGTH_SHORT).show()
        }
        val assetsPath = RawAssetsUtil.getAssetsFilePath(this@TransformActivity, "add_wire.png")
        transformUtils.transform("/storage/emulated/0/tencent/MicroMsg/Weixin/test.mp4", "/storage/emulated/0/test.mp4", 0, 0, 0, 0, 0, String.format("movie=%s[wm];[in]boxblur=2:1:cr=0:ar=0,colorchannelmixer=.3:.4:.3:0:.3:.4:.3:0:.3:.4:.3[main];[main][wm]overlay=20:20[out]", assetsPath))
      }
    }
  }
}
