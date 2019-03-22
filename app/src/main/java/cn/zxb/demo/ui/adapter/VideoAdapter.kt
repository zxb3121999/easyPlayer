package cn.zxb.demo.ui.adapter

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.databinding.DataBindingUtil
import androidx.recyclerview.widget.DiffUtil
import cn.zxb.demo.R
import cn.zxb.demo.bean.Video
import cn.zxb.demo.databinding.VideoItemBinding
import com.example.zxb.mvvm.binding.DataBindingAdapter

class VideoAdapter() : DataBindingAdapter<Video, VideoItemBinding>(
        diffCallback = object:DiffUtil.ItemCallback<Video>(){
            override fun areContentsTheSame(oldItem: Video, newItem: Video): Boolean {
               return oldItem.url==newItem.url&&oldItem.image==newItem.image
            }

            override fun areItemsTheSame(oldItem: Video, newItem: Video): Boolean {
                return oldItem.url==newItem.url
            }

        }
) {
    override fun createBinding(parent: ViewGroup) =
            DataBindingUtil.inflate<VideoItemBinding>(
                    LayoutInflater.from(parent.context),
                    R.layout.video_item,
                    parent,
                    false
            )!!


    override fun binding(binding: VideoItemBinding, item: Video) {
        binding.video = item
    }
}