# UNDOCUMENTED CHANGE LOG

## MAJOR STUFF
browser link drag and drop
- [x] resize floating windows
- [x] mouse flowing option set by default
- [x] autostart xsession setup
showing what WM Im running
- [] Add debian build //Will do it properly after first release I dont wanna tar ball

Crate.io creation

##  Fint Bugs Fixing

- [x] Bar should be a separate program(NOPE).
- [x] Have The amount of Window in Scroll configurable. this should be done through the config
- [x] Auto release is buggy and not consistent.
- [x] The window flickering is still an issue.(Compositor should be a thing)
- [x] Restarting the WM not displays the other windows hides everything.( After the multimonitor support this is still and issue)

- [x] Bar input have been removed cuz of my trackpad which I will add later. Write a better mechansim when I could have a bool to disable and enable it.

# Change-Log
- Addition of multi monitor support with libXrandr.
- Controllable amount of application on horizontal window.
- Also default-config.hpp will generate config.hpp if u do not have config.hpp
```c++
    #define SCROLL_WINDOWS_VISIBLE 2
```

# Feature fix(Mon Jan 12)

 - Unification of scroll-left/right to focus.(@rekado [#18](https://github.com/xsoder/nwm/issues/18))
 - This feature allows us to have unifed way mostly for those desiring scroll feature.
 - The pull-request can be viewed here: [#19](https://github.com/xsoder/nwm/pull/19)

```C++
void move_horizontal(void *arg, Base &base, bool forward, bool window_based, bool animate, bool change_focus);
```
- This fix included addition of move_horizontal function.
