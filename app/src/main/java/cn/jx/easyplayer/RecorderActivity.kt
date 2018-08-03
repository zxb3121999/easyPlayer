package cn.jx.easyplayer

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.graphics.ImageFormat
import android.hardware.Camera
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.HandlerThread
import android.os.Message
import android.support.v4.app.ActivityCompat
import android.support.v7.app.AppCompatActivity
import android.util.Log
import android.util.SparseArray
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.widget.TextView
import android.widget.Toast
import cn.jx.easyplayerlib.recorder.EasyMediaRecorder
import cn.jx.easyplayerlib.recorder.IMediaRecorder
import java.io.IOException

class RecorderActivity : AppCompatActivity() {
  private val TAG = javaClass.simpleName
  private var mHolder: SurfaceHolder? = null
  private var mCamera: android.hardware.Camera? = null
  private var mParameters: android.hardware.Camera.Parameters? = null
  private var mBackgroundThread: HandlerThread? = null
  private var mBackgroundHandler: Handler? = null
  private var mRecord: AudioRecord? = null
  private var isRecording = false
  private var bufferSize = 0
  private var mRecorder: EasyMediaRecorder? = null
  private var tv: TextView? = null

  internal var time: Long = 0

  private val mCallback = Camera.PreviewCallback { data, camera ->
    if (data != null) {
      if (isRecording && mBackgroundHandler != null) {
        mBackgroundHandler!!.sendMessage(Message.obtain(mBackgroundHandler, 0, data))
      }
      mCamera!!.addCallbackBuffer(data)
    }
  }

  private val AudioRunnable = Runnable {
    val buffer = ByteArray(4096)
    try {
      mRecord!!.startRecording()
      var result = 0
      while (isRecording) {
        result = mRecord!!.read(buffer, 0, 4096)
        if (result == AudioRecord.ERROR_INVALID_OPERATION || result == AudioRecord.ERROR_BAD_VALUE) {
          continue
        }
        if (result > 0) {
          mRecorder!!.recorderAudio(buffer, result)
        }
      }
      mRecord!!.stop()
      mRecord!!.release()
    } catch (e: Exception) {
      e.printStackTrace()
    }
  }

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.activity_recorder)
    val surfaceView = findViewById<View>(R.id.surface) as SurfaceView
    tv = findViewById<View>(R.id.recorder) as TextView
    mHolder = surfaceView.holder
    mHolder!!.addCallback(object : SurfaceHolder.Callback {
      override fun surfaceCreated(holder: SurfaceHolder) {
        if (mCamera == null) {
          initCamera()
        } else {
          startPreviewDisplay()
          mCamera!!.cancelAutoFocus()
        }
      }

      override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        if (mCamera != null) {
          stopPreviewDisplay()
          startPreviewDisplay()
          mCamera!!.cancelAutoFocus()
        }
      }

      override fun surfaceDestroyed(holder: SurfaceHolder) {
        stopPreviewDisplay()
      }
    })
  }

  override fun onPause() {
    super.onPause()
  }

  public override fun onResume() {
    super.onResume()

  }

  override fun onDestroy() {
    if (mCamera != null) mCamera!!.release()
    if (isRecording) {
      isRecording = false
      mBackgroundHandler!!.sendEmptyMessage(IMediaRecorder.STOP_NOW)
    }
    super.onDestroy()
  }

  private fun initAudioRecorder() {
    bufferSize = AudioRecord.getMinBufferSize(44100, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT)
    mRecord = AudioRecord(MediaRecorder.AudioSource.MIC, 44100, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT, bufferSize)
  }

  private fun initCamera() {
    if (ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
      ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.CAMERA, Manifest.permission.RECORD_AUDIO), 0)
      return
    }
    mRecorder = EasyMediaRecorder()
    mRecorder!!.setCompletedListener { tv!!.text = "开始" }
    mRecorder!!.setStartListener { tv!!.text = "停止" }
    mRecorder!!.setErrorListener { errorCode, msg ->
      run {
        tv!!.text = "停止"
        Log.e(TAG, msg)
      }
    }
    mCamera = android.hardware.Camera.open()
    if (mCamera == null) {
      Toast.makeText(this, "打开相机失败", Toast.LENGTH_SHORT).show()
      return
    }
    mParameters = mCamera!!.parameters
    mParameters!!.focusMode = Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO
    if (mParameters!!.isVideoStabilizationSupported) mParameters!!.videoStabilization = true
    val list = mParameters!!.supportedPreviewSizes
    var maxSize: Camera.Size = list[0]
    for (size in list) {
      if (size.width * size.height >= maxSize.width * maxSize.height) {
        maxSize = size
      }
    }
    mParameters!!.setPreviewSize(640, 480)
    mParameters!!.previewFormat = ImageFormat.YV12
    val frameList = mParameters!!.supportedPreviewFrameRates
    if (frameList.contains(30)) {
      mParameters!!.previewFrameRate = 30
    } else {
      mParameters!!.previewFrameRate = frameList[0]
    }
    mCamera!!.parameters = mParameters
    followScreenOrientation()
    startPreviewDisplay()
    mCamera!!.cancelAutoFocus()
  }

  /**
   * 检测是否支持指定特性
   */
  private fun isSupported(list: List<String>?, key: String): Boolean {
    return list != null && list.contains(key)
  }

  fun Recorder(view: View) {
    if (mBackgroundThread == null) {
      mBackgroundThread = HandlerThread("start_recorder")
      mBackgroundThread!!.start()
      mBackgroundHandler = Handler(mBackgroundThread!!.looper, Handler.Callback { msg ->
        if (msg.obj != null && msg.what == 0) {
          val data = msg.obj as ByteArray
          mRecorder!!.recorderVideo(data, data.size)
        } else if (msg.what == IMediaRecorder.STOP_NOW || msg.what == IMediaRecorder.STOP_WAIT_FOR_COMPLETED) {
          mBackgroundHandler!!.removeMessages(0)
          if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            mBackgroundThread!!.quitSafely()
          } else {
            mBackgroundThread!!.quit()
          }
          mRecorder!!.stopRecorder(msg.what)
          mBackgroundThread = null
        }
        false
      })
    }
    val tv = view as TextView
    if (isRecording) {
      tv.text = "开始"
      isRecording = false
      mBackgroundHandler!!.sendEmptyMessage(-1)
    } else {
      initAudioRecorder()
      //mRecorder.startRecorder("rtmp://192.168.232.22:1935/live/12345", mParameters);
      mRecorder!!.startRecorder(Environment.getExternalStorageDirectory().absolutePath + "/ffmpeg/tt.mp4", mParameters!!)
      isRecording = true
      Thread(AudioRunnable).start()
    }
  }

  fun getState(v: View) {
    startActivity(Intent(this, MainActivity::class.java))
  }

  private fun followScreenOrientation() {
    val orientation = resources.configuration.orientation
    if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
      mCamera!!.setDisplayOrientation(180)
    } else if (orientation == Configuration.ORIENTATION_PORTRAIT) {
      mCamera!!.setDisplayOrientation(90)
    }
  }

  private fun startPreviewDisplay() {
    if (mCamera != null) {
      try {
        followScreenOrientation()
        mCamera!!.setPreviewDisplay(mHolder)
        val size = mParameters!!.previewSize.width * mParameters!!.previewSize.height * ImageFormat.getBitsPerPixel(mParameters!!.previewFormat) / 8
        mCamera!!.addCallbackBuffer(ByteArray(size))
        mCamera!!.addCallbackBuffer(ByteArray(size))
        mCamera!!.addCallbackBuffer(ByteArray(size))
        mCamera!!.setPreviewCallbackWithBuffer(mCallback)
        mCamera!!.startPreview()
      } catch (e: IOException) {
        e.printStackTrace()
      }

    }
  }

  private fun stopPreviewDisplay() {
    if (mCamera != null) {
      mCamera!!.stopPreview()
    }
  }

  override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
    if (requestCode == 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
      initCamera()
    }
  }

  companion object {
    private val ORIENTATIONS = SparseArray<Int>()

    init {
      ORIENTATIONS.append(Surface.ROTATION_0, 90)
      ORIENTATIONS.append(Surface.ROTATION_90, 0)
      ORIENTATIONS.append(Surface.ROTATION_180, 270)
      ORIENTATIONS.append(Surface.ROTATION_270, 180)
    }
  }
}
