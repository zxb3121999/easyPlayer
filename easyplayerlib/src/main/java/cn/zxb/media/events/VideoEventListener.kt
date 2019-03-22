package cn.zxb.media.events

/**
 *
 */

interface VideoEventListener {

  fun onResolutionChange(width: Int, height: Int)
}
