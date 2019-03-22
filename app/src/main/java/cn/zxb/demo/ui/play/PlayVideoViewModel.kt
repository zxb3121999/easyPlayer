package cn.zxb.demo.ui.play

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import cn.zxb.media.player.IMediaPlayer
import com.example.zxb.mvvm.scope.ScopedViewModel

class PlayVideoViewModel : ScopedViewModel() {
    var player: IMediaPlayer? = null
    override fun onCleared() {
        player?.release()
        super.onCleared()
    }
}

class PlayViewModelFactory() : ViewModelProvider.Factory {
    override fun <T : ViewModel?> create(modelClass: Class<T>): T {
        return PlayVideoViewModel() as T
    }

}
