package cn.zxb.demo.ui

import android.os.Bundle
import androidx.databinding.DataBindingUtil
import androidx.navigation.findNavController
import cn.zxb.demo.R
import cn.zxb.demo.databinding.ActivityHomeBinding
import com.example.zxb.mvvm.base.BaseActivity

class HomeActivity : BaseActivity<ActivityHomeBinding>() {
    override val layoutId: Int = R.layout.activity_home
    override fun onSupportNavigateUp(): Boolean = findNavController(R.id.navHostFragment).navigateUp()
}
