package com.qi.vovotool;

/** Persistent level is owned by Settings; this object owns one logo-tap sequence. */
final class DeveloperMode {
    static final String LEVEL_KEY="developer_level";
    static final long RESET_IDLE_MS=1000;
    private static final String CODE="0329";
    private static final long TAP_GAP_MS=1500;
    private int level,taps;
    private long lastTap=-1;
    private boolean awaitingCode;
    DeveloperMode(int level){this.level=normalize(level);}
    static int normalize(int level){return Math.max(0,Math.min(2,level));}
    static int requiredLevel(String key){
        switch(key){
            case "unlock_regions":return 2;
            case "portable_craft":case "portable_daily":case "portable_expedition":return 1;
            case "hide_uid":case "qr_login":case "skip_team_loading":case "no_avatar_switch_cd":return 1;
            default:return 0;
        }
    }
    static boolean allowed(int level,String key){return normalize(level)>=requiredLevel(key);}
    static int visibleCategoryCount(int level){int value=normalize(level);return value==0?2:value==1?4:5;}
    int level(){return level;}
    // Schedule an idle check at a meaningful count; every new tap cancels the old check.
    boolean tap(long now){
        if(awaitingCode)return false;
        if(lastTap<0||now-lastTap>TAP_GAP_MS||now<lastTap)taps=0;
        lastTap=now;++taps;
        return taps==5||(level==1&&taps==8);
    }
    // 1 = advanced features enabled; 2 = request code; 3 = reset to basic mode.
    int finishTaps(long now){
        if(awaitingCode||lastTap<0||now-lastTap<RESET_IDLE_MS)return 0;
        if(taps==5){
            boolean opening=level==0;level=opening?1:0;resetTaps();return opening?1:3;
        }
        if(level==1&&taps==8){resetTaps();awaitingCode=true;return 2;}
        return 0;
    }
    boolean submit(String code){
        boolean correct=awaitingCode&&CODE.equals(code);awaitingCode=false;
        if(correct)level=2;return correct;
    }
    void cancel(){awaitingCode=false;resetTaps();}
    void resetTaps(){taps=0;lastTap=-1;}
}
