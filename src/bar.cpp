#include "bar.hpp"
#include "nwm.hpp"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cstring>

void nwm::bar_init(Base &base) {
    base.bar.height = base.bar_height;
    base.bar.width = WIDTH(base.display, base.screen);
    base.bar.x = 0;
    base.bar.hover_segment = -1;
    base.bar.systray_width = 0;
    base.bar.status_text = "";

    if (base.bar_position == 1) {
        base.bar.y = HEIGHT(base.display, base.screen) - base.bar_height;
    } else {
        base.bar.y = 0;
    }

    XSetWindowAttributes attrs;
    attrs.background_pixel = base.bar_bg_color;
    attrs.override_redirect = True;
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                       PointerMotionMask | Button4MotionMask | Button5MotionMask;

    base.bar.window = XCreateWindow(
        base.display, base.root,
        base.bar.x, base.bar.y,
        base.bar.width, base.bar.height,
        0,
        DefaultDepth(base.display, base.screen),
        CopyFromParent,
        DefaultVisual(base.display, base.screen),
        CWBackPixel | CWOverrideRedirect | CWEventMask,
        &attrs
    );

    XMapWindow(base.display, base.bar.window);
    XRaiseWindow(base.display, base.bar.window);

    base.bar.xft_draw = XftDrawCreate(
        base.display, base.bar.window,
        DefaultVisual(base.display, base.screen),
        DefaultColormap(base.display, base.screen)
    );

    auto create_color = [&](unsigned long hex, XftColor& color) {
        XRenderColor xrender_color = {
            static_cast<unsigned short>(((hex >> 16) & 0xFF) * 257),
            static_cast<unsigned short>(((hex >> 8) & 0xFF) * 257),
            static_cast<unsigned short>((hex & 0xFF) * 257),
            65535
        };
        XftColorAllocValue(base.display, DefaultVisual(base.display, base.screen),
                           DefaultColormap(base.display, base.screen),
                           &xrender_color, &color);
    };

    create_color(base.bar_fg_color, base.bar.xft_fg);
    create_color(base.bar_bg_color, base.bar.xft_bg);
    create_color(base.bar_active_color, base.bar.xft_active);
    create_color(base.bar_inactive_color, base.bar.xft_inactive);
    create_color(base.bar_accent_color, base.bar.xft_accent);

    bar_update_status_text(base);
}

void nwm::bar_cleanup(Base &base) {
    if (base.bar.xft_draw) {
        XftDrawDestroy(base.bar.xft_draw);
        base.bar.xft_draw = nullptr;
    }

    auto free_color = [&](XftColor& color) {
        XftColorFree(base.display, DefaultVisual(base.display, base.screen),
                     DefaultColormap(base.display, base.screen), &color);
    };

    free_color(base.bar.xft_fg);
    free_color(base.bar.xft_bg);
    free_color(base.bar.xft_active);
    free_color(base.bar.xft_inactive);
    free_color(base.bar.xft_accent);

    if (base.bar.window) {
        XDestroyWindow(base.display, base.bar.window);
        base.bar.window = 0;
    }
}

void nwm::bar_update_status_text(Base &base) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = nullptr;

    Atom wm_name = XInternAtom(base.display, "WM_NAME", False);

    if (XGetWindowProperty(base.display, base.root, wm_name, 0, 1024,
                          False, AnyPropertyType, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        base.bar.status_text = std::string((char*)prop);
        XFree(prop);
    } else {
        base.bar.status_text = "";
    }
}

void nwm::bar_draw(Base &base) {
    XClearWindow(base.display, base.bar.window);
    base.bar.segments.clear();

    if (!base.xft_font || !base.bar.xft_draw) return;

    int x_offset = 0;
    int y_offset = base.bar.height / 2 + 6;

    GC gc = XCreateGC(base.display, base.bar.window, 0, nullptr);

    for (size_t i = 0; i < base.workspaces.size(); ++i) {
        std::string ws_label = base.widget[i % base.widget.size()];

        XGlyphInfo extents;
        XftTextExtentsUtf8(base.display, base.xft_font,
                          (XftChar8*)ws_label.c_str(), ws_label.length(),
                          &extents);

        int btn_width = extents.width + 16;
        int btn_height = base.bar.height;
        int btn_x = x_offset;
        int btn_y = 0;

        bool is_active = (i == base.current_workspace);
        bool has_windows = !base.workspaces[i].windows.empty();

        if (is_active) {
            XSetForeground(base.display, gc, base.bar.xft_active.pixel);
            XFillRectangle(base.display, base.bar.window, gc,
                          btn_x, btn_y, btn_width, btn_height);

            int square_size = 5;
            int square_margin = 2;
            XSetForeground(base.display, gc, base.bar_fg_color);
            XFillRectangle(base.display, base.bar.window, gc,
                          btn_x + square_margin,
                          btn_y + square_margin,
                          square_size, square_size);

            XftDrawStringUtf8(base.bar.xft_draw, &base.bar.xft_fg, base.xft_font,
                             btn_x + 8, y_offset,
                             (XftChar8*)ws_label.c_str(), ws_label.length());
        } else if (has_windows) {
            XftDrawStringUtf8(base.bar.xft_draw, &base.bar.xft_fg, base.xft_font,
                             btn_x + 8, y_offset,
                             (XftChar8*)ws_label.c_str(), ws_label.length());
        } else {
            XftDrawStringUtf8(base.bar.xft_draw, &base.bar.xft_inactive, base.xft_font,
                             btn_x + 8, y_offset,
                             (XftChar8*)ws_label.c_str(), ws_label.length());
        }

        BarSegment seg;
        seg.x = btn_x;
        seg.y = btn_y;
        seg.width = btn_width;
        seg.height = btn_height;
        seg.workspace_index = i;
        seg.type = BarSegment::WORKSPACE;
        base.bar.segments.push_back(seg);

        x_offset += btn_width;
    }

    Monitor *mon = get_current_monitor(base);
    std::string layout_mode = (mon && mon->horizontal_mode) ? "[S]" : "[]=";

    XGlyphInfo layout_ext;
    XftTextExtentsUtf8(base.display, base.xft_font,
                      (XftChar8*)layout_mode.c_str(), layout_mode.length(),
                      &layout_ext);

    int layout_width = layout_ext.width + 16;

    XftDrawStringUtf8(base.bar.xft_draw, &base.bar.xft_fg, base.xft_font,
                     x_offset + 8, y_offset,
                     (XftChar8*)layout_mode.c_str(), layout_mode.length());

    x_offset += layout_width;

    int status_width = 0;
    if (!base.bar.status_text.empty()) {
        XGlyphInfo status_extents;
        XftTextExtentsUtf8(base.display, base.xft_font,
                          (XftChar8*)base.bar.status_text.c_str(),
                          base.bar.status_text.length(),
                          &status_extents);
        status_width = status_extents.width + 20;
    }

    int title_bar_width = base.bar.width - x_offset - status_width - base.bar.systray_width;

    XSetForeground(base.display, gc, base.bar.xft_active.pixel);
    XFillRectangle(base.display, base.bar.window, gc,
                  x_offset, 0, title_bar_width, base.bar.height);

    std::string window_title = "";
    if (base.focused_window) {
        window_title = get_window_title(base.display, base.focused_window->window);
        if (window_title.length() > 60) {
            window_title = window_title.substr(0, 57) + "...";
        }
    }

    if (!window_title.empty()) {
        XftDrawStringUtf8(base.bar.xft_draw, &base.bar.xft_fg, base.xft_font,
                         x_offset + 8, y_offset,
                         (XftChar8*)window_title.c_str(), window_title.length());
    }

    if (!base.bar.status_text.empty()) {
        int status_x = base.bar.width - status_width - base.bar.systray_width + 10;

        XftDrawStringUtf8(base.bar.xft_draw, &base.bar.xft_fg, base.xft_font,
                         status_x, y_offset,
                         (XftChar8*)base.bar.status_text.c_str(),
                         base.bar.status_text.length());
    }

    XFreeGC(base.display, gc);
    XFlush(base.display);
}

void nwm::bar_update_workspaces(Base &base) {
    bar_draw(base);
}

void nwm::bar_update_time(Base &base) {
    bar_update_status_text(base);
}

void nwm::bar_handle_click(Base &base, int x, int y, int button) {
    if (button != Button1) return;

    for (const auto& seg : base.bar.segments) {
        if (x >= seg.x && x < seg.x + seg.width &&
            y >= seg.y && y < seg.y + seg.height) {
            if (seg.type == BarSegment::WORKSPACE && seg.workspace_index >= 0) {
                if (seg.workspace_index != (int)base.current_workspace) {
                    switch_workspace((void*)&seg.workspace_index, base);
                }
                break;
            }
        }
    }
}

// TODO: implementation
void nwm::bar_handle_motion(Base &base, int x, int y) {
    (void)base;
    (void)x;
    (void)y;
}

void nwm::bar_handle_scroll(Base &base, int direction) {
    int next_ws = base.current_workspace;

    if (direction > 0) {
        next_ws = (base.current_workspace + 1) % NUM_WORKSPACES;
    } else {
        next_ws = (base.current_workspace - 1 + NUM_WORKSPACES) % NUM_WORKSPACES;
    }

    if (next_ws != (int)base.current_workspace) {
        switch_workspace((void*)&next_ws, base);
    }
}
