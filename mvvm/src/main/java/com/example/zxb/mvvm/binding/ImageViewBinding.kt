package com.example.zxb.mvvm.binding

import android.widget.ImageView
import androidx.databinding.BindingAdapter
import com.example.zxb.mvvm.glide.GlideApp
import java.io.File

@BindingAdapter("imageUrl")
fun bindImageUrl(imageView: ImageView, url: String) {
    GlideApp.with(imageView).load(url).into(imageView)
}
@BindingAdapter("imageCircleUrl")
fun bindCircleImageUrl(imageView: ImageView,url: String){
    GlideApp.with(imageView).load(url).into(imageView)
}
@BindingAdapter("imageFile")
fun bindImageFile(imageView: ImageView,url: String){
    GlideApp.with(imageView).load(File(url)).into(imageView)
}
