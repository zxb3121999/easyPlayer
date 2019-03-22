package cn.zxb.demo.ui.scan

import android.content.ContentResolver
import android.database.Cursor
import android.media.MediaMetadataRetriever
import android.os.Environment
import android.provider.MediaStore
import android.webkit.MimeTypeMap
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.paging.Config
import androidx.paging.toLiveData
import cn.zxb.demo.bean.Video
import cn.zxb.demo.room.dao.VideoDao
import cn.zxb.demo.utils.TimeUtils
import com.example.zxb.mvvm.scope.ScopedViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import java.io.File

class HomeViewModel(private val dao: VideoDao) : ScopedViewModel() {
    //分页加载本地视频
    val allVideos = dao.allVideos().toLiveData(Config(
            20, enablePlaceholders = false
    ))

    /**
     * 插入视频
     * @param v
     */
    fun insertVideo(v: Video) {
        async(Dispatchers.IO) {
            dao.insert(v)
        }
    }

    /**
     * 插入视频
     * @param list
     */
    fun insertVideo(list: List<Video>) = launch(Dispatchers.IO) {
        dao.insert(list)
    }

    /**
     * 删除视频
     * @param v
     */
    suspend fun deleteVideo(v: Video): Boolean {
        val job = async(Dispatchers.IO) {
            if (dao.delete(v) > 0) {
                with(File(v.image ?: "")) {
                    if (this.exists())
                        this.delete()
                }
                return@async true
            } else
                return@async false
        }
        return job.await()
    }

    /**
     * 扫描视频文件
     * @param provider
     */
    fun scanVideo(provider: ContentResolver) = launch(Dispatchers.IO) {
        val provider = async{ scanFromProvider(provider) }
        val sdcard = async{ scanFromSdcard() }
        val list = mutableListOf<Video>()
        list.addAll(provider.await())
        list.addAll(sdcard.await())
        list.map {
            println("找到视频文件:${it.url}")
        }
        insertVideo(list)
    }

    /**
     * 从ContendProvider中获取视频文件
     */
    private fun scanFromProvider(provider: ContentResolver): List<Video> {
        var list = mutableListOf<Video>()
        var array = arrayOf(
                MediaStore.Video.Media._ID,
                MediaStore.Video.Media.DATA,
                MediaStore.Video.Media.SIZE,
                MediaStore.Video.Media.DURATION,
                MediaStore.Video.Media.DATE_ADDED,
                MediaStore.Video.Media.DISPLAY_NAME)
        var thumbColumns = arrayOf(MediaStore.Video.Thumbnails.DATA)
        var cursor: Cursor = provider.query(
                MediaStore.Video.Media.EXTERNAL_CONTENT_URI,
                array, null, null,
                MediaStore.Video.Media.DATE_ADDED) ?: return list
        cursor.use {
            if (it.count > 0) {
                it.moveToFirst()
                do {
                    var v = Video(
                            url = it.getString(1),
                            size = it.getLong(2),
                            duration = TimeUtils.stringForTime(it.getInt(3)),
                            viewTime = it.getLong(4),
                            name = it.getString(5)
                    )
                    var c = provider.query(
                            MediaStore.Video.Thumbnails.EXTERNAL_CONTENT_URI,
                            thumbColumns,
                            "${MediaStore.Video.Thumbnails.VIDEO_ID}=${it.getInt(0)}", null, null)
                    c?.use { c ->
                        c.moveToFirst()
                        v.image = c.getString(0)
                    }
                    list.add(v)
                } while (cursor.moveToNext())
            }
        }
        return list
    }

    private fun scanFromSdcard(): List<Video> {
        var root = Environment.getExternalStorageDirectory()
        val list = mutableListOf<Video>()
        scanFiles(root,list)
        return list
    }

    /**
     *递规遍历文件目录，找出其中的视频文件
     * @param f
     * @param list
     */
    private fun scanFiles(f: File, list: MutableList<Video>) {
        if (f.isDirectory) {
            val files = f.listFiles { file ->
                if (file.isDirectory)
                    return@listFiles true
                else {
                    var mimeTypeStr = MimeTypeMap.getFileExtensionFromUrl(file.absolutePath)
                    var mimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(mimeTypeStr)
                    return@listFiles mimeType?.contains("video") ?: false
                }
            }
            for (f in files) {
                if (f.isFile) {
                    with(MediaMetadataRetriever()) {
                        setDataSource(f.absolutePath)
                        list.add(Video(
                                url = f.absolutePath,
                                duration = TimeUtils.stringForTime(extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION).toInt()),
                                size = f.length(),
                                viewTime = f.lastModified(),
                                name = f.name
                                ))
                        release()
                    }
                }else{
                    scanFiles(f,list)
                }
            }
        }
    }
}

class HomeViewModelFactory(private val dao: VideoDao) : ViewModelProvider.Factory {
    override fun <T : ViewModel?> create(modelClass: Class<T>): T {
        return HomeViewModel(dao) as T
    }

}
