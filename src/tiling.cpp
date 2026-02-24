#include "tiling.hpp"
#include "bar.hpp"
#include "config.hpp"
#include <algorithm>
#include <X11/Xlib.h>
#include "animations.hpp"


void nwm::tile_horizontal(Base &base) {
    auto &current_ws = get_current_workspace(base);

    for (auto &mon : base.monitors) {
        std::vector<ManagedWindow*> tiled_windows;
        for (auto &w : current_ws.windows) {
            if (!w.is_floating && !w.is_fullscreen && w.monitor == mon.id) {
                tiled_windows.push_back(&w);
            }
        }

        if (tiled_windows.empty()) continue;

        int own_bar = (base.bar_visible && base.use_builtin_bar) ? base.bar.height : 0;
        int own_bar_top = 0;
        int own_bar_bottom = 0;

        if (base.bar_position == 0) {
            own_bar_top = own_bar;
        } else {
            own_bar_bottom = own_bar;
        }

        int strut_top = 0, strut_bottom = 0;
        int strut_left = 0, strut_right = 0;
        int screen_height = HEIGHT(base.display, base.screen);
        int screen_width = WIDTH(base.display, base.screen);

        if (mon.id >= 0 && mon.id < (int)base.struts.size()) {
            int raw_top = base.struts[mon.id].top;
            int raw_bottom = base.struts[mon.id].bottom;
            int raw_left = base.struts[mon.id].left;
            int raw_right = base.struts[mon.id].right;

            if (raw_top > 0 && raw_top > mon.y) {
                strut_top = std::min(raw_top - mon.y, mon.height);
            }

            if (raw_bottom > 0) {
                int strut_bottom_abs = screen_height - raw_bottom;
                if (strut_bottom_abs < mon.y + mon.height) {
                    strut_bottom = std::min((mon.y + mon.height) - strut_bottom_abs, mon.height);
                }
            }

            if (raw_left > 0 && raw_left > mon.x) {
                strut_left = std::min(raw_left - mon.x, mon.width);
            }

            if (raw_right > 0) {
                int strut_right_abs = screen_width - raw_right;
                if (strut_right_abs < mon.x + mon.width) {
                    strut_right = std::min((mon.x + mon.width) - strut_right_abs, mon.width);
                }
            }
        }

        int reserved_top = std::max(own_bar_top, strut_top);
        int reserved_bottom = std::max(own_bar_bottom, strut_bottom);

        int usable_height = mon.height - reserved_top - reserved_bottom;
        int usable_width = mon.width - strut_left - strut_right;
        int x_start = mon.x + strut_left;
        int y_start = mon.y + reserved_top;

        if (usable_height < 10) usable_height = mon.height;
        if (usable_width < 10) usable_width = mon.width;

        int scroll_visible = mon.scroll_windows_visible;
        if (scroll_visible < 1) scroll_visible = 1;

        int base_window_width = usable_width / scroll_visible;
        int window_width = current_ws.scroll_maximized ? usable_width : (int)(base_window_width * mon.master_factor);

        if (window_width < 200) window_width = 200;
        if (window_width > usable_width) window_width = usable_width;

        int num_tiled = tiled_windows.size();
        bool all_fit = (num_tiled <= scroll_visible);
        if (all_fit && !current_ws.scroll_maximized) {
            current_ws.scroll_offset = 0;
            window_width = (int)(base_window_width * mon.master_factor);
        }

        for (size_t i = 0; i < tiled_windows.size(); ++i) {
            int scroll_offset = all_fit ? 0 : current_ws.scroll_offset;
            int x_pos = x_start + i * window_width - scroll_offset + base.gaps;
            int y_pos = y_start + base.gaps;
            int win_width = window_width - 2 * base.gaps - 2 * base.border_width;
            int win_height = usable_height - 2 * base.gaps - 2 * base.border_width;

            if (win_width < 1) win_width = 1;
            if (win_height < 1) win_height = 1;

            tiled_windows[i]->x = x_pos;
            tiled_windows[i]->y = y_pos;
            tiled_windows[i]->width = win_width;
            tiled_windows[i]->height = win_height;

            XMoveResizeWindow(base.display, tiled_windows[i]->window,
                             tiled_windows[i]->x, tiled_windows[i]->y,
                             tiled_windows[i]->width, tiled_windows[i]->height);

            if (tiled_windows[i]->has_titlebar) {
                tiled_windows[i]->titlebar.width = win_width;
                nwm::titlebar_draw(tiled_windows[i], base);
            }
        }
    }

    XFlush(base.display);
}

void nwm::tile_windows(Base &base) {
    auto &current_ws = get_current_workspace(base);

    for (auto &mon : base.monitors) {
        std::vector<ManagedWindow*> tiled_windows;
        for (auto &w : current_ws.windows) {
            if (!w.is_floating && !w.is_fullscreen && w.monitor == mon.id) {
                tiled_windows.push_back(&w);
            }
        }

        if (tiled_windows.empty()) continue;

        int own_bar = (base.bar_visible && base.use_builtin_bar) ? base.bar.height : 0;
        int own_bar_top = 0;
        int own_bar_bottom = 0;

        if (base.bar_position == 0) {
            own_bar_top = own_bar;
        } else {
            own_bar_bottom = own_bar;
        }

        int strut_top = 0, strut_bottom = 0;
        int strut_left = 0, strut_right = 0;
        int screen_height = HEIGHT(base.display, base.screen);
        int screen_width = WIDTH(base.display, base.screen);

        if (mon.id >= 0 && mon.id < (int)base.struts.size()) {
            int raw_top = base.struts[mon.id].top;
            int raw_bottom = base.struts[mon.id].bottom;
            int raw_left = base.struts[mon.id].left;
            int raw_right = base.struts[mon.id].right;

            if (raw_top > 0 && raw_top > mon.y) {
                strut_top = std::min(raw_top - mon.y, mon.height);
            }

            if (raw_bottom > 0) {
                int strut_bottom_abs = screen_height - raw_bottom;
                if (strut_bottom_abs < mon.y + mon.height) {
                    strut_bottom = std::min((mon.y + mon.height) - strut_bottom_abs, mon.height);
                }
            }

            if (raw_left > 0 && raw_left > mon.x) {
                strut_left = std::min(raw_left - mon.x, mon.width);
            }

            if (raw_right > 0) {
                int strut_right_abs = screen_width - raw_right;
                if (strut_right_abs < mon.x + mon.width) {
                    strut_right = std::min((mon.x + mon.width) - strut_right_abs, mon.width);
                }
            }
        }

        int reserved_top = std::max(own_bar_top, strut_top);
        int reserved_bottom = std::max(own_bar_bottom, strut_bottom);

        int usable_height = mon.height - reserved_top - reserved_bottom;
        int usable_width = mon.width - strut_left - strut_right;
        int x_start = mon.x + strut_left;
        int y_start = mon.y + reserved_top;

        if (usable_height < 10) usable_height = mon.height;
        if (usable_width < 10) usable_width = mon.width;

        if (tiled_windows.size() == 1) {
            tiled_windows[0]->x = x_start + base.gaps;
            tiled_windows[0]->y = y_start + base.gaps;
            tiled_windows[0]->width = usable_width - 2 * base.gaps - 2 * base.border_width;
            tiled_windows[0]->height = usable_height - 2 * base.gaps - 2 * base.border_width;

            if (tiled_windows[0]->width < 1) tiled_windows[0]->width = 1;
            if (tiled_windows[0]->height < 1) tiled_windows[0]->height = 1;

            XMoveResizeWindow(base.display, tiled_windows[0]->window,
                             tiled_windows[0]->x, tiled_windows[0]->y,
                             tiled_windows[0]->width, tiled_windows[0]->height);

            if (tiled_windows[0]->has_titlebar) {
                tiled_windows[0]->titlebar.width = tiled_windows[0]->width;
                nwm::titlebar_draw(tiled_windows[0], base);
            }
        } else {
            int master_width = (int)(usable_width * mon.master_factor) - base.gaps - base.gaps / 2 - 2 * base.border_width;
            int stack_x = x_start + (int)(usable_width * mon.master_factor) + base.gaps / 2;
            int stack_width = usable_width - (int)(usable_width * mon.master_factor) - base.gaps - base.gaps / 2 - 2 * base.border_width;

            if (master_width < 1) master_width = 1;
            if (stack_width < 1) stack_width = 1;

            int stack_height = (usable_height - base.gaps * tiled_windows.size()) / (tiled_windows.size() - 1) - 2 * base.border_width;
            if (stack_height < 1) stack_height = 1;

            tiled_windows[0]->x = x_start + base.gaps;
            tiled_windows[0]->y = y_start + base.gaps;
            tiled_windows[0]->width = master_width;
            tiled_windows[0]->height = usable_height - 2 * base.gaps - 2 * base.border_width;
            if (tiled_windows[0]->height < 1) tiled_windows[0]->height = 1;

            for (size_t i = 1; i < tiled_windows.size(); ++i) {
                tiled_windows[i]->x = stack_x;
                tiled_windows[i]->y = y_start + base.gaps + (i - 1) * (stack_height + base.gaps + 2 * base.border_width);
                tiled_windows[i]->width = stack_width;
                tiled_windows[i]->height = stack_height;
            }

            for (auto *w : tiled_windows) {
                XMoveResizeWindow(base.display, w->window, w->x, w->y, w->width, w->height);

                if (w->has_titlebar) {
                    w->titlebar.width = w->width;
                    nwm::titlebar_draw(w, base);
                }
            }
        }
    }

    XFlush(base.display);
}

void nwm::resize_master(void *arg, Base &base) {
    auto &current_ws = get_current_workspace(base);
    Monitor *mon = get_current_monitor(base);
    if (!mon) return;

    int delta = (int)(long)arg;

    if (base.horizontal_mode) {
        if (current_ws.windows.empty()) return;

        int scroll_visible = mon->scroll_windows_visible;
        if (scroll_visible < 1) scroll_visible = 1;

        float delta_factor = (float)delta / (mon->width / scroll_visible);
        mon->master_factor += delta_factor;

        if (mon->master_factor < 0.3f) mon->master_factor = 0.3f;
        if (mon->master_factor > 3.0f) mon->master_factor = 3.0f;

        tile_horizontal(base);
    } else {
        if (current_ws.windows.size() < 2) return;

        float delta_factor = (float)delta / mon->width;
        mon->master_factor += delta_factor;

        if (mon->master_factor < 0.1f) mon->master_factor = 0.1f;
        if (mon->master_factor > 0.9f) mon->master_factor = 0.9f;

        tile_windows(base);
    }
}

void nwm::scroll_left(void *arg, Base &base) {
    move_horizontal(arg, base, false, false, true, false);
}

void nwm::scroll_right(void *arg, Base &base) {
    move_horizontal(arg, base, true, false, true, false);
}

void nwm::move_horizontal(void *arg, Base &base, bool forward, bool window_based, bool animate, bool change_focus) {
    (void)arg;
    if (!base.horizontal_mode) return;

    Monitor *mon = get_current_monitor(base);
    if (!mon) return;

    auto &current_ws = get_current_workspace(base);
    if (current_ws.windows.empty()) return;

    std::vector<ManagedWindow*> all_windows;
    for (auto &w : current_ws.windows) {
        all_windows.push_back(&w);
    }

    if (all_windows.empty()) return;

    int target_offset = current_ws.scroll_offset;

    if (window_based) {
        int current_idx = -1;
        for (size_t i = 0; i < all_windows.size(); ++i) {
            if (current_ws.focused_window && all_windows[i]->window == current_ws.focused_window->window) {
                current_idx = i;
                break;
            }
        }

        int target_idx;
        if (forward) {
            target_idx = (current_idx + 1) % all_windows.size();
        } else {
            target_idx = (current_idx - 1 + all_windows.size()) % all_windows.size();
        }

        if (change_focus) {
            focus_window(all_windows[target_idx], base);
        }

        if (!all_windows[target_idx]->is_floating && !all_windows[target_idx]->is_fullscreen) {
            int scroll_visible = mon->scroll_windows_visible;
            if (scroll_visible < 1) scroll_visible = 1;

            int base_window_width = mon->width / scroll_visible;
            int window_width = current_ws.scroll_maximized ? mon->width : (int)(base_window_width * mon->master_factor);

            int tiled_idx = 0;
            for (int i = 0; i <= target_idx; ++i) {
                if (!all_windows[i]->is_floating && !all_windows[i]->is_fullscreen) {
                    if (i == target_idx) break;
                    tiled_idx++;
                }
            }

            target_offset = tiled_idx * window_width;

            if (target_offset < current_ws.scroll_offset) {
                target_offset = target_offset;
            } else if (target_offset + window_width > current_ws.scroll_offset + mon->width) {
                target_offset = target_offset + window_width - mon->width;
            } else {
                target_offset = current_ws.scroll_offset;
            }
        }
    } else {
        int scroll_amount;
        if (current_ws.scroll_maximized) {
            scroll_amount = mon->width;
        } else {
            int scroll_visible = mon->scroll_windows_visible;
            if (scroll_visible < 1) scroll_visible = 1;

            int base_window_width = mon->width / scroll_visible;
            scroll_amount = (int)(base_window_width * mon->master_factor);
        }

        if (forward) {
            int scroll_visible = mon->scroll_windows_visible;
            if (scroll_visible < 1) scroll_visible = 1;

            int base_window_width = mon->width / scroll_visible;
            int window_width = current_ws.scroll_maximized ? mon->width : (int)(base_window_width * mon->master_factor);
            int total_width = current_ws.windows.size() * window_width;
            int max_scroll = std::max(0, total_width - mon->width);
            target_offset = std::min(max_scroll, current_ws.scroll_offset + scroll_amount);
        } else {
            target_offset = std::max(0, current_ws.scroll_offset - scroll_amount);
        }
    }

    if (animate && base.anim_manager && base.anim_manager->animations_enabled &&
        base.anim_manager->scroll_enabled) {
        animate_scroll(base, target_offset);
    } else {
        current_ws.scroll_offset = target_offset;
        tile_horizontal(base);
    }
}

void nwm::toggle_layout(void *arg, Base &base) {
    (void)arg;
    Monitor *mon = get_current_monitor(base);
    if (!mon) return;

    mon->horizontal_mode = !mon->horizontal_mode;
    base.horizontal_mode = mon->horizontal_mode;

    auto &current_ws = get_current_workspace(base);
    current_ws.scroll_offset = 0;

    if (mon->horizontal_mode) {
        mon->master_factor = 1.0f;
        tile_horizontal(base);
    } else {
        mon->master_factor = 0.5f;
        tile_windows(base);
    }

    bar_draw(base);
}

void nwm::swap_next(void *arg, Base &base) {
    (void)arg;
    auto &current_ws = get_current_workspace(base);
    if (current_ws.windows.size() < 2) return;

    int current_idx = -1;
    for (size_t i = 0; i < current_ws.windows.size(); ++i) {
        if (current_ws.focused_window && current_ws.windows[i].window == current_ws.focused_window->window) {
            current_idx = i;
            break;
        }
    }

    if (current_idx == -1 || current_ws.windows[current_idx].is_floating) return;

    int next_idx = (current_idx + 1) % current_ws.windows.size();
    std::swap(current_ws.windows[current_idx], current_ws.windows[next_idx]);

    if (base.horizontal_mode) {
        tile_horizontal(base);
    } else {
        tile_windows(base);
    }
    focus_window(&current_ws.windows[next_idx], base);
}

void nwm::swap_prev(void *arg, Base &base) {
    (void)arg;
    auto &current_ws = get_current_workspace(base);
    if (current_ws.windows.size() < 2) return;

    int current_idx = -1;
    for (size_t i = 0; i < current_ws.windows.size(); ++i) {
        if (current_ws.focused_window && current_ws.windows[i].window == current_ws.focused_window->window) {
            current_idx = i;
            break;
        }
    }

    if (current_idx == -1 || current_ws.windows[current_idx].is_floating) return;

    int prev_idx = (current_idx - 1 + current_ws.windows.size()) % current_ws.windows.size();
    std::swap(current_ws.windows[current_idx], current_ws.windows[prev_idx]);

    if (base.horizontal_mode) {
        tile_horizontal(base);
    } else {
        tile_windows(base);
    }
    focus_window(&current_ws.windows[prev_idx], base);
}

void nwm::increment_scroll_visible(void *arg, Base &base) {
    (void)arg;
    Monitor *mon = get_current_monitor(base);
    if (!mon) return;

    mon->scroll_windows_visible++;
    if (mon->scroll_windows_visible > 10) mon->scroll_windows_visible = 10;

    auto &current_ws = get_current_workspace(base);
    current_ws.scroll_offset = 0;

    if (mon->horizontal_mode) {
        tile_horizontal(base);
    }

    bar_draw(base);
}

void nwm::decrement_scroll_visible(void *arg, Base &base) {
    (void)arg;
    Monitor *mon = get_current_monitor(base);
    if (!mon) return;

    mon->scroll_windows_visible--;
    if (mon->scroll_windows_visible < 1) mon->scroll_windows_visible = 1;

    auto &current_ws = get_current_workspace(base);
    current_ws.scroll_offset = 0;

    if (mon->horizontal_mode) {
        tile_horizontal(base);
    }

    bar_draw(base);
}
