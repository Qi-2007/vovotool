package com.qi.vovotool;

import android.view.KeyEvent;

final class Shortcut {
    static final int DEFAULT_KEY=KeyEvent.KEYCODE_F8;
    static final int DEFAULT_DAILY_KEY=KeyEvent.KEYCODE_F9;
    static final int META_MASK=KeyEvent.META_CTRL_ON|KeyEvent.META_ALT_ON|KeyEvent.META_SHIFT_ON|KeyEvent.META_META_ON;
    private Shortcut() {}
    static int modifiers(KeyEvent event){return KeyEvent.normalizeMetaState(event.getMetaState())&META_MASK;}
    static boolean supported(int code){
        return code>KeyEvent.KEYCODE_UNKNOWN && code<=KeyEvent.getMaxKeyCode() && !KeyEvent.isModifierKey(code)
            && code!=KeyEvent.KEYCODE_HOME && code!=KeyEvent.KEYCODE_BACK && code!=KeyEvent.KEYCODE_ESCAPE
            && code!=KeyEvent.KEYCODE_POWER && code!=KeyEvent.KEYCODE_VOLUME_UP && code!=KeyEvent.KEYCODE_VOLUME_DOWN
            && code!=KeyEvent.KEYCODE_VOLUME_MUTE && code!=KeyEvent.KEYCODE_APP_SWITCH;
    }
    static String label(int code,int meta){
        StringBuilder text=new StringBuilder();
        if((meta&KeyEvent.META_CTRL_ON)!=0)text.append("Ctrl + ");
        if((meta&KeyEvent.META_ALT_ON)!=0)text.append("Alt + ");
        if((meta&KeyEvent.META_SHIFT_ON)!=0)text.append("Shift + ");
        if((meta&KeyEvent.META_META_ON)!=0)text.append("Meta + ");
        String key=KeyEvent.keyCodeToString(code);
        return text.append(key.startsWith("KEYCODE_")?key.substring(8).replace('_',' '):key).toString();
    }
}
