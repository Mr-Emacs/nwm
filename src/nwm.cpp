#include "nwm.hpp"
#include "config.hpp"
#include "bar.hpp"
#include "tiling.hpp"
#include "systray.hpp"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrandr.h>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include "animations.hpp"

int x_error_handler(Display *dpy, XErrorEvent *error) {
    char error_text[1024];
    XGetErrorText(dpy, error->error_code, error_text, sizeof(error_text));
    std::cerr << "X Error: " << error_text
              << " Request code: " << (int)error->request_code
              << " Resource ID: " << error->resourceid << std::endl;
    return 0;
}

void nwm::monitors_init(Base &base) {
    base.monitors.clear();
    base.current_monitor = 0;

    if (base.use_xinerama) {
        int num_screens = 0;
        XineramaScreenInfo *screen_info = XineramaQueryScreens(base.display, &num_screens);

        if (screen_info && num_screens > 0) {
            std::cout << "Using Xinerama: detected " << num_screens << " monitors\n";

            for (int i = 0; i < num_screens; i++) {
                Monitor mon;
                mon.id = i;
                mon.x = screen_info[i].x_org;
                mon.y = screen_info[i].y_org;
                mon.width = screen_info[i].width;
                mon.height = screen_info[i].height;
                mon.current_workspace = i % NUM_WORKSPACES;
                mon.master_factor = 0.5f;
                mon.horizontal_mode = false;
                mon.scroll_windows_visible = SCROLL_WINDOWS_VISIBLE;
                mon.crtc = 0;
                base.monitors.push_back(mon);

                std::cout << "Monitor " << i << ": " << mon.width << "x" << mon.height
                         << " at (" << mon.x << "," << mon.y << ")\n";
            }

            XFree(screen_info);
            return;
        }
    }

    int event_base, error_base;
    if (!XRRQueryExtension(base.display, &event_base, &error_base)) {
        Monitor mon;
        mon.id = 0;
        mon.x = 0;
        mon.y = 0;
        mon.width = WIDTH(base.display, base.screen);
        mon.height = HEIGHT(base.display, base.screen);
        mon.current_workspace = 0;
        mon.master_factor = 0.5f;
        mon.horizontal_mode = false;
        mon.scroll_windows_visible = SCROLL_WINDOWS_VISIBLE;
        mon.crtc = 0;
        base.monitors.push_back(mon);
        return;
    }

    base.xrandr_event_base = event_base;
    XRRSelectInput(base.display, base.root, RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask);

    XRRScreenResources *sr = XRRGetScreenResources(base.display, base.root);
    if (!sr) {
        Monitor mon;
        mon.id = 0;
        mon.x = 0;
        mon.y = 0;
        mon.width = WIDTH(base.display, base.screen);
        mon.height = HEIGHT(base.display, base.screen);
        mon.current_workspace = 0;
        mon.master_factor = 0.5f;
        mon.horizontal_mode = false;
        mon.scroll_windows_visible = SCROLL_WINDOWS_VISIBLE;
        mon.crtc = 0;
        base.monitors.push_back(mon);
        return;
    }

    std::cout << "Using XRandR: detected " << sr->ncrtc << " CRTCs\n";

    for (int i = 0; i < sr->ncrtc; i++) {
        XRRCrtcInfo *ci = XRRGetCrtcInfo(base.display, sr, sr->crtcs[i]);
        if (!ci) continue;

        if (ci->width > 0 && ci->height > 0 && ci->noutput > 0) {
            Monitor mon;
            mon.id = base.monitors.size();
            mon.x = ci->x;
            mon.y = ci->y;
            mon.width = ci->width;
            mon.height = ci->height;
            mon.current_workspace = mon.id % NUM_WORKSPACES;
            mon.master_factor = 0.5f;
            mon.horizontal_mode = false;
            mon.scroll_windows_visible = SCROLL_WINDOWS_VISIBLE;
            mon.crtc = sr->crtcs[i];
            base.monitors.push_back(mon);

            std::cout << "Monitor " << mon.id << ": " << mon.width << "x" << mon.height
                     << " at (" << mon.x << "," << mon.y << ")\n";
        }

        XRRFreeCrtcInfo(ci);
    }

    XRRFreeScreenResources(sr);

    if (base.monitors.empty()) {
        Monitor mon;
        mon.id = 0;
        mon.x = 0;
        mon.y = 0;
        mon.width = WIDTH(base.display, base.screen);
        mon.height = HEIGHT(base.display, base.screen);
        mon.current_workspace = 0;
        mon.master_factor = 0.5f;
        mon.horizontal_mode = false;
        mon.scroll_windows_visible = SCROLL_WINDOWS_VISIBLE;
        mon.crtc = 0;
        base.monitors.push_back(mon);
    }
}

void nwm::monitors_update(Base &base) {
    std::vector<Monitor> old_monitors = base.monitors;
    base.monitors.clear();

    XRRScreenResources *sr = XRRGetScreenResources(base.display, base.root);
    if (!sr) return;

    for (int i = 0; i < sr->ncrtc; i++) {
        XRRCrtcInfo *ci = XRRGetCrtcInfo(base.display, sr, sr->crtcs[i]);
        if (!ci) continue;

        if (ci->width > 0 && ci->height > 0 && ci->noutput > 0) {
            Monitor mon;

            bool found = false;
            for (const auto &old_mon : old_monitors) {
                if (old_mon.crtc == sr->crtcs[i]) {
                    mon.current_workspace = old_mon.current_workspace;
                    mon.master_factor = old_mon.master_factor;
                    mon.horizontal_mode = old_mon.horizontal_mode;
                    mon.scroll_windows_visible = old_mon.scroll_windows_visible;
                    found = true;
                    break;
                }
            }

            if (!found) {
                mon.current_workspace = base.monitors.size() % NUM_WORKSPACES;
                mon.master_factor = 0.5f;
                mon.horizontal_mode = false;
            }

            mon.id = base.monitors.size();
            mon.x = ci->x;
            mon.y = ci->y;
            mon.width = ci->width;
            mon.height = ci->height;
            mon.crtc = sr->crtcs[i];
            base.monitors.push_back(mon);
        }

        XRRFreeCrtcInfo(ci);
    }

    XRRFreeScreenResources(sr);

    if (base.monitors.empty()) {
        Monitor mon;
        mon.id = 0;
        mon.x = 0;
        mon.y = 0;
        mon.width = WIDTH(base.display, base.screen);
        mon.height = HEIGHT(base.display, base.screen);
        mon.current_workspace = 0;
        mon.master_factor = 0.5f;
        mon.horizontal_mode = false;
        mon.crtc = 0;
        base.monitors.push_back(mon);
    }

    if (base.current_monitor >= (int)base.monitors.size()) {
        base.current_monitor = base.monitors.size() - 1;
    }

    for (auto &ws : base.workspaces) {
        for (auto &w : ws.windows) {
            Monitor *mon = get_monitor_at_point(base, w.x + w.width / 2, w.y + w.height / 2);
            if (mon) {
                w.monitor = mon->id;
            }
        }
    }

    if (base.horizontal_mode) {
        tile_horizontal(base);
    } else {
        tile_windows(base);
    }
}

nwm::Monitor* nwm::get_monitor_at_point(Base &base, int x, int y) {
    for (auto &mon : base.monitors) {
        if (x >= mon.x && x < mon.x + mon.width &&
            y >= mon.y && y < mon.y + mon.height) {
            return &mon;
        }
    }
    return base.monitors.empty() ? nullptr : &base.monitors[0];
}

nwm::Monitor* nwm::get_current_monitor(Base &base) {
    if (base.current_monitor >= 0 && base.current_monitor < (int)base.monitors.size()) {
        return &base.monitors[base.current_monitor];
    }
    return base.monitors.empty() ? nullptr : &base.monitors[0];
}

void nwm::focus_monitor(void *arg, Base &base) {
    if (!arg) return;
    int target_mon = *(int*)arg;

    if (target_mon >= 0 && target_mon < (int)base.monitors.size()) {
        base.current_monitor = target_mon;

        Window root_return, child_return;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;
        XQueryPointer(base.display, base.root, &root_return, &child_return,
                     &root_x, &root_y, &win_x, &win_y, &mask);

        Monitor *mon = &base.monitors[target_mon];
        XWarpPointer(base.display, None, base.root, 0, 0, 0, 0,
                    mon->x + mon->width / 2, mon->y + mon->height / 2);

        base.current_workspace = mon->current_workspace;
        bar_update_workspaces(base);
    }
}

void nwm::set_scroll_visible(void *arg, Base &base) {
    if (!arg) return;
    int visible = *(int*)arg;

    Monitor *mon = get_current_monitor(base);
    if (!mon) return;

    if (visible < 1) visible = 1;
    if (visible > 10) visible = 10;

    mon->scroll_windows_visible = visible;

    if (mon->horizontal_mode) {
        tile_horizontal(base);
    }

    bar_draw(base);
}

bool should_float(Display *display, Window window) {
    XWindowAttributes attr;
    if (!XGetWindowAttributes(display, window, &attr)) {
        return false;
    }

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = nullptr;

    Atom window_type_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    if (XGetWindowProperty(display, window, window_type_atom, 0, 1,
                          False, XA_ATOM, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        Atom type = *(Atom*)prop;
        XFree(prop);

        Atom dialog = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
        Atom splash = XInternAtom(display, "_NET_WM_WINDOW_TYPE_SPLASH", False);
        Atom utility = XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", False);

        if (type == dialog || type == splash || type == utility) {
            return true;
        }
    }

    Atom state_atom = XInternAtom(display, "_NET_WM_STATE", False);
    if (XGetWindowProperty(display, window, state_atom, 0, 32,
                          False, XA_ATOM, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        Atom *states = (Atom*)prop;
        Atom modal = XInternAtom(display, "_NET_WM_STATE_MODAL", False);
        Atom above = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);

        for (unsigned long i = 0; i < nitems; i++) {
            if (states[i] == modal || states[i] == above) {
                XFree(prop);
                return true;
            }
        }
        XFree(prop);
    }

    Window transient_for;
    if (XGetTransientForHint(display, window, &transient_for)) {
        if (transient_for != None && transient_for != window) {
            return true;
        }
    }

    XSizeHints hints;
    long supplied;
    if (XGetWMNormalHints(display, window, &hints, &supplied)) {
        if ((hints.flags & PMaxSize) && (hints.flags & PMinSize)) {
            if (hints.max_width == hints.min_width &&
                hints.max_height == hints.min_height &&
                hints.max_width < 800 && hints.max_height < 600) {
                return true;
            }
        }
    }

    return false;
}

bool should_ignore_window(Display *display, Window window) {
    XWindowAttributes attr;
    if (!XGetWindowAttributes(display, window, &attr)) {
        return true;
    }

    if (attr.override_redirect) {
        return true;
    }

    if (attr.c_class == InputOnly) {
        return true;
    }

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = nullptr;

    Atom window_type_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    if (XGetWindowProperty(display, window, window_type_atom, 0, (~0L),
                          False, XA_ATOM, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        Atom *types = (Atom*)prop;

        Atom dock = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
        Atom desktop = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
        Atom notification = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
        Atom tooltip = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
        Atom combo = XInternAtom(display, "_NET_WM_WINDOW_TYPE_COMBO", False);
        Atom dnd = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DND", False);
        Atom dropdown = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
        Atom popup = XInternAtom(display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);

        for (unsigned long i = 0; i < nitems; i++) {
            if (types[i] == dock || types[i] == desktop || types[i] == notification ||
                types[i] == tooltip || types[i] == combo || types[i] == dnd ||
                types[i] == dropdown || types[i] == popup) {
                XFree(prop);
                return true;
            }
        }
        XFree(prop);
    }

    Atom state_atom = XInternAtom(display, "_NET_WM_STATE", False);
    if (XGetWindowProperty(display, window, state_atom, 0, (~0L),
                          False, XA_ATOM, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        Atom *states = (Atom*)prop;
        Atom skip_taskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
        Atom skip_pager = XInternAtom(display, "_NET_WM_STATE_SKIP_PAGER", False);

        bool has_skip_taskbar = false;
        bool has_skip_pager = false;

        for (unsigned long i = 0; i < nitems; i++) {
            if (states[i] == skip_taskbar) has_skip_taskbar = true;
            if (states[i] == skip_pager) has_skip_pager = true;
        }

        XFree(prop);

        if (has_skip_taskbar && has_skip_pager) {
            return true;
        }
    }

    XClassHint class_hint;
    if (XGetClassHint(display, window, &class_hint)) {
        bool ignore = false;
        if (class_hint.res_class) {
            std::string class_name(class_hint.res_class);
            if (class_name == "Dunst" ||
                class_name == "Xfce4-notifyd" ||
                class_name == "Notify-osd" ||
                class_name == "notification" ||
                class_name == "Notification") {
                ignore = true;
            }
            XFree(class_hint.res_class);
        }
        if (class_hint.res_name) {
            XFree(class_hint.res_name);
        }
        if (ignore) return true;
    }

    return false;
}

void nwm::raise_override_redirect_windows(Display *display) {
    Window root = DefaultRootWindow(display);
    Window root_return, parent_return;
    Window *children;
    unsigned int nchildren;

    if (!XQueryTree(display, root, &root_return, &parent_return, &children, &nchildren)) {
        return;
    }

    for (unsigned int i = 0; i < nchildren; ++i) {
        XWindowAttributes attr;
        if (XGetWindowAttributes(display, children[i], &attr)) {
            if (attr.override_redirect && attr.map_state == IsViewable) {
                XRaiseWindow(display, children[i]);
            }
        }
    }

    if (children) {
        XFree(children);
    }
}

void nwm::handle_property_notify(XPropertyEvent *e, Base &base) {
    if (e->window == base.root) {
        Atom wm_name = XInternAtom(base.display, "WM_NAME", False);
        if (e->atom == wm_name) {
            bar_update_status_text(base);
            bar_draw(base);
            return;
        }
    }

    if (!base.show_window_titles) return;

    Atom net_wm_name = XInternAtom(base.display, "_NET_WM_NAME", False);
    Atom wm_name = XInternAtom(base.display, "WM_NAME", False);

    if (e->atom == net_wm_name || e->atom == wm_name) {
        auto &current_ws = get_current_workspace(base);
        for (auto &w : current_ws.windows) {
            if (w.window == e->window) {
                titlebar_update_title(&w, base);
                break;
            }
        }
    }
}

void nwm::handle_special_window_map(Display *display, Window window) {
    XWindowAttributes attr;
    if (!XGetWindowAttributes(display, window, &attr)) {
        return;
    }

    if (attr.override_redirect) {
        XRaiseWindow(display, window);
        return;
    }

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = nullptr;

    Atom window_type_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    if (XGetWindowProperty(display, window, window_type_atom, 0, (~0L),
                          False, XA_ATOM, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        Atom *types = (Atom*)prop;

        Atom notification = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
        Atom tooltip = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
        Atom dropdown = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
        Atom popup = XInternAtom(display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
        Atom combo = XInternAtom(display, "_NET_WM_WINDOW_TYPE_COMBO", False);

        for (unsigned long i = 0; i < nitems; i++) {
            if (types[i] == notification || types[i] == tooltip ||
                types[i] == dropdown || types[i] == popup || types[i] == combo) {
                XFree(prop);
                XRaiseWindow(display, window);
                return;
            }
        }
        XFree(prop);
    }
}

void nwm::raise_special_windows(Display *display) {
    Window root = DefaultRootWindow(display);
    Window root_return, parent_return;
    Window *children;
    unsigned int nchildren;

    if (!XQueryTree(display, root, &root_return, &parent_return, &children, &nchildren)) {
        return;
    }

    std::vector<Window> special_windows;

    for (unsigned int i = 0; i < nchildren; ++i) {
        XWindowAttributes attr;
        if (!XGetWindowAttributes(display, children[i], &attr)) {
            continue;
        }

        if (attr.map_state != IsViewable) {
            continue;
        }

        if (attr.override_redirect) {
            special_windows.push_back(children[i]);
            continue;
        }

        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = nullptr;

        Atom window_type_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
        if (XGetWindowProperty(display, children[i], window_type_atom, 0, (~0L),
                              False, XA_ATOM, &actual_type, &actual_format,
                              &nitems, &bytes_after, &prop) == Success && prop) {
            Atom *types = (Atom*)prop;

            Atom notification = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
            Atom dropdown = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
            Atom popup = XInternAtom(display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
            Atom combo = XInternAtom(display, "_NET_WM_WINDOW_TYPE_COMBO", False);
            Atom tooltip = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);

            for (unsigned long j = 0; j < nitems; j++) {
                if (types[j] == notification || types[j] == dropdown ||
                    types[j] == popup || types[j] == combo || types[j] == tooltip) {
                    special_windows.push_back(children[i]);
                    break;
                }
            }
            XFree(prop);
        }
    }

    for (Window w : special_windows) {
        XRaiseWindow(display, w);
    }

    if (children) {
        XFree(children);
    }
}

void nwm::workspace_init(Base &base) {
    base.workspaces.resize(NUM_WORKSPACES);
    for (auto &ws : base.workspaces) {
        ws.focused_window = nullptr;
        ws.scroll_offset = 0;
        ws.scroll_maximized = false;
    }
    base.current_workspace = 0;
    base.overview_mode = false;
    base.dragging = false;
    base.drag_window = None;
    base.drag_start_x = 0;
    base.drag_start_y = 0;
}

nwm::Workspace& nwm::get_current_workspace(Base &base) {
    return base.workspaces[base.current_workspace];
}

void nwm::toggle_scroll_maximize(void *arg, Base &base) {
    (void)arg;
    if (!base.horizontal_mode) return;
    auto &current_ws = get_current_workspace(base);
    current_ws.scroll_maximized = !current_ws.scroll_maximized;
    current_ws.scroll_offset = 0;
    tile_horizontal(base);
}

void nwm::toggle_fullscreen(void *arg, Base &base) {
    (void)arg;
    if (!base.focused_window) return;

    auto &current_ws = get_current_workspace(base);
    for (auto &w : current_ws.windows) {
        if (w.window == base.focused_window->window) {
            w.is_fullscreen = !w.is_fullscreen;

            if (w.is_fullscreen) {
                w.pre_fs_x = w.x;
                w.pre_fs_y = w.y;
                w.pre_fs_width = w.width;
                w.pre_fs_height = w.height;
                w.pre_fs_floating = w.is_floating;

                Monitor *mon = get_monitor_at_point(base, w.x + w.width / 2, w.y + w.height / 2);
                if (!mon) mon = get_current_monitor(base);
                if (!mon) return;

                XSetWindowBorderWidth(base.display, w.window, 0);
                XMoveResizeWindow(base.display, w.window, mon->x, mon->y, mon->width, mon->height);
                XRaiseWindow(base.display, w.window);

                Atom wm_state = XInternAtom(base.display, "_NET_WM_STATE", False);
                Atom fullscreen = XInternAtom(base.display, "_NET_WM_STATE_FULLSCREEN", False);
                XChangeProperty(base.display, w.window, wm_state, XA_ATOM, 32,
                              PropModeReplace, (unsigned char*)&fullscreen, 1);
            } else {
                w.is_floating = w.pre_fs_floating;
                XSetWindowBorderWidth(base.display, w.window, base.border_width);

                Atom wm_state = XInternAtom(base.display, "_NET_WM_STATE", False);
                XDeleteProperty(base.display, w.window, wm_state);

                if (base.horizontal_mode) {
                    tile_horizontal(base);
                } else {
                    tile_windows(base);
                }
            }
            break;
        }
    }

    XFlush(base.display);
}

void nwm::switch_workspace(void *arg, Base &base) {
    if (!arg) return;

    int target_ws = *(int*)arg;
    if (target_ws < 0 || target_ws >= NUM_WORKSPACES) return;
    if (target_ws == (int)base.current_workspace) return;

    if (base.anim_manager) {
        cancel_all_animations(base);
    }

    auto &current_ws = base.workspaces[base.current_workspace];

    for (auto &w : current_ws.windows) {
        if (!is_window_animating(base, w.window)) {
            XUnmapWindow(base.display, w.window);
            if (w.has_titlebar) {
                XUnmapWindow(base.display, w.titlebar.window);
            }
        }
    }

    int old_workspace = base.current_workspace;
    base.current_workspace = target_ws;

    Monitor *mon = get_current_monitor(base);
    if (mon) {
        mon->current_workspace = target_ws;
    }

    if (base.anim_manager && base.anim_manager->animations_enabled &&
        base.anim_manager->workspace_switch_enabled) {
        animate_workspace_switch(base, old_workspace, target_ws);
    }

    auto &new_ws = base.workspaces[target_ws];

    for (auto &w : new_ws.windows) {
        XMapWindow(base.display, w.window);
        if (w.has_titlebar && !w.is_floating && !w.is_fullscreen) {
            XMapWindow(base.display, w.titlebar.window);
            XRaiseWindow(base.display, w.titlebar.window);
        }
        if (w.is_floating || w.is_fullscreen) {
            XRaiseWindow(base.display, w.window);
        }
    }

    if (base.horizontal_mode) {
        tile_horizontal(base);
    } else {
        tile_windows(base);
    }

    base.focused_window = new_ws.focused_window;

    if (base.focused_window) {
        bool found = false;
        for (auto &w : new_ws.windows) {
            if (w.window == base.focused_window->window) {
                found = true;
                break;
            }
        }

        if (found) {
            focus_window(base.focused_window, base);
        } else {
            base.focused_window = nullptr;
            new_ws.focused_window = nullptr;
            if (!new_ws.windows.empty()) {
                focus_window(&new_ws.windows[0], base);
            }
        }
    } else if (!new_ws.windows.empty()) {
        focus_window(&new_ws.windows[0], base);
    } else {
        base.focused_window = nullptr;
        XSetInputFocus(base.display, base.root, RevertToPointerRoot, CurrentTime);
    }

    bar_update_workspaces(base);

    XSync(base.display, False);
}

void nwm::move_to_workspace(void *arg, Base &base) {
    if (!arg || !base.focused_window) return;

    if (base.focused_window->is_fullscreen) {
        toggle_fullscreen(nullptr, base);
    }

    int target_ws = *(int*)arg;
    if (target_ws < 0 || target_ws >= NUM_WORKSPACES) return;

    if (target_ws == (int)base.current_workspace) return;
    auto &current_ws = get_current_workspace(base);

    for (auto it = current_ws.windows.begin(); it != current_ws.windows.end(); ++it) {
        if (it->window == base.focused_window->window) {
            ManagedWindow w = *it;
            w.workspace = target_ws;

            if (base.anim_manager && base.anim_manager->animations_enabled &&
                base.anim_manager->window_close_enabled) {
                animate_window_close(base, w.window, [&base, w, target_ws, it]() mutable {
                    auto &curr_ws = base.workspaces[base.current_workspace];

                    for (auto iter = curr_ws.windows.begin(); iter != curr_ws.windows.end(); ++iter) {
                        if (iter->window == w.window) {
                            curr_ws.windows.erase(iter);
                            break;
                        }
                    }

                    base.workspaces[target_ws].windows.push_back(w);

                    Atom workspace_atom = XInternAtom(base.display, "_NWM_WORKSPACE", False);
                    long workspace_id = target_ws;
                    XChangeProperty(base.display, w.window, workspace_atom,
                                  XA_CARDINAL, 32, PropModeReplace,
                                  (unsigned char*)&workspace_id, 1);

                    XUnmapWindow(base.display, w.window);

                    if (curr_ws.focused_window && curr_ws.focused_window->window == w.window) {
                        curr_ws.focused_window = nullptr;
                    }
                    base.focused_window = nullptr;

                    if (!curr_ws.windows.empty()) {
                        focus_window(&curr_ws.windows[0], base);
                    }

                    if (base.horizontal_mode) {
                        tile_horizontal(base);
                    } else {
                        tile_windows(base);
                    }
                });
                return;
            }

            current_ws.windows.erase(it);
            base.workspaces[target_ws].windows.push_back(w);

            Atom workspace_atom = XInternAtom(base.display, "_NWM_WORKSPACE", False);
            long workspace_id = target_ws;
            XChangeProperty(base.display, w.window, workspace_atom,
                          XA_CARDINAL, 32, PropModeReplace,
                          (unsigned char*)&workspace_id, 1);

            XUnmapWindow(base.display, w.window);

            if (current_ws.focused_window && current_ws.focused_window->window == w.window) {
                current_ws.focused_window = nullptr;
            }
            base.focused_window = nullptr;

            if (!current_ws.windows.empty()) {
                focus_window(&current_ws.windows[0], base);
            }

            break;
        }
    }

    if (base.horizontal_mode) {
        tile_horizontal(base);
    } else {
        tile_windows(base);
    }
}

void nwm::setup_keys(nwm::Base &base) {
    XUngrabKey(base.display, AnyKey, AnyModifier, base.root);

    for (auto &k : keys) {
        KeyCode code = XKeysymToKeycode(base.display, k.keysym);
        if (!code) continue;

        unsigned int modifiers[] = {0, LockMask, Mod2Mask, Mod2Mask | LockMask};
        for (unsigned int mod : modifiers) {
            XGrabKey(
                base.display,
                code,
                k.mod | mod,
                base.root,
                False,
                GrabModeAsync,
                GrabModeAsync
            );
        }
    }

    XGrabButton(base.display, Button1, MODKEY, base.root, False,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);

    XGrabButton(base.display, Button3, MODKEY, base.root, False,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);

    unsigned int modifiers[] = {0, LockMask, Mod2Mask, Mod2Mask | LockMask};
    for (unsigned int mod : modifiers) {
        XGrabButton(base.display, Button4, MODKEY | mod, base.root, False,
                    ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

        XGrabButton(base.display, Button5, MODKEY | mod, base.root, False,
                    ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    }

    XSync(base.display, False);
}

void nwm::spawn(void *arg, nwm::Base &base) {
    const char **cmd = (const char **)arg;
    if (fork() == 0) {
        setsid();
        execvp(cmd[0], (char **)cmd);
        perror("execvp failed");
        exit(1);
    }

    XSetInputFocus(base.display, base.root, RevertToPointerRoot, CurrentTime);
}

void nwm::toggle_gap(void *arg, nwm::Base &base) {
    (void)arg;
    base.gaps_enabled = !base.gaps_enabled;
    base.gaps = base.gaps_enabled ? GAP_SIZE : 0;

    if (base.horizontal_mode) {
        tile_horizontal(base);
    } else {
        tile_windows(base);
    }
}

void nwm::toggle_bar(void *arg, Base &base) {
    (void)arg;
    base.bar_visible = !base.bar_visible;

    if (base.bar_visible) {
        XMapWindow(base.display, base.bar.window);
    } else {
        XUnmapWindow(base.display, base.bar.window);
    }

    if (base.horizontal_mode) {
        tile_horizontal(base);
    } else {
        tile_windows(base);
    }

    XFlush(base.display);
}

void nwm::toggle_float(void *arg, Base &base) {
    (void)arg;
    if (!base.focused_window) return;

    auto &current_ws = get_current_workspace(base);
    for (auto &w : current_ws.windows) {
        if (w.window == base.focused_window->window) {
            int start_x = w.x;
            int start_y = w.y;
            int start_width = w.width;
            int start_height = w.height;

            w.is_floating = !w.is_floating;

            if (w.is_floating) {
                Monitor *mon = get_monitor_at_point(base, w.x + w.width / 2, w.y + w.height / 2);
                if (!mon) mon = get_current_monitor(base);
                if (!mon) return;

                int target_width = mon->width / 2;
                int target_height = mon->height / 2;
                int target_x = mon->x + (mon->width - target_width) / 2;
                int target_y = mon->y + (mon->height - target_height) / 2;

                if (base.anim_manager && base.anim_manager->animations_enabled &&
                    (base.anim_manager->window_move_enabled || base.anim_manager->window_resize_enabled)) {
                    animate_floating_transition(base, w.window, true,
                                               start_x, start_y, start_width, start_height,
                                               target_x, target_y, target_width, target_height);
                    w.x = target_x;
                    w.y = target_y;
                    w.width = target_width;
                    w.height = target_height;
                } else {
                    w.width = target_width;
                    w.height = target_height;
                    w.x = target_x;
                    w.y = target_y;
                    XMoveResizeWindow(base.display, w.window, w.x, w.y, w.width, w.height);
                }

                XSetWindowBorderWidth(base.display, w.window, 2);
                XSetWindowBorder(base.display, w.window, base.focus_color);
                XRaiseWindow(base.display, w.window);
                XSetInputFocus(base.display, w.window, RevertToPointerRoot, CurrentTime);

                if (w.has_titlebar) {
                    XUnmapWindow(base.display, w.titlebar.window);
                    titlebar_cleanup(&w, base);
                }
            } else {
                if (base.horizontal_mode) {
                    tile_horizontal(base);
                } else {
                    tile_windows(base);
                }

                int target_x = w.x;
                int target_y = w.y;
                int target_width = w.width;
                int target_height = w.height;

                if (base.anim_manager && base.anim_manager->animations_enabled &&
                    (base.anim_manager->window_move_enabled || base.anim_manager->window_resize_enabled)) {
                    animate_floating_transition(base, w.window, false,
                                               start_x, start_y, start_width, start_height,
                                               target_x, target_y, target_width, target_height);
                }

                XSetWindowBorderWidth(base.display, w.window, base.border_width);
                XSetWindowBorder(base.display, w.window, base.focus_color);

                titlebar_init(&w, base);
            }
            break;
        }
    }

    if (!base.focused_window->is_floating) {
        if (base.horizontal_mode) {
            tile_horizontal(base);
        } else {
            tile_windows(base);
        }
    }
}


void nwm::unmanage_window(Window window, Base &base) {
    auto &current_ws = get_current_workspace(base);

    for (auto it = current_ws.windows.begin(); it != current_ws.windows.end(); ++it) {
        if (it->window == window) {
            titlebar_cleanup(&*it, base);
        }
    }

    int closed_idx = -1;
    for (size_t i = 0; i < current_ws.windows.size(); ++i) {
        if (current_ws.windows[i].window == window) {
            closed_idx = i;
            break;
        }
    }

    if (closed_idx == -1) return;

    bool was_focused = (current_ws.focused_window &&
                       current_ws.focused_window->window == window);

    current_ws.windows.erase(current_ws.windows.begin() + closed_idx);

    if (was_focused) {
        current_ws.focused_window = nullptr;
        base.focused_window = nullptr;

        if (!current_ws.windows.empty()) {
            int new_focus_idx = closed_idx > 0 ? closed_idx - 1 : 0;
            if (new_focus_idx >= (int)current_ws.windows.size()) {
                new_focus_idx = current_ws.windows.size() - 1;
            }
            focus_window(&current_ws.windows[new_focus_idx], base);
        }
    }

    if (base.horizontal_mode) {
        Monitor *mon = get_current_monitor(base);
        if (mon) {
            int window_width = mon->width / SCROLL_WINDOWS_VISIBLE;
            int total_width = current_ws.windows.size() * window_width;
            int max_scroll = std::max(0, total_width - mon->width);
            current_ws.scroll_offset = std::min(current_ws.scroll_offset, max_scroll);
        }
    }
}

void nwm::focus_window(ManagedWindow *window, Base &base) {
    auto &current_ws = get_current_workspace(base);

    for (auto &w : current_ws.windows) {
        if (!w.is_floating && !w.is_fullscreen) {
            if (base.anim_manager && base.anim_manager->animations_enabled &&
                base.anim_manager->border_color_enabled) {
                animate_border_color(base, w.window, base.border_color);
            } else {
                XSetWindowBorder(base.display, w.window, base.border_color);
            }
        }
        w.is_focused = false;

        if (w.has_titlebar) {
            titlebar_draw(&w, base);
        }
    }

    current_ws.focused_window = nullptr;
    base.focused_window = nullptr;

    if (window) {
        current_ws.focused_window = window;
        base.focused_window = window;
        window->is_focused = true;

        if (!window->is_floating && !window->is_fullscreen) {
            if (base.anim_manager && base.anim_manager->animations_enabled &&
                base.anim_manager->border_color_enabled) {
                animate_border_color(base, window->window, base.focus_color);
            } else {
                XSetWindowBorder(base.display, window->window, base.focus_color);
            }
        }

        if (window->is_floating || window->is_fullscreen) {
            XRaiseWindow(base.display, window->window);
        }

        if (window->has_titlebar) {
            titlebar_draw(window, base);
        }

        XSetInputFocus(base.display, window->window, RevertToPointerRoot, CurrentTime);
        XFlush(base.display);
    } else {
        XSetInputFocus(base.display, base.root, RevertToPointerRoot, CurrentTime);
        XFlush(base.display);
    }
}

void nwm::focus_next(void *arg, Base &base) {
    (void)arg;
    auto &current_ws = get_current_workspace(base);
    if (current_ws.windows.empty()) return;

    std::vector<ManagedWindow*> all_windows;
    for (auto &w : current_ws.windows) {
        all_windows.push_back(&w);
    }

    if (all_windows.empty()) return;

    int current_idx = -1;
    for (size_t i = 0; i < all_windows.size(); ++i) {
        if (current_ws.focused_window && all_windows[i]->window == current_ws.focused_window->window) {
            current_idx = i;
            break;
        }
    }

    int next_idx = (current_idx + 1) % all_windows.size();
    focus_window(all_windows[next_idx], base);

    if (base.horizontal_mode && !all_windows[next_idx]->is_floating && !all_windows[next_idx]->is_fullscreen) {
        Monitor *mon = get_current_monitor(base);
        if (!mon) return;

        int window_width = mon->width / 2;

        int tiled_idx = 0;
        for (int i = 0; i <= next_idx; ++i) {
            if (!all_windows[i]->is_floating && !all_windows[i]->is_fullscreen) {
                if (i == next_idx) break;
                tiled_idx++;
            }
        }

        int target_scroll = tiled_idx * window_width;

        if (target_scroll < current_ws.scroll_offset) {
            current_ws.scroll_offset = target_scroll;
        } else if (target_scroll + window_width > current_ws.scroll_offset + mon->width) {
            current_ws.scroll_offset = target_scroll + window_width - mon->width;
        }

        tile_horizontal(base);
    }
}

void nwm::focus_prev(void *arg, Base &base) {
    (void)arg;
    auto &current_ws = get_current_workspace(base);
    if (current_ws.windows.empty()) return;

    std::vector<ManagedWindow*> all_windows;
    for (auto &w : current_ws.windows) {
        all_windows.push_back(&w);
    }

    if (all_windows.empty()) return;

    int current_idx = -1;
    for (size_t i = 0; i < all_windows.size(); ++i) {
        if (current_ws.focused_window && all_windows[i]->window == current_ws.focused_window->window) {
            current_idx = i;
            break;
        }
    }

    int prev_idx = (current_idx - 1 + all_windows.size()) % all_windows.size();
    focus_window(all_windows[prev_idx], base);

    if (base.horizontal_mode && !all_windows[prev_idx]->is_floating && !all_windows[prev_idx]->is_fullscreen) {
        Monitor *mon = get_current_monitor(base);
        if (!mon) return;

        int window_width = mon->width / 2;

        int tiled_idx = 0;
        for (int i = 0; i <= prev_idx; ++i) {
            if (!all_windows[i]->is_floating && !all_windows[i]->is_fullscreen) {
                if (i == prev_idx) break;
                tiled_idx++;
            }
        }

        int target_scroll = tiled_idx * window_width;

        if (target_scroll < current_ws.scroll_offset) {
            current_ws.scroll_offset = target_scroll;
        } else if (target_scroll + window_width > current_ws.scroll_offset + mon->width) {
            current_ws.scroll_offset = target_scroll + window_width - mon->width;
        }

        tile_horizontal(base);
    }
}

void nwm::move_window(ManagedWindow *window, int x, int y, Base &base) {
    if (window) {
        window->x = x;
        window->y = y;
        XMoveWindow(base.display, window->window, x, y);

        if (window->has_titlebar) {
            window->titlebar.x = x - base.border_width;
            window->titlebar.y = y - base.titlebar_height;
            XMoveWindow(base.display, window->titlebar.window,
                       window->titlebar.x, window->titlebar.y);
            XRaiseWindow(base.display, window->titlebar.window);
        }
    }
}

void nwm::resize_window(ManagedWindow *window, int width, int height, Base &base) {
    if (window) {
        window->width = width;
        window->height = height;
        XResizeWindow(base.display, window->window, width, height);

        if (window->has_titlebar) {
            window->titlebar.width = width + (2 * base.border_width);
            XResizeWindow(base.display, window->titlebar.window,
                         window->titlebar.width, window->titlebar.height);
            XRaiseWindow(base.display, window->titlebar.window);
            titlebar_draw(window, base);
        }
    }
}

void nwm::close_window(void *arg, Base &base) {
    (void)arg;
    if (!base.focused_window) return;

    Window window = base.focused_window->window;

    if (base.anim_manager && base.anim_manager->animations_enabled &&
        base.anim_manager->window_close_enabled) {

        animate_window_close(base, window, [&base, window]() {
            XEvent ev;
            ev.type = ClientMessage;
            ev.xclient.window = window;
            ev.xclient.message_type = XInternAtom(base.display, "WM_PROTOCOLS", True);
            ev.xclient.format = 32;
            ev.xclient.data.l[0] = XInternAtom(base.display, "WM_DELETE_WINDOW", False);
            ev.xclient.data.l[1] = CurrentTime;
            XSendEvent(base.display, window, False, NoEventMask, &ev);
        });
    } else {
        XEvent ev;
        ev.type = ClientMessage;
        ev.xclient.window = window;
        ev.xclient.message_type = XInternAtom(base.display, "WM_PROTOCOLS", True);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = XInternAtom(base.display, "WM_DELETE_WINDOW", False);
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(base.display, window, False, NoEventMask, &ev);
    }
}

void nwm::quit_wm(void *arg, Base &base) {
    bool restart = static_cast<bool>(arg);
    (void)arg;
    base.running = false;
    base.restart = restart;
}

void nwm::handle_map_request(XMapRequestEvent *e, Base &base) {
    if (should_ignore_window(base.display, e->window)) {
        XMapWindow(base.display, e->window);
        handle_special_window_map(base.display, e->window);
        return;
    }

    manage_window(e->window, base);

    auto &current_ws = get_current_workspace(base);

    if (!current_ws.windows.empty()) {
        ManagedWindow *new_window = &current_ws.windows.back();

        if (base.anim_manager && base.anim_manager->animations_enabled &&
            base.anim_manager->window_open_enabled) {
            animate_window_open(base, new_window->window);
        }

        bool had_floating_focus = (base.focused_window && (base.focused_window->is_floating || base.focused_window->is_fullscreen));

        if (!had_floating_focus) {
            focus_window(new_window, base);
        }

        if (!new_window->is_floating && !new_window->is_fullscreen) {
            if (base.horizontal_mode) {
                Monitor *mon = get_current_monitor(base);
                if (mon) {
                    int window_width = mon->width / 2;

                    int tiled_idx = 0;
                    for (size_t i = 0; i < current_ws.windows.size() - 1; ++i) {
                        if (!current_ws.windows[i].is_floating && !current_ws.windows[i].is_fullscreen) {
                            tiled_idx++;
                        }
                    }

                    int target_scroll = tiled_idx * window_width;

                    if (target_scroll + window_width > current_ws.scroll_offset + mon->width) {
                        current_ws.scroll_offset = target_scroll + window_width - mon->width;
                    }
                }
            }

            if (base.horizontal_mode) {
                tile_horizontal(base);
            } else {
                tile_windows(base);
            }
        } else {
            XRaiseWindow(base.display, new_window->window);
        }
    }
}

void nwm::handle_unmap_notify(XUnmapEvent *e, Base &base) {
    unmanage_window(e->window, base);

    if (base.horizontal_mode) {
        tile_horizontal(base);
    } else {
        tile_windows(base);
    }
}

void nwm::handle_destroy_notify(XDestroyWindowEvent *e, Base &base) {
    for (const auto &icon : base.systray.icons) {
        if (icon.window == e->window) {
            systray_handle_destroy(base, e->window);
            return;
        }
    }

    unmanage_window(e->window, base);

    if (base.horizontal_mode) {
        tile_horizontal(base);
    } else {
        tile_windows(base);
    }
}

void nwm::handle_configure_request(XConfigureRequestEvent *e, Base &base) {
    for (const auto &icon : base.systray.icons) {
        if (icon.window == e->window) {
            systray_handle_configure_request(base, e);
            return;
        }
    }

    XWindowChanges wc;
    wc.x = e->x;
    wc.y = e->y;
    wc.width = e->width;
    wc.height = e->height;
    wc.border_width = e->border_width;
    wc.sibling = e->above;
    wc.stack_mode = e->detail;

    auto &current_ws = get_current_workspace(base);
    bool is_floating = false;
    for (auto &w : current_ws.windows) {
        if (w.window == e->window && (w.is_floating || w.is_fullscreen)) {
            is_floating = true;
            if (!w.is_fullscreen) {
                w.x = e->x;
                w.y = e->y;
                w.width = e->width;
                w.height = e->height;
            }
            break;
        }
    }

    if (is_floating) {
        XConfigureWindow(base.display, e->window, e->value_mask, &wc);
    } else {
        XConfigureWindow(base.display, e->window, e->value_mask & (CWSibling | CWStackMode), &wc);
    }
}

void nwm::handle_client_message(XClientMessageEvent *e, Base &base) {
    systray_handle_client_message(base, e);
}

void nwm::handle_key_press(XKeyEvent *e, Base &base) {
    KeySym keysym = XLookupKeysym(e, 0);

    unsigned int cleaned_state = e->state & ~(LockMask | Mod2Mask);

    for (auto &k : keys) {
        unsigned int cleaned_mod = k.mod & ~(LockMask | Mod2Mask);
        if (keysym == k.keysym && cleaned_state == cleaned_mod) {
            if (k.func) {
                k.func((void*)k.arg, base);
            }
            break;
        }
    }
}

void nwm::reload_config(void *arg, nwm::Base &base) {
    (void)arg;
    setup_keys(base);

    if (base.horizontal_mode) {
        tile_horizontal(base);
    } else {
        tile_windows(base);
    }

    XFlush(base.display);
}

void nwm::handle_button_press(XButtonEvent *e, Base &base) {

    if ((e->state & MODKEY) && (e->button == Button4 || e->button == Button5)) {
        if (base.horizontal_mode) {
            if (e->button == Button4) {
                scroll_left(nullptr, base);
            } else {
                scroll_right(nullptr, base);
            }
            return;
        }
    }

    if (e->window == base.bar.window) {
        if (e->button == Button1) {
            bar_handle_click(base, e->x, e->y, e->button);
        } else if (e->button == Button4) {
            bar_handle_scroll(base, -1);
        } else if (e->button == Button5) {
            bar_handle_scroll(base, 1);
        }
        return;
    }

    auto &current_ws = get_current_workspace(base);

    Window target_window = (e->subwindow != None) ? e->subwindow : e->window;

    if (e->button == Button1 && (e->state & MODKEY)) {
        for (auto &w : current_ws.windows) {
            if (w.has_titlebar && w.titlebar.window == e->window) {
                focus_window(&w, base);
                if (e->button == Button1) {
                    base.dragging = true;
                    base.drag_window = w.window;
                    base.drag_start_x = e->x_root;
                    base.drag_start_y = e->y_root;
                    base.drag_window_start_x = w.x;
                    base.drag_window_start_y = w.y;
                    XDefineCursor(base.display, base.root, base.cursor_move);
                }
                return;
            }

            if (target_window == w.window) {
                if (base.dragging || base.resizing) {
                    XUngrabPointer(base.display, CurrentTime);
                    XDefineCursor(base.display, base.root, base.cursor);
                }

                base.dragging = true;
                base.resizing = false;
                base.drag_window = target_window;
                base.drag_start_x = e->x_root;
                base.drag_start_y = e->y_root;

                XDefineCursor(base.display, base.root, base.cursor_move);

                XWindowAttributes attr;
                if (XGetWindowAttributes(base.display, target_window, &attr)) {
                    base.drag_window_start_x = attr.x;
                    base.drag_window_start_y = attr.y;
                }

                focus_window(&w, base);
                return;
            }
        }
    } else if (e->button == Button3 && (e->state & MODKEY)) {
        for (auto &w : current_ws.windows) {
            if (target_window == w.window) {
                if (base.dragging || base.resizing) {
                    XUngrabPointer(base.display, CurrentTime);
                    XDefineCursor(base.display, base.root, base.cursor);
                }

                base.resizing = true;
                base.dragging = false;
                base.drag_window = target_window;
                base.drag_start_x = e->x_root;
                base.drag_start_y = e->y_root;

                XDefineCursor(base.display, base.root, base.cursor_resize);

                XWindowAttributes attr;
                if (XGetWindowAttributes(base.display, target_window, &attr)) {
                    base.resize_start_width = attr.width;
                    base.resize_start_height = attr.height;
                }

                focus_window(&w, base);
                return;
            }
        }
    } else if (e->button == Button1) {
        for (auto &w : current_ws.windows) {
            if (target_window == w.window) {
                focus_window(&w, base);
                break;
            }
        }
    }
}

void nwm::handle_button_release(XButtonEvent *e, Base &base) {
    if (!base.dragging && !base.resizing) return;

    if (e->button == Button1 || e->button == Button3) {
        XUngrabPointer(base.display, CurrentTime);
        XDefineCursor(base.display, base.root, base.cursor);

        auto &current_ws = get_current_workspace(base);

        if (base.dragging) {
            bool is_floating = false;
            for (auto &w : current_ws.windows) {
                if (w.window == base.drag_window) {
                    is_floating = w.is_floating;
                    if (is_floating) {
                        XWindowAttributes attr;
                        if (XGetWindowAttributes(base.display, base.drag_window, &attr)) {
                            w.x = attr.x;
                            w.y = attr.y;

                            Monitor *new_mon = get_monitor_at_point(base, w.x + w.width / 2, w.y + w.height / 2);
                            if (new_mon) {
                                w.monitor = new_mon->id;
                            }
                        }
                    }
                    break;
                }
            }

            if (!is_floating && current_ws.windows.size() > 1) {
                int dragged_idx = -1;

                for (size_t i = 0; i < current_ws.windows.size(); ++i) {
                    if (current_ws.windows[i].window == base.drag_window) {
                        dragged_idx = i;
                        break;
                    }
                }

                if (dragged_idx != -1) {
                    int target_idx = -1;

                    if (base.horizontal_mode) {
                        Monitor *mon = get_current_monitor(base);
                        if (mon) {
                            int window_width = mon->width / 2;
                            int window_center_x = e->x_root;
                            target_idx = (window_center_x + current_ws.scroll_offset) / window_width;
                        }
                    } else {
                        Monitor *mon = get_monitor_at_point(base, e->x_root, e->y_root);
                        if (!mon) mon = get_current_monitor(base);
                        if (!mon) return;

                        int bar_height = base.bar_visible ? base.bar.height : 0;

                        int tiled_count = 0;
                        for (auto &w : current_ws.windows) {
                            if (!w.is_floating && !w.is_fullscreen && w.monitor == mon->id) tiled_count++;
                        }

                        if (tiled_count == 1) {
                            target_idx = dragged_idx;
                        } else {
                            int master_width = (int)(mon->width * mon->master_factor);

                            if (e->x_root < mon->x + master_width / 2) {
                                target_idx = 0;
                            } else if (e->x_root > mon->x + master_width) {
                                int usable_height = mon->height - bar_height;
                                int stack_window_height = usable_height / (tiled_count - 1);
                                int relative_y = e->y_root - (mon->y + (base.bar_position == 0 ? bar_height : 0));

                                int stack_idx = relative_y / stack_window_height;
                                target_idx = std::min(std::max(1, stack_idx + 1), tiled_count - 1);
                            } else {
                                if (e->y_root < (mon->y + mon->height / 2)) {
                                    target_idx = 0;
                                } else {
                                    target_idx = 1;
                                }
                            }
                        }
                    }

                    if (target_idx < 0) target_idx = 0;
                    if (target_idx >= (int)current_ws.windows.size()) target_idx = current_ws.windows.size() - 1;

                    if (target_idx != dragged_idx) {
                        ManagedWindow dragged_window = current_ws.windows[dragged_idx];
                        current_ws.windows.erase(current_ws.windows.begin() + dragged_idx);
                        current_ws.windows.insert(current_ws.windows.begin() + target_idx, dragged_window);
                    }
                }
            }
        } else if (base.resizing) {
            bool is_floating = false;
            for (auto &w : current_ws.windows) {
                if (w.window == base.drag_window) {
                    is_floating = w.is_floating;
                    if (is_floating) {
                        XWindowAttributes attr;
                        if (XGetWindowAttributes(base.display, base.drag_window, &attr)) {
                            w.width = attr.width;
                            w.height = attr.height;
                        }
                    }
                    break;
                }
            }

            if (!is_floating && !base.horizontal_mode && current_ws.windows.size() >= 2) {
                for (size_t i = 0; i < current_ws.windows.size(); ++i) {
                    if (current_ws.windows[i].window == base.drag_window && i == 0) {
                        Monitor *mon = get_monitor_at_point(base,
                            current_ws.windows[i].x + current_ws.windows[i].width / 2,
                            current_ws.windows[i].y + current_ws.windows[i].height / 2);
                        if (!mon) mon = get_current_monitor(base);
                        if (!mon) break;

                        XWindowAttributes attr;
                        if (XGetWindowAttributes(base.display, base.drag_window, &attr)) {
                            mon->master_factor = (float)attr.width / mon->width;
                            if (mon->master_factor < 0.1f) mon->master_factor = 0.1f;
                            if (mon->master_factor > 0.9f) mon->master_factor = 0.9f;
                        }
                        break;
                    }
                }
            }
        }

        if (base.horizontal_mode) {
            tile_horizontal(base);
        } else {
            tile_windows(base);
        }

        base.dragging = false;
        base.resizing = false;
        base.drag_window = None;

        if (base.focused_window) {
            XRaiseWindow(base.display, base.focused_window->window);
        }

        XFlush(base.display);
    }
}

void nwm::handle_motion_notify(XMotionEvent *e, Base &base) {
    if (e->window == base.systray.window) {
        return;
    }

    if (e->window == base.bar.window) {
        bar_handle_motion(base, e->x, e->y);
        return;
    }

    if (!base.dragging && !base.resizing) return;
    if (base.drag_window == None) return;

    auto &current_ws = get_current_workspace(base);

    if (base.dragging) {
        for (auto &w : current_ws.windows) {
            if (w.window == base.drag_window) {
                int delta_x = e->x_root - base.drag_start_x;
                int delta_y = e->y_root - base.drag_start_y;

                int new_x = base.drag_window_start_x + delta_x;
                int new_y = base.drag_window_start_y + delta_y;

                w.x = new_x;
                w.y = new_y;
                XMoveWindow(base.display, w.window, new_x, new_y);
                XRaiseWindow(base.display, w.window);

                if (w.has_titlebar) {
                    w.titlebar.x = new_x - base.border_width;
                    w.titlebar.y = new_y - base.titlebar_height;
                    XMoveWindow(base.display, w.titlebar.window,
                               w.titlebar.x, w.titlebar.y);
                    XRaiseWindow(base.display, w.titlebar.window);
                }
                break;
            }
        }
    } else if (base.resizing) {
        for (auto &w : current_ws.windows) {
            if (w.window == base.drag_window) {
                int delta_x = e->x_root - base.drag_start_x;
                int delta_y = e->y_root - base.drag_start_y;

                int new_width = base.resize_start_width + delta_x;
                int new_height = base.resize_start_height + delta_y;

                if (new_width < 100) new_width = 100;
                if (new_height < 100) new_height = 100;

                w.width = new_width;
                w.height = new_height;
                XResizeWindow(base.display, w.window, new_width, new_height);
                XRaiseWindow(base.display, w.window);

                if (w.has_titlebar) {
                    w.titlebar.width = new_width + (2 * base.border_width);
                    XResizeWindow(base.display, w.titlebar.window,
                                 w.titlebar.width, w.titlebar.height);
                    XRaiseWindow(base.display, w.titlebar.window);
                    titlebar_draw(&w, base);
                }
                break;
            }
        }
    }
}

void nwm::handle_enter_notify(XCrossingEvent *e, Base &base) {
    if (e->window == base.bar.window) {
        return;
    }

    if (base.dragging || base.resizing) {
        return;
    }

    XWindowAttributes attr;
    if (XGetWindowAttributes(base.display, e->window, &attr)) {
        if (attr.override_redirect) {
            return;
        }
    }

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = nullptr;

    Atom window_type_atom = XInternAtom(base.display, "_NET_WM_WINDOW_TYPE", False);
    if (XGetWindowProperty(base.display, e->window, window_type_atom, 0, 1,
                          False, XA_ATOM, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        Atom type = *(Atom*)prop;
        XFree(prop);

        Atom dropdown = XInternAtom(base.display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
        Atom popup = XInternAtom(base.display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
        Atom combo = XInternAtom(base.display, "_NET_WM_WINDOW_TYPE_COMBO", False);

        if (type == dropdown || type == popup || type == combo) {
            return;
        }
    }

    auto &current_ws = get_current_workspace(base);
    for (auto &w : current_ws.windows) {
        if (e->window == w.window) {
            focus_window(&w, base);
            break;
        }
    }
}

void nwm::handle_expose(XExposeEvent *e, Base &base) {
    if (e->window == base.bar.window) {
        bar_draw(base);
        return;
    }

    auto &current_ws = get_current_workspace(base);
    for (auto &w : current_ws.windows) {
        if (w.has_titlebar && w.titlebar.window == e->window) {
            titlebar_draw(&w, base);
            return;
        }
    }
}


std::string nwm::get_window_title(Display* display, Window window) {
    std::string title = "Untitled";

    Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = nullptr;

    if (XGetWindowProperty(display, window, net_wm_name, 0, 1024,
                          False, utf8_string, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        title = std::string((char*)prop);
        XFree(prop);
        return title;
    }

    XTextProperty text_prop;
    if (XGetWMName(display, window, &text_prop)) {
        if (text_prop.value) {
            title = std::string((char*)text_prop.value);
            XFree(text_prop.value);
        }
    }

    return title;
}

void nwm::titlebar_init(ManagedWindow* window, Base &base) {
    window->has_titlebar = false;
    window->titlebar.xft_draw = nullptr;
    window->titlebar.window = 0;
    window->titlebar.needs_redraw = false;

    if (!base.show_window_titles || window->is_floating || window->is_fullscreen) {
        return;
    }

    window->title = get_window_title(base.display, window->window);

    window->titlebar.xft_draw = XftDrawCreate(
        base.display, window->window,
        DefaultVisual(base.display, base.screen),
        DefaultColormap(base.display, base.screen)
    );

    if (!window->titlebar.xft_draw) {
        return;
    }

    window->titlebar.x = 0;
    window->titlebar.y = 0;
    window->titlebar.width = window->width;
    window->titlebar.height = base.titlebar_height;
    window->titlebar.needs_redraw = true;
    window->titlebar.window = window->window;

    window->has_titlebar = true;

    titlebar_draw(window, base);
}

void nwm::titlebar_cleanup(ManagedWindow* window, Base &base) {
    (void)base;
    if (!window->has_titlebar) return;

    if (window->titlebar.xft_draw) {
        XftDrawDestroy(window->titlebar.xft_draw);
        window->titlebar.xft_draw = nullptr;
    }

    window->titlebar.window = 0;
    window->has_titlebar = false;
}

void nwm::titlebar_draw(ManagedWindow* window, Base &base) {
    if (!window->has_titlebar || !window->titlebar.xft_draw) {
        return;
    }

    GC gc = XCreateGC(base.display, window->window, 0, nullptr);
    if (!gc) {
        return;
    }

    unsigned long bg_color = window->is_focused ? TITLE_BAR_FOCUS_BG : TITLE_BAR_BG;
    XftColor* text_color = window->is_focused ? &base.titlebar_focus_fg : &base.titlebar_fg;

    XSetForeground(base.display, gc, bg_color);
    XSetFunction(base.display, gc, GXcopy);

    XFillRectangle(base.display, window->window, gc,
                   0, 0, window->width, base.titlebar_height);

    if (!window->title.empty() && base.xft_font) {
        int text_x = 5;
        int text_y = base.titlebar_height / 2 + 5;

        std::string display_title = window->title;
        XGlyphInfo extents;
        XftTextExtentsUtf8(base.display, base.xft_font,
                          (XftChar8*)display_title.c_str(), display_title.length(),
                          &extents);

        while (extents.width > (window->width - 10) && display_title.length() > 3) {
            display_title = display_title.substr(0, display_title.length() - 4) + "...";
            XftTextExtentsUtf8(base.display, base.xft_font,
                              (XftChar8*)display_title.c_str(), display_title.length(),
                              &extents);
        }

        XftDrawStringUtf8(window->titlebar.xft_draw, text_color, base.xft_font,
                         text_x, text_y,
                         (XftChar8*)display_title.c_str(), display_title.length());
    }

    XFreeGC(base.display, gc);
    XSync(base.display, False);
    window->titlebar.needs_redraw = false;
}

void nwm::titlebar_update_title(ManagedWindow* window, Base &base) {
    if (!window->has_titlebar) return;

    std::string new_title = get_window_title(base.display, window->window);
    if (new_title != window->title) {
        window->title = new_title;
        window->titlebar.needs_redraw = true;
        titlebar_draw(window, base);
    }
}

void nwm::setup_ewmh(Base &base) {
    Atom net_supporting_wm_check = XInternAtom(base.display, "_NET_SUPPORTING_WM_CHECK", False);
    Atom net_wm_name = XInternAtom(base.display, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(base.display, "UTF8_STRING", False);
    Atom net_supported = XInternAtom(base.display, "_NET_SUPPORTED", False);

    Window check_win = XCreateSimpleWindow(base.display, base.root, 0, 0, 1, 1, 0, 0, 0);

    XChangeProperty(base.display, check_win, net_supporting_wm_check, XA_WINDOW, 32,
                   PropModeReplace, (unsigned char *)&check_win, 1);

    XChangeProperty(base.display, base.root, net_supporting_wm_check, XA_WINDOW, 32,
                   PropModeReplace, (unsigned char *)&check_win, 1);

    const char *wm_name = "NWM";
    XChangeProperty(base.display, check_win, net_wm_name, utf8_string, 8,
                   PropModeReplace, (unsigned char *)wm_name, strlen(wm_name));

    Atom supported[] = {
        net_supporting_wm_check,
        net_wm_name,
        XInternAtom(base.display, "_NET_WM_STATE", False),
        XInternAtom(base.display, "_NET_WM_STATE_FULLSCREEN", False),
        XInternAtom(base.display, "_NET_WM_STATE_MODAL", False),
        XInternAtom(base.display, "_NET_WM_WINDOW_TYPE", False),
        XInternAtom(base.display, "_NET_WM_WINDOW_TYPE_DIALOG", False),
        XInternAtom(base.display, "_NET_WM_WINDOW_TYPE_UTILITY", False),
        XInternAtom(base.display, "_NET_WM_WINDOW_TYPE_SPLASH", False),
        XInternAtom(base.display, "_NET_ACTIVE_WINDOW", False),
        XInternAtom(base.display, "_NET_CLIENT_LIST", False),
        XInternAtom(base.display, "_NET_NUMBER_OF_DESKTOPS", False),
        XInternAtom(base.display, "_NET_CURRENT_DESKTOP", False),
    };

    XChangeProperty(base.display, base.root, net_supported, XA_ATOM, 32,
                   PropModeReplace, (unsigned char *)supported, sizeof(supported) / sizeof(Atom));

    long num_desktops = NUM_WORKSPACES;
    Atom net_number_of_desktops = XInternAtom(base.display, "_NET_NUMBER_OF_DESKTOPS", False);
    XChangeProperty(base.display, base.root, net_number_of_desktops, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char *)&num_desktops, 1);

    long current_desktop = 0;
    Atom net_current_desktop = XInternAtom(base.display, "_NET_CURRENT_DESKTOP", False);
    XChangeProperty(base.display, base.root, net_current_desktop, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char *)&current_desktop, 1);

    XSync(base.display, False);

    base.hint_check_window = check_win;
}

void nwm::init(Base &base) {
    struct sigaction sa;
    sa.sa_handler = [](int) {
        while (waitpid(-1, NULL, WNOHANG) > 0);
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    base.display = XOpenDisplay(NULL);
    if (!base.display) {
        std::cerr << "Error: Cannot open display\n";
        std::exit(1);
    }

    XSetErrorHandler(x_error_handler);

    base.gaps_enabled = true;
    base.gaps = GAP_SIZE;
    base.screen = DefaultScreen(base.display);
    base.root = RootWindow(base.display, base.screen);
    base.focused_window = nullptr;
    base.running = false;
    base.restart = false;
    base.master_factor = 0.5f;
    base.horizontal_mode = false;
    base.widget = WIDGET;
    base.bar_visible = true;
    base.bar_position = BAR_POSITION;
    base.bar_height = BAR_HEIGHT;
    base.bar_bg_color = BAR_BG_COLOR;
    base.bar_fg_color = BAR_FG_COLOR;
    base.bar_active_color = BAR_ACTIVE_COLOR;
    base.bar_inactive_color = BAR_INACTIVE_COLOR;
    base.bar_accent_color = BAR_ACCENT_COLOR;
    base.bar_indicator_color = BAR_INDICATOR_COLOR;
    base.border_width = BORDER_WIDTH;
    base.border_color = BORDER_COLOR;
    base.focus_color = FOCUS_COLOR;
    base.resize_step = RESIZE_STEP;
    base.scroll_step = SCROLL_STEP;

    base.show_window_titles = SHOW_WINDOW_TITLES;
    base.titlebar_height = TITLE_BAR_HEIGHT;
    base.use_xinerama = USE_XINERAMA;
    base.use_builtin_bar = USE_BUILTIN_BAR;

    base.cursor = XCreateFontCursor(base.display, XC_left_ptr);
    base.cursor_move = XCreateFontCursor(base.display, XC_fleur);
    base.cursor_resize = XCreateFontCursor(base.display, XC_bottom_right_corner);
    XDefineCursor(base.display, base.root, base.cursor);

    base.xft_draw = XftDrawCreate(base.display, base.root,
                                 DefaultVisual(base.display, base.screen),
                                 DefaultColormap(base.display, base.screen));
    if (!base.xft_draw) {
        std::cerr << "Error: Failed to create XftDraw\n";
        std::exit(1);
    }

    base.xft_font = XftFontOpenName(base.display, base.screen, FONT);
    if (!base.xft_font) {
        base.xft_font = XftFontOpenName(base.display, base.screen, "monospace:size=10");
    }
    if (!base.xft_font) {
        base.xft_font = XftFontOpenName(base.display, base.screen, "fixed");
    }
    if (!base.xft_font) {
        std::cerr << "Error: Failed to load any Xft font\n";
        std::exit(1);
    }

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

    create_color(TITLE_BAR_BG, base.titlebar_bg);
    create_color(TITLE_BAR_FG, base.titlebar_fg);
    create_color(TITLE_BAR_FOCUS_BG, base.titlebar_focus_bg);
    create_color(TITLE_BAR_FOCUS_FG, base.titlebar_focus_fg);

    workspace_init(base);
    monitors_init(base);
    animations_init(base);

    Atom current_ws_atom = XInternAtom(base.display, "_NWM_CURRENT_WORKSPACE", False);
    Atom current_mon_atom = XInternAtom(base.display, "_NWM_CURRENT_MONITOR", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = nullptr;

    if (XGetWindowProperty(base.display, base.root, current_ws_atom, 0, 1,
                          True, XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        long saved_workspace = *(long*)prop;
        XFree(prop);
        if (saved_workspace >= 0 && saved_workspace < NUM_WORKSPACES) {
            base.current_workspace = saved_workspace;

            if (base.current_monitor >= 0 && base.current_monitor < (int)base.monitors.size()) {
                base.monitors[base.current_monitor].current_workspace = saved_workspace;
            }
        }
    }

    prop = nullptr;
    if (XGetWindowProperty(base.display, base.root, current_mon_atom, 0, 1,
                          True, XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        long saved_monitor = *(long*)prop;
        XFree(prop);
        if (saved_monitor >= 0 && saved_monitor < (int)base.monitors.size()) {
            base.current_monitor = saved_monitor;
        }
    }

    if (base.use_builtin_bar) {
        bar_init(base);
    }
    systray_init(base);

    XSelectInput(base.display, base.root,
                 SubstructureRedirectMask | SubstructureNotifyMask |
                 ButtonPressMask | EnterWindowMask | KeyPressMask | PropertyChangeMask);

    Window root_return, parent_return;
    Window *children;
    unsigned int nchildren;

    if (XQueryTree(base.display, base.root, &root_return, &parent_return, &children, &nchildren)) {
        for (unsigned int i = 0; i < nchildren; ++i) {
            XWindowAttributes attr;
            if (XGetWindowAttributes(base.display, children[i], &attr)) {
                bool should_manage = (attr.map_state == IsViewable) &&
                                   !should_ignore_window(base.display, children[i]);

                if (should_manage) {
                    nwm::manage_window(children[i], base);
                }
            }
        }
        if (children) XFree(children);
    }

    setup_ewmh(base);
    nwm::tile_windows(base);
    nwm::setup_keys(base);
    prop = nullptr;
    Atom focused_win_atom = XInternAtom(base.display, "_NWM_FOCUSED_WINDOW", False);
    if (XGetWindowProperty(base.display, base.root, focused_win_atom, 0, 1,
        True, XA_WINDOW, &actual_type, &actual_format,
        &nitems, &bytes_after, &prop) == Success && prop) {
            Window saved_focused = *(Window*)prop;
            XFree(prop);

        auto &current_ws = get_current_workspace(base);
        for (auto &w : current_ws.windows) {
            if (w.window == saved_focused) {
                focus_window(&w, base);
                break;
            }
        }
    }
    bar_draw(base);
}

void nwm::cleanup(Base &base) {
    if (base.hint_check_window) {
        XDestroyWindow(base.display, base.hint_check_window);
        base.hint_check_window = 0;
    }

    if (base.dragging || base.resizing) {
        XUngrabPointer(base.display, CurrentTime);
        base.dragging = false;
        base.resizing = false;
        base.drag_window = None;
    }

    if (base.restart) {
        Atom current_ws_atom = XInternAtom(base.display, "_NWM_CURRENT_WORKSPACE", False);
        long current_ws = base.current_workspace;
        XChangeProperty(base.display, base.root, current_ws_atom,
        XA_CARDINAL, 32, PropModeReplace,
        (unsigned char*)&current_ws, 1);

        Atom current_mon_atom = XInternAtom(base.display, "_NWM_CURRENT_MONITOR", False);
        long current_mon = base.current_monitor;
        XChangeProperty(base.display, base.root, current_mon_atom,
        XA_CARDINAL, 32, PropModeReplace,
        (unsigned char*)&current_mon, 1);

        if (base.focused_window) {
            Atom focused_win_atom = XInternAtom(base.display, "_NWM_FOCUSED_WINDOW", False);
            XChangeProperty(base.display, base.root, focused_win_atom,
            XA_WINDOW, 32, PropModeReplace,
            (unsigned char*)&base.focused_window->window, 1);
        }

        XSync(base.display, False);
    }

    if (!base.restart) {
        for (auto &ws : base.workspaces) {
            for (auto &w : ws.windows) {
                XUnmapWindow(base.display, w.window);
            }
        }
    } else {
        for (auto &ws : base.workspaces) {
            for (auto &w : ws.windows) {
                Atom workspace_atom = XInternAtom(base.display, "_NWM_WORKSPACE", False);
                long workspace_id = w.workspace;
                XChangeProperty(base.display, w.window, workspace_atom,
                              XA_CARDINAL, 32, PropModeReplace,
                              (unsigned char*)&workspace_id, 1);

                Atom floating_atom = XInternAtom(base.display, "_NWM_FLOATING", False);
                long is_floating = w.is_floating ? 1 : 0;
                XChangeProperty(base.display, w.window, floating_atom,
                              XA_CARDINAL, 32, PropModeReplace,
                              (unsigned char*)&is_floating, 1);

                Atom fullscreen_atom = XInternAtom(base.display, "_NWM_FULLSCREEN", False);
                long is_fullscreen = w.is_fullscreen ? 1 : 0;
                XChangeProperty(base.display, w.window, fullscreen_atom,
                              XA_CARDINAL, 32, PropModeReplace,
                              (unsigned char*)&is_fullscreen, 1);

                Atom monitor_atom = XInternAtom(base.display, "_NWM_MONITOR", False);
                long monitor_id = w.monitor;
                XChangeProperty(base.display, w.window, monitor_atom,
                              XA_CARDINAL, 32, PropModeReplace,
                              (unsigned char*)&monitor_id, 1);

                XMapWindow(base.display, w.window);
            }
        }
        XSync(base.display, False);
    }

    for (auto &ws : base.workspaces) {
        for (auto &w : ws.windows) {
            titlebar_cleanup(&w, base);
        }
    }

    auto free_color = [&](XftColor& color) {
        XftColorFree(base.display, DefaultVisual(base.display, base.screen),
                     DefaultColormap(base.display, base.screen), &color);
    };

    free_color(base.titlebar_bg);
    free_color(base.titlebar_fg);
    free_color(base.titlebar_focus_bg);
    free_color(base.titlebar_focus_fg);

    systray_cleanup(base);
    animations_cleanup(base);
    bar_cleanup(base);

    if (base.xft_font) {
        XftFontClose(base.display, base.xft_font);
        base.xft_font = nullptr;
    }

    if (base.xft_draw) {
        XftDrawDestroy(base.xft_draw);
        base.xft_draw = nullptr;
    }

    if (base.cursor) {
        XFreeCursor(base.display, base.cursor);
        base.cursor = 0;
    }

    if (base.cursor_move) {
        XFreeCursor(base.display, base.cursor_move);
        base.cursor_move = 0;
    }

    if (base.cursor_resize) {
        XFreeCursor(base.display, base.cursor_resize);
        base.cursor_resize = 0;
    }

    if (base.display) {
        XCloseDisplay(base.display);
        base.display = nullptr;
    }
}

void nwm::manage_window(Window window, Base &base) {
    XWindowAttributes attr;

    if (XGetWindowAttributes(base.display, window, &attr) == 0) {
        return;
    }

    if (window == base.hint_check_window) {
        return;
    }

    if (should_ignore_window(base.display, window)) {
        return;
    }

    int target_workspace = base.current_workspace;
    int target_monitor = base.current_monitor;
    bool saved_floating = false;
    bool saved_fullscreen = false;

    Atom workspace_atom = XInternAtom(base.display, "_NWM_WORKSPACE", False);
    Atom floating_atom = XInternAtom(base.display, "_NWM_FLOATING", False);
    Atom fullscreen_atom = XInternAtom(base.display, "_NWM_FULLSCREEN", False);
    Atom monitor_atom = XInternAtom(base.display, "_NWM_MONITOR", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = nullptr;

    if (XGetWindowProperty(base.display, window, workspace_atom, 0, 1,
                          True, XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        long saved_workspace = *(long*)prop;
        XFree(prop);
        if (saved_workspace >= 0 && saved_workspace < NUM_WORKSPACES) {
            target_workspace = saved_workspace;
        }
    }

    prop = nullptr;
    if (XGetWindowProperty(base.display, window, floating_atom, 0, 1,
                          True, XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        long is_floating = *(long*)prop;
        saved_floating = (is_floating == 1);
        XFree(prop);
    }

    prop = nullptr;
    if (XGetWindowProperty(base.display, window, fullscreen_atom, 0, 1,
                          True, XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        long is_fullscreen = *(long*)prop;
        saved_fullscreen = (is_fullscreen == 1);
        XFree(prop);
    }

    prop = nullptr;
    if (XGetWindowProperty(base.display, window, monitor_atom, 0, 1,
                          True, XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        long saved_monitor = *(long*)prop;
        XFree(prop);
        if (saved_monitor >= 0 && saved_monitor < (int)base.monitors.size()) {
            target_monitor = saved_monitor;
        }
    }

    auto &target_ws = base.workspaces[target_workspace];

    for (const auto &w : target_ws.windows) {
        if (w.window == window) {
            return;
        }
    }

    bool is_float = saved_floating || should_float(base.display, window);

    ManagedWindow w;
    w.window = window;
    w.is_floating = is_float;
    w.is_focused = false;
    w.is_fullscreen = saved_fullscreen;
    w.workspace = target_workspace;
    w.pre_fs_x = 0;
    w.pre_fs_y = 0;
    w.pre_fs_width = 0;
    w.pre_fs_height = 0;
    w.pre_fs_floating = false;

    w.monitor = target_monitor;

    if (is_float) {
        XSizeHints hints;
        long supplied;
        Monitor *mon = (target_monitor >= 0 && target_monitor < (int)base.monitors.size())
                       ? &base.monitors[target_monitor]
                       : get_current_monitor(base);
        if (!mon && !base.monitors.empty()) mon = &base.monitors[0];

        if (XGetWMNormalHints(base.display, window, &hints, &supplied)) {
            if (hints.flags & (PSize | USSize)) {
                w.width = hints.width;
                w.height = hints.height;
            } else {
                w.width = attr.width > 0 ? attr.width : 400;
                w.height = attr.height > 0 ? attr.height : 300;
            }

            if (hints.flags & (PPosition | USPosition)) {
                w.x = hints.x;
                w.y = hints.y;
            } else {
                if (mon) {
                    w.x = mon->x + (mon->width - w.width) / 2;
                    w.y = mon->y + (mon->height - w.height) / 2;
                } else {
                    w.x = (WIDTH(base.display, base.screen) - w.width) / 2;
                    w.y = (HEIGHT(base.display, base.screen) - w.height) / 2;
                }
            }
        } else {
            w.width = attr.width > 0 ? attr.width : 400;
            w.height = attr.height > 0 ? attr.height : 300;
            if (mon) {
                w.x = mon->x + (mon->width - w.width) / 2;
                w.y = mon->y + (mon->height - w.height) / 2;
            } else {
                w.x = (WIDTH(base.display, base.screen) - w.width) / 2;
                w.y = (HEIGHT(base.display, base.screen) - w.height) / 2;
            }
        }

        if (w.x != attr.x || w.y != attr.y || w.width != attr.width || w.height != attr.height) {
            XMoveResizeWindow(base.display, window, w.x, w.y, w.width, w.height);
        }
    } else {
        w.x = base.gaps;
        w.y = base.gaps + base.bar.height;
        w.width = WIDTH(base.display, base.screen) / 2;
        w.height = HEIGHT(base.display, base.screen) / 2;
    }

    target_ws.windows.push_back(w);

    ManagedWindow* new_window = &target_ws.windows.back();
    titlebar_init(new_window, base);

    XSetWindowAttributes attrs;
    attrs.event_mask = EnterWindowMask | LeaveWindowMask | PropertyChangeMask |
                       StructureNotifyMask | FocusChangeMask;
    XChangeWindowAttributes(base.display, window, CWEventMask, &attrs);

    XSetWindowBorder(base.display, window, base.border_color);
    XSetWindowBorderWidth(base.display, window, is_float ? 1 : base.border_width);

    if (saved_fullscreen) {
        Monitor *mon = (target_monitor >= 0 && target_monitor < (int)base.monitors.size())
                       ? &base.monitors[target_monitor]
                       : get_current_monitor(base);
        if (mon) {
            XSetWindowBorderWidth(base.display, window, 0);
            XMoveResizeWindow(base.display, window, mon->x, mon->y, mon->width, mon->height);

            Atom wm_state = XInternAtom(base.display, "_NET_WM_STATE", False);
            Atom fullscreen = XInternAtom(base.display, "_NET_WM_STATE_FULLSCREEN", False);
            XChangeProperty(base.display, window, wm_state, XA_ATOM, 32,
                          PropModeReplace, (unsigned char*)&fullscreen, 1);
        }
    }

    if (target_workspace == (int)base.current_workspace) {
        XMapWindow(base.display, window);
        if (is_float || saved_fullscreen) {
            XRaiseWindow(base.display, window);
        }
    } else {
        XUnmapWindow(base.display, window);
    }

    XFlush(base.display);
}

void nwm::run(Base &base) {
    base.running = true;

    XSelectInput(base.display, base.root,
                 SubstructureRedirectMask | SubstructureNotifyMask |
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                 EnterWindowMask | KeyPressMask | PropertyChangeMask);

    XSelectInput(base.display, base.bar.window,
                 ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | Button4Mask | Button5Mask);

    XSetErrorHandler(x_error_handler);

    bool bar_needs_update = false;

    while (base.running) {
        bool has_animations = base.anim_manager && !base.anim_manager->animations.empty();

        if (!has_animations && !XPending(base.display)) {
            XEvent e;
            XPeekEvent(base.display, &e);
        }
        while (XPending(base.display)) {
            XEvent e;
            XNextEvent(base.display, &e);

            if (e.type == base.xrandr_event_base + RRScreenChangeNotify ||
                e.type == base.xrandr_event_base + RRNotify) {
                monitors_update(base);
                bar_draw(base);
                continue;
            }

            switch (e.type) {
                case PropertyNotify:
                    handle_property_notify(&e.xproperty, base);
                    if (e.xproperty.window == base.root) {
                        bar_needs_update = true;
                    }
                    break;
                case MapRequest:
                    handle_map_request(&e.xmaprequest, base);
                    bar_needs_update = true;
                    break;
                case UnmapNotify:
                    handle_unmap_notify(&e.xunmap, base);
                    bar_needs_update = true;
                    break;
                case DestroyNotify:
                    handle_destroy_notify(&e.xdestroywindow, base);
                    bar_needs_update = true;
                    break;
                case ConfigureRequest:
                    handle_configure_request(&e.xconfigurerequest, base);
                    break;
                case KeyPress:
                    handle_key_press(&e.xkey, base);
                    break;
                case ButtonPress:
                    handle_button_press(&e.xbutton, base);
                    break;
                case ButtonRelease:
                    handle_button_release(&e.xbutton, base);
                    break;
                case MotionNotify:
                    handle_motion_notify(&e.xmotion, base);
                    break;
                case EnterNotify:
                    handle_enter_notify(&e.xcrossing, base);
                    break;
                case Expose:
                    handle_expose(&e.xexpose, base);
                    break;
                case ClientMessage:
                    handle_client_message(&e.xclient, base);
                    break;
                default:
                    break;
            }
        }

        if (has_animations) {
            animations_update(base);
        }

        if (bar_needs_update) {
            base.bar.systray_width = systray_get_width(base);
            bar_draw(base);
            bar_needs_update = false;
        }

        if (has_animations) {
            usleep(16666);
        }
    }

}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "-v") == 0) {
        printf("nwm %.1f\n", VERSION);
        return 0;
    }

    nwm::Base wm;
    nwm::init(wm);
    nwm::run(wm);
    nwm::cleanup(wm);
    if (wm.restart == true) {
        execv(*argv, argv);
        perror("Failed to execv");
    }
    return 0;
}
