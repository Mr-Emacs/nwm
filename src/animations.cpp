#include "animations.hpp"
#include "tiling.hpp"
#include "config.hpp"
#include "nwm.hpp"
#include <cmath>
#include <algorithm>
#include <X11/Xatom.h>

float nwm::ease_linear(float t) {
    return t;
}

float nwm::ease_in_quad(float t) {
    return t * t;
}

float nwm::ease_out_quad(float t) {
    return t * (2.0f - t);
}

float nwm::ease_in_out_quad(float t) {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

float nwm::ease_in_cubic(float t) {
    return t * t * t;
}

float nwm::ease_out_cubic(float t) {
    float f = t - 1.0f;
    return f * f * f + 1.0f;
}

float nwm::ease_in_out_cubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
}

float nwm::ease_out_elastic(float t) {
    if (t == 0.0f || t == 1.0f) return t;

    float p = 0.3f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t - p / 4.0f) * (2.0f * M_PI) / p) + 1.0f;
}

float nwm::ease_out_bounce(float t) {
    if (t < (1.0f / 2.75f)) {
        return 7.5625f * t * t;
    } else if (t < (2.0f / 2.75f)) {
        t -= (1.5f / 2.75f);
        return 7.5625f * t * t + 0.75f;
    } else if (t < (2.5f / 2.75f)) {
        t -= (2.25f / 2.75f);
        return 7.5625f * t * t + 0.9375f;
    } else {
        t -= (2.625f / 2.75f);
        return 7.5625f * t * t + 0.984375f;
    }
}

float nwm::apply_easing(float t, EasingType type) {
    switch (type) {
        case EasingType::LINEAR:
            return ease_linear(t);
        case EasingType::EASE_IN_QUAD:
            return ease_in_quad(t);
        case EasingType::EASE_OUT_QUAD:
            return ease_out_quad(t);
        case EasingType::EASE_IN_OUT_QUAD:
            return ease_in_out_quad(t);
        case EasingType::EASE_IN_CUBIC:
            return ease_in_cubic(t);
        case EasingType::EASE_OUT_CUBIC:
            return ease_out_cubic(t);
        case EasingType::EASE_IN_OUT_CUBIC:
            return ease_in_out_cubic(t);
        case EasingType::EASE_OUT_ELASTIC:
            return ease_out_elastic(t);
        case EasingType::EASE_OUT_BOUNCE:
            return ease_out_bounce(t);
        default:
            return ease_linear(t);
    }
}

void nwm::ScrollAnimation::update(Base &base, float progress) {
    float eased = apply_easing(progress, easing);
    int current_offset = start_offset + (int)((target_offset - start_offset) * eased);

    auto &ws = base.workspaces[workspace_index];
    ws.scroll_offset = current_offset;

    if (base.horizontal_mode) {
        tile_horizontal(base);
    }
}

void nwm::WindowMoveAnimation::update(Base &base, float progress) {
    float eased = apply_easing(progress, easing);

    int current_x = start_x + (int)((target_x - start_x) * eased);
    int current_y = start_y + (int)((target_y - start_y) * eased);

    for (auto &ws : base.workspaces) {
        for (auto &w : ws.windows) {
            if (w.window == window) {
                w.x = current_x;
                w.y = current_y;
                XMoveWindow(base.display, window, current_x, current_y);
                XFlush(base.display);
                return;
            }
        }
    }
}

void nwm::WindowResizeAnimation::update(Base &base, float progress) {
    float eased = apply_easing(progress, easing);

    int current_width = start_width + (int)((target_width - start_width) * eased);
    int current_height = start_height + (int)((target_height - start_height) * eased);

    for (auto &ws : base.workspaces) {
        for (auto &w : ws.windows) {
            if (w.window == window) {
                w.width = current_width;
                w.height = current_height;
                XResizeWindow(base.display, window, current_width, current_height);
                XFlush(base.display);
                return;
            }
        }
    }
}

void nwm::WindowOpacityAnimation::update(Base &base, float progress) {
    float eased = apply_easing(progress, easing);
    float current_opacity = start_opacity + (target_opacity - start_opacity) * eased;

    Atom opacity_atom = XInternAtom(base.display, "_NET_WM_WINDOW_OPACITY", False);
    unsigned long opacity_value = (unsigned long)(current_opacity * 0xFFFFFFFF);

    XChangeProperty(base.display, window, opacity_atom, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char*)&opacity_value, 1);
    XFlush(base.display);
}

void nwm::MasterFactorAnimation::update(Base &base, float progress) {
    float eased = apply_easing(progress, easing);
    float current_factor = start_factor + (target_factor - start_factor) * eased;

    Monitor *mon = get_current_monitor(base);
    if (mon) {
        mon->master_factor = current_factor;

        if (!base.horizontal_mode) {
            tile_windows(base);
        }
    }
}

void nwm::WindowOpenAnimation::update(Base &base, float progress) {
    float eased = apply_easing(progress, easing);

    ManagedWindow *managed_win = nullptr;
    for (auto &ws : base.workspaces) {
        for (auto &w : ws.windows) {
            if (w.window == window) {
                managed_win = &w;
                break;
            }
        }
        if (managed_win) break;
    }

    if (!managed_win) return;

    switch (base.anim_manager->window_open_style) {
        case AnimationManager::FADE: {
            float current_opacity = start_opacity + (target_opacity - start_opacity) * eased;
            Atom opacity_atom = XInternAtom(base.display, "_NET_WM_WINDOW_OPACITY", False);
            unsigned long opacity_value = (unsigned long)(current_opacity * 0xFFFFFFFF);
            XChangeProperty(base.display, window, opacity_atom, XA_CARDINAL, 32,
                           PropModeReplace, (unsigned char*)&opacity_value, 1);
            break;
        }
        case AnimationManager::SCALE: {
            float current_scale = start_scale + (target_scale - start_scale) * eased;
            float current_opacity = start_opacity + (target_opacity - start_opacity) * eased;

            int center_x = managed_win->x + managed_win->width / 2;
            int center_y = managed_win->y + managed_win->height / 2;

            int scaled_width = (int)(managed_win->width * current_scale);
            int scaled_height = (int)(managed_win->height * current_scale);
            int scaled_x = center_x - scaled_width / 2;
            int scaled_y = center_y - scaled_height / 2;

            XMoveResizeWindow(base.display, window, scaled_x, scaled_y, scaled_width, scaled_height);

            Atom opacity_atom = XInternAtom(base.display, "_NET_WM_WINDOW_OPACITY", False);
            unsigned long opacity_value = (unsigned long)(current_opacity * 0xFFFFFFFF);
            XChangeProperty(base.display, window, opacity_atom, XA_CARDINAL, 32,
                           PropModeReplace, (unsigned char*)&opacity_value, 1);
            break;
        }
        case AnimationManager::SLIDE_FROM_TOP:
        case AnimationManager::SLIDE_FROM_BOTTOM:
        case AnimationManager::SLIDE_FROM_LEFT:
        case AnimationManager::SLIDE_FROM_RIGHT: {
            break;
        }
    }

    XFlush(base.display);
}

void nwm::WindowCloseAnimation::update(Base &base, float progress) {
    float eased = apply_easing(progress, easing);

    switch (base.anim_manager->window_close_style) {
        case AnimationManager::FADE_OUT: {
            float current_opacity = start_opacity - (start_opacity - target_opacity) * eased;
            Atom opacity_atom = XInternAtom(base.display, "_NET_WM_WINDOW_OPACITY", False);
            unsigned long opacity_value = (unsigned long)(current_opacity * 0xFFFFFFFF);
            XChangeProperty(base.display, window, opacity_atom, XA_CARDINAL, 32,
                           PropModeReplace, (unsigned char*)&opacity_value, 1);
            break;
        }
        case AnimationManager::SCALE_DOWN: {
            float current_scale = start_scale - (start_scale - target_scale) * eased;
            float current_opacity = start_opacity - (start_opacity - target_opacity) * eased;

            int center_x = original_x + original_width / 2;
            int center_y = original_y + original_height / 2;

            int scaled_width = (int)(original_width * current_scale);
            int scaled_height = (int)(original_height * current_scale);
            int scaled_x = center_x - scaled_width / 2;
            int scaled_y = center_y - scaled_height / 2;

            XMoveResizeWindow(base.display, window, scaled_x, scaled_y,
                            std::max(1, scaled_width), std::max(1, scaled_height));

            Atom opacity_atom = XInternAtom(base.display, "_NET_WM_WINDOW_OPACITY", False);
            unsigned long opacity_value = (unsigned long)(current_opacity * 0xFFFFFFFF);
            XChangeProperty(base.display, window, opacity_atom, XA_CARDINAL, 32,
                           PropModeReplace, (unsigned char*)&opacity_value, 1);
            break;
        }
        case AnimationManager::SLIDE_TO_TOP:
        case AnimationManager::SLIDE_TO_BOTTOM:
        case AnimationManager::SLIDE_TO_LEFT:
        case AnimationManager::SLIDE_TO_RIGHT: {
            float current_opacity = start_opacity - (start_opacity - target_opacity) * eased;
            Atom opacity_atom = XInternAtom(base.display, "_NET_WM_WINDOW_OPACITY", False);
            unsigned long opacity_value = (unsigned long)(current_opacity * 0xFFFFFFFF);
            XChangeProperty(base.display, window, opacity_atom, XA_CARDINAL, 32,
                           PropModeReplace, (unsigned char*)&opacity_value, 1);
            break;
        }
    }

    XFlush(base.display);
}

void nwm::WorkspaceSwitchAnimation::update(Base &base, float progress) {
    (void)base;
    (void)progress;
}

void nwm::BorderColorAnimation::update(Base &base, float progress) {
    float eased = apply_easing(progress, easing);

    unsigned long start_r = (start_color >> 16) & 0xFF;
    unsigned long start_g = (start_color >> 8) & 0xFF;
    unsigned long start_b = start_color & 0xFF;

    unsigned long target_r = (target_color >> 16) & 0xFF;
    unsigned long target_g = (target_color >> 8) & 0xFF;
    unsigned long target_b = target_color & 0xFF;

    unsigned long current_r = start_r + (unsigned long)((target_r - start_r) * eased);
    unsigned long current_g = start_g + (unsigned long)((target_g - start_g) * eased);
    unsigned long current_b = start_b + (unsigned long)((target_b - start_b) * eased);

    unsigned long current_color = (current_r << 16) | (current_g << 8) | current_b;

    XSetWindowBorder(base.display, window, current_color);
    XFlush(base.display);
}

void nwm::animations_init(Base &base) {
    base.anim_manager = new AnimationManager();

    base.anim_manager->animations_enabled = ANIMATIONS_ENABLED;

    base.anim_manager->scroll_enabled = ANIM_SCROLL_ENABLED;
    base.anim_manager->window_move_enabled = ANIM_WINDOW_MOVE_ENABLED;
    base.anim_manager->window_resize_enabled = ANIM_WINDOW_RESIZE_ENABLED;
    base.anim_manager->opacity_enabled = ANIM_OPACITY_ENABLED;
    base.anim_manager->master_factor_enabled = ANIM_MASTER_FACTOR_ENABLED;
    base.anim_manager->window_open_enabled = ANIM_WINDOW_OPEN_ENABLED;
    base.anim_manager->window_close_enabled = ANIM_WINDOW_CLOSE_ENABLED;
    base.anim_manager->workspace_switch_enabled = ANIM_WORKSPACE_SWITCH_ENABLED;
    base.anim_manager->border_color_enabled = ANIM_BORDER_COLOR_ENABLED;

    base.anim_manager->scroll_duration = ANIM_SCROLL_DURATION;
    base.anim_manager->window_move_duration = ANIM_WINDOW_MOVE_DURATION;
    base.anim_manager->window_resize_duration = ANIM_WINDOW_RESIZE_DURATION;
    base.anim_manager->opacity_duration = ANIM_OPACITY_DURATION;
    base.anim_manager->master_factor_duration = ANIM_MASTER_FACTOR_DURATION;
    base.anim_manager->window_open_duration = ANIM_WINDOW_OPEN_DURATION;
    base.anim_manager->window_close_duration = ANIM_WINDOW_CLOSE_DURATION;
    base.anim_manager->workspace_switch_duration = ANIM_WORKSPACE_SWITCH_DURATION;
    base.anim_manager->border_color_duration = ANIM_BORDER_COLOR_DURATION;

    base.anim_manager->scroll_easing = ANIM_SCROLL_EASING;
    base.anim_manager->window_move_easing = ANIM_WINDOW_MOVE_EASING;
    base.anim_manager->window_resize_easing = ANIM_WINDOW_RESIZE_EASING;
    base.anim_manager->opacity_easing = ANIM_OPACITY_EASING;
    base.anim_manager->master_factor_easing = ANIM_MASTER_FACTOR_EASING;
    base.anim_manager->window_open_easing = ANIM_WINDOW_OPEN_EASING;
    base.anim_manager->window_close_easing = ANIM_WINDOW_CLOSE_EASING;
    base.anim_manager->workspace_switch_easing = ANIM_WORKSPACE_SWITCH_EASING;
    base.anim_manager->border_color_easing = ANIM_BORDER_COLOR_EASING;

    base.anim_manager->window_open_style = WINDOW_OPEN_STYLE;
    base.anim_manager->window_close_style = WINDOW_CLOSE_STYLE;
}

void nwm::animations_cleanup(Base &base) {
    if (!base.anim_manager) return;

    for (auto *anim : base.anim_manager->animations) {
        delete anim;
    }
    base.anim_manager->animations.clear();

    delete base.anim_manager;
    base.anim_manager = nullptr;
}

void nwm::animations_update(Base &base) {
    if (!base.anim_manager) return;
    if (!base.anim_manager->animations_enabled) return;
    if (base.anim_manager->animations.empty()) return;

    auto now = std::chrono::steady_clock::now();
    std::vector<Animation*> to_remove;

    for (auto *anim : base.anim_manager->animations) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - anim->start_time).count();

        float progress = std::min(1.0f, (float)elapsed / anim->duration_ms);

        anim->update(base, progress);

        if (progress >= 1.0f) {
            anim->completed = true;
            if (anim->on_complete) {
                anim->on_complete();
            }
            to_remove.push_back(anim);
        }
    }

    for (auto *anim : to_remove) {
        base.anim_manager->animations.erase(
            std::remove(base.anim_manager->animations.begin(),
                       base.anim_manager->animations.end(),
                       anim),
            base.anim_manager->animations.end()
        );
        delete anim;
    }
}

void nwm::animate_scroll(Base &base, int target_offset) {
    if (!base.anim_manager || !base.anim_manager->animations_enabled ||
        !base.anim_manager->scroll_enabled) {
        auto &ws = get_current_workspace(base);
        ws.scroll_offset = target_offset;
        tile_horizontal(base);
        return;
    }

    animate_scroll_smooth(base, target_offset,
                         base.anim_manager->scroll_duration,
                         base.anim_manager->scroll_easing);
}

void nwm::animate_scroll_smooth(Base &base, int target_offset, int duration_ms, EasingType easing) {
    if (!base.anim_manager) return;

    auto &current_ws = get_current_workspace(base);

    for (auto it = base.anim_manager->animations.begin();
         it != base.anim_manager->animations.end();) {
        if ((*it)->type == AnimationType::SCROLL_OFFSET &&
            (*it)->workspace_index == (int)base.current_workspace) {
            delete *it;
            it = base.anim_manager->animations.erase(it);
            continue;
        }
        ++it;
    }

    ScrollAnimation *anim = new ScrollAnimation();
    anim->type = AnimationType::SCROLL_OFFSET;
    anim->start_time = std::chrono::steady_clock::now();
    anim->duration_ms = duration_ms;
    anim->easing = easing;
    anim->completed = false;
    anim->workspace_index = base.current_workspace;
    anim->monitor_index = base.current_monitor;
    anim->start_offset = current_ws.scroll_offset;
    anim->target_offset = target_offset;

    base.anim_manager->animations.push_back(anim);
}

void nwm::FloatingTransitionAnimation::update(Base &base, float progress) {
    float eased = apply_easing(progress, easing);

    int current_x = start_x + (int)((target_x - start_x) * eased);
    int current_y = start_y + (int)((target_y - start_y) * eased);
    int current_width = start_width + (int)((target_width - start_width) * eased);
    int current_height = start_height + (int)((target_height - start_height) * eased);

    for (auto &ws : base.workspaces) {
        for (auto &w : ws.windows) {
            if (w.window == window) {
                w.x = current_x;
                w.y = current_y;
                w.width = current_width;
                w.height = current_height;
                XMoveResizeWindow(base.display, window, current_x, current_y,
                                current_width, current_height);
                XFlush(base.display);
                return;
            }
        }
    }
}

void nwm::animate_floating_transition(Base &base, Window window, bool to_floating,
                                      int start_x, int start_y, int start_width, int start_height,
                                      int target_x, int target_y, int target_width, int target_height) {
    if (!base.anim_manager || !base.anim_manager->animations_enabled ||
        !base.anim_manager->window_move_enabled || !base.anim_manager->window_resize_enabled) {
        XMoveResizeWindow(base.display, window, target_x, target_y, target_width, target_height);
        return;
    }

    for (auto it = base.anim_manager->animations.begin();
         it != base.anim_manager->animations.end();) {
        if (((*it)->type == AnimationType::FLOATING_TRANSITION ||
             (*it)->type == AnimationType::WINDOW_MOVE ||
             (*it)->type == AnimationType::WINDOW_RESIZE)) {
            bool should_remove = false;
            if ((*it)->type == AnimationType::FLOATING_TRANSITION) {
                FloatingTransitionAnimation *anim = static_cast<FloatingTransitionAnimation*>(*it);
                if (anim->window == window) should_remove = true;
            } else if ((*it)->type == AnimationType::WINDOW_MOVE) {
                WindowMoveAnimation *anim = static_cast<WindowMoveAnimation*>(*it);
                if (anim->window == window) should_remove = true;
            } else if ((*it)->type == AnimationType::WINDOW_RESIZE) {
                WindowResizeAnimation *anim = static_cast<WindowResizeAnimation*>(*it);
                if (anim->window == window) should_remove = true;
            }

            if (should_remove) {
                delete *it;
                it = base.anim_manager->animations.erase(it);
                continue;
            }
        }
        ++it;
    }

    FloatingTransitionAnimation *anim = new FloatingTransitionAnimation();
    anim->type = AnimationType::FLOATING_TRANSITION;
    anim->start_time = std::chrono::steady_clock::now();
    anim->duration_ms = base.anim_manager->window_move_duration;
    anim->easing = base.anim_manager->window_move_easing;
    anim->completed = false;
    anim->workspace_index = base.current_workspace;
    anim->monitor_index = base.current_monitor;
    anim->window = window;
    anim->start_x = start_x;
    anim->start_y = start_y;
    anim->start_width = start_width;
    anim->start_height = start_height;
    anim->target_x = target_x;
    anim->target_y = target_y;
    anim->target_width = target_width;
    anim->target_height = target_height;
    anim->to_floating = to_floating;

    base.anim_manager->animations.push_back(anim);
}


void nwm::animate_window_move(Base &base, Window window, int target_x, int target_y) {
    if (!base.anim_manager || !base.anim_manager->animations_enabled ||
        !base.anim_manager->window_move_enabled) {
        XMoveWindow(base.display, window, target_x, target_y);
        return;
    }

    XWindowAttributes attr;
    if (!XGetWindowAttributes(base.display, window, &attr)) {
        return;
    }

    for (auto it = base.anim_manager->animations.begin();
         it != base.anim_manager->animations.end();) {
        if ((*it)->type == AnimationType::WINDOW_MOVE) {
            WindowMoveAnimation *move_anim = static_cast<WindowMoveAnimation*>(*it);
            if (move_anim->window == window) {
                delete *it;
                it = base.anim_manager->animations.erase(it);
                continue;
            }
        }
        ++it;
    }

    WindowMoveAnimation *anim = new WindowMoveAnimation();
    anim->type = AnimationType::WINDOW_MOVE;
    anim->start_time = std::chrono::steady_clock::now();
    anim->duration_ms = base.anim_manager->window_move_duration;
    anim->easing = base.anim_manager->window_move_easing;
    anim->completed = false;
    anim->workspace_index = base.current_workspace;
    anim->monitor_index = base.current_monitor;
    anim->window = window;
    anim->start_x = attr.x;
    anim->start_y = attr.y;
    anim->target_x = target_x;
    anim->target_y = target_y;

    base.anim_manager->animations.push_back(anim);
}

void nwm::animate_window_resize(Base &base, Window window, int target_width, int target_height) {
    if (!base.anim_manager || !base.anim_manager->animations_enabled ||
        !base.anim_manager->window_resize_enabled) {
        XResizeWindow(base.display, window, target_width, target_height);
        return;
    }

    XWindowAttributes attr;
    if (!XGetWindowAttributes(base.display, window, &attr)) {
        return;
    }

    for (auto it = base.anim_manager->animations.begin();
         it != base.anim_manager->animations.end();) {
        if ((*it)->type == AnimationType::WINDOW_RESIZE) {
            WindowResizeAnimation *resize_anim = static_cast<WindowResizeAnimation*>(*it);
            if (resize_anim->window == window) {
                delete *it;
                it = base.anim_manager->animations.erase(it);
                continue;
            }
        }
        ++it;
    }

    WindowResizeAnimation *anim = new WindowResizeAnimation();
    anim->type = AnimationType::WINDOW_RESIZE;
    anim->start_time = std::chrono::steady_clock::now();
    anim->duration_ms = base.anim_manager->window_resize_duration;
    anim->easing = base.anim_manager->window_resize_easing;
    anim->completed = false;
    anim->workspace_index = base.current_workspace;
    anim->monitor_index = base.current_monitor;
    anim->window = window;
    anim->start_width = attr.width;
    anim->start_height = attr.height;
    anim->target_width = target_width;
    anim->target_height = target_height;

    base.anim_manager->animations.push_back(anim);
}

void nwm::animate_window_opacity(Base &base, Window window, float target_opacity) {
    if (!base.anim_manager || !base.anim_manager->animations_enabled ||
        !base.anim_manager->opacity_enabled) {
        Atom opacity_atom = XInternAtom(base.display, "_NET_WM_WINDOW_OPACITY", False);
        unsigned long opacity_value = (unsigned long)(target_opacity * 0xFFFFFFFF);
        XChangeProperty(base.display, window, opacity_atom, XA_CARDINAL, 32,
                       PropModeReplace, (unsigned char*)&opacity_value, 1);
        return;
    }

    Atom opacity_atom = XInternAtom(base.display, "_NET_WM_WINDOW_OPACITY", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = nullptr;

    float current_opacity = 1.0f;
    if (XGetWindowProperty(base.display, window, opacity_atom, 0, 1,
                          False, XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        unsigned long opacity_value = *(unsigned long*)prop;
        current_opacity = (float)opacity_value / 0xFFFFFFFF;
        XFree(prop);
    }

    for (auto it = base.anim_manager->animations.begin();
         it != base.anim_manager->animations.end();) {
        if ((*it)->type == AnimationType::WINDOW_OPACITY) {
            WindowOpacityAnimation *opacity_anim = static_cast<WindowOpacityAnimation*>(*it);
            if (opacity_anim->window == window) {
                delete *it;
                it = base.anim_manager->animations.erase(it);
                continue;
            }
        }
        ++it;
    }

    WindowOpacityAnimation *anim = new WindowOpacityAnimation();
    anim->type = AnimationType::WINDOW_OPACITY;
    anim->start_time = std::chrono::steady_clock::now();
    anim->duration_ms = base.anim_manager->opacity_duration;
    anim->easing = base.anim_manager->opacity_easing;
    anim->completed = false;
    anim->workspace_index = base.current_workspace;
    anim->monitor_index = base.current_monitor;
    anim->window = window;
    anim->start_opacity = current_opacity;
    anim->target_opacity = target_opacity;

    base.anim_manager->animations.push_back(anim);
}

void nwm::animate_master_factor(Base &base, float target_factor) {
    Monitor *mon = get_current_monitor(base);
    if (!mon) return;

    if (!base.anim_manager || !base.anim_manager->animations_enabled ||
        !base.anim_manager->master_factor_enabled) {
        mon->master_factor = target_factor;
        tile_windows(base);
        return;
    }

    for (auto it = base.anim_manager->animations.begin();
         it != base.anim_manager->animations.end();) {
        if ((*it)->type == AnimationType::MASTER_FACTOR &&
            (*it)->monitor_index == base.current_monitor) {
            delete *it;
            it = base.anim_manager->animations.erase(it);
            continue;
        }
        ++it;
    }

    MasterFactorAnimation *anim = new MasterFactorAnimation();
    anim->type = AnimationType::MASTER_FACTOR;
    anim->start_time = std::chrono::steady_clock::now();
    anim->duration_ms = base.anim_manager->master_factor_duration;
    anim->easing = base.anim_manager->master_factor_easing;
    anim->completed = false;
    anim->workspace_index = base.current_workspace;
    anim->monitor_index = base.current_monitor;
    anim->start_factor = mon->master_factor;
    anim->target_factor = target_factor;

    base.anim_manager->animations.push_back(anim);
}

void nwm::animate_window_open(Base &base, Window window) {
    if (!base.anim_manager || !base.anim_manager->animations_enabled ||
        !base.anim_manager->window_open_enabled) {
        return;
    }

    WindowOpenAnimation *anim = new WindowOpenAnimation();
    anim->type = AnimationType::WINDOW_OPEN;
    anim->start_time = std::chrono::steady_clock::now();
    anim->duration_ms = base.anim_manager->window_open_duration;
    anim->easing = base.anim_manager->window_open_easing;
    anim->completed = false;
    anim->workspace_index = base.current_workspace;
    anim->monitor_index = base.current_monitor;
    anim->window = window;

    switch (base.anim_manager->window_open_style) {
        case AnimationManager::FADE:
            anim->start_opacity = 0.0f;
            anim->target_opacity = 1.0f;
            anim->start_scale = 1.0f;
            anim->target_scale = 1.0f;
            break;
        case AnimationManager::SCALE:
            anim->start_scale = 0.8f;
            anim->target_scale = 1.0f;
            anim->start_opacity = 0.0f;
            anim->target_opacity = 1.0f;
            break;
        default:
            anim->start_scale = 1.0f;
            anim->target_scale = 1.0f;
            anim->start_opacity = 0.0f;
            anim->target_opacity = 1.0f;
            break;
    }

    base.anim_manager->animations.push_back(anim);
}

void nwm::animate_window_close(Base &base, Window window, std::function<void()> on_complete) {
    if (!base.anim_manager || !base.anim_manager->animations_enabled ||
        !base.anim_manager->window_close_enabled) {
        if (on_complete) on_complete();
        return;
    }

    XWindowAttributes attr;
    if (!XGetWindowAttributes(base.display, window, &attr)) {
        if (on_complete) on_complete();
        return;
    }

    WindowCloseAnimation *anim = new WindowCloseAnimation();
    anim->type = AnimationType::WINDOW_CLOSE;
    anim->start_time = std::chrono::steady_clock::now();
    anim->duration_ms = base.anim_manager->window_close_duration;
    anim->easing = base.anim_manager->window_close_easing;
    anim->completed = false;
    anim->workspace_index = base.current_workspace;
    anim->monitor_index = base.current_monitor;
    anim->window = window;
    anim->original_x = attr.x;
    anim->original_y = attr.y;
    anim->original_width = attr.width;
    anim->original_height = attr.height;
    anim->on_complete = on_complete;

    switch (base.anim_manager->window_close_style) {
        case AnimationManager::FADE_OUT:
            anim->start_opacity = 1.0f;
            anim->target_opacity = 0.0f;
            anim->start_scale = 1.0f;
            anim->target_scale = 1.0f;
            break;
        case AnimationManager::SCALE_DOWN:
            anim->start_scale = 1.0f;
            anim->target_scale = 0.8f;
            anim->start_opacity = 1.0f;
            anim->target_opacity = 0.0f;
            break;
        default:
            anim->start_scale = 1.0f;
            anim->target_scale = 1.0f;
            anim->start_opacity = 1.0f;
            anim->target_opacity = 0.0f;
            break;
    }

    base.anim_manager->animations.push_back(anim);
}

void nwm::animate_workspace_switch(Base &base, int from_workspace, int to_workspace) {
    if (!base.anim_manager || !base.anim_manager->animations_enabled ||
        !base.anim_manager->workspace_switch_enabled) {
        return;
    }

    WorkspaceSwitchAnimation *anim = new WorkspaceSwitchAnimation();
    anim->type = AnimationType::WORKSPACE_SWITCH;
    anim->start_time = std::chrono::steady_clock::now();
    anim->duration_ms = base.anim_manager->workspace_switch_duration;
    anim->easing = base.anim_manager->workspace_switch_easing;
    anim->completed = false;
    anim->workspace_index = base.current_workspace;
    anim->monitor_index = base.current_monitor;
    anim->from_workspace = from_workspace;
    anim->to_workspace = to_workspace;
    anim->slide_progress = 0.0f;

    base.anim_manager->animations.push_back(anim);
}

void nwm::animate_border_color(Base &base, Window window, unsigned long target_color) {
    if (!base.anim_manager || !base.anim_manager->animations_enabled ||
        !base.anim_manager->border_color_enabled) {
        XSetWindowBorder(base.display, window, target_color);
        return;
    }

    XWindowAttributes attr;
    if (!XGetWindowAttributes(base.display, window, &attr)) {
        return;
    }

    for (auto it = base.anim_manager->animations.begin();
         it != base.anim_manager->animations.end();) {
        if ((*it)->type == AnimationType::BORDER_COLOR_CHANGE) {
            BorderColorAnimation *border_anim = static_cast<BorderColorAnimation*>(*it);
            if (border_anim->window == window) {
                delete *it;
                it = base.anim_manager->animations.erase(it);
                continue;
            }
        }
        ++it;
    }

    BorderColorAnimation *anim = new BorderColorAnimation();
    anim->type = AnimationType::BORDER_COLOR_CHANGE;
    anim->start_time = std::chrono::steady_clock::now();
    anim->duration_ms = base.anim_manager->border_color_duration;
    anim->easing = base.anim_manager->border_color_easing;
    anim->completed = false;
    anim->workspace_index = base.current_workspace;
    anim->monitor_index = base.current_monitor;
    anim->window = window;

    bool is_focused = false;
    for (auto &ws : base.workspaces) {
        for (auto &w : ws.windows) {
            if (w.window == window && w.is_focused) {
                is_focused = true;
                break;
            }
        }
        if (is_focused) break;
    }

    anim->start_color = is_focused ? base.focus_color : base.border_color;
    anim->target_color = target_color;

    base.anim_manager->animations.push_back(anim);
}

void nwm::cancel_animations_for_window(Base &base, Window window) {
    if (!base.anim_manager) return;
    for (auto it = base.anim_manager->animations.begin();
         it != base.anim_manager->animations.end();) {
        bool should_remove = false;

        if ((*it)->type == AnimationType::WINDOW_MOVE) {
            WindowMoveAnimation *anim = static_cast<WindowMoveAnimation*>(*it);
            if (anim->window == window) should_remove = true;
        } else if ((*it)->type == AnimationType::WINDOW_RESIZE) {
            WindowResizeAnimation *anim = static_cast<WindowResizeAnimation*>(*it);
            if (anim->window == window) should_remove = true;
        } else if ((*it)->type == AnimationType::WINDOW_OPACITY) {
            WindowOpacityAnimation *anim = static_cast<WindowOpacityAnimation*>(*it);
            if (anim->window == window) should_remove = true;
        } else if ((*it)->type == AnimationType::WINDOW_OPEN) {
            WindowOpenAnimation *anim = static_cast<WindowOpenAnimation*>(*it);
            if (anim->window == window) should_remove = true;
        } else if ((*it)->type == AnimationType::WINDOW_CLOSE) {
            WindowCloseAnimation *anim = static_cast<WindowCloseAnimation*>(*it);
            if (anim->window == window) should_remove = true;
        } else if ((*it)->type == AnimationType::BORDER_COLOR_CHANGE) {
            BorderColorAnimation *anim = static_cast<BorderColorAnimation*>(*it);
            if (anim->window == window) should_remove = true;
        }

        if (should_remove) {
            delete *it;
            it = base.anim_manager->animations.erase(it);
        } else {
            ++it;
        }
    }
}

void nwm::cancel_animations_for_workspace(Base &base, int workspace_index) {
    if (!base.anim_manager) return;
    for (auto it = base.anim_manager->animations.begin();
         it != base.anim_manager->animations.end();) {
        if ((*it)->workspace_index == workspace_index &&
            (*it)->type == AnimationType::SCROLL_OFFSET) {
            delete *it;
            it = base.anim_manager->animations.erase(it);
        } else {
            ++it;
        }
    }
}

void nwm::cancel_all_animations(Base &base) {
    if (!base.anim_manager) return;
    for (auto *anim : base.anim_manager->animations) {
        delete anim;
    }
    base.anim_manager->animations.clear();
}

bool nwm::is_animating(Base &base) {
    if (!base.anim_manager) return false;
    return !base.anim_manager->animations.empty();
}

bool nwm::is_window_animating(Base &base, Window window) {
    if (!base.anim_manager) return false;
    for (auto *anim : base.anim_manager->animations) {
        if (anim->type == AnimationType::WINDOW_MOVE) {
            WindowMoveAnimation *move_anim = static_cast<WindowMoveAnimation*>(anim);
            if (move_anim->window == window) return true;
        } else if (anim->type == AnimationType::WINDOW_RESIZE) {
            WindowResizeAnimation *resize_anim = static_cast<WindowResizeAnimation*>(anim);
            if (resize_anim->window == window) return true;
        } else if (anim->type == AnimationType::WINDOW_OPACITY) {
            WindowOpacityAnimation *opacity_anim = static_cast<WindowOpacityAnimation*>(anim);
            if (opacity_anim->window == window) return true;
        } else if (anim->type == AnimationType::WINDOW_OPEN) {
            WindowOpenAnimation *open_anim = static_cast<WindowOpenAnimation*>(anim);
            if (open_anim->window == window) return true;
        } else if (anim->type == AnimationType::WINDOW_CLOSE) {
            WindowCloseAnimation *close_anim = static_cast<WindowCloseAnimation*>(anim);
            if (close_anim->window == window) return true;
        }
    }
    return false;
}

bool nwm::is_workspace_animating(Base &base, int workspace_index) {
    if (!base.anim_manager) return false;
    for (auto *anim : base.anim_manager->animations) {
        if (anim->workspace_index == workspace_index) {
            return true;
        }
    }
    return false;
}

void nwm::toggle_animations(void *arg, Base &base) {
    (void)arg;
    if (!base.anim_manager) return;

    base.anim_manager->animations_enabled = !base.anim_manager->animations_enabled;

    if (!base.anim_manager->animations_enabled) {
        cancel_all_animations(base);
    }
}
