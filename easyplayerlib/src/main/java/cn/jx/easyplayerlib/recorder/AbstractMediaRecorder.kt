package cn.jx.easyplayerlib.recorder

/**
 * AbstractMediaRecorder
 *
 * @author 张小兵
 * 功能描述：
 * 2018/3/1 0001
 */

abstract class AbstractMediaRecorder : IMediaRecorder {
  private lateinit var mCompletedListener: () -> Unit
  private lateinit var mErrorListener: (Int,String)->Unit
  private lateinit var mStartListener: () -> Unit
  fun setCompletedListener(completedListener: () -> Unit) {
    mCompletedListener = completedListener
  }

  fun setErrorListener(errorListener: (Int, String) -> Unit) {
    mErrorListener = errorListener
  }

  fun setStartListener(startListener: () -> Unit) {
    mStartListener = startListener
  }

  protected fun notifyCompleted() {
    mCompletedListener?.invoke()
  }

  protected fun notifyError(errorCode: Int, msg: String) {
    mErrorListener?.invoke(errorCode,msg)
  }

  protected fun notifyStart() {
    mStartListener?.invoke()
  }
}
