package com.qi.vovotool;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.PathInterpolator;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

/** Three accessible click targets above one moving, resizing selection capsule. */
final class FloatingNavigation extends ViewGroup {
    interface OnSelect { void select(int index); }
    private final MaterialUi ui;
    private final LinearLayout[] items=new LinearLayout[3];
    private final ImageView[] icons=new ImageView[3];
    private final TextView[] labels=new TextView[3];
    private final float[] expansion={1,0,0};
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF capsule=new RectF();
    private ValueAnimator animator;
    private float pillLeft,pillWidth;
    private int selected;
    private final HorizontalDrag drag;
    private float dragOrigin,dragPosition;
    FloatingNavigation(Context context,MaterialUi ui,OnSelect action){
        super(context);this.ui=ui;setWillNotDraw(false);setBackground(ui.round(ui.navigationSurface,40));
        String[] names={"首页","管理","关于"};
        for(int i=0;i<3;i++){
            final int index=i;
            LinearLayout item=new LinearLayout(context);item.setGravity(Gravity.CENTER);item.setOrientation(LinearLayout.HORIZONTAL);
            item.setContentDescription(names[i]);item.setFocusable(true);item.setBackground(ui.feedback(android.graphics.Color.TRANSPARENT,32));ui.pressFeedback(item);
            item.setOnClickListener(v->action.select(index));items[i]=item;addView(item);
            ImageView icon=new ImageView(context);icon.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);icons[i]=icon;item.addView(icon,new LinearLayout.LayoutParams(ui.dp(24),ui.dp(24)));
            TextView label=new TextView(context);label.setText(names[i]);label.setSingleLine(true);label.setGravity(Gravity.CENTER);label.setTextSize(15);label.setTypeface(null,android.graphics.Typeface.BOLD);label.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);labels[i]=label;item.addView(label,new LinearLayout.LayoutParams(0,-1));
        }
        drag=new HorizontalDrag(this,new HorizontalDrag.Listener(){
            public void start(float x){if(animator!=null)animator.cancel();dragOrigin=x;dragPosition=(pillLeft-ui.dp(8))/Math.max(1,unit());}
            public void move(float x){
                float position=Math.max(0,Math.min(2,dragPosition+(x-dragOrigin)/Math.max(1,unit())));
                pillLeft=ui.dp(8)+unit()*position;pillWidth=unit()*1.65f;
                for(int i=0;i<3;i++)expansion[i]=Math.max(0,1-Math.abs(position-i));
                updateItems();requestLayout();invalidate();
            }
            public void end(boolean canceled){int target=canceled?selected:Math.round((pillLeft-ui.dp(8))/Math.max(1,unit()));if(target==selected)select(selected,true);else action.select(target);}
        });
        updateItems();
    }
    @Override public boolean onInterceptTouchEvent(MotionEvent event){return drag.intercept(event);}
    @Override public boolean onTouchEvent(MotionEvent event){return drag.touch(event)||super.onTouchEvent(event);}
    private float unit(){return Math.max(0,getMeasuredWidth()-ui.dp(16))/3.65f;}
    private float targetLeft(int index){return ui.dp(8)+unit()*index;}
    void select(int index,boolean animate){
        if(animator!=null)animator.cancel();
        selected=index;for(int i=0;i<3;i++)items[i].setSelected(i==index);
        if(!animate||!isLaidOut()||!ValueAnimator.areAnimatorsEnabled()){
            for(int i=0;i<3;i++)expansion[i]=i==index?1:0;
            pillLeft=targetLeft(index);pillWidth=unit()*1.65f;updateItems();requestLayout();invalidate();return;
        }
        final float[] from=expansion.clone();final float left=pillLeft,width=pillWidth;
        animator=ValueAnimator.ofFloat(0,1);animator.setDuration(300);animator.setInterpolator(new PathInterpolator(.2f,0,0,1));
        animator.addUpdateListener(a->{float t=(float)a.getAnimatedValue();for(int i=0;i<3;i++)expansion[i]=from[i]+((i==selected?1:0)-from[i])*t;
            pillLeft=left+(targetLeft(selected)-left)*t;pillWidth=width+(unit()*1.65f-width)*t;updateItems();requestLayout();invalidate();});animator.start();
    }
    private void updateItems(){
        int on=ui.onPrimary;
        for(int i=0;i<3;i++){
            int color=MaterialUi.blend(ui.muted,on,expansion[i]);icons[i].setImageDrawable(ui.icon(i,color));labels[i].setTextColor(on);labels[i].setAlpha(expansion[i]);
            labels[i].getLayoutParams().width=Math.round(ui.dp(48)*expansion[i]);labels[i].requestLayout();
        }
    }
    @Override protected void onMeasure(int w,int h){
        int width=MeasureSpec.getSize(w),height=ui.dp(72);setMeasuredDimension(width,resolveSize(height,h));
        for(int i=0;i<3;i++)items[i].measure(MeasureSpec.makeMeasureSpec(Math.round(unit()*(1+.65f*expansion[i])),MeasureSpec.EXACTLY),MeasureSpec.makeMeasureSpec(ui.dp(56),MeasureSpec.EXACTLY));
    }
    @Override protected void onSizeChanged(int w,int h,int oldw,int oldh){if(animator!=null)animator.cancel();for(int i=0;i<3;i++)expansion[i]=i==selected?1:0;pillLeft=targetLeft(selected);pillWidth=unit()*1.65f;updateItems();}
    @Override protected void onLayout(boolean changed,int l,int t,int r,int b){float x=ui.dp(8);for(LinearLayout item:items){int width=item.getMeasuredWidth();item.layout(Math.round(x),ui.dp(8),Math.round(x)+width,ui.dp(64));x+=width;}}
    @Override protected void onDraw(Canvas c){super.onDraw(c);paint.setColor(ui.dark?ui.selected:ui.primary);capsule.set(pillLeft,ui.dp(8),pillLeft+pillWidth,ui.dp(64));c.drawRoundRect(capsule,ui.dp(28),ui.dp(28),paint);}
    @Override protected void onDetachedFromWindow(){if(animator!=null)animator.cancel();super.onDetachedFromWindow();}
}
