package cn.jx.easyplayerlib.recorder;

import android.hardware.Camera;

/**
 * IMediaRecorder
 *
 * @author 张小兵
 *         功能描述：
 *         2018/3/1 0001
 */

public interface IMediaRecorder {
  void startRecorder(String path,Camera.Parameters parameters);
  void stopRecorder(int stop_flag);
  void recorderVideo(byte[] data,int len);
  void recorderAudio(byte[] data,int len);
  void onVideoError(int what,String msg);
  void onAudioError(int what,String msg);
  int getRecorderState();
  boolean isRecording();
  void onError(int errorCode,String msg);
  void recorderCompleted();
  void release();

  interface OnRecorderCompletedListener{
    void onCompleted();
  }
  interface OnStartListener{
    void onStart();
  }
  interface OnErrorListener{
    void onError(int errorCode,String msg);
  }
  int STOP_NOW = -2;
  int STOP_WAIT_FOR_COMPLETED = -1;
}
