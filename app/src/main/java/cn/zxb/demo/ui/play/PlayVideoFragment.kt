package cn.zxb.demo.ui.play


import android.os.Bundle
import android.view.ViewGroup
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.lifecycle.ViewModelProviders
import cn.zxb.demo.R
import cn.zxb.demo.databinding.PlayVideoFragmentBinding
import cn.zxb.media.player.EasyMediaPlayer
import cn.zxb.media.player.IMediaPlayer
import com.example.zxb.mvvm.base.BaseFragment
import kotlinx.android.synthetic.main.play_video_fragment.*


class PlayVideoFragment : BaseFragment<PlayVideoFragmentBinding>() {
    override val layoutId = R.layout.play_video_fragment
    private lateinit var viewModel: PlayVideoViewModel
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        retainInstance = true
    }

    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)
        viewModel = ViewModelProviders.of(this, PlayViewModelFactory()).get(PlayVideoViewModel::class.java)
        val arguments = PlayVideoFragmentArgs.fromBundle(arguments)
//        addPlayVideoView()
//        binding.url = "http://replay.live.tianya.cn/live-151574941100022906--20180112173015.mp4"
//        viewModel.play("http://replay.live.tianya.cn/live-151574941100022906--20180112173015.mp4")
        easy_video_view.setCanPlayBackGround(true)
        easy_video_view.registerOnPreparedListener(object:IMediaPlayer.OnPreparedListener{
            override fun onPrepared(mp: IMediaPlayer) {
                viewModel.player = mp
            }
        })
        if (viewModel.player != null){
            easy_video_view.mMediaPlayer = viewModel.player
        }else{
            easy_video_view.setVideoPath("https://vd3.bdstatic.com/mda-im2inn4vub8dime9/sc/mda-im2inn4vub8dime9.mp4?auth_key=1551928029-0-0-cae70180679f09e65d4c1a8dfbe4f1b3&bcevod_channel=searchbox_feed&pd=bjh&abtest=all")
        }
    }

}
