package cn.zxb.demo.utils

object TimeUtils {
    /**
     * 将时间格式化为h:MM:ss
     * @param time
     */
    fun stringForTime(time:Int):String{
        val totalSeconds = time / 1000

        val seconds = totalSeconds % 60
        val minutes = totalSeconds / 60 % 60
        val hours = totalSeconds / 3600
        return if (hours > 0) {
            String.format("%d:%02d:%02d", hours, minutes, seconds)
        } else {
            String.format("%02d:%02d", minutes, seconds)
        }
    }

    /**
     * 将h:MM:ss的字符串格式化为时间长度
     * @param s
     */
    fun timeForString(s:String):Int{
        val array = s.split(":")
        return when {
            array.size==3 -> (array[0].toInt()*3600+ array[1].toInt()*60+ array[2].toInt())*1000
            array.size==2 -> (array[0].toInt()*60+ array[1].toInt())*1000
            else -> 0
        }
    }
}