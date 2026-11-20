package com.qi.vovotool;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.Gravity;
import android.view.animation.PathInterpolator;
import android.widget.LinearLayout;
import android.widget.TextView;

/** A single selection pill moves under the four category labels. */
final class CategoryTabs extends LinearLayout {
    interface OnSelect { void select(int index); }
    private final MaterialUi ui;
    private final TextView[] tabs=new TextView[4];
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF pill=new RectF();
    private ValueAnimator animator;
    private float position;
    CategoryTabs(Context context,MaterialUi ui,String[] names,OnSelect action){
        super(context);this.ui=ui;setWillNotDraw(false);setPadding(0,ui.dp(12),0,ui.dp(16));
        for(int i=0;i<tabs.length;i++){
            final int index=i;TextView tab=new TextView(context);tabs[i]=tab;
            tab.setText(names[i]);tab.setTextSize(14);tab.setGravity(Gravity.CENTER);tab.setMinHeight(ui.dp(48));
            tab.setTextColor(i==0?ui.primary:ui.muted);tab.setBackground(ui.feedback(Color.TRANSPARENT,24));ui.pressFeedback(tab);
            tab.setOnClickListener(v->action.select(index));addView(tab,new LayoutParams(0,-2,1));
        }
    }
    void select(int index,boolean animate){
        if(animator!=null)animator.cancel();
        for(int i=0;i<tabs.length;i++)tabs[i].setSelected(i==index);
        if(!animate||!isLaidOut()||!ValueAnimator.areAnimatorsEnabled()){
            position=index;for(int i=0;i<tabs.length;i++)tabs[i].setTextColor(i==index?ui.primary:ui.muted);invalidate();return;
        }
        float start=position;int[] colors=new int[tabs.length];
        for(int i=0;i<tabs.length;i++)colors[i]=tabs[i].getCurrentTextColor();
        animator=ValueAnimator.ofFloat(0,1);animator.setDuration(300);animator.setInterpolator(new PathInterpolator(.2f,0,0,1));
        animator.addUpdateListener(a->{float fraction=(float)a.getAnimatedValue();position=start+(index-start)*fraction;
            for(int i=0;i<tabs.length;i++)tabs[i].setTextColor(MaterialUi.blend(colors[i],i==index?ui.primary:ui.muted,fraction));invalidate();});
        animator.start();
    }
    @Override protected void onDraw(Canvas canvas){
        super.onDraw(canvas);float width=(getWidth()-getPaddingLeft()-getPaddingRight())/(float)tabs.length;
        float left=getPaddingLeft()+position*width;paint.setColor(ui.selected);
        pill.set(left,getPaddingTop(),left+width,getHeight()-getPaddingBottom());canvas.drawRoundRect(pill,ui.dp(24),ui.dp(24),paint);
    }
    @Override protected void onDetachedFromWindow(){if(animator!=null)animator.cancel();super.onDetachedFromWindow();}
}
