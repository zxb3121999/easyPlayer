package com.example.zxb.mvvm.glide

import android.content.Context
import com.bumptech.glide.Glide
import com.bumptech.glide.GlideBuilder
import com.bumptech.glide.Registry
import com.bumptech.glide.annotation.GlideModule
import com.bumptech.glide.load.engine.cache.InternalCacheDiskCacheFactory
import com.bumptech.glide.load.engine.cache.LruResourceCache
import com.bumptech.glide.module.AppGlideModule

@GlideModule
class GlideAppModule : AppGlideModule() {
    override fun registerComponents(context: Context, glide: Glide, registry: Registry) {

    }

    override fun applyOptions(context: Context, builder: GlideBuilder) {
        with(builder) {
            setDiskCache(InternalCacheDiskCacheFactory(context,
                    diskCacheFolderName(),
                    diskCacheSizeBytes()))
            setMemoryCache(LruResourceCache(memoryCacheSizeBytes()))
        }
    }

    /**
     * Implementations should return `false` after they and their dependencies have migrated
     * to Glide's annotation processor.
     */
    override fun isManifestParsingEnabled(): Boolean {
        return false
    }

    /**
     * set the memory cache size, unit is the [Byte].
     */
    private fun memoryCacheSizeBytes(): Long {
        return 1024 * 1024 * 20L // 20 MB
    }

    /**
     * set the disk cache size, unit is the [Byte].
     */
    private fun diskCacheSizeBytes(): Long {
        return 1024 * 1024 * 512 // 512 MB
    }

    /**
     * set the disk cache folder's name.
     */
    private fun diskCacheFolderName(): String {
        return "player-image"
    }
}