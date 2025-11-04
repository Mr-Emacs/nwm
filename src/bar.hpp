#ifndef BAR_HPP
#define BAR_HPP

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <string>
#include <vector>

namespace nwm {

struct Base;

struct BarSegment {
    int x, y, width, height;
    int workspace_index;
    enum Type { WORKSPACE, LAYOUT, SEPARATOR } type;
};

struct StatusBar {
    Window window;
    int x, y;
    int width, height;
    XftDraw* xft_draw;
    XftColor xft_fg;
    XftColor xft_bg;
    XftColor xft_active;
    XftColor xft_inactive;
    XftColor xft_accent;

    std::vector<BarSegment> segments;
    int hover_segment;
    int systray_width;
    std::string status_text;
};

void bar_init(Base &base);
void bar_cleanup(Base &base);
void bar_draw(Base &base);
void bar_update_workspaces(Base &base);
void bar_update_time(Base &base);
void bar_update_status_text(Base &base);
void bar_handle_click(Base &base, int x, int y, int button);
void bar_handle_motion(Base &base, int x, int y);
void bar_handle_scroll(Base &base, int direction);

}

#endif // BAR_HPP
