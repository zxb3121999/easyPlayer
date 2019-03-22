package cn.zxb.media.player

import android.graphics.SurfaceTexture
import android.os.Handler
import android.os.Message
import android.view.Surface
import android.view.SurfaceHolder
import cn.zxb.media.util.EasyLog
import java.io.IOException

/**
 * Java wrapper of native easyPlayer.
 */

class EasyMediaPlayer : AbstractMediaPlayer() {
    override var currentState: Int = STATE_IDLE

    override fun getMediaId(): Int {
        return mediaId
    }

    private var isSeekTo = false
    private var seekTime = 0L
    private var mediaId: Int = -1
    private var mScreenOnWhilePlaying: Boolean = false
    private val mStayAwake: Boolean = false
    override var videoWidth: Int = 0
    override var videoHeight: Int = 0
    private var mSurfaceHolder: SurfaceHolder? = null

    private val mHander = Handler(Handler.Callback { msg ->
        when (msg.what) {
            MEDIA_SET_VIDEO_SIZE -> {
                videoWidth = msg.arg1
                videoHeight = msg.arg2
                notifyOnVideoSizeChanged(videoWidth, videoHeight)
            }
            MEDIA_PREPARED -> {
                currentState = STATE_PREPARED
                notifyOnPrepared()
            }
            MEDIA_PLAYBACK_COMPLETE -> {
                currentState = STATE_PLAYBACK_COMPLETED
                notifyOnCompletion()
            }
            MEDIA_SEEK_COMPLETE -> {
                isSeekTo = false
                seekTime = 0L
            }
            MEDIA_PAUSE -> {
                currentState = STATE_PAUSED
            }
            MEDIA_PLAYING -> {
                currentState = STATE_PLAYING
            }
            MEDIA_ERROR -> {
                currentState = STATE_ERROR
                notifyOnError(msg.arg1, msg.arg2, msg.obj.toString())
                release()
            }
            6 -> {
                println(msg.obj.toString())
            }
        }
        true
    })
    override var isPlaying = false
        get() = getPlaying(mediaId)

    private external fun getPlaying(playerId: Int): Boolean
    override var currentPosition = 0L
        get() = if (isSeekTo) seekTime else getCurrentPosition(mediaId)

    private external fun getCurrentPosition(playerId: Int): Long

    override var duration = 0L
        get() = getDuration(mediaId)

    private external fun getDuration(playerId: Int): Long

    override var audioSessionId: Int = 0

    private external fun _nativeInit(): Int

    init {
        mediaId = _nativeInit()
        println("======init media player completed")
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
        _setDataSource(path, mediaId)
    }

    @Throws(IOException::class, IllegalArgumentException::class, SecurityException::class, IllegalStateException::class)
    private external fun _setDataSource(path: String, id: Int)

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
    override fun setDisplay(sh: SurfaceHolder?) {
        mSurfaceHolder = sh
        _setVideoSurface(sh?.surface, mediaId)
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
        _setVideoSurface(surface, mediaId)
        updateSurfaceScreenOn()
    }

    /*
       * Update the SurfaceTexture. Call after setting a new
       * display surface.
       */
    private external fun _setVideoSurface(surface: Surface?, playerId: Int)


    @Throws(IllegalStateException::class)
    override fun prepareAsync() {
        _prepareAsync(mediaId)
    }

    @Throws(IllegalStateException::class)
    override fun start() {
        _start(mediaId)
    }

    @Throws(IllegalStateException::class)
    private external fun _start(playerId: Int)

    @Throws(IllegalStateException::class)
    override fun restart() {
        _restart(mediaId)
    }

    @Throws(IllegalStateException::class)
    private external fun _restart(playerId: Int)

    @Throws(IllegalStateException::class)
    override fun stop() {

    }

    @Throws(IllegalStateException::class)
    override fun pause() {
        _pause(mediaId)
    }

    @Throws(IllegalStateException::class)
    private external fun _pause(playerId: Int)

    @Throws(IllegalStateException::class)
    override fun seekTo(msec: Long) {
        seekTime = msec
        isSeekTo = _seekTo(msec, mediaId)
    }

    private external fun _seekTo(msec: Long, playerId: Int): Boolean
    override fun release() {
        updateSurfaceScreenOn()
        resetListeners()
        _release(mediaId)
    }

    private external fun _release(playerId: Int)

    override fun reset() {

    }

    override fun setVolume(leftVolume: Float, rightVolume: Float) {

    }

    override fun setBackground(background: Boolean) {
        _canPlayBackground(background, mediaId)
    }

    private external fun _canPlayBackground(canPlay: Boolean, playerId: Int)
    override fun setBackgroundState(isInBackground: Boolean) {
        _setBackgroundState(isInBackground, mediaId)
    }

    external fun _setBackgroundState(isBackground: Boolean, playerId: Int)
    @Throws(IllegalStateException::class)
    external fun _prepareAsync(playerId: Int)

    private external fun _startRecorder(path: String, playerId: Int)
    override fun startRecorder(path: String) {
        _startRecorder(path, playerId = mediaId)
    }

    private external fun _stopRecorder(playerId: Int)
    override fun stopRecorder() {
        _stopRecorder(mediaId)
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
        mSurfaceHolder?.setKeepScreenOn(mScreenOnWhilePlaying && mStayAwake)
    }


    private fun postEventFromNative(what: Int, arg1: Int, arg2: Int, msg: String) {
        EasyLog.i(TAG, "recv message from native,what $what")
        Message.obtain(mHander, what, arg1, arg2, msg).sendToTarget()
    }

    companion object {
        init {
            System.loadLibrary("player-lib")
        }

        private val TAG = EasyMediaPlayer::class.java!!.simpleName

        private val MEDIA_NOP = 0 // interface test message
        private val MEDIA_PREPARED = 10
        private val MEDIA_PLAYBACK_COMPLETE = 20
        private val MEDIA_BUFFERING_UPDATE = 30
        private val MEDIA_SEEK_COMPLETE = 40
        private val MEDIA_SET_VIDEO_SIZE = 50
        private val MEDIA_PAUSE = 80
        private val MEDIA_PLAYING = 70
        private val MEDIA_TIMED_TEXT = 99
        private val MEDIA_ERROR = -1
        private val MEDIA_INFO = 200
        const val STATE_ERROR = -1
        const val STATE_IDLE = 0
        const val STATE_PREPARING = 1
        const val STATE_PREPARED = 2
        const val STATE_PLAYING = 3
        const val STATE_PAUSED = 4
        const val STATE_PLAYBACK_COMPLETED = 5
    }


}
