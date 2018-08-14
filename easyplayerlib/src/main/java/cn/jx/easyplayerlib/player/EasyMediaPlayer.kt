package cn.jx.easyplayerlib.player

import android.graphics.SurfaceTexture
import android.os.Handler
import android.os.Message
import android.view.Surface
import android.view.SurfaceHolder
import cn.jx.easyplayerlib.util.EasyLog
import java.io.IOException

/**
 * Java wrapper of native easyPlayer.
 */

class EasyMediaPlayer : AbstractMediaPlayer() {
  private var mScreenOnWhilePlaying: Boolean = false
  private val mStayAwake: Boolean = false
  override var videoWidth: Int = 0
    private set
  override var videoHeight: Int = 0
    private set
  private var mSurfaceHolder: SurfaceHolder? = null

  private val mHander = Handler(Handler.Callback { msg ->
    when (msg.what) {
      MEDIA_SET_VIDEO_SIZE -> {
        EasyLog.d(TAG, "on media size changed " + msg.arg1 + "*" + msg.arg2)
        videoWidth = msg.arg1
        videoHeight = msg.arg2
        notifyOnVideoSizeChanged(videoWidth, videoHeight)
      }
      MEDIA_PREPARED -> {
        EasyLog.d(TAG, "on media prepared ")
        notifyOnPrepared()
      }
      MEDIA_PLAYBACK_COMPLETE -> {
        EasyLog.d(TAG, "play completed")
        notifyOnCompletion()
      }
      MEDIA_SEEK_COMPLETE -> EasyLog.d(TAG, "seek completed")
    }
    true
  })
  override val isPlaying: Boolean
    external get

  override val currentPosition: Long
    external get

  override val duration: Long
    external get

  override val audioSessionId: Int
    get() = 0


  private external fun _nativeInit()

  init {
    _nativeInit()
  }


  /**
   * Sets the data source (file-path or http/rtsp URL) to use.
   *
   * @param path
   * the path of the file, or the http/rtsp URL of the stream you
   * want to play
   * @throws IllegalStateException
   * if it is called in an invalid state
   *
   *
   *
   * When `path` refers to a local file, the file may
   * actually be opened by a process other than the calling
   * application. This implies that the pathname should be an
   * absolute path (as any other process runs with unspecified
   * current working directory), and that the pathname should
   * reference a world-readable file.
   */
  @Throws(IOException::class, IllegalArgumentException::class, SecurityException::class, IllegalStateException::class)
  override fun setDataSource(path: String) {
    _setDataSource(path)
  }
  @Throws(IOException::class, IllegalArgumentException::class, SecurityException::class, IllegalStateException::class)
  private external fun _setDataSource(path: String)

  /**
   * Sets the [SurfaceHolder] to use for displaying the video portion of
   * the media.
   *
   * Either a surface holder or surface must be set if a display or video sink
   * is needed. Not calling this method or [.setSurface] when
   * playing back a video will result in only the audio track being played. A
   * null surface holder or surface will result in only the audio track being
   * played.
   *
   * @param sh
   * the SurfaceHolder to use for video display
   */
  override fun setDisplay(sh: SurfaceHolder){
    mSurfaceHolder = sh
    val surface: Surface?
    if (sh != null) {
      surface = sh.surface
    } else {
      surface = null
    }
    _setVideoSurface(surface)
    updateSurfaceScreenOn()
  }


  /**
   * Sets the [Surface] to be used as the sink for the video portion of
   * the media. This is similar to [.setDisplay], but
   * does not support [.setScreenOnWhilePlaying]. Setting a
   * Surface will un-set any Surface or SurfaceHolder that was previously set.
   * A null surface will result in only the audio track being played.
   *
   * If the Surface sends frames to a [SurfaceTexture], the timestamps
   * returned from [SurfaceTexture.getTimestamp] will have an
   * unspecified zero point. These timestamps cannot be directly compared
   * between different media sources, different instances of the same media
   * source, or multiple runs of the same program. The timestamp is normally
   * monotonically increasing and is unaffected by time-of-day adjustments,
   * but it is reset when the position is set.
   *
   * @param surface
   * The [Surface] to be used for the video portion of the
   * media.
   */
  override fun setSurface(surface: Surface) {
    if (mScreenOnWhilePlaying && surface != null) {
      EasyLog.w(TAG, "setScreenOnWhilePlaying(true) is ineffective for Surface")
    }
    mSurfaceHolder = null
    _setVideoSurface(surface)
    updateSurfaceScreenOn()
  }

  /*
     * Update the SurfaceTexture. Call after setting a new
     * display surface.
     */
  private external fun _setVideoSurface(surface: Surface?)


  @Throws(IllegalStateException::class)
  override fun prepareAsync() {
    _prepareAsync()
  }

  @Throws(IllegalStateException::class)
  override fun start() {
    _start()
  }

  @Throws(IllegalStateException::class)
  private external fun _start()

  @Throws(IllegalStateException::class)
  override fun restart() {
    _restart()
  }

  @Throws(IllegalStateException::class)
  private external fun _restart()

  @Throws(IllegalStateException::class)
  override fun stop() {

  }

  @Throws(IllegalStateException::class)
  override fun pause() {
    _pause()
  }

  @Throws(IllegalStateException::class)
  private external fun _pause()

  @Throws(IllegalStateException::class)
  override external fun seekTo(msec: Long)

  override fun release() {
    updateSurfaceScreenOn()
    resetListeners()
    _release()
  }

  private external fun _release()

  override fun reset() {

  }

  override fun setVolume(leftVolume: Float, rightVolume: Float) {

  }

  override fun setBackground(background: Boolean) {
    _canPlayBackground(background)
  }

  private external fun _canPlayBackground(canPlay: Boolean)
  override fun setBackgroundState(isInBackground: Boolean) {
    _setBackgroundState(isInBackground)
  }

  external fun _setBackgroundState(isBackground: Boolean)
  @Throws(IllegalStateException::class)
  external fun _prepareAsync()

  private external fun _startRecorder(path: String)
  override fun startRecorder(path: String) {
    _startRecorder(path)
  }

  private external fun _stopRecorder()
  override fun stopRecorder() {
    _stopRecorder()
  }

  override fun setScreenOnWhilePlaying(screenOn: Boolean) {
    if (mScreenOnWhilePlaying != screenOn) {
      if (screenOn && mSurfaceHolder == null) {
        EasyLog.w(TAG, "setScreenOnWhilePlaying(true) is ineffective without a SurfaceHolder")
      }
      mScreenOnWhilePlaying = screenOn
      updateSurfaceScreenOn()
    }
  }


  private fun updateSurfaceScreenOn() {
    if (mSurfaceHolder != null) {
      mSurfaceHolder!!.setKeepScreenOn(mScreenOnWhilePlaying && mStayAwake)
    }
  }


  private fun postEventFromNative(what: Int, arg1: Int, arg2: Int, msg: String) {
    EasyLog.i(TAG, "recv message from native,what $what")
    Message.obtain(mHander, what, arg1, arg2, msg).sendToTarget()
  }

  companion object {

    private val TAG = EasyMediaPlayer::class.java!!.getSimpleName()

    private val MEDIA_NOP = 0 // interface test message
    private val MEDIA_PREPARED = 1
    private val MEDIA_PLAYBACK_COMPLETE = 2
    private val MEDIA_BUFFERING_UPDATE = 3
    private val MEDIA_SEEK_COMPLETE = 4
    private val MEDIA_SET_VIDEO_SIZE = 5
    private val MEDIA_TIMED_TEXT = 99
    private val MEDIA_ERROR = -1
    private val MEDIA_INFO = 200
  }

}
