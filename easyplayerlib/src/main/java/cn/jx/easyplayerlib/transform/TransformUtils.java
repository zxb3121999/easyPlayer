package cn.jx.easyplayerlib.transform;

import android.os.Handler;
import android.os.Message;
import cn.jx.easyplayerlib.util.EasyLog;
import java.io.File;

/**
 * TransformUtils
 *
 * @author 张小兵
 * 功能描述：
 * 2018/6/19 0019
 */
public class TransformUtils {
  static {
    System.loadLibrary("player-lib");
  }
  private static String TAG = TransformUtils.class.getSimpleName();
  private static final int STATE_READY = 1;
  private static final int STATE_RECORDING = 2;
  private static final int STATE_PAUSE = 3;
  private static final int STATE_COMPLETED = 4;
  private static final int STATE_ERROR = -1;
  public static final int ERROR_CODE_INPUT_FILE_NOT_EXIST = -100001;
  private OnTransformCompletedListener mCompletedListener;
  private OnStartListener mStartListener;
  private OnErrorListener mOnErrorListener;

  public void setCompletedListener(OnTransformCompletedListener completedListener) {
    mCompletedListener = completedListener;
  }

  public void setStartListener(OnStartListener startListener) {
    mStartListener = startListener;
  }

  public void setOnErrorListener(OnErrorListener onErrorListener) {
    mOnErrorListener = onErrorListener;
  }

  public void transform(String srcFilePath,String dstFilePath,long startTime,long endTime,long duration,int width,int height,String filterDesc){
    //if(!new File(srcFilePath).exists()){
    //  postEventFromNative(STATE_ERROR,ERROR_CODE_INPUT_FILE_NOT_EXIST,-1,"视频文件不存在");
    //  return;
    //}
    File f = new File(dstFilePath);
    if(f.exists()){
      f.delete();
    }else if (!f.getParentFile().exists()){
      f.mkdirs();
    }
    transform_native(srcFilePath,dstFilePath,startTime,endTime,duration,width,height,filterDesc);
  }
  /**
   * 视频转换
   * @param srcFilePath
   * @param dstFilePath
   * @param width 转换后的宽
   * @param height 转换后的高
   * @param filterDesc 需要添加的滤镜
   */
  private native int transform_native(String srcFilePath,String dstFilePath,long startTime,long endTime,long duration,int width,int height,String filterDesc);

  private Handler mHandler = new Handler(new Handler.Callback() {
    @Override
    public boolean handleMessage(Message msg) {
      switch (msg.what) {
        case STATE_READY:
          EasyLog.d(TAG, "准备好转换视频" );
          if (mStartListener!=null){
            mStartListener.onStart();
          }
          break;
        case STATE_RECORDING:
          EasyLog.d(TAG, "正在转换视频..." );
          break;
        case STATE_PAUSE:
          EasyLog.d(TAG, "暂停转换视频..." );
          break;
        case STATE_COMPLETED:
          EasyLog.d(TAG, "转换视频结束..." );
          if(mCompletedListener!=null){
            mCompletedListener.onCompleted();
          }
          break;
        case STATE_ERROR:
          EasyLog.d(TAG, "转换视频出错:"+msg);
          if (mOnErrorListener!=null){
            mOnErrorListener.onError(msg.arg1,msg.arg2,(String) msg.obj);
          }
          break;
      }
      return true;
    }
  });
  public interface OnTransformCompletedListener{
    void onCompleted();
  }
  public interface OnStartListener{
    void onStart();
  }
  public interface OnErrorListener{
    void onError(int errorCode,int ffmpegCode,String msg);
  }
  private void postEventFromNative(int what,int errorCode,int ffmpegErrorCode,String msg) {
    EasyLog.i(TAG, "recv message from native,what " + what);
    Message.obtain(mHandler, what,errorCode,ffmpegErrorCode,msg).sendToTarget();
  }
}
