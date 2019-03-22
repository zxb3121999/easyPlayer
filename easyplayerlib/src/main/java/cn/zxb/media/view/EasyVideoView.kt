package cn.zxb.media.view

import android.annotation.TargetApi
import android.content.Context
import android.os.Build.VERSION_CODES
import android.util.AttributeSet
import android.view.*
import android.widget.FrameLayout
import android.widget.MediaController
import cn.zxb.media.R
import cn.zxb.media.player.EasyMediaPlayer
import cn.zxb.media.player.EasyMediaPlayer.Companion.STATE_ERROR
import cn.zxb.media.player.EasyMediaPlayer.Companion.STATE_IDLE
import cn.zxb.media.player.EasyMediaPlayer.Companion.STATE_PLAYBACK_COMPLETED
import cn.zxb.media.player.EasyMediaPlayer.Companion.STATE_PREPARED
import cn.zxb.media.player.EasyMediaPlayer.Companion.STATE_PREPARING
import cn.zxb.media.player.IMediaPlayer
import cn.zxb.media.player.IMediaPlayer.*
import cn.zxb.media.util.EasyLog

/**
 *
 */

class EasyVideoView : FrameLayout, MediaController.MediaPlayerControl, SurfaceHolder.Callback {

    private var mAppContext: Context? = null
    var mMediaPlayer: IMediaPlayer? = null
        set(mp) {
            field = mp
            mCurrentState = mMediaPlayer!!.currentState
            mMediaPlayer!!.setOnVideoSizeChangedListener(mSizeChangedListener)
            mMediaPlayer!!.setOnPreparedListener(mPreparedListener)
            mMediaPlayer!!.setScreenOnWhilePlaying(true)
            mMediaPlayer!!.setOnCompletionListener(mCompletionListener)
            mMediaPlayer!!.setOnErrorListener(mErrorListener)

        }
    private var mMediaController: MediaController? = null
    private var mUri: String? = null
    private var mSurfaceHolder: SurfaceHolder? = null
    private var mSurfaceView: SurfaceView? = null
    private var mVideoWidth: Int = 0
    private var mVideoHeight: Int = 0
    private val mSurfaceWidth: Int = 0
    private val mSurfaceHeight: Int = 0
    private var mOnErrorListener: IMediaPlayer.OnErrorListener? = null
    private var mOnCompleteListener: IMediaPlayer.OnCompletionListener? = null
    private var mOnPreparedListener: IMediaPlayer.OnPreparedListener? = null
    private var mCurrentState = STATE_IDLE

    private val isInPlaybackState: Boolean
        get() = mMediaPlayer != null && mCurrentState != STATE_ERROR && mCurrentState != STATE_IDLE && mCurrentState != STATE_PREPARING

    private var mSizeChangedListener = object : OnVideoSizeChangedListener {
        @TargetApi(VERSION_CODES.JELLY_BEAN_MR1)
        override fun onVideoSizeChanged(mp: IMediaPlayer, width: Int, height: Int) {
            println("player width:${mp.videoWidth},height:${mp.videoHeight}")
            mVideoWidth = mp.videoWidth
            mVideoHeight = mp.videoHeight
            val displayWidth = display.width
            if (mVideoWidth != 0 && mVideoHeight != 0) {
                val params = layoutParams
                params.height = displayWidth * mVideoHeight / mVideoWidth
                layoutParams = params
            }
        }
    }
    private var mPreparedListener = object : OnPreparedListener {
        override fun onPrepared(mp: IMediaPlayer) {
            mCurrentState = STATE_PREPARED
            mMediaController?.show()
            mOnPreparedListener?.onPrepared(mp)
        }
    }
    private var mCompletionListener = object : OnCompletionListener {
        override fun onCompletion(mp: IMediaPlayer) {
            mCurrentState = STATE_PLAYBACK_COMPLETED
            mOnCompleteListener?.onCompletion(mp)
        }
    }
    private val mErrorListener = object : IMediaPlayer.OnErrorListener {
        override fun onError(mp: IMediaPlayer, what: Int, extra: Int, msg: String) {
            mOnErrorListener?.onError(mp, what, extra, msg)
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


    fun registerOnErrorListener(listener: IMediaPlayer.OnErrorListener) {
        mOnErrorListener = listener
    }

    fun registerOnCompletedListener(listener: OnCompletionListener) {
        mOnCompleteListener = listener
    }

    fun registerOnPreparedListener(listener: OnPreparedListener) {
        mOnPreparedListener = listener
    }

    /**
     * 设置视频地址
     *
     * @param path 视频url
     */
    fun setVideoPath(path: String) {
        if (this.mUri != path) {
            this.mUri = path
            openVideo()
        }
    }

    /**
     * 开始播放
     */
    override fun start() {
        if (mMediaPlayer != null) {
            mMediaPlayer!!.start()
        }
    }

    /**
     * 暂停播放
     */
    override fun pause() {
        if (mMediaPlayer != null) {
            mMediaPlayer!!.pause()
        }
    }

    /**
     * 开始录制播放的视频
     * @param path 录制的视频保存地址
     */
    fun startRecorder(path: String) {
        if (mMediaPlayer != null) {
            mMediaPlayer!!.startRecorder(path)
        }
    }

    /**
     * 停止录制
     */
    fun stopRecorder() {
        if (mMediaPlayer != null) {
            mMediaPlayer!!.stopRecorder()
        }
    }

    /**
     * 获取视频长度
     */
    override fun getDuration(): Int {
        return if (mMediaPlayer != null && mCurrentState >= STATE_PREPARED) {
            mMediaPlayer!!.duration.toInt()
        } else -1
    }

    /**
     * 获取视频的当前播放进度
     */
    override fun getCurrentPosition(): Int {
        return if (mMediaPlayer != null && mCurrentState >= STATE_PREPARED) {
            mMediaPlayer!!.currentPosition.toInt()
        } else 0
    }

    /**
     * 拖动视频
     * @param pos 拖动的位置
     */
    override fun seekTo(pos: Int) {
        if (mMediaPlayer != null && mCurrentState >= STATE_PREPARED) {
            mMediaPlayer!!.seekTo(pos.toLong())
        }
    }

    /**
     * 是否正在播放视频
     */
    override fun isPlaying(): Boolean {
        return if (mMediaPlayer == null) {
            false
        } else
            mMediaPlayer!!.isPlaying
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
            mMediaPlayer!!.setDisplay(holder)
            mMediaPlayer!!.setBackgroundState(false)
            attachController()
            post {
                mSizeChangedListener.onVideoSizeChanged(mMediaPlayer!!, mVideoWidth, mVideoHeight)
            }
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
        println("====surfaceDestroyed")
        if (mMediaPlayer != null) {
            mMediaPlayer!!.setBackgroundState(true)
        }
    }

    override fun onTouchEvent(ev: MotionEvent): Boolean {
        if (/*isInPlaybackState() &&*/ mMediaController != null && parent != null) {
            toggleMediaControlsVisiblity()
        }
        return false
    }

    private fun openVideo() {
        if (mUri == null || mSurfaceHolder == null) {
            return
        }
        if (mMediaPlayer == null)
            mMediaPlayer = EasyMediaPlayer()
        try {
            mMediaPlayer!!.setDataSource(mUri!!)
            println("======set data source completed")
            mMediaPlayer!!.setSurface(mSurfaceHolder!!.surface)
            println("======set surface completed")
            mMediaPlayer!!.prepareAsync()
            println("======prepare completed")
            mCurrentState = STATE_PREPARING
            attachController()
        } catch (e: Exception) {

        }

    }

    private fun attachController() {
        mMediaController = MediaController(context)
        mMediaController!!.setMediaPlayer(this)
        mMediaController!!.setAnchorView(mSurfaceView)
        mMediaController!!.isEnabled = true
        mMediaController!!.hide()
    }

    private fun toggleMediaControlsVisiblity() {
        try {
            if (mMediaController?.isShowing == true) {
                mMediaController?.hide()
            } else {
                mMediaController?.show()
            }
        } catch (e: Exception) {
            println(e.message)
        }

    }

    /**
     * 是否可以后台播放音频
     * @param canPlayInBackGround 是否可以后台播放视频 默认不可以
     */
    fun setCanPlayBackGround(canPlayInBackGround: Boolean) {
        mMediaPlayer?.setBackground(canPlayInBackGround)
    }

    /**
     * 进入后台
     */
    fun onActivityPause() {
        mMediaPlayer?.setBackgroundState(true)
    }

    /**
     * 进入前台
     */
    fun onActivityResume() {
        mMediaPlayer?.setBackgroundState(false)
    }

    /**
     * 销毁,清除播放器
     */
    fun onDestroy() {
        mMediaPlayer?.release()
    }

}
