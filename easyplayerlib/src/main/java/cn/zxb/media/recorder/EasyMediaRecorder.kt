package cn.zxb.media.recorder

import android.hardware.Camera
import android.os.Handler
import android.os.Message
import android.util.Log
import cn.zxb.media.util.EasyLog

/**
 * EasyMediaRecorder
 *
 * @author 张小兵
 * 功能描述：
 * 2018/3/1 0001
 */

class EasyMediaRecorder : AbstractMediaRecorder() {

  override val recorderState: Int
    get() = _getRecorderState()

  override val isRecording: Boolean
    get() = _getRecorderState() == 1
  private val mHander = Handler(Handler.Callback { msg ->
    when (msg.what) {
      RECORDER_STATE_READY -> {
        EasyLog.d(TAG, "准备好录制视频")
        notifyStart()
      }
      RECORDER_STATE_RECORDING -> EasyLog.d(TAG, "正在录制视频...")
      RECORDER_STATE_PAUSE -> EasyLog.d(TAG, "暂停录制视频...")
      RECORDER_STATE_COMPLETED -> {
        EasyLog.d(TAG, "录制视频结束...")
        recorderCompleted()
        EasyLog.d(TAG, "handler...")
      }
      RECORDER_STATE_ERROR -> {
        EasyLog.d(TAG, "录制视频出错:$msg")
        onError(msg.arg1, msg.obj as String)
      }
    }
    true
  })

  override fun startRecorder(path: String, parameters: Camera.Parameters) {
    _startRecorder(path, parameters.previewSize.height, parameters.previewSize.width, parameters.previewFrameRate, 580000)
  }

  override fun stopRecorder(flag: Int) {
    _stopRecorder(flag)
  }


  override fun recorderVideo(data: ByteArray, len: Int) {
    _recorderVideo(data, len)
  }

  override fun recorderAudio(data: ByteArray, len: Int) {
    _recorderAudio(data, len)
  }

  override fun onVideoError(what: Int, msg: String) {

  }

  override fun onAudioError(what: Int, msg: String) {

  }

  override fun onError(errorCode: Int, msg: String) {
    notifyError(errorCode, msg)
  }

  override fun recorderCompleted() {
    notifyCompleted()
    release()
  }

  override fun release() {
    _release()
    Log.d(TAG, "完成")
  }


  private external fun _startRecorder(path: String, width: Int, height: Int, frame: Int, bit: Int)
  private external fun _stopRecorder(flag: Int)
  private external fun _recorderVideo(data: ByteArray, len: Int)
  private external fun _recorderAudio(data: ByteArray, len: Int)
  private external fun _release()
  private external fun _getRecorderState(): Int
  private fun postEventFromNative(what: Int, errorCode: Int, msg: String) {
    EasyLog.i(TAG, "recv message from native,what $what")
    Message.obtain(mHander, what, errorCode, 0, msg).sendToTarget()
  }

  companion object {
    init {
      System.loadLibrary("player-lib")
    }

    private val TAG = javaClass.simpleName
    private val RECORDER_STATE_READY = 1
    private val RECORDER_STATE_RECORDING = 2
    private val RECORDER_STATE_PAUSE = 3
    private val RECORDER_STATE_COMPLETED = 4
    private val RECORDER_STATE_ERROR = -1
  }
}
