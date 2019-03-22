package com.example.zxb.mvvm.binding

import android.view.View
import android.view.ViewGroup
import androidx.databinding.DataBindingComponent
import androidx.databinding.ViewDataBinding
import androidx.recyclerview.widget.AsyncDifferConfig
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.channels.ActorScope
import kotlinx.coroutines.channels.actor

class DataBindingViewHolder<out T:ViewDataBinding>constructor(val binding: T)
    :RecyclerView.ViewHolder(binding.root)

/**
 * 使用[ViewDataBinding]的recyclerView的adapter的基类，
 * @param T [getItem]的类型
 * @param V [ViewDataBinding]的实现类
 */
abstract class DataBindingAdapter<T,V:ViewDataBinding>(diffCallback: DiffUtil.ItemCallback<T>)
    :ListAdapter<T,DataBindingViewHolder<V>>(
        AsyncDifferConfig.Builder<T>(diffCallback).build()){
    var itemClickListener: ((Int) -> Unit)? = null
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DataBindingViewHolder<V> {
        val binding = createBinding(parent)
        return DataBindingViewHolder(binding)
    }
    protected abstract fun createBinding(parent: ViewGroup):V

    override fun onBindViewHolder(holder: DataBindingViewHolder<V>, position: Int) {
        binding(holder.binding,getItem(position))
        holder.binding.executePendingBindings()
        if (itemClickListener!=null){
            holder.binding.root.click(position, itemClickListener!!)
        }
    }
    protected abstract fun binding(binding:V,item:T)
    public override fun getItem(position: Int): T {
        return super.getItem(position)
    }
}
fun View.click(position: Int,action:(Int)->Unit){
    val actor = GlobalScope.actor<Int>(Dispatchers.Main){
        for (e in channel) action(e)
    }
    setOnClickListener {
       actor.offer(position)
    }
}