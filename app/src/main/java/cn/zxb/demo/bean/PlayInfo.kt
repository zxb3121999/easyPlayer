package cn.zxb.demo.bean

import androidx.room.Entity
import androidx.room.ForeignKey
import androidx.room.PrimaryKey

@Entity(tableName = "playInfo", foreignKeys = [ForeignKey(
        entity = Video::class,
        parentColumns =["url"],
        childColumns = ["url"],
        onUpdate = ForeignKey.CASCADE,
        onDelete = ForeignKey.CASCADE
        )])
data class PlayInfo(
        @PrimaryKey
        val url: String) {
        var playTime:Long=0
}