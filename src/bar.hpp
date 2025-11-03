#ifndef BAR_HPP
#define BAR_HPP

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <string>
#include <vector>
#include <chrono>

namespace nwm {

struct Base;

struct SystemInfo {
    float cpu_usage;
    float memory_usage;
    float disk_usage;
    std::string network_rx;
    std::string network_tx;
    std::string battery_status;
    int battery_percent;
    std::chrono::steady_clock::time_point last_update;
};

struct BarSegment {
    int x, y, width, height;
    int workspace_index;
    enum Type { WORKSPACE, LAYOUT, SYSTEM_INFO, TIME, SEPARATOR } type;
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
    XftColor xft_warning;
    XftColor xft_critical;
    XftColor xft_hover;

    std::vector<BarSegment> segments;
    int hover_segment;
    SystemInfo sys_info;
    int systray_width;
};

void bar_init(Base &base);
void bar_cleanup(Base &base);
void bar_draw(Base &base);
void bar_update_workspaces(Base &base);
void bar_update_time(Base &base);
void bar_update_system_info(Base &base);
void bar_handle_click(Base &base, int x, int y, int button);
void bar_handle_motion(Base &base, int x, int y);
void bar_handle_scroll(Base &base, int direction);

float get_cpu_usage();
float get_memory_usage();
float get_disk_usage(const char* path);
void get_network_stats(std::string& rx, std::string& tx);
void get_battery_info(std::string& status, int& percent);

}

#endif // BAR_HPP
