package com.qi.vovotool;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.*;
import android.graphics.drawable.*;
import android.view.*;
import android.widget.*;

/** Small platform-View palette; no additional runtime UI dependencies. */
final class MaterialUi {
    final boolean dark;
    final int background,surface,navigationSurface,onPrimary,ink,muted,primary,selected;
    private final Context context;
    MaterialUi(Context context){
        this.context=context;
        dark=(context.getResources().getConfiguration().uiMode&Configuration.UI_MODE_NIGHT_MASK)==Configuration.UI_MODE_NIGHT_YES;
        background=Color.parseColor(dark?"#141318":"#EDEDF6");
        surface=Color.parseColor(dark?"#211F26":"#FAF8FE");
        navigationSurface=Color.parseColor(dark?"#292A34":"#E1E2ED");
        onPrimary=Color.parseColor(dark?"#192B51":"#F9F8FF");
        ink=Color.parseColor(dark?"#E6E1E9":"#30323B");
        muted=Color.parseColor(dark?"#CAC4D0":"#5D5F68");
        primary=Color.parseColor(dark?"#B5C4FF":"#4D5E8B");
        selected=Color.parseColor(dark?"#2D406C":"#DDE2FA");
    }
    int dp(int n){return Math.round(n*context.getResources().getDisplayMetrics().density);}
    Drawable round(int color,int radius){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;}
    LinearLayout card(LinearLayout parent){
        LinearLayout card=new LinearLayout(context);card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(20),dp(12),dp(20),dp(12));card.setBackground(round(surface,24));
        LinearLayout.LayoutParams params=new LinearLayout.LayoutParams(-1,-2);params.bottomMargin=dp(12);parent.addView(card,params);return card;
    }
    Drawable feedback(int base,int radius){
        // A different overlay color is essential: the previous ripple matched its fill.
        int overlay=dark?0x40FFFFFF:0x334D5E8B;
        StateListDrawable fill=new StateListDrawable();
        fill.addState(new int[]{android.R.attr.state_pressed},round(blend(base,primary,.18f),radius));
        fill.addState(new int[]{},round(base,radius));
        return new RippleDrawable(ColorStateList.valueOf(overlay),fill,round(Color.WHITE,radius));
    }
    static int blend(int a,int b,float f){return (Integer) new android.animation.ArgbEvaluator().evaluate(f,a,b);}
    void pressFeedback(View view){
        android.animation.StateListAnimator states=new android.animation.StateListAnimator();
        android.animation.AnimatorSet pressed=new android.animation.AnimatorSet();
        pressed.playTogether(android.animation.ObjectAnimator.ofFloat(view,"scaleX",.97f),android.animation.ObjectAnimator.ofFloat(view,"scaleY",.97f));pressed.setDuration(90);
        android.animation.AnimatorSet released=new android.animation.AnimatorSet();
        released.playTogether(android.animation.ObjectAnimator.ofFloat(view,"scaleX",1f),android.animation.ObjectAnimator.ofFloat(view,"scaleY",1f));released.setDuration(160);
        states.addState(new int[]{android.R.attr.state_pressed},pressed);states.addState(new int[]{},released);view.setStateListAnimator(states);
    }
    private ColorStateList tint(){return new ColorStateList(new int[][]{new int[]{android.R.attr.state_checked},new int[]{}},new int[]{primary,muted});}
    void styleTree(View view){
        if(view instanceof TextView){TextView t=(TextView)view;t.setTextColor(t.getTextSize()/context.getResources().getDisplayMetrics().scaledDensity<=15?muted:ink);t.setFontFeatureSettings("kern");}
        if(view instanceof Switch){Switch s=(Switch)view;s.setTextColor(ink);s.setMinHeight(dp(64));s.setPadding(0,dp(8),0,dp(8));s.setSwitchPadding(dp(16));s.setThumbTintList(tint());s.setTrackTintList(tint());}
        else if(view instanceof Button){Button b=(Button)view;b.setAllCaps(false);b.setTextColor(primary);b.setTextSize(14);b.setMinHeight(dp(48));b.setStateListAnimator(null);b.setElevation(0);b.setTranslationZ(0);b.setBackground(feedback(selected,24));pressFeedback(b);ViewGroup.LayoutParams old=b.getLayoutParams();if(old instanceof LinearLayout.LayoutParams){LinearLayout.LayoutParams lp=(LinearLayout.LayoutParams)old;lp.height=-2;lp.topMargin=dp(8);lp.bottomMargin=dp(4);b.setLayoutParams(lp);}b.setPadding(dp(16),dp(12),dp(16),dp(12));}
        else if(view instanceof EditText){EditText e=(EditText)view;e.setTextColor(ink);e.setMinHeight(dp(52));e.setBackgroundTintList(ColorStateList.valueOf(primary));}
        if(view instanceof SeekBar)((SeekBar)view).setProgressTintList(ColorStateList.valueOf(primary));
        if(view instanceof ViewGroup){ViewGroup group=(ViewGroup)view;for(int i=0;i<group.getChildCount();i++)styleTree(group.getChildAt(i));}
    }
    Drawable icon(final int kind,final int color){
        Drawable d=new Drawable(){
            final Paint p=new Paint(Paint.ANTI_ALIAS_FLAG);
            @Override public void draw(Canvas canvas){
                canvas.save();canvas.translate(getBounds().left,getBounds().top);canvas.scale(getBounds().width()/24f,getBounds().height()/24f);p.setColor(color);p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(1.8f);p.setStrokeJoin(Paint.Join.ROUND);p.setStrokeCap(Paint.Cap.ROUND);
                if(kind==0){Path h=new Path();h.moveTo(3,11);h.lineTo(12,3);h.lineTo(21,11);h.moveTo(5,10);h.lineTo(5,21);h.lineTo(10,21);h.lineTo(10,15);h.lineTo(14,15);h.lineTo(14,21);h.lineTo(19,21);h.lineTo(19,10);canvas.drawPath(h,p);}
                else if(kind==1){for(int y=6;y<=18;y+=6)canvas.drawLine(3,y,21,y,p);canvas.drawCircle(8,6,2,p);canvas.drawCircle(16,12,2,p);canvas.drawCircle(9,18,2,p);}
                else{canvas.drawCircle(12,12,9,p);canvas.drawLine(12,11,12,17,p);canvas.drawPoint(12,7,p);}
                canvas.restore();
            }
            @Override public void setAlpha(int alpha){p.setAlpha(alpha);}
            @Override public void setColorFilter(ColorFilter filter){p.setColorFilter(filter);}
            @Override public int getOpacity(){return PixelFormat.TRANSLUCENT;}
            @Override public int getIntrinsicWidth(){return dp(24);}
            @Override public int getIntrinsicHeight(){return dp(24);}
        };return d;
    }
}
