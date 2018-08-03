package cn.jx.easyplayerlib.view

import android.annotation.TargetApi
import android.content.Context
import android.os.Build.VERSION_CODES
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import android.widget.MediaController
import android.widget.RelativeLayout
import cn.jx.easyplayerlib.R
import cn.jx.easyplayerlib.player.EasyMediaPlayer
import cn.jx.easyplayerlib.player.IMediaPlayer
import cn.jx.easyplayerlib.player.IMediaPlayer.OnCompletionListener
import cn.jx.easyplayerlib.player.IMediaPlayer.OnPreparedListener
import cn.jx.easyplayerlib.player.IMediaPlayer.OnVideoSizeChangedListener
import cn.jx.easyplayerlib.util.EasyLog

/**
 *
 */

class EasyVideoView : FrameLayout, MediaController.MediaPlayerControl, SurfaceHolder.Callback {

  private var mAppContext: Context? = null
  private var mMediaPlayer: IMediaPlayer? = null
  private var mMediaController: MediaController? = null
  private var mUri: String? = null
  private var mSurfaceHolder: SurfaceHolder? = null
  private var mSurfaceView: SurfaceView? = null
  private var mVideoWidth: Int = 0
  private var mVideoHeight: Int = 0
  private val mSurfaceWidth: Int = 0
  private val mSurfaceHeight: Int = 0

  private var mCurrentState = STATE_IDLE

  private val isInPlaybackState: Boolean
    get() = mMediaPlayer != null && mCurrentState != STATE_ERROR && mCurrentState != STATE_IDLE && mCurrentState != STATE_PREPARING

  private var mSizeChangedListener = object:OnVideoSizeChangedListener{
    @TargetApi(VERSION_CODES.JELLY_BEAN_MR1)
    override fun onVideoSizeChanged(mp: IMediaPlayer, width: Int, height: Int) {
      mVideoWidth = mp.videoWidth
      mVideoHeight = mp.videoHeight
      val displayWidth = display.width
      if (mVideoWidth != 0 && mVideoHeight != 0) {
        val params = layoutParams as RelativeLayout.LayoutParams
        params.height = displayWidth * mVideoHeight / mVideoWidth
        layoutParams = params
      }
    }
  }

  private var mPreparedListener = object:OnPreparedListener{
    override fun onPrepared(mp: IMediaPlayer) {
      mCurrentState = STATE_PREPARED
    }
  }
  private var mCompletionListener = object:OnCompletionListener{
    override fun onCompletion(mp: IMediaPlayer) {
      mCurrentState = STATE_PLAYBACK_COMPLETED
    }
  }

  constructor(context: Context) : super(context) {
    initVideoView(context)
  }

  constructor(context: Context, attrs: AttributeSet) : super(context, attrs) {
    initVideoView(context)
  }

  constructor(context: Context, attrs: AttributeSet, defStyleAttr: Int) : super(context, attrs, defStyleAttr) {
    initVideoView(context)
  }

  private fun initVideoView(context: Context) {
    mAppContext = context.applicationContext
    LayoutInflater.from(context).inflate(R.layout.view_easy_video, this)
    mSurfaceView = findViewById(R.id.surface_easy_video_view)
    mSurfaceView!!.holder.addCallback(this)
  }

  /**
   * Sets video path.
   *
   * @param path the path of the video.
   */
  fun setVideoPath(path: String) {
    this.mUri = path
    openVideo()
  }

  override fun start() {
    if (mMediaPlayer != null) {
      mMediaPlayer!!.start()
    }
  }

  override fun pause() {
    if (mMediaPlayer != null) {
      mMediaPlayer!!.pause()
    }
  }

  fun startRecorder(path: String) {
    if (mMediaPlayer != null) {
      mMediaPlayer!!.startRecorder(path)
    }
  }

  fun stopRecorder() {
    if (mMediaPlayer != null) {
      mMediaPlayer!!.stopRecorder()
    }
  }

  override fun getDuration(): Int {
    return if (mMediaPlayer != null && mCurrentState >= STATE_PREPARED) {
      mMediaPlayer!!.duration.toInt()
    } else -1
  }

  override fun getCurrentPosition(): Int {
    return if (mMediaPlayer != null && mCurrentState >= STATE_PREPARED) {
      mMediaPlayer!!.currentPosition.toInt()
    } else 0
  }

  override fun seekTo(pos: Int) {
    if (mMediaPlayer != null && mCurrentState >= STATE_PREPARED) {
      mMediaPlayer!!.seekTo(pos.toLong())
    }
  }

  override fun isPlaying(): Boolean {
    return mMediaPlayer!!.isPlaying
  }

  override fun getBufferPercentage(): Int {
    return 0
  }

  override fun canPause(): Boolean {
    return true
  }

  override fun canSeekBackward(): Boolean {
    return true
  }

  override fun canSeekForward(): Boolean {
    return true
  }

  /**
   * Get the audio session id for the player used by this VideoView. This can be used to
   * apply audio effects to the audio track of a video.
   *
   * @return The audio session, or 0 if there was an error.
   */
  override fun getAudioSessionId(): Int {
    return 0
  }

  /**
   * This is called immediately after the surface is first created.
   * Implementations of this should start up whatever rendering code
   * they desire.  Note that only one thread can ever draw into
   * a [Surface], so you should not draw into the Surface here
   * if your normal rendering will be in another thread.
   *
   * @param holder The SurfaceHolder whose surface is being created.
   */
  override fun surfaceCreated(holder: SurfaceHolder) {
    mSurfaceHolder = holder
    if (mMediaPlayer == null) {
      openVideo()
    } else {
      mMediaPlayer!!.setSurface(holder.surface)
      mMediaPlayer!!.setBackgroundState(false)
    }
  }

  /**
   * This is called immediately after any structural changes (format or
   * size) have been made to the surface.  You should at this point update
   * the imagery in the surface.  This method is always called at least
   * once, after [.surfaceCreated].
   *
   * @param holder The SurfaceHolder whose surface has changed.
   * @param format The new PixelFormat of the surface.
   * @param width The new width of the surface.
   * @param height The new height of the surface.
   */
  override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {

  }

  /**
   * This is called immediately before a surface is being destroyed. After
   * returning from this call, you should no longer try to access this
   * surface.  If you have a rendering thread that directly accesses
   * the surface, you must ensure that thread is no longer touching the
   * Surface before returning from this function.
   *
   * @param holder The SurfaceHolder whose surface is being destroyed.
   */
  override fun surfaceDestroyed(holder: SurfaceHolder) {
    if (mMediaPlayer != null) {
      mMediaPlayer!!.setBackgroundState(true)
    }
  }

  override fun onTouchEvent(ev: MotionEvent): Boolean {
    if (/*isInPlaybackState() &&*/ mMediaController != null) {
      toggleMediaControlsVisiblity()
    }
    return false
  }

  private fun openVideo() {

    if (mUri == null || mSurfaceHolder == null) {
      return
    }
    EasyLog.i(TAG, "video view is ready,creating player")
    mMediaPlayer = EasyMediaPlayer()
    mMediaPlayer!!.setOnVideoSizeChangedListener(mSizeChangedListener)
    mMediaPlayer!!.setOnPreparedListener(mPreparedListener)
    mMediaPlayer!!.setScreenOnWhilePlaying(true)
    mMediaPlayer!!.setOnCompletionListener(mCompletionListener)
    try {
      mMediaPlayer!!.setDataSource(mUri!!)
      mMediaPlayer!!.setSurface(mSurfaceHolder!!.surface)
      mMediaPlayer!!.prepareAsync()
      mCurrentState = STATE_PREPARING
      if (mMediaController == null) {
        mMediaController = MediaController(context)
        mMediaController!!.setMediaPlayer(this@EasyVideoView)
        mMediaController!!.setAnchorView(mSurfaceView)
        mMediaController!!.isEnabled = true
        mMediaController!!.show()
      }
    } catch (e: Exception) {

    }

  }

  private fun toggleMediaControlsVisiblity() {
    if (mMediaController!!.isShowing) {
      mMediaController!!.hide()
    } else {
      mMediaController!!.show()
    }
  }

  fun onActivityPause() {
    if (mMediaPlayer != null) {
      mMediaPlayer!!.setBackgroundState(true)
    }
  }

  fun onActivityResume() {
    if (mMediaPlayer != null) {
      mMediaPlayer!!.setBackgroundState(false)
    }
  }

  fun onDestory() {
    if (mMediaPlayer != null) {
      mMediaPlayer!!.release()
    }
  }

  companion object {

    private val TAG = javaClass.simpleName

    // all possible internal states
    private const val STATE_ERROR = -1
    private const val STATE_IDLE = 0
    private const val STATE_PREPARING = 1
    private const val STATE_PREPARED = 2
    private const val STATE_PLAYING = 3
    private const val STATE_PAUSED = 4
    private const val STATE_PLAYBACK_COMPLETED = 5
  }
}
