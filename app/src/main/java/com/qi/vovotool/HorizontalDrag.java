package com.qi.vovotool;

import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

/** Locks to horizontal motion after touch slop; leaves taps and vertical scrolling to children. */
final class HorizontalDrag {
    interface Listener { void start(float x); void move(float x); void end(boolean canceled); }
    private final View owner;
    private final Listener listener;
    private final int slop;
    private float downX,downY;
    private boolean dragging,vertical;
    HorizontalDrag(View owner,Listener listener){this.owner=owner;this.listener=listener;slop=ViewConfiguration.get(owner.getContext()).getScaledTouchSlop();}
    boolean intercept(MotionEvent event){
        if(event.getActionMasked()==MotionEvent.ACTION_DOWN){downX=event.getX();downY=event.getY();dragging=false;vertical=false;}
        else if(event.getActionMasked()==MotionEvent.ACTION_MOVE&&!vertical&&!dragging&&event.getPointerCount()==1){
            float dx=event.getX()-downX,dy=event.getY()-downY;
            if(Math.abs(dy)>slop&&Math.abs(dy)>Math.abs(dx))vertical=true;
            else if(Math.abs(dx)>slop&&Math.abs(dx)>Math.abs(dy)*1.4f){
                dragging=true;owner.getParent().requestDisallowInterceptTouchEvent(true);listener.start(downX);
            }
        }
        return dragging;
    }
    boolean touch(MotionEvent event){
        if(!dragging)return false;
        if(event.getPointerCount()>1||event.getActionMasked()==MotionEvent.ACTION_CANCEL){listener.end(true);dragging=false;return true;}
        if(event.getActionMasked()==MotionEvent.ACTION_MOVE)listener.move(event.getX());
        if(event.getActionMasked()==MotionEvent.ACTION_UP){listener.move(event.getX());listener.end(false);dragging=false;}
        return true;
    }
}
