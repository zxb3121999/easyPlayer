package cn.zxb.demo.ui.view.recycler

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Rect
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.Drawable
import android.util.TypedValue
import android.view.View
import androidx.annotation.ColorInt
import androidx.annotation.ColorRes
import androidx.annotation.DimenRes
import androidx.annotation.DrawableRes
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.StaggeredGridLayoutManager


class GirdDividerItemDecoration(private var divider:Drawable) : RecyclerView.ItemDecoration() {
    private var orientation = -1
    private var spanCount = -1
    private var isInit = false
    /**
     * 画分隔线
     * @param c
     * @param parent
     * @param state
     */
    override fun onDraw(c: Canvas, parent: RecyclerView, state: RecyclerView.State) {
        if(!isInit)
            init(parent)
        drawVertical(c, parent)
        drawHorizontal(c, parent)
    }

    /**
     * 获取每个item的位置偏移量
     * @param outRect
     * @param view
     * @param parent
     * @param state
     */
    override fun getItemOffsets(outRect: Rect, view: View, parent: RecyclerView, state: RecyclerView.State) {
        if(!isInit)
            init(parent)
        if (divider.intrinsicHeight <= 0) {
            outRect.set(0, 0, 0, 0)
            return
        }
        val itemPosition = parent.getChildAdapterPosition(view)

        if (itemPosition < 0) {
            return
        }
        val column = itemPosition % spanCount
        with(outRect){
            left = column * divider.intrinsicWidth / spanCount
            right = divider.intrinsicWidth - (column + 1) * divider.intrinsicWidth / spanCount
            bottom = if(isLastRow(itemPosition,parent.adapter?.itemCount?:0)){
                0
            }else{
                divider.intrinsicHeight
            }
        }
    }

    /**
     * 画水平线
     * @param c
     * @param parent
     */
    private fun drawHorizontal(c: Canvas, parent: RecyclerView) {
        var childCount = parent.childCount
        for (index in 0 until childCount) {
            val child = parent.getChildAt(index)
            val params = child.layoutParams as RecyclerView.LayoutParams
            val left = child.left - params.leftMargin
            val right = child.right + params.rightMargin+divider.intrinsicWidth
            val top = child.bottom + params.bottomMargin
            val bottom = top + divider.intrinsicHeight
            divider.setBounds(left, top, right, bottom)
            divider.draw(c)
        }
    }

    /**
     * 画垂直分隔线
     * @param c
     * @param parent
     */
    private fun drawVertical(c: Canvas, parent: RecyclerView) {
        val childCount = parent.childCount
        for (i in 0 until childCount) {
            val child = parent.getChildAt(i)
            val params = child.layoutParams as RecyclerView.LayoutParams
            val top = child.top - params.topMargin
            val bottom = child.bottom + params.bottomMargin
            val left = child.right + params.rightMargin
            var right = left + divider.intrinsicWidth
            divider.setBounds(left, top, right, bottom)
            divider.draw(c)

        }
    }

    /**
     * 初始相关信息
     */
    private fun init(parent: RecyclerView) {
        val layoutManager = parent.layoutManager
        when (layoutManager) {
            is GridLayoutManager -> {
                orientation = layoutManager.orientation
                spanCount = layoutManager.spanCount
            }
            is StaggeredGridLayoutManager -> {
                orientation = layoutManager.orientation
                spanCount = layoutManager.spanCount
            }
            else -> {
                orientation = -1
                spanCount = -1
            }
        }
        isInit = true
    }

    /**
     * 判断当前item是否是最后一行和是否需要绘制水平分隔线
     * @param parent
     * @param position
     */
    private fun shouldDrawLastRow(parent: RecyclerView, position: Int): Boolean {
        val layoutManager = parent.layoutManager
        return when (layoutManager) {
            is GridLayoutManager -> isLastRow(position,parent.adapter?.itemCount?:0)
            is StaggeredGridLayoutManager -> {
                if (orientation == StaggeredGridLayoutManager.VERTICAL) {
                    isLastRow(position, parent.adapter?.itemCount?:0)
                } else {
                    (position + 1) % spanCount == 0
                }
            }
            else -> false

        }
    }

    /**
     * 判断当前item是否是最后一行和是否需要绘制分隔线
     * @param position
     * @param childCount
     */
    private fun isLastRow(position: Int, childCount: Int): Boolean {
        val remainCount = childCount % spanCount//获取余数
        //如果正好最后一行完整;
        return if (remainCount == 0) {
            position >= childCount - spanCount//最后一行全部不绘制;
        } else {
            position >= childCount - childCount % spanCount
        }
    }
}
private class _Drawable(private var width:Int=0,private var height:Int=0,@ColorInt color: Int):ColorDrawable(color){

    override fun getIntrinsicWidth() =width

    override fun getIntrinsicHeight()=height
}
class GirdDividerItemDecorationFactory(private val context: Context) {
    private var dividerHeight: Int = 10
    private var color = Color.TRANSPARENT
    private val resources = context.resources
    private var drawable: Drawable? = null
    private var dividerWidth: Int = 10
    /**
     * 通过资源文件设置颜色
     * @param res 颜色值的资源id
     */
    fun setColorRes(@ColorRes res: Int): GirdDividerItemDecorationFactory {
        return setColor(ContextCompat.getColor(context, res))
    }

    /**
     * 通过颜色值设置颜色
     * @param color
     */
    fun setColor(@ColorInt color: Int): GirdDividerItemDecorationFactory {
        this.color = color
        return this
    }

    /**
     * 设置分隔线的高度(px)
     *@param h(px)
     */
    fun setDividerHeight(h: Int): GirdDividerItemDecorationFactory {
        this.dividerHeight = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_PX, h.toFloat(), resources.displayMetrics).toInt()
        return this
    }

    /**
     * 通过资源文件设置分隔线的高度
     * @param res 资源id
     */
    fun setDividerHeightRes(@DimenRes res: Int): GirdDividerItemDecorationFactory {
        dividerHeight = resources.getDimensionPixelSize(res)
        return this
    }
    /**
     * 设置分隔线的宽度(px)
     *@param w(px)
     */
    fun setDividerWidth(w: Int): GirdDividerItemDecorationFactory {
        this.dividerHeight = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_PX, w.toFloat(), resources.displayMetrics).toInt()
        return this
    }

    /**
     * 通过资源文件设置分隔线的宽度
     * @param res 资源id
     */
    fun setDividerWidthRes(@DimenRes res: Int): GirdDividerItemDecorationFactory {
        dividerHeight = resources.getDimensionPixelSize(res)
        return this
    }

    /**
     * 设置分隔线的宽高度
     * @param wh
     */
    fun setWidthAndHeight(wh:Int):GirdDividerItemDecorationFactory{
        TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_PX, wh.toFloat(), resources.displayMetrics).toInt().also {
            dividerWidth = it
            dividerHeight = it
        }
        return this
    }
    /**
     * 通过资源文件设置分隔线的宽高度
     * @param res 资源id
     */
    fun setWidthAndHeightRes(@DimenRes res:Int):GirdDividerItemDecorationFactory{
        resources.getDimensionPixelSize(res).also {
            dividerHeight = it
            dividerWidth =it
        }
        return this
    }
    fun setDrawable(@DrawableRes res:Int):GirdDividerItemDecorationFactory{
        drawable = resources.getDrawable(res)
        return this
    }
    fun build(): GirdDividerItemDecoration {
        return GirdDividerItemDecoration(drawable?:_Drawable(dividerWidth,dividerHeight,color))
    }
}