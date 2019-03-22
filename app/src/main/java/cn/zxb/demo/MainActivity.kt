package cn.zxb.demo

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import cn.zxb.media.player.IMediaPlayer
import kotlinx.android.synthetic.main.activity_main.*

class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        easy_video_view.registerOnErrorListener(object : IMediaPlayer.OnErrorListener {
            override fun onError(mp: IMediaPlayer, errorCode: Int, ffmpegCode: Int, errMsg: String) {
                println("播放出错,错误码$errorCode,ffmpeg错误码$ffmpegCode,错误消息$errMsg")
            }
        })
        easy_video_view.registerOnPreparedListener(object : IMediaPlayer.OnPreparedListener {
            override fun onPrepared(mp: IMediaPlayer) {
                println("准备完成")
            }
        })
        easy_video_view.registerOnCompletedListener(object : IMediaPlayer.OnCompletionListener {
            override fun onCompletion(mp: IMediaPlayer) {
                println("播放完成")
            }
        })
        //videoView.setVideoPath("mms://video.fjtv.net/setv");
        //videoView.setVideoPath("http://clips.vorwaerts-gmbh.de/big_buck_bunny.mp4");
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE), 0)
        } else {
//      easy_video_view.setVideoPath("http://replay.live.tianya.cn/live-151574941100022906--20180112173015.mp4");
//      easy_video_view.setVideoPath("/storage/emulated/0/DCIM/Camera/VID_20190301_130627.mp4")
            easy_video_view.setVideoPath("http://vd3.bdstatic.com/mda-im2inn4vub8dime9/sc/mda-im2inn4vub8dime9.mp4?auth_key=1551928029-0-0-cae70180679f09e65d4c1a8dfbe4f1b3&bcevod_channel=searchbox_feed&pd=bjh&abtest=all")

//      easy_video_view.setVideoPath(Environment.getExternalStorageDirectory().absolutePath + "/ffmpeg/" + "test.mp4")
            //videoView.setVideoPath("/storage/emulated/0/tencent/MicroMsg/Weixin/test.mp4");
//      easy_video_view.setVideoPath("rtmp://58.200.131.2:1935/livetv/hunantv")
//      easy_video_view.setVideoPath("http://ivi.bupt.edu.cn/hls/cctv6hd.m3u8")
//        easy_video_view.setVideoPath("http://stream2.ahtv.cn/jjsh/cd/live.m3u8")
//        easy_video_view.setVideoPath("http://cctv5.v.myalicdn.com/live/cctv5_1ud.m3u8")
        }
    }

    override fun onPause() {
        super.onPause()
//    easy_video_view.onActivityPause()
    }

    override fun onResume() {
        super.onResume()
//    easy_video_view.onActivityResume()
    }

    override fun onDestroy() {
        easy_video_view.onDestroy()
        println("====onDestroy")
        super.onDestroy()
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            easy_video_view.setVideoPath("http://replay.live.tianya.cn/live-151574941100022906--20180112173015.mp4")
//      easy_video_view.setVideoPath("rtmp://live.hkstv.hk.lxdns.com/live/hks")
        }
    }

    companion object {
        init {
            System.loadLibrary("player-lib")
        }

        private val TAG = MainActivity::class.java.simpleName
    }
}
