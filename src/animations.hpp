#ifndef ANIMATIONS_HPP
#define ANIMATIONS_HPP

#include "nwm.hpp"
#include <chrono>
#include <functional>
#include <vector>

namespace nwm {

enum class EasingType {
    LINEAR,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_OUT_ELASTIC,
    EASE_OUT_BOUNCE
};

enum class AnimationType {
    SCROLL_OFFSET,
    WINDOW_MOVE,
    WINDOW_RESIZE,
    WINDOW_OPACITY,
    MASTER_FACTOR,
    WINDOW_OPEN,
    WINDOW_CLOSE,
    WORKSPACE_SWITCH,
    BORDER_COLOR_CHANGE,
    FLOATING_TRANSITION
};

struct Animation {
    AnimationType type;
    std::chrono::steady_clock::time_point start_time;
    int duration_ms;
    EasingType easing;
    bool completed;
    int workspace_index;
    int monitor_index;

    std::function<void()> on_complete;

    virtual ~Animation() = default;
    virtual void update(Base &base, float progress) = 0;
};

struct ScrollAnimation : public Animation {
    int start_offset;
    int target_offset;

    void update(Base &base, float progress) override;
};

struct WindowMoveAnimation : public Animation {
    Window window;
    int start_x, start_y;
    int target_x, target_y;

    void update(Base &base, float progress) override;
};

struct WindowResizeAnimation : public Animation {
    Window window;
    int start_width, start_height;
    int target_width, target_height;

    void update(Base &base, float progress) override;
};

struct WindowOpacityAnimation : public Animation {
    Window window;
    float start_opacity;
    float target_opacity;

    void update(Base &base, float progress) override;
};

struct MasterFactorAnimation : public Animation {
    float start_factor;
    float target_factor;

    void update(Base &base, float progress) override;
};

struct WindowOpenAnimation : public Animation {
    Window window;
    float start_scale;
    float target_scale;
    float start_opacity;
    float target_opacity;

    void update(Base &base, float progress) override;
};

struct WindowCloseAnimation : public Animation {
    Window window;
    float start_scale;
    float target_scale;
    float start_opacity;
    float target_opacity;
    int original_x, original_y;
    int original_width, original_height;

    void update(Base &base, float progress) override;
};

struct WorkspaceSwitchAnimation : public Animation {
    int from_workspace;
    int to_workspace;
    float slide_progress;

    void update(Base &base, float progress) override;
};

struct BorderColorAnimation : public Animation {
    Window window;
    unsigned long start_color;
    unsigned long target_color;

    void update(Base &base, float progress) override;
};

struct FloatingTransitionAnimation : public Animation {
    Window window;
    int start_x, start_y;
    int start_width, start_height;
    int target_x, target_y;
    int target_width, target_height;
    bool to_floating;

    void update(Base &base, float progress) override;
};

struct AnimationManager {
    std::vector<Animation*> animations;
    bool animations_enabled;

    bool scroll_enabled;
    bool window_move_enabled;
    bool window_resize_enabled;
    bool opacity_enabled;
    bool master_factor_enabled;
    bool window_open_enabled;
    bool window_close_enabled;
    bool workspace_switch_enabled;
    bool border_color_enabled;

    int scroll_duration;
    int window_move_duration;
    int window_resize_duration;
    int opacity_duration;
    int master_factor_duration;
    int window_open_duration;
    int window_close_duration;
    int workspace_switch_duration;
    int border_color_duration;

    EasingType scroll_easing;
    EasingType window_move_easing;
    EasingType window_resize_easing;
    EasingType opacity_easing;
    EasingType master_factor_easing;
    EasingType window_open_easing;
    EasingType window_close_easing;
    EasingType workspace_switch_easing;
    EasingType border_color_easing;

    enum OpenStyle { FADE, SCALE, SLIDE_FROM_TOP, SLIDE_FROM_BOTTOM, SLIDE_FROM_LEFT, SLIDE_FROM_RIGHT } window_open_style;
    enum CloseStyle { FADE_OUT, SCALE_DOWN, SLIDE_TO_TOP, SLIDE_TO_BOTTOM, SLIDE_TO_LEFT, SLIDE_TO_RIGHT } window_close_style;
};

float ease_linear(float t);
float ease_in_quad(float t);
float ease_out_quad(float t);
float ease_in_out_quad(float t);
float ease_in_cubic(float t);
float ease_out_cubic(float t);
float ease_in_out_cubic(float t);
float ease_out_elastic(float t);
float ease_out_bounce(float t);
float apply_easing(float t, EasingType type);

void animations_init(Base &base);
void animations_cleanup(Base &base);
void animations_update(Base &base);

void animate_scroll(Base &base, int target_offset);
void animate_scroll_smooth(Base &base, int target_offset, int duration_ms, EasingType easing);
void animate_window_move(Base &base, Window window, int target_x, int target_y);
void animate_window_resize(Base &base, Window window, int target_width, int target_height);
void animate_window_opacity(Base &base, Window window, float target_opacity);
void animate_master_factor(Base &base, float target_factor);
void animate_window_open(Base &base, Window window);
void animate_window_close(Base &base, Window window, std::function<void()> on_complete);
void animate_workspace_switch(Base &base, int from_workspace, int to_workspace);
void animate_border_color(Base &base, Window window, unsigned long target_color);
void animate_border_color(Base &base, Window window, unsigned long target_color);
void animate_floating_transition(Base &base, Window window, bool to_floating,
                                 int start_x, int start_y, int start_width, int start_height,
                                 int target_x, int target_y, int target_width, int target_height);
void cancel_animations_for_window(Base &base, Window window);
void cancel_animations_for_window(Base &base, Window window);
void cancel_animations_for_workspace(Base &base, int workspace_index);
void cancel_all_animations(Base &base);

bool is_animating(Base &base);
bool is_window_animating(Base &base, Window window);
bool is_workspace_animating(Base &base, int workspace_index);

void toggle_animations(void *arg, Base &base);

}

#endif // ANIMATIONS_HPP
