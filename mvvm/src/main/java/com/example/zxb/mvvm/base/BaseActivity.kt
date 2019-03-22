package com.example.zxb.mvvm.base

import android.os.Bundle
import android.os.PersistableBundle
import androidx.appcompat.app.AppCompatActivity
import androidx.databinding.DataBindingUtil
import androidx.databinding.ViewDataBinding
import com.example.zxb.mvvm.BR

abstract class BaseActivity<T:ViewDataBinding>: AppCompatActivity(){
    lateinit var mBinding: T
    abstract val layoutId:Int
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        initBinding()
        initView()
    }
    open fun initView(){
    }

    override fun onDestroy() {
        super.onDestroy()
        mBinding.unbind()
    }
    private fun initBinding(){
        mBinding = DataBindingUtil.setContentView(this,layoutId)
        with(mBinding){
            setVariable(BR.activity,this@BaseActivity)
            lifecycleOwner = this@BaseActivity
        }
    }
}