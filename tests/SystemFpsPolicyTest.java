package com.qi.vovotool;

public final class SystemFpsPolicyTest {
    private static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
    public static void main(String[] args){
        check(SystemFpsPolicy.resolve(0,144)==144,"follow game uses the panel maximum");
        check(SystemFpsPolicy.resolve(0,120)==120,"follow game follows a 120 Hz panel");
        check(SystemFpsPolicy.resolve(0,165)==165,"follow game follows a 165 Hz panel");
        check(SystemFpsPolicy.resolve(0,0)==SystemFpsPolicy.FALLBACK_MAX_HZ,"unknown panel falls back");
        check(SystemFpsPolicy.resolve(0,60)==0,"a 60 Hz panel has nothing to unlock");
        for(int fps:new int[]{30,45,60})check(SystemFpsPolicy.resolve(fps,144)==0,"no policy change at or below 60");
        check(SystemFpsPolicy.resolve(90,144)==90,"90 Hz passes through");
        check(SystemFpsPolicy.resolve(90,120)==90,"90 Hz fits a 120 Hz panel");
        check(SystemFpsPolicy.resolve(90,0)==90,"90 Hz fits the fallback panel");
        check(SystemFpsPolicy.resolve(120,144)==120,"120 Hz passes through");
        check(SystemFpsPolicy.resolve(144,144)==144,"144 Hz passes through");
        check(SystemFpsPolicy.resolve(144,0)==SystemFpsPolicy.FALLBACK_MAX_HZ,"unsupported 144 Hz clamps to fallback");
        check(SystemFpsPolicy.resolve(165,144)==144,"165 Hz clamps to a 144 Hz panel");
        check(SystemFpsPolicy.resolve(165,165)==165,"165 Hz passes through a 165 Hz panel");
        check(SystemFpsPolicy.resolve(165,120)==120,"165 Hz clamps to a 120 Hz panel");
        check(SystemFpsPolicy.describe(0,144).contains("144"),"description mentions the resolved rate");
        check(SystemFpsPolicy.describe(165,144).contains("144"),"description mentions the clamped rate");
        check(SystemFpsPolicy.describe(60,144).contains("60"),"description explains the no-op");
        System.out.println("PASS: follow-game, explicit limits, panel clamping and unknown-display fallback");
    }
}
