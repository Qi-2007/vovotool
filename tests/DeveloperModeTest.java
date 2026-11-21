package com.qi.vovotool;

public final class DeveloperModeTest {
    private static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
    private static void taps(DeveloperMode mode,int count){for(int i=0;i<count;i++)mode.tap(i*100L);}
    private static void prompt(DeveloperMode mode){taps(mode,8);check(mode.finishTaps(1700)==2,"eight taps then idle opens code");}
    public static void main(String[] args){
        DeveloperMode mode=new DeveloperMode(0);taps(mode,5);
        check(mode.level()==0&&mode.finishTaps(1399)==0,"level zero waits for idle");
        check(mode.finishTaps(1400)==1&&mode.level()==1,"five idle enters level one");
        check(mode.finishTaps(1500)==0,"action consumed once");
        prompt(mode);check(mode.level()==1&&!mode.submit("329"),"leading zero matters");
        check(!mode.submit("0329"),"no submission outside prompt");
        prompt(mode);check(!mode.submit("0329 "),"exact code");
        prompt(mode);mode.cancel();check(!mode.submit("0329"),"canceled prompt");
        prompt(mode);check(mode.submit("0329")&&mode.level()==2,"correct code enables level two");
        check(new DeveloperMode(mode.level()).level()==2,"restored level");
        for(int level=0;level<=2;level++){
            DeveloperMode five=new DeveloperMode(level);taps(five,5);check(five.level()==level,"five taps do not act immediately");
            check(five.finishTaps(1400)==(level==0?1:3),"five idle action");check(five.level()==(level==0?1:0),"five idle final level");
            DeveloperMode six=new DeveloperMode(level);taps(six,6);check(six.finishTaps(2000)==0&&six.level()==level,"sixth tap cancels five");
            DeveloperMode leave=new DeveloperMode(level);taps(leave,5);leave.resetTaps();check(leave.finishTaps(2000)==0,"leaving cancels idle");
        }
        DeveloperMode eight=new DeveloperMode(1);taps(eight,8);check(eight.finishTaps(1699)==0,"eighth tap waits for idle");
        eight.tap(800);check(eight.finishTaps(2000)==0&&!eight.submit("0329"),"ninth tap cancels eight");
        DeveloperMode zeroEight=new DeveloperMode(0);taps(zeroEight,8);check(zeroEight.finishTaps(2000)==0,"overshooting initial five stays zero");
        DeveloperMode gap=new DeveloperMode(0);gap.tap(0);for(int i=0;i<4;i++)gap.tap(2000+i*100);check(gap.finishTaps(3300)==0,"long gap starts new sequence");gap.tap(2400);check(gap.finishTaps(3400)==1,"new fifth tap completes sequence");
        String[] portable={"portable_craft","portable_daily","portable_expedition"};
        String[] other={"hide_uid","qr_login","skip_team_loading","no_avatar_switch_cd"};
        for(int level=0;level<=2;level++){
            for(String key:portable)check(DeveloperMode.allowed(level,key)==(level>0),"portable gate "+key);
            for(String key:other)check(DeveloperMode.allowed(level,key)==(level>0),"other gate "+key);
            for(String key:new String[]{"enabled","capture","graphics_enabled","disable_character_fade"})check(DeveloperMode.allowed(level,key),"ordinary feature "+key);
        }
        check(DeveloperMode.visibleCategoryCount(0)==2&&DeveloperMode.visibleCategoryCount(1)==4&&DeveloperMode.visibleCategoryCount(2)==5,"category visibility by level");
        check(DeveloperMode.normalize(-1)==0&&DeveloperMode.normalize(8)==2,"level normalization");
        System.out.println("PASS: all exact-count idle actions, overshoot, cancellation, code validation, persistence and feature gates");
    }
}
