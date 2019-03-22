package cn.zxb.demo

import android.content.Intent
import android.os.Bundle
import android.view.View.OnClickListener
import androidx.appcompat.app.AppCompatActivity
import kotlinx.android.synthetic.main.activity_launcher.*

class LauncherActivity : AppCompatActivity() {

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.activity_launcher)
    play.setOnClickListener(listener)
    recorder.setOnClickListener(listener)
    transform.setOnClickListener(listener)
    second.setOnClickListener(listener)
  }
  private var listener:OnClickListener = OnClickListener {
    when(it.id){
      R.id.play->startActivity(Intent(this@LauncherActivity,MainActivity::class.java))
      R.id.recorder->startActivity(Intent(this@LauncherActivity,CameraActivity::class.java))
      R.id.transform->startActivity(Intent(this@LauncherActivity,TransformActivity::class.java))
    }
  }
}
