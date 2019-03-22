package cn.zxb.demo.bean

import androidx.room.Entity

@Entity(tableName = "videos",primaryKeys = ["url"])
data class Video(
    var url:String,
    var name:String?="",
    var size:Long=0,
    var duration: String,
    var viewTime:Long
){
    var image:String? = null
    fun sizeText():String{
        return when {
            size>1024*1024 -> String.format("%.2fMB",size / (1024 * 1024.00))
            size>1024 -> String.format("%.2fKB",size / 1024.00)
            else -> "${size}B"
        }
    }
}