package cn.zxb.media.player

import android.view.Surface
import android.view.SurfaceHolder

import java.io.IOException

/**
 * media player interface
 */

interface IMediaPlayer {
  var videoWidth: Int

  var videoHeight: Int

  var isPlaying: Boolean

  var currentPosition: Long

  var duration: Long

  var audioSessionId: Int
  var currentState:Int

  fun setDisplay(sh: SurfaceHolder?)
  fun getMediaId():Int
  @Throws(IOException::class, IllegalArgumentException::class, SecurityException::class, IllegalStateException::class)
  fun setDataSource(path: String)


  fun setSurface(surface: Surface)

  fun setScreenOnWhilePlaying(screenOn: Boolean)

  @Throws(IllegalStateException::class)
  fun prepareAsync()


  @Throws(IllegalStateException::class)
  fun start()

  @Throws(IllegalStateException::class)
  fun restart()

  @Throws(IllegalStateException::class)
  fun stop()

  @Throws(IllegalStateException::class)
  fun pause()

  @Throws(IllegalStateException::class)
  fun seekTo(msec: Long)

  fun release()

  fun reset()
  fun startRecorder(path: String)
  fun stopRecorder()
  fun setVolume(leftVolume: Float, rightVolume: Float)


  fun setOnPreparedListener(listener: OnPreparedListener)

  fun setOnCompletionListener(listener: OnCompletionListener)

  fun setOnBufferingUpdateListener(
      listener: OnBufferingUpdateListener)

  fun setOnSeekCompleteListener(
      listener: OnSeekCompleteListener)

  fun setOnVideoSizeChangedListener(
      listener: OnVideoSizeChangedListener)

  fun setOnErrorListener(listener: OnErrorListener)

  fun setOnInfoListener(listener: OnInfoListener)


  fun setBackground(background: Boolean)
  fun setBackgroundState(isInBackground: Boolean)

  /*--------------------
     * Listeners
     */
  interface OnPreparedListener {
    fun onPrepared(mp: IMediaPlayer)
  }

  interface OnCompletionListener {
    fun onCompletion(mp: IMediaPlayer)
  }

  interface OnBufferingUpdateListener {
    fun onBufferingUpdate(mp: IMediaPlayer, percent: Int)
  }

  interface OnSeekCompleteListener {
    fun onSeekComplete(mp: IMediaPlayer)
  }

  interface OnVideoSizeChangedListener {
    fun onVideoSizeChanged(mp: IMediaPlayer, width: Int, height: Int)
  }

  interface OnErrorListener {
    fun onError(mp: IMediaPlayer, errorCode: Int, ffmpegCode: Int,errMsg:String)
  }

  interface OnInfoListener {
    fun onInfo(mp: IMediaPlayer, what: Int, extra: Int)
  }


}
