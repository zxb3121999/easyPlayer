/*
 * Copyright (C) 2013-2014 Zhang Rui <bbcallen@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package cn.jx.easyplayerlib.player


abstract class AbstractMediaPlayer : IMediaPlayer {
  private var mOnPreparedListener: IMediaPlayer.OnPreparedListener? = null
  private var mOnCompletionListener: IMediaPlayer.OnCompletionListener? = null
  private var mOnBufferingUpdateListener: IMediaPlayer.OnBufferingUpdateListener? = null
  private var mOnSeekCompleteListener: IMediaPlayer.OnSeekCompleteListener? = null
  private var mOnVideoSizeChangedListener: IMediaPlayer.OnVideoSizeChangedListener? = null
  private var mOnErrorListener: IMediaPlayer.OnErrorListener? = null
  private var mOnInfoListener: IMediaPlayer.OnInfoListener? = null

  override fun setOnPreparedListener(listener: IMediaPlayer.OnPreparedListener) {
    mOnPreparedListener = listener
  }

  override fun setOnCompletionListener(listener: IMediaPlayer.OnCompletionListener) {
    mOnCompletionListener = listener
  }

  override fun setOnBufferingUpdateListener(
      listener: IMediaPlayer.OnBufferingUpdateListener) {
    mOnBufferingUpdateListener = listener
  }

  override fun setOnSeekCompleteListener(listener: IMediaPlayer.OnSeekCompleteListener) {
    mOnSeekCompleteListener = listener
  }

  override fun setOnVideoSizeChangedListener(
      listener: IMediaPlayer.OnVideoSizeChangedListener) {
    mOnVideoSizeChangedListener = listener
  }

  override fun setOnErrorListener(listener: IMediaPlayer.OnErrorListener) {
    mOnErrorListener = listener
  }

  override fun setOnInfoListener(listener: IMediaPlayer.OnInfoListener) {
    mOnInfoListener = listener
  }

  fun resetListeners() {
    mOnPreparedListener = null
    mOnBufferingUpdateListener = null
    mOnCompletionListener = null
    mOnSeekCompleteListener = null
    mOnVideoSizeChangedListener = null
    mOnErrorListener = null
    mOnInfoListener = null
  }

  protected fun notifyOnPrepared() {
    if (mOnPreparedListener != null)
      mOnPreparedListener!!.onPrepared(this)
  }

  protected fun notifyOnCompletion() {
    if (mOnCompletionListener != null)
      mOnCompletionListener!!.onCompletion(this)
  }

  protected fun notifyOnBufferingUpdate(percent: Int) {
    if (mOnBufferingUpdateListener != null)
      mOnBufferingUpdateListener!!.onBufferingUpdate(this, percent)
  }

  protected fun notifyOnSeekComplete() {
    if (mOnSeekCompleteListener != null)
      mOnSeekCompleteListener!!.onSeekComplete(this)
  }

  protected fun notifyOnVideoSizeChanged(width: Int, height: Int) {
    if (mOnVideoSizeChangedListener != null)
      mOnVideoSizeChangedListener!!.onVideoSizeChanged(this, width, height)
  }

  protected fun notifyOnError(what: Int, extra: Int): Boolean {
    return mOnErrorListener != null && mOnErrorListener!!.onError(this, what, extra)
  }

  protected fun notifyOnInfo(what: Int, extra: Int): Boolean {
    return mOnInfoListener != null && mOnInfoListener!!.onInfo(this, what, extra)
  }


}
