package cn.zxb.demo.room.dao

import androidx.paging.DataSource
import androidx.room.*
import cn.zxb.demo.bean.Video

@Dao
interface VideoDao {
    @Query("SELECT * From videos ORDER BY viewTime COLLATE NOCASE DESC")
    fun allVideos():DataSource.Factory<Int,Video>
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    fun insert(v:Video)
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    fun insert(list:List<Video>)
    @Delete
    fun delete(v:Video):Int
}