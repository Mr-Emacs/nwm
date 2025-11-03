#include "bar.hpp"
#include "nwm.hpp"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <X11/Xlib.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <cstring>

#define BAR_HEIGHT 30
#define BAR_BG_COLOR 0x0e0e0e
#define BAR_FG_COLOR 0xffffff
#define BAR_ACTIVE_COLOR 0xcdaa7d
#define BAR_INACTIVE_COLOR 0x7f7f7f
#define BAR_ACCENT_COLOR 0x3a3a8f
#define BAR_WARNING_COLOR 0xcd950c
#define BAR_CRITICAL_COLOR 0xf44747
#define BAR_HOVER_COLOR 0x1a1a1a

#define PADDING 14
#define ITEM_SPACING 10
#define SEGMENT_PADDING 18

static unsigned long last_cpu_total = 0;
static unsigned long last_cpu_idle = 0;
static auto last_net_time = std::chrono::steady_clock::now();
static unsigned long long last_rx_bytes = 0;
static unsigned long long last_tx_bytes = 0;

float nwm::get_cpu_usage() {
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) return 0.0f;

    std::string line;
    std::getline(stat_file, line);
    stat_file.close();

    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
    sscanf(line.c_str(), "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    unsigned long total = user + nice + system + idle + iowait + irq + softirq + steal;

    unsigned long total_diff = total - last_cpu_total;
    unsigned long idle_diff = idle - last_cpu_idle;

    last_cpu_total = total;
    last_cpu_idle = idle;

    if (total_diff == 0) return 0.0f;

    float usage = 100.0f * (total_diff - idle_diff) / total_diff;
    return usage;
}

float nwm::get_memory_usage() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) return 0.0f;

    unsigned long mem_total = 0, mem_free = 0, buffers = 0, cached = 0;
    std::string line;

    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            sscanf(line.c_str(), "MemTotal: %lu kB", &mem_total);
        } else if (line.find("MemFree:") == 0) {
            sscanf(line.c_str(), "MemFree: %lu kB", &mem_free);
        } else if (line.find("Buffers:") == 0) {
            sscanf(line.c_str(), "Buffers: %lu kB", &buffers);
        } else if (line.find("Cached:") == 0) {
            sscanf(line.c_str(), "Cached: %lu kB", &cached);
        }
    }
    meminfo.close();

    if (mem_total == 0) return 0.0f;

    unsigned long mem_used = mem_total - mem_free - buffers - cached;
    return 100.0f * mem_used / mem_total;
}

float nwm::get_disk_usage(const char* path) {
    struct statvfs stat;
    if (statvfs(path, &stat) != 0) return 0.0f;

    unsigned long total = stat.f_blocks * stat.f_frsize;
    unsigned long available = stat.f_bavail * stat.f_frsize;
    unsigned long used = total - available;

    if (total == 0) return 0.0f;
    return 100.0f * used / total;
}

void nwm::get_network_stats(std::string& rx, std::string& tx) {
    std::ifstream net_dev("/proc/net/dev");
    if (!net_dev.is_open()) {
        rx = "0 KB/s";
        tx = "0 KB/s";
        return;
    }

    unsigned long long total_rx = 0, total_tx = 0;
    std::string line;

    std::getline(net_dev, line);
    std::getline(net_dev, line);

    while (std::getline(net_dev, line)) {
        if (line.find("lo:") != std::string::npos) continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        unsigned long long rx_bytes, tx_bytes;
        const char* data = line.c_str() + colon + 1;
        sscanf(data, "%llu %*u %*u %*u %*u %*u %*u %*u %llu", &rx_bytes, &tx_bytes);

        total_rx += rx_bytes;
        total_tx += tx_bytes;
    }
    net_dev.close();

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_net_time).count();

    if (duration > 0 && last_rx_bytes > 0) {
        double rx_rate = (total_rx - last_rx_bytes) / (duration / 1000.0);
        double tx_rate = (total_tx - last_tx_bytes) / (duration / 1000.0);

        auto format_rate = [](double rate) -> std::string {
            std::ostringstream oss;
            if (rate < 1024) {
                oss << std::fixed << std::setprecision(0) << rate << " B/s";
            } else if (rate < 1024 * 1024) {
                oss << std::fixed << std::setprecision(1) << rate / 1024 << " KB/s";
            } else {
                oss << std::fixed << std::setprecision(1) << rate / (1024 * 1024) << " MB/s";
            }
            return oss.str();
        };

        rx = format_rate(rx_rate);
        tx = format_rate(tx_rate);
    } else {
        rx = "0 B/s";
        tx = "0 B/s";
    }

    last_rx_bytes = total_rx;
    last_tx_bytes = total_tx;
    last_net_time = now;
}

void nwm::get_battery_info(std::string& status, int& percent) {
    std::ifstream capacity_file("/sys/class/power_supply/BAT0/capacity");
    std::ifstream status_file("/sys/class/power_supply/BAT0/status");

    percent = -1;
    status = "N/A";

    if (capacity_file.is_open()) {
        capacity_file >> percent;
        capacity_file.close();
    }

    if (status_file.is_open()) {
        status_file >> status;
        status_file.close();
    }
}

void nwm::bar_init(Base &base) {
    base.bar.height = BAR_HEIGHT;
    base.bar.width = WIDTH(base.display, base.screen);
    base.bar.x = 0;
    base.bar.hover_segment = -1;
    base.bar.systray_width = 0;

    if (base.bar_position == 1) {
        base.bar.y = HEIGHT(base.display, base.screen) - BAR_HEIGHT;
    } else {
        base.bar.y = 0;
    }

    XSetWindowAttributes attrs;
    attrs.background_pixel = BAR_BG_COLOR;
    attrs.override_redirect = True;
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                       PointerMotionMask | Button4 | Button5;

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

    create_color(BAR_FG_COLOR, base.bar.xft_fg);
    create_color(BAR_BG_COLOR, base.bar.xft_bg);
    create_color(BAR_ACTIVE_COLOR, base.bar.xft_active);
    create_color(BAR_INACTIVE_COLOR, base.bar.xft_inactive);
    create_color(BAR_ACCENT_COLOR, base.bar.xft_accent);
    create_color(BAR_WARNING_COLOR, base.bar.xft_warning);
    create_color(BAR_CRITICAL_COLOR, base.bar.xft_critical);
    create_color(BAR_HOVER_COLOR, base.bar.xft_hover);

    base.bar.sys_info.last_update = std::chrono::steady_clock::now();
    bar_update_system_info(base);
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
    free_color(base.bar.xft_warning);
    free_color(base.bar.xft_critical);
    free_color(base.bar.xft_hover);

    if (base.bar.window) {
        XDestroyWindow(base.display, base.bar.window);
        base.bar.window = 0;
    }
}

void nwm::bar_update_system_info(Base &base) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - base.bar.sys_info.last_update).count();

    if (elapsed < 2) return;

    base.bar.sys_info.cpu_usage = get_cpu_usage();
    base.bar.sys_info.memory_usage = get_memory_usage();
    base.bar.sys_info.disk_usage = get_disk_usage("/");
    get_network_stats(base.bar.sys_info.network_rx, base.bar.sys_info.network_tx);
    get_battery_info(base.bar.sys_info.battery_status, base.bar.sys_info.battery_percent);

    base.bar.sys_info.last_update = now;
}

void draw_rounded_rect(Display* display, Drawable drawable, GC gc,
                       int x, int y, int width, int height, int radius) {
    XFillArc(display, drawable, gc, x, y, radius * 2, radius * 2, 90 * 64, 90 * 64);
    XFillArc(display, drawable, gc, x + width - radius * 2, y, radius * 2, radius * 2, 0, 90 * 64);
    XFillArc(display, drawable, gc, x, y + height - radius * 2, radius * 2, radius * 2, 180 * 64, 90 * 64);
    XFillArc(display, drawable, gc, x + width - radius * 2, y + height - radius * 2, radius * 2, radius * 2, 270 * 64, 90 * 64);

    XFillRectangle(display, drawable, gc, x + radius, y, width - radius * 2, height);
    XFillRectangle(display, drawable, gc, x, y + radius, radius, height - radius * 2);
    XFillRectangle(display, drawable, gc, x + width - radius, y + radius, radius, height - radius * 2);
}

void nwm::bar_draw(Base &base) {
    XClearWindow(base.display, base.bar.window);
    base.bar.segments.clear();

    if (!base.xft_font || !base.bar.xft_draw) return;

    int x_offset = PADDING;
    int y_offset = BAR_HEIGHT / 2 + 6;

    GC gc = XCreateGC(base.display, base.bar.window, 0, nullptr);

    for (size_t i = 0; i < base.workspaces.size(); ++i) {
        std::string ws_label = base.widget[i % base.widget.size()];

        XGlyphInfo extents;
        XftTextExtentsUtf8(base.display, base.xft_font,
                          (XftChar8*)ws_label.c_str(), ws_label.length(),
                          &extents);

        int btn_width = extents.width + 16;
        int btn_height = BAR_HEIGHT - 8;
        int btn_x = x_offset;
        int btn_y = 4;

        bool is_active = (i == base.current_workspace);
        bool has_windows = !base.workspaces[i].windows.empty();
        bool is_hover = (base.bar.hover_segment >= 0 &&
                        base.bar.segments.size() > (size_t)base.bar.hover_segment &&
                        base.bar.segments[base.bar.hover_segment].workspace_index == (int)i);

        if (is_hover && !is_active) {
            XSetForeground(base.display, gc, BAR_HOVER_COLOR);
            draw_rounded_rect(base.display, base.bar.window, gc,
                            btn_x, btn_y, btn_width, btn_height, 4);
        } else if (is_active) {
            XSetForeground(base.display, gc, 0x444444);
            draw_rounded_rect(base.display, base.bar.window, gc,
                            btn_x, btn_y, btn_width, btn_height, 4);
        } else if (has_windows) {
            XSetForeground(base.display, gc, 0x2A2A2A);
            draw_rounded_rect(base.display, base.bar.window, gc,
                            btn_x, btn_y, btn_width, btn_height, 4);
        }

        XftColor *color = is_active ? &base.bar.xft_active :
                         (has_windows ? &base.bar.xft_fg : &base.bar.xft_inactive);

        XftDrawStringUtf8(base.bar.xft_draw, color, base.xft_font,
                         btn_x + 8, y_offset,
                         (XftChar8*)ws_label.c_str(), ws_label.length());

        BarSegment seg;
        seg.x = btn_x;
        seg.y = btn_y;
        seg.width = btn_width;
        seg.height = btn_height;
        seg.workspace_index = i;
        seg.type = BarSegment::WORKSPACE;
        base.bar.segments.push_back(seg);

        x_offset += btn_width + ITEM_SPACING;
    }

    x_offset += SEGMENT_PADDING;

    Monitor *mon = get_current_monitor(base);
    std::string layout_mode = (mon && mon->horizontal_mode) ? "[SCROLL]" : "[TILE]";

    XftDrawStringUtf8(base.bar.xft_draw, &base.bar.xft_accent, base.xft_font,
                     x_offset, y_offset,
                     (XftChar8*)layout_mode.c_str(), layout_mode.length());

    XGlyphInfo layout_ext;
    XftTextExtentsUtf8(base.display, base.xft_font,
                      (XftChar8*)layout_mode.c_str(), layout_mode.length(),
                      &layout_ext);
    x_offset += layout_ext.width + SEGMENT_PADDING;

    time_t now = time(nullptr);
    tm* local = localtime(&now);
    std::ostringstream time_stream;
    time_stream << std::put_time(local, "%H:%M")
                << "  " << std::put_time(local, "%a %b %d");
    std::string time_str = time_stream.str();

    XGlyphInfo time_extents;
    XftTextExtentsUtf8(base.display, base.xft_font,
                      (XftChar8*)time_str.c_str(), time_str.length(),
                      &time_extents);

    int time_x = (base.bar.width - time_extents.width) / 2;
    XftDrawStringUtf8(base.bar.xft_draw, &base.bar.xft_fg, base.xft_font,
                     time_x, y_offset,
                     (XftChar8*)time_str.c_str(), time_str.length());

    std::ostringstream sys_stream;
    sys_stream << std::fixed << std::setprecision(0);

    sys_stream << "CPU " << base.bar.sys_info.cpu_usage << "%";

    sys_stream << "  RAM " << base.bar.sys_info.memory_usage << "%";

    sys_stream << "  DISK " << base.bar.sys_info.disk_usage << "%";

    sys_stream << "  DOWN " << base.bar.sys_info.network_rx
               << " UP " << base.bar.sys_info.network_tx;

    if (base.bar.sys_info.battery_percent >= 0) {
        std::string bat_icon = base.bar.sys_info.battery_status == "Charging" ? "CHG" : "BAT";
        sys_stream << "  " << bat_icon << " " << base.bar.sys_info.battery_percent << "%";
    }

    std::string sys_str = sys_stream.str();

    XGlyphInfo sys_extents;
    XftTextExtentsUtf8(base.display, base.xft_font,
                      (XftChar8*)sys_str.c_str(), sys_str.length(),
                      &sys_extents);

    int sys_x = base.bar.width - sys_extents.width - PADDING - base.bar.systray_width;

    XftColor* sys_color = &base.bar.xft_fg;
    if (base.bar.sys_info.cpu_usage > 90 || base.bar.sys_info.memory_usage > 90) {
        sys_color = &base.bar.xft_critical;
    } else if (base.bar.sys_info.cpu_usage > 75 || base.bar.sys_info.memory_usage > 75) {
        sys_color = &base.bar.xft_warning;
    }

    XftDrawStringUtf8(base.bar.xft_draw, sys_color, base.xft_font,
                     sys_x, y_offset,
                     (XftChar8*)sys_str.c_str(), sys_str.length());

    XFreeGC(base.display, gc);
    XFlush(base.display);
}

void nwm::bar_update_workspaces(Base &base) {
    bar_draw(base);
}

void nwm::bar_update_time(Base &base) {
    bar_update_system_info(base);
    bar_draw(base);
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

void nwm::bar_handle_motion(Base &base, int x, int y) {
    int old_hover = base.bar.hover_segment;
    base.bar.hover_segment = -1;

    for (size_t i = 0; i < base.bar.segments.size(); ++i) {
        const auto& seg = base.bar.segments[i];
        if (x >= seg.x && x < seg.x + seg.width &&
            y >= seg.y && y < seg.y + seg.height) {
            if (seg.type == BarSegment::WORKSPACE) {
                base.bar.hover_segment = i;
                break;
            }
        }
    }

    if (old_hover != base.bar.hover_segment) {
        bar_draw(base);
    }
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
