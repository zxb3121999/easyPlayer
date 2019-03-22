package cn.zxb.media.recorder

import android.hardware.Camera

/**
 * IMediaRecorder
 *
 * @author 张小兵
 * 功能描述：
 * 2018/3/1 0001
 */

interface IMediaRecorder {
  val recorderState: Int
  val isRecording: Boolean
  fun startRecorder(path: String, parameters: Camera.Parameters)
  fun stopRecorder(stop_flag: Int)
  fun recorderVideo(data: ByteArray, len: Int)
  fun recorderAudio(data: ByteArray, len: Int)
  fun onVideoError(what: Int, msg: String)
  fun onAudioError(what: Int, msg: String)
  fun onError(errorCode: Int, msg: String)
  fun recorderCompleted()
  fun release()
  companion object {
    const val STOP_NOW = -2
    const val STOP_WAIT_FOR_COMPLETED = -1
  }
}
