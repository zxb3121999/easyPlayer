package cn.jx.easyplayerlib.recorder;

import cn.jx.easyplayerlib.player.IMediaPlayer;

/**
 * AbstractMediaRecorder
 *
 * @author 张小兵
 *         功能描述：
 *         2018/3/1 0001
 */

public abstract class AbstractMediaRecorder implements IMediaRecorder {
  private OnRecorderCompletedListener mCompletedListener;
  private OnErrorListener mErrorListener;
  private OnStartListener mStartListener;
  public void setCompletedListener(OnRecorderCompletedListener completedListener) {
    mCompletedListener = completedListener;
  }

  public void setErrorListener(OnErrorListener errorListener) {
    mErrorListener = errorListener;
  }

  public void setStartListener(OnStartListener startListener) {
    mStartListener = startListener;
  }

  protected void notifyCompleted(){
    if (mCompletedListener!=null){
      mCompletedListener.onCompleted();
    }
  }
  protected void notifyError(int errorCode,String msg){
    if (mErrorListener!=null){
      mErrorListener.onError(errorCode,msg);
    }
  }
  protected void notifyStart(){
    if (mStartListener!=null){
      mStartListener.onStart();
    }
  }
}
