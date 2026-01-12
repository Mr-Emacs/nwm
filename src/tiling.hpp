#ifndef TILING_HPP
#define TILING_HPP

#include "nwm.hpp"

namespace nwm {

void tile_windows(Base &base);
void tile_horizontal(Base &base);

void resize_master(void *arg, Base &base);

void scroll_left(void *arg, Base &base);
void scroll_right(void *arg, Base &base);

void move_horizontal(void *arg, Base &base, bool forward, bool window_based, bool animate, bool change_focus);

void toggle_layout(void *arg, Base &base);

void swap_next(void *arg, Base &base);
void swap_prev(void *arg, Base &base);

void raise_special_windows(Display *display);

void increment_scroll_visible(void *arg, Base &base);
void decrement_scroll_visible(void *arg, Base &base);

}

#endif // TILING_HPP
