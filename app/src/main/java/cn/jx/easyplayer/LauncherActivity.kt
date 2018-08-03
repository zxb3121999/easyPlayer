package cn.jx.easyplayer

import android.content.Intent
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.view.View.OnClickListener
import kotlinx.android.synthetic.main.activity_launcher.play
import kotlinx.android.synthetic.main.activity_launcher.recorder
import kotlinx.android.synthetic.main.activity_launcher.transform

class LauncherActivity : AppCompatActivity() {

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.activity_launcher)
    play.setOnClickListener(listener)
    recorder.setOnClickListener(listener)
    transform.setOnClickListener(listener)
  }
  private var listener:OnClickListener = OnClickListener {
    when(it.id){
      R.id.play->startActivity(Intent(this,MainActivity::class.java))
      R.id.recorder->startActivity(Intent(this,RecorderActivity::class.java))
      R.id.transform->startActivity(Intent(this,MainActivity::class.java))
    }
  }
}
