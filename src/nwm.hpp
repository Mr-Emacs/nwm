#ifndef NWM_HPP
#define NWM_HPP

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xinerama.h>
#include <string>
#include <vector>
#include "bar.hpp"
#include "systray.hpp"

#define WIDTH(display, screen_number) XDisplayWidth((display), (screen_number))
#define HEIGHT(display, screen_number) XDisplayHeight((display), (screen_number))
#define NUM_WORKSPACES 9

namespace nwm {
    struct AnimationManager;
}

namespace nwm {

struct TitleBar {
    Window window;
    XftDraw* xft_draw;
    int x, y;
    int width, height;
    bool needs_redraw;
};

struct ManagedWindow {
    Window window;
    int x, y;
    int width, height;
    bool is_floating;
    bool is_focused;
    bool is_fullscreen;
    int workspace;
    int monitor;

    int pre_fs_x, pre_fs_y;
    int pre_fs_width, pre_fs_height;
    bool pre_fs_floating;
    
    // Title bar support
    TitleBar titlebar;
    std::string title;
    bool has_titlebar;
};

struct Monitor {
    int id;
    int x, y;
    int width, height;
    int current_workspace;
    float master_factor;
    bool horizontal_mode;
    int scroll_windows_visible;
    RRCrtc crtc;
};

struct Workspace {
    std::vector<ManagedWindow> windows;
    ManagedWindow* focused_window;
    int scroll_offset;
    bool scroll_maximized;
};

struct Base {
    AnimationManager* anim_manager;
    int screen;
    int gaps;
    bool gaps_enabled;
    Window root;
    Display *display;
    std::vector<ManagedWindow> windows;
    ManagedWindow* focused_window;
    bool running;
    bool restart;
    Cursor cursor;
    Cursor cursor_move;
    Cursor cursor_resize;
    float master_factor;
    bool horizontal_mode;
    bool bar_visible;
    int bar_position;
    bool resizing;
    int resize_start_width;
    int resize_start_height;

    XftFont* xft_font;
    XftDraw* xft_draw;
    
    // Title bar support
    bool show_window_titles;
    int titlebar_height;
    XftColor titlebar_bg;
    XftColor titlebar_fg;
    XftColor titlebar_focus_bg;
    XftColor titlebar_focus_fg;

    StatusBar bar;
    SystemTray systray;
    std::vector<Workspace> workspaces;
    size_t current_workspace;
    bool overview_mode;

    std::vector<std::string> widget;

    bool dragging;
    Window drag_window;
    int drag_start_x;
    int drag_start_y;
    int drag_window_start_x;
    int drag_window_start_y;

    int border_width;
    unsigned long border_color;
    unsigned long focus_color;
    int resize_step;
    int scroll_step;

    Window hint_check_window;

    std::vector<Monitor> monitors;
    int current_monitor;
    int xrandr_event_base;
    bool use_xinerama;
    bool use_builtin_bar;
};

void manage_window(Window window, Base &base);
void unmanage_window(Window window, Base &base);
void focus_window(ManagedWindow* window, Base &base);
void move_window(ManagedWindow* window, int x, int y, Base &base);
void resize_window(ManagedWindow* window, int width, int height, Base &base);
void raise_override_redirect_windows(Display *display);
void close_window(void *arg, Base &base);
void focus_next(void *arg, Base &base);
void focus_prev(void *arg, Base &base);
void quit_wm(void *arg, Base &base);
void toggle_gap(void *arg, Base &base);
void toggle_toggle(void *arg, Base &base);
void toggle_bar(void *arg, Base &base);
void toggle_float(void *arg, Base &base);
void toggle_fullscreen(void *arg, Base &base);

void toggle_layout(void *arg, Base &base);
void swap_prev(void *arg, Base &base);
void swap_next(void *arg, Base &base);
void resize_master(void *arg, Base &base);
void scroll_left(void *arg, Base &base);
void scroll_right(void *arg, Base &base);

void switch_workspace(void *arg, Base &base);
void move_to_workspace(void *arg, Base &base);
void workspace_init(Base &base);
Workspace& get_current_workspace(Base &base);
void toggle_scroll_maximize(void *arg, Base &base);

void handle_key_press(XKeyEvent *e, Base &base);
void handle_button_press(XButtonEvent *e, Base &base);
void handle_button_release(XButtonEvent *e, Base &base);
void handle_motion_notify(XMotionEvent *e, Base &base);
void handle_configure_request(XConfigureRequestEvent *e, Base &base);
void handle_map_request(XMapRequestEvent *e, Base &base);
void handle_unmap_notify(XUnmapEvent *e, Base &base);
void handle_enter_notify(XCrossingEvent *e, Base &base);
void handle_destroy_notify(XDestroyWindowEvent *e, Base &base);
void handle_expose(XExposeEvent *e, Base &base);
void handle_client_message(XClientMessageEvent *e, Base &base);
void handle_property_notify(XPropertyEvent *e, Base &base);

void setup_ewmh(Base &base);
void reload_config(void *arg, Base &base);
void spawn(void *arg, Base &base);
void setup_keys(Base &base);
void run(Base &base);
void init(Base &base);
void cleanup(Base &base);

void handle_special_window_map(Display *display, Window window);
void raise_special_windows(Display *display);

void monitors_init(Base &base);
void monitors_update(Base &base);
Monitor* get_monitor_at_point(Base &base, int x, int y);
Monitor* get_current_monitor(Base &base);
void focus_monitor(void *arg, Base &base);
void set_scroll_visible(void *arg, Base &base);

void increment_scroll_visible(void *arg, Base &base);
void decrement_scroll_visible(void *arg, Base &base);

// Title bar functions
void titlebar_init(ManagedWindow* window, Base &base);
void titlebar_cleanup(ManagedWindow* window, Base &base);
void titlebar_draw(ManagedWindow* window, Base &base);
void titlebar_update_title(ManagedWindow* window, Base &base);
std::string get_window_title(Display* display, Window window);
}

#endif //NWM_HPP
