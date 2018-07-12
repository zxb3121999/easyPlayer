package cn.jx.easyplayer;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.view.KeyEvent;
import android.view.View;
import android.widget.Button;
import cn.jx.easyplayerlib.transform.TransformUtils;
import cn.jx.easyplayerlib.util.RawAssetsUtil;
import cn.jx.easyplayerlib.view.EasyVideoView;
import java.io.File;

public class MainActivity extends AppCompatActivity {
  private static final String TAG = MainActivity.class.getSimpleName();
  EasyVideoView videoView;
  boolean isRecorder = false;
  boolean isRunning = false;

  @Override protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    videoView = (EasyVideoView) findViewById(R.id.easy_video_view);
    //videoView.setVideoPath("mms://video.fjtv.net/setv");
    //videoView.setVideoPath("http://clips.vorwaerts-gmbh.de/big_buck_bunny.mp4");
    if (ActivityCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
      ActivityCompat.requestPermissions(this, new String[] { Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE }, 0);
    } else {
      videoView.setVideoPath("http://replay.live.tianya.cn/live-151574941100022906--20180112173015.mp4");
      //videoView.setVideoPath( Environment.getExternalStorageDirectory().getAbsolutePath()+"/ffmpeg/"+"tt.mp4");
      //videoView.setVideoPath("/storage/emulated/0/tencent/MicroMsg/Weixin/test.mp4");
      //videoView.setVideoPath("rtmp://live.hkstv.hk.lxdns.com/live/hks");
      //videoView.setVideoPath("http://hdl3a.douyucdn.cn/live/64609rdkw7Kej3bb_900.flv?wsAuth=0dbce31573d7c2a42efc9d3b1e2efe11&token=app-android1-0-64609-b8ea9667eba374a3133e415a4fe74082&logo=0&expire=0&did=10786b63-0b2b-4204-a149-1181f7cb3480&ver=v3.7.1&channel=31&pt=1");
    }
    Button pause = (Button) findViewById(R.id.pause);
    pause.setOnClickListener(new View.OnClickListener() {
      @Override public void onClick(View v) {
        //if (videoView.isPlaying()){
        //    videoView.pause();
        //} else
        //    videoView.start();
        if (!isRunning) {
          isRunning = true;
          TransformUtils transformUtils = new TransformUtils();
          transformUtils.setCompletedListener(new TransformUtils.OnTransformCompletedListener() {
            @Override public void onCompleted() {
              isRunning = false;
            }
          });
          transformUtils.setOnErrorListener(new TransformUtils.OnErrorListener() {
            @Override public void onError(int errorCode, int ffmpegCode, String msg) {
              isRunning = false;
            }
          });
          String assetsPath = RawAssetsUtil.getAssetsFilePath(MainActivity.this,"add_wire.png");
          transformUtils.transform("/storage/emulated/0/tencent/MicroMsg/Weixin/test.mp4", "/storage/emulated/0/test.mp4", 2, 0, 3, 0, 0, String.format("movie=%s[wm];[in]boxblur=2:1:cr=0:ar=0,colorchannelmixer=.3:.4:.3:0:.3:.4:.3:0:.3:.4:.3[main];[main][wm]overlay=20:20[out]",assetsPath));
        }
      }
    });
    findViewById(R.id.record).setOnClickListener(new View.OnClickListener() {
      @Override public void onClick(View v) {
        if (isRecorder) {
          isRecorder = false;
          videoView.stopRecorder();
        } else {
          isRecorder = true;
          String root = Environment.getExternalStorageDirectory().getAbsolutePath() + "/ffmpeg/";
          File dir = new File(root);
          if (!dir.exists()) {
            dir.mkdirs();
          }
          videoView.startRecorder(Environment.getExternalStorageDirectory().getAbsolutePath() + "/ffmpeg/" + System.currentTimeMillis() + ".mp4");
        }
      }
    });
  }

  @Override protected void onPause() {
    super.onPause();
    videoView.onPause();
  }

  @Override protected void onResume() {
    super.onResume();
    videoView.onResume();
  }

  @Override protected void onDestroy() {
    videoView.onDestory();
    super.onDestroy();
  }
  @Override public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
    if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
      videoView.setVideoPath("http://replay.live.tianya.cn/live-151574941100022906--20180112173015.mp4");
    }
  }

  public void ToRecorder(View view) {
    startActivityForResult(new Intent(this, RecorderActivity.class), 0);
  }

  @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    if (requestCode == 0 && resultCode == RESULT_OK) {

    }
  }

  @Override public boolean onKeyDown(int keyCode, KeyEvent event) {
    return super.onKeyDown(keyCode, event);
  }
}
