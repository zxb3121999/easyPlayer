package cn.jx.easyplayerlib.recorder;

import android.hardware.Camera;
import android.os.Handler;
import android.os.Message;
import cn.jx.easyplayerlib.util.EasyLog;

/**
 * EasyMediaRecorder
 *
 * @author 张小兵
 *         功能描述：
 *         2018/3/1 0001
 */

public class EasyMediaRecorder extends AbstractMediaRecorder{
  static {
    System.loadLibrary("player-lib");
  }
  private static String TAG = EasyMediaRecorder.class.getSimpleName();
  private static final int RECORDER_STATE_READY = 1;
  private static final int RECORDER_STATE_RECORDING = 2;
  private static final int RECORDER_STATE_PAUSE = 3;
  private static final int RECORDER_STATE_COMPLETED = 4;
  private static final int RECORDER_STATE_ERROR = -1;
  @Override public void startRecorder(String path, Camera.Parameters parameters) {
    _startRecorder(path,parameters.getPreviewSize().height,parameters.getPreviewSize().width,parameters.getPreviewFrameRate(),580000);
  }

  @Override public void stopRecorder(int flag) {
    _stopRecorder(flag);
  }


  @Override public void recorderVideo(byte[] data, int len) {
    _recorderVideo(data,len);
  }

  @Override public void recorderAudio(byte[] data, int len) {
    _recorderAudio(data,len);
  }

  @Override public void onVideoError(int what, String msg) {

  }

  @Override public void onAudioError(int what, String msg) {

  }

  @Override public void onError(int errorCode,String msg) {
    notifyError(errorCode,msg);
  }

  @Override public void recorderCompleted() {
    notifyCompleted();
    release();
  }

  @Override public void release() {
    _release();
  }

  @Override public int getRecorderState() {
    return _getRecorderState();
  }

  @Override public boolean isRecording() {
    return _getRecorderState()==1;
  }


  private native void _startRecorder(String path,int width,int height,int frame,int bit);
  private native void _stopRecorder(int flag);
  private native void _recorderVideo(byte data[],int len);
  private native void _recorderAudio(byte data[],int len);
  private native void _release();
  private native int _getRecorderState();
  private void postEventFromNative(int what,int errorCode,String msg) {
    EasyLog.i(TAG, "recv message from native,what " + what);
    Message.obtain(mHander, what,errorCode,0,msg).sendToTarget();
  }
  private Handler mHander = new Handler(new Handler.Callback() {
    @Override
    public boolean handleMessage(Message msg) {
      switch (msg.what) {
        case RECORDER_STATE_READY:
          EasyLog.d(TAG, "准备好录制视频" );
          notifyStart();
          break;
        case RECORDER_STATE_RECORDING:
          EasyLog.d(TAG, "正在录制视频..." );
          break;
        case RECORDER_STATE_PAUSE:
          EasyLog.d(TAG, "暂停录制视频..." );
          break;
        case RECORDER_STATE_COMPLETED:
          EasyLog.d(TAG, "录制视频结束..." );
          recorderCompleted();
          break;
        case RECORDER_STATE_ERROR:
          EasyLog.d(TAG, "录制视频出错:"+msg);
          onError(msg.arg1,(String) msg.obj);
          break;
      }
      return true;
    }
  });
}
