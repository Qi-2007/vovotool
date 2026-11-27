package com.qi.vovotool;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Hand-rolled M3-style tabs (no Material dependency).
 * Two fixed tabs with an active underline. Disabled tabs are grey and untouchable.
 */
final class RegionTabs extends LinearLayout {
    interface OnSelect { void select(int index); }

    private final MaterialUi ui;
    private final TextView[] tabs;
    private final boolean[] available;
    private final Paint indicator = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF bar = new RectF();
    private int selected;
    private OnSelect action;

    RegionTabs(Context context, MaterialUi ui, String[] names, boolean[] available, int initial, OnSelect action) {
        super(context);
        this.ui = ui;
        this.available = available;
        this.action = action;
        this.selected = initial;
        setOrientation(LinearLayout.HORIZONTAL);
        setWillNotDraw(false);
        indicator.setColor(ui.primary);
        tabs = new TextView[names.length];
        for (int i = 0; i < names.length; i++) {
            final int index = i;
            TextView tab = new TextView(context);
            tab.setText(names[i]);
            tab.setTextSize(14);
            tab.setGravity(Gravity.CENTER);
            tab.setMinHeight(ui.dp(48));
            tab.setPadding(0, ui.dp(12), 0, ui.dp(12));
            tab.setEnabled(available[i]);
            tab.setOnClickListener(v -> {
                if (!available[index]) return;
                if (index == selected) return;
                selected = index;
                refreshColors();
                invalidate();
                if (action != null) action.select(index);
            });
            tabs[i] = tab;
            addView(tab, new LayoutParams(0, -2, 1f));
        }
        refreshColors();
        post(() -> select(initial));
    }

    void select(int index) {
        selected = index;
        refreshColors();
        invalidate();
    }

    private void refreshColors() {
        int disabled = Color.parseColor(ui.dark ? "#5A5A60" : "#BDBDBD");
        for (int i = 0; i < tabs.length; i++) {
            tabs[i].setTextColor(!available[i] ? disabled : (i == selected ? ui.primary : ui.muted));
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (available[selected]) {
            float w = getWidth() / (float) tabs.length;
            float padding = ui.dp(12);
            float left = selected * w + padding;
            float right = (selected + 1) * w - padding;
            float bottom = getHeight() - ui.dp(2);
            bar.set(left, bottom - ui.dp(3), right, bottom);
            canvas.drawRoundRect(bar, ui.dp(2), ui.dp(2), indicator);
        }
    }
}
