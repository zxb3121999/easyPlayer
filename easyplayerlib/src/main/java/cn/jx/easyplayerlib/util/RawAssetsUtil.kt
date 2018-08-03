package cn.jx.easyplayerlib.util

import android.content.Context
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream

/**
 * RawAssetsUtil
 *
 * @author 张小兵
 * 功能描述：
 * 2018/6/27 0027
 */
object RawAssetsUtil {
  /**
   * 读raw文件
   * @param context
   * @param rawId
   * @param audioName
   * @return
   */
  fun getRawFilePath(context: Context, rawId: Int, audioName: String): String? {
    var afile: File? = null
    var input: InputStream? = null
    var fos: FileOutputStream? = null
    try {
      input = context.resources.openRawResource(rawId)
      if (input == null) {
        return null
      }
      val file = context.getDir("music", Context.MODE_PRIVATE)
      afile = File(file, audioName)
      if (!afile.exists()) {

        fos = FileOutputStream(afile)
        val buffer = ByteArray(input.available())
        var lenght: Int
        while (true) {
          lenght = input.read(buffer)
          if (lenght != -1)
            fos.write(buffer, 0, lenght)
          else
            break
        }
        fos.flush()
        fos.close()
        input.close()
      }
    } catch (e: Exception) {
      e.printStackTrace()
    } finally {
      if (input != null) {
        try {
          input.close()
        } catch (e: IOException) {
          e.printStackTrace()
        }

      }
      if (fos != null) {
        try {
          fos.close()
        } catch (e: IOException) {
          e.printStackTrace()
        }

      }
    }
    return if (afile != null && afile.exists()) {
      afile.absolutePath
    } else null
  }

  /**
   * 读assets文件
   * @param context
   * @param fileName
   * @return
   */
  fun getAssetsFilePath(context: Context, fileName: String): String? {
    var afile: File? = null
    var input: InputStream? = null
    var fos: FileOutputStream? = null
    try {
      input = context.resources.assets.open(fileName)
      val file = context.getDir("music", Context.MODE_PRIVATE)
      afile = File(file.absolutePath + "/" + fileName)
      if (!afile.exists()) {
        fos = FileOutputStream(afile)
        val buffer = ByteArray(1024)
        var count: Int
        while (true) {
          count = input!!.read(buffer)
          if(count > 0)
            fos.write(buffer, 0, count)
          else
            break
        }
        fos.flush()
        fos.close()
        input!!.close()
      }
    } catch (e: IOException) {
      e.printStackTrace()
    } finally {
      if (input != null) {
        try {
          input.close()
        } catch (e: IOException) {
          e.printStackTrace()
        }

      }
      if (fos != null) {
        try {
          fos.close()
        } catch (e: IOException) {
          e.printStackTrace()
        }

      }
    }
    return if (afile != null && afile.exists()) {
      afile.absolutePath
    } else null
  }
}
