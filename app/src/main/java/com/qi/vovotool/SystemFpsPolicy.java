package com.qi.vovotool;

/** Maps the module frame limit onto the HyperOS display frame policy. */
final class SystemFpsPolicy {
    static final int MINIMUM=60;
    static final int FALLBACK_MAX_HZ=120;
    private SystemFpsPolicy() {}
    /**
     * Returns the display frame rate to request from PowerKeeper, or 0 to leave the
     * system policy untouched. "Follow the game" (0) resolves to the panel maximum so
     * HyperOS does not pin the display below what the game actually renders; explicit
     * limits are clamped to the panel, and anything at or below 60 Hz needs no change.
     */
    static int resolve(int userFps,int panelMaxHz){
        int max=panelMaxHz>=MINIMUM?panelMaxHz:FALLBACK_MAX_HZ;
        int target=userFps<=0?max:Math.min(userFps,max);
        return target<=MINIMUM?0:target;
    }
    /** User-facing explanation of what {@link #resolve} does for the current selection. */
    static String describe(int userFps,int panelMaxHz){
        int max=panelMaxHz>=MINIMUM?panelMaxHz:FALLBACK_MAX_HZ;
        int target=resolve(userFps,panelMaxHz);
        if(target==0)return userFps<=0?"设备最高刷新率为 "+panelMaxHz+" FPS，无需修改系统策略":"所选上限不高于 60 FPS，无需修改系统策略";
        if(userFps>max)return "所选 "+userFps+" FPS 超出设备最高 "+max+" FPS，系统策略按 "+max+" FPS 应用";
        if(userFps<=0)return "「跟随游戏」将系统策略设为设备最高 "+max+" FPS；帧率仍由游戏自身决定";
        return "系统策略将按 "+target+" FPS 放行（设备最高 "+max+" FPS）";
    }
}
