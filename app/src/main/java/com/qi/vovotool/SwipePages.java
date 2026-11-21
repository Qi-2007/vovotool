package com.qi.vovotool;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.VelocityTracker;
import android.view.ViewConfiguration;
import android.widget.*;

/** Native View pager with horizontal drags, edge resistance, and nested-control exclusions. */
final class SwipePages extends FrameLayout {
    interface OnSelect { void select(int index); }
    private final OnSelect action;
    private final HorizontalDrag drag;
    private final int flingVelocity;
    private VelocityTracker velocity;
    private int selected,count;
    private boolean blocked;
    private float origin,delta;
    private View moving;
    SwipePages(Context context,OnSelect action){
        super(context);this.action=action;flingVelocity=ViewConfiguration.get(context).getScaledMinimumFlingVelocity()*4;
        drag=new HorizontalDrag(this,new HorizontalDrag.Listener(){
            public void start(float x){origin=x;delta=0;moving=getChildAt(selected);moving.animate().cancel();}
            public void move(float x){
                delta=x-origin;boolean edge=(selected==0&&delta>0)||(selected==count-1&&delta<0);
                moving.setTranslationX(Math.max(-getWidth()*.65f,Math.min(getWidth()*.65f,delta*(edge?.2f:.75f))));
                moving.setAlpha(1-Math.min(.3f,Math.abs(delta)/Math.max(1,getWidth())*.4f));
            }
            public void end(boolean canceled){
                int next=selected;float speed=0;
                if(velocity!=null){velocity.computeCurrentVelocity(1000);speed=velocity.getXVelocity();}
                float distance=getResources().getDisplayMetrics().density*48;
                if(!canceled&&(Math.abs(delta)>Math.max(distance,getWidth()*.16f)||(Math.abs(delta)>distance*.4f&&Math.abs(speed)>flingVelocity&&speed*delta>0)))
                    next=Math.max(0,Math.min(count-1,selected+(delta<0?1:-1)));
                if(next!=selected)action.select(next);
                else moving.animate().translationX(0).alpha(1).setDuration(200).start();
                moving=null;
            }
        });
    }
    void setPage(int index,int count){this.selected=index;this.count=count;}
    @Override public boolean dispatchTouchEvent(MotionEvent event){
        if(event.getActionMasked()==MotionEvent.ACTION_DOWN){if(velocity!=null)velocity.recycle();velocity=VelocityTracker.obtain();}
        if(velocity!=null)velocity.addMovement(event);
        boolean result=super.dispatchTouchEvent(event);
        if(event.getActionMasked()==MotionEvent.ACTION_UP||event.getActionMasked()==MotionEvent.ACTION_CANCEL){if(velocity!=null)velocity.recycle();velocity=null;}
        return result;
    }
    @Override public boolean onInterceptTouchEvent(MotionEvent event){
        if(event.getActionMasked()==MotionEvent.ACTION_DOWN)blocked=hitControl(this,event.getX(),event.getY());
        return !blocked&&count>1&&drag.intercept(event);
    }
    @Override public boolean onTouchEvent(MotionEvent event){
        // Own DOWN on non-clickable text/blank space so the enclosing ScrollView
        // cannot become the sole touch target before horizontal movement begins.
        if(!blocked&&count>1)drag.intercept(event);
        return drag.touch(event)||event.getActionMasked()==MotionEvent.ACTION_DOWN||super.onTouchEvent(event);
    }
    private boolean hitControl(View view,float x,float y){
        if(view!=this&&(view instanceof SeekBar||view instanceof EditText||view instanceof Spinner||view instanceof Switch||view instanceof CategoryTabs||view instanceof SwipePages))return true;
        if(view instanceof ViewGroup){
            ViewGroup group=(ViewGroup)view;
            for(int i=group.getChildCount()-1;i>=0;i--){View child=group.getChildAt(i);if(child.getVisibility()!=VISIBLE)continue;
                float cx=x+group.getScrollX()-child.getX(),cy=y+group.getScrollY()-child.getY();
                if(cx>=0&&cy>=0&&cx<child.getWidth()&&cy<child.getHeight()&&hitControl(child,cx,cy))return true;
            }
        }
        return false;
    }
    @Override protected void onDetachedFromWindow(){if(velocity!=null)velocity.recycle();velocity=null;super.onDetachedFromWindow();}
}
