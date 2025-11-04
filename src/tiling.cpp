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

        int bar_height = base.bar_visible ? base.bar.height : 0;
        int usable_height = mon.height - bar_height;
        int y_start = mon.y + (base.bar_position == 0 ? bar_height : 0);

        int scroll_visible = mon.scroll_windows_visible;
        if (scroll_visible < 1) scroll_visible = 1;

        int window_width = current_ws.scroll_maximized ? mon.width : (mon.width / scroll_visible);

        int num_tiled = tiled_windows.size();
        bool all_fit = (num_tiled <= scroll_visible);
        if (all_fit && !current_ws.scroll_maximized) {
            current_ws.scroll_offset = 0;
            window_width = mon.width / num_tiled;
        }

        for (size_t i = 0; i < tiled_windows.size(); ++i) {
            int scroll_offset = all_fit ? 0 : current_ws.scroll_offset;
            int x_pos = mon.x + i * window_width - scroll_offset + base.gaps;
            int y_pos = y_start + base.gaps;
            int win_width = window_width - 2 * base.gaps - 2 * base.border_width;
            int win_height = usable_height - 2 * base.gaps - 2 * base.border_width;

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

        int bar_height = base.bar_visible && base.use_builtin_bar ? base.bar.height : 0;
        int usable_height = mon.height - bar_height;
        int y_start = mon.y + (base.bar_position == 0 ? bar_height : 0);

        if (tiled_windows.size() == 1) {
            tiled_windows[0]->x = mon.x + base.gaps;
            tiled_windows[0]->y = y_start + base.gaps;
            tiled_windows[0]->width = mon.width - 2 * base.gaps - 2 * base.border_width;
            tiled_windows[0]->height = usable_height - 2 * base.gaps - 2 * base.border_width;

            XMoveResizeWindow(base.display, tiled_windows[0]->window,
                             tiled_windows[0]->x, tiled_windows[0]->y,
                             tiled_windows[0]->width, tiled_windows[0]->height);

            if (tiled_windows[0]->has_titlebar) {
                tiled_windows[0]->titlebar.width = tiled_windows[0]->width;
                nwm::titlebar_draw(tiled_windows[0], base);
            }
        } else {
            int master_width = (int)(mon.width * mon.master_factor) - base.gaps - base.gaps / 2 - 2 * base.border_width;
            int stack_x = mon.x + (int)(mon.width * mon.master_factor) + base.gaps / 2;
            int stack_width = mon.width - (int)(mon.width * mon.master_factor) - base.gaps - base.gaps / 2 - 2 * base.border_width;

            int stack_height = (usable_height - base.gaps * tiled_windows.size()) / (tiled_windows.size() - 1) - 2 * base.border_width;

            tiled_windows[0]->x = mon.x + base.gaps;
            tiled_windows[0]->y = y_start + base.gaps;
            tiled_windows[0]->width = master_width;
            tiled_windows[0]->height = usable_height - 2 * base.gaps - 2 * base.border_width;

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
    if (current_ws.windows.size() < 2 || base.horizontal_mode) return;

    Monitor *mon = get_current_monitor(base);
    if (!mon) return;

    int delta = (int)(long)arg;
    float delta_factor = (float)delta / mon->width;
    mon->master_factor += delta_factor;

    if (mon->master_factor < 0.1f) mon->master_factor = 0.1f;
    if (mon->master_factor > 0.9f) mon->master_factor = 0.9f;

    tile_windows(base);
}

void nwm::scroll_left(void *arg, Base &base) {
    (void)arg;
    if (!base.horizontal_mode) return;

    Monitor *mon = get_current_monitor(base);
    if (!mon) return;

    auto &current_ws = get_current_workspace(base);

    int scroll_amount;
    if (current_ws.scroll_maximized) {
        scroll_amount = mon->width;
    } else {
        int scroll_visible = mon->scroll_windows_visible;
        if (scroll_visible < 1) scroll_visible = 1;
        scroll_amount = mon->width / scroll_visible;
    }

    int target_offset = std::max(0, current_ws.scroll_offset - scroll_amount);

    if (base.anim_manager && base.anim_manager->animations_enabled &&
        base.anim_manager->scroll_enabled) {
        animate_scroll(base, target_offset);
    } else {
        current_ws.scroll_offset = target_offset;
        tile_horizontal(base);
    }
}

void nwm::scroll_right(void *arg, Base &base) {
    (void)arg;
    if (!base.horizontal_mode) return;

    Monitor *mon = get_current_monitor(base);
    if (!mon) return;

    auto &current_ws = get_current_workspace(base);

    int scroll_visible = mon->scroll_windows_visible;
    if (scroll_visible < 1) scroll_visible = 1;

    int window_width = current_ws.scroll_maximized ? mon->width : (mon->width / scroll_visible);
    int scroll_amount = current_ws.scroll_maximized ? mon->width : window_width;

    int total_width = current_ws.windows.size() * window_width;
    int max_scroll = std::max(0, total_width - mon->width);

    int target_offset = std::min(max_scroll, current_ws.scroll_offset + scroll_amount);
    if (base.anim_manager && base.anim_manager->animations_enabled &&
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
        tile_horizontal(base);
    } else {
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
