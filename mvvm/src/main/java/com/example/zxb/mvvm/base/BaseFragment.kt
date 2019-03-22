package com.example.zxb.mvvm.base

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.databinding.DataBindingUtil
import androidx.databinding.ViewDataBinding
import androidx.fragment.app.Fragment
import com.example.zxb.mvvm.BR
import com.example.zxb.mvvm.utils.autoCleared

abstract class BaseFragment<T:ViewDataBinding>:Fragment(){
    abstract val layoutId:Int
    var binding:T by autoCleared()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        binding = initBinding(inflater,container)
        return binding.root
    }
    private fun initBinding(inflater: LayoutInflater,container: ViewGroup?):T{
        val dataBinding = DataBindingUtil.inflate<T>(inflater,layoutId,container,false)
        with(dataBinding){
            setVariable(BR.fragment,this@BaseFragment)
            lifecycleOwner = this@BaseFragment
        }
        return dataBinding
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        initView()
    }
    open fun initView(){}
}