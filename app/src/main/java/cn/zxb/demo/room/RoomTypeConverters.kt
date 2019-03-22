package cn.zxb.demo.room

import androidx.room.TypeConverter
import cn.zxb.demo.utils.TimeUtils

class RoomTypeConverters {
    companion object {
        @TypeConverter
        fun stringForTime(time:Int):String= TimeUtils.stringForTime(time)
        @TypeConverter
        fun timeForString(time:String):Int = TimeUtils.timeForString(time)
    }
}