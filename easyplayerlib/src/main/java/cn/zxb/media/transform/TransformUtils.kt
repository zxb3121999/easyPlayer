package cn.zxb.media.transform

import android.os.Handler
import android.os.Message
import cn.zxb.media.util.EasyLog
import java.io.File

/**
 * TransformUtils
 *
 * @author 张小兵
 * 功能描述：
 * 2018/6/19 0019
 */
class TransformUtils {
  private var mCompletedListener: (() -> Unit)? =null
  private var mStartListener: (()->Unit)? = null
  private var mOnErrorListener:((Int,Int,String?)->Unit)? = null
  private var mProgressListener:((Int)->Unit)?=null
  private var time = 0
  private val mHandler = Handler(Handler.Callback { msg ->
    when (msg.what) {
      STATE_READY -> {
        EasyLog.d(TAG, "准备好转换视频")
        mStartListener?.invoke()
      }
      STATE_RECORDING -> EasyLog.d(TAG, "正在转换视频...")
      STATE_PAUSE -> EasyLog.d(TAG, "暂停转换视频...")
      STATE_COMPLETED -> {
        EasyLog.d(TAG, "转换视频结束...")
        mCompletedListener?.invoke()
      }
      STATE_ERROR -> {
        EasyLog.d(TAG, "转换视频出错:$msg")
        mOnErrorListener?.invoke(msg.arg1, msg.arg2, msg.obj as String)
      }
      STATE_PROCESS ->{
        System.out.println("===进度："+msg.arg1)
        if(msg.arg1==0&&time == 0)
            time = System.currentTimeMillis().toInt()
        if(msg.arg1==100)
          System.out.println("总共用时:"+(System.currentTimeMillis().toInt()-time))
        mProgressListener?.invoke(msg.arg1)
      }
    }
    true
  })

  fun setCompletedListener(completedListener: ()->Unit) {
    mCompletedListener = completedListener
  }

  fun setStartListener(startListener: ()->Unit) {
    mStartListener = startListener
  }

  fun setOnErrorListener(onErrorListener: (Int,Int,String?)->Unit) {
    mOnErrorListener = onErrorListener
  }
  fun setOnProgressLintener(onProgressListener:(Int)->Unit){
    mProgressListener = onProgressListener
  }
  fun transform(srcFilePath: String, dstFilePath: String, startTime: Long, endTime: Long, duration: Long, width: Int, height: Int, filterDesc: String) {
    if(!File(srcFilePath).exists()){
      postEventFromNative(STATE_ERROR,ERROR_CODE_INPUT_FILE_NOT_EXIST,-1,"视频文件不存在")
      return
    }
    val f = File(dstFilePath)
    if (f.exists()) {
      f.delete()
    } else if (!f.parentFile.exists()) {
      f.mkdirs()
    }
    transform_native(srcFilePath, dstFilePath, startTime, endTime, duration, width, height, filterDesc)
  }

  /**
   * 视频转换
   * @param srcFilePath
   * @param dstFilePath
   * @param width 转换后的宽
   * @param height 转换后的高
   * @param filterDesc 需要添加的滤镜
   */
  private external fun transform_native(srcFilePath: String, dstFilePath: String, startTime: Long, endTime: Long, duration: Long, width: Int, height: Int, filterDesc: String): Int


  private fun postEventFromNative(what: Int, errorCode: Int, ffmpegErrorCode: Int, msg: String) {
    EasyLog.i(TAG, "recv message from native,what $what")
    Message.obtain(mHandler, what, errorCode, ffmpegErrorCode, msg).sendToTarget()
  }

  companion object {
    init {
      System.loadLibrary("player-lib")
    }

    private val TAG = TransformUtils::class.java!!.getSimpleName()
    private const val STATE_READY = 1
    private const val STATE_RECORDING = 2
    private const val STATE_PAUSE = 3
    private const val STATE_COMPLETED = 4
    private const val STATE_ERROR = -1
    private const val STATE_PROCESS = 5
    private const val ERROR_CODE_INPUT_FILE_NOT_EXIST = -100001
  }
}
