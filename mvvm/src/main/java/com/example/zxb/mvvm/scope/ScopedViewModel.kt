package com.example.zxb.mvvm.scope

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel

/**
 * 所有的ViewModel都应该继承此类，在[onCleared]方法中统一取消所有协程
 */
abstract class ScopedViewModel:ViewModel(),CoroutineScope by MainScope() {
    override fun onCleared() {
        super.onCleared()
        cancel()
    }
}