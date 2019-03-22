package cn.zxb.demo.room

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import cn.zxb.demo.bean.Video
import cn.zxb.demo.room.dao.VideoDao

@Database(entities = [Video::class], version = 1)
abstract class VideoDb : RoomDatabase() {
    abstract fun videoDao(): VideoDao

    companion object {
        @Volatile
        private var INSTANCE: VideoDb? = null

        fun get(context: Context): VideoDb =
                INSTANCE ?: synchronized(this) {
                    INSTANCE ?: buildDateBase(context).also { INSTANCE = it }
                }

        private fun buildDateBase(context: Context) =
                Room.databaseBuilder(
                        context.applicationContext,
                        VideoDb::class.java,
                        "video.db")
                        .build()
    }
}