package cn.zxb.demo.ui.scan

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Bundle
import androidx.core.app.ActivityCompat
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProviders
import androidx.navigation.fragment.findNavController
import cn.zxb.demo.R
import cn.zxb.demo.databinding.ScanVideoFragmentBinding
import cn.zxb.demo.room.VideoDb
import cn.zxb.demo.ui.adapter.VideoAdapter
import cn.zxb.demo.ui.view.recycler.GirdDividerItemDecoration
import cn.zxb.demo.ui.view.recycler.GirdDividerItemDecorationFactory
import cn.zxb.media.OpenSSlDemo
import com.example.zxb.mvvm.base.BaseFragment

class ScanVideoFragment: BaseFragment<ScanVideoFragmentBinding>() {
    override val layoutId: Int = R.layout.scan_video_fragment
    lateinit var viewModel: HomeViewModel
    val adapter = VideoAdapter()
    lateinit var itemDivider: GirdDividerItemDecoration
    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)
        adapter.itemClickListener={
            var item = adapter.getItem(it)
            findNavController().navigate(ScanVideoFragmentDirections.playVideo(item.url))
        }
        itemDivider = GirdDividerItemDecorationFactory(activity!!)
                .setColor(Color.BLACK)
                .setWidthAndHeight(15)
                .build()
        viewModel = ViewModelProviders.of(this,HomeViewModelFactory(VideoDb.get(activity!!).videoDao())).get(HomeViewModel::class.java)
        viewModel.allVideos.observe(this, Observer{
            adapter.submitList(it)
        })
    }
    fun scanVideo(){
        if (ActivityCompat.checkSelfPermission(activity!!, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE), 0)
        }else{
            viewModel.scanVideo(activity!!.contentResolver)
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        if (requestCode==0&&grantResults[0]==PackageManager.PERMISSION_GRANTED){
            viewModel.scanVideo(activity!!.contentResolver)
        }
    }
}
