#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <unistd.h>

struct App {
    Display   *dpy = nullptr;
    int        scr = 0;
    Window    root = None;
    Window     win = None;
    GLXContext ctx = nullptr;
    int w = 0, h = 0;
};

struct vec2f { float x = 0.f, y = 0.f; };

vec2f operator*(vec2f v, float s)  { return { v.x * s,   v.y * s   }; }
vec2f operator/(vec2f v, float s)  { return { v.x / s,   v.y / s   }; }
vec2f operator+(vec2f a, vec2f b)  { return { a.x + b.x, a.y + b.y }; }
vec2f operator-(vec2f a, vec2f b)  { return { a.x - b.x, a.y - b.y }; }

struct Camera {
    vec2f pos;
    float scale = 1.f;
};

static std::string read_file(const std::string &path) {
    std::ifstream f(path);
    if (!f) { std::printf("ERROR: Cannot open file: %s\n", path.c_str()); return {}; }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compile_shader(const std::string &src, GLenum kind) {
    if (src.empty()) return 0;
    GLuint shader = glCreateShader(kind);
    const char *p = src.c_str();
    glShaderSource(shader, 1, &p, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::printf("Shader compile error (%s): %s\n",
                    kind == GL_VERTEX_SHADER ? "vert" : "frag", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint make_program(const char *vert_path, const char *frag_path) {
    GLuint v = compile_shader(read_file(vert_path), GL_VERTEX_SHADER);
    GLuint f = compile_shader(read_file(frag_path), GL_FRAGMENT_SHADER);
    if (!v || !f) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::printf("Program link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static GLuint upload_image(App &app, XImage *img) {
    auto shift_of = [](unsigned long mask) {
        int s = 0;
        if (!mask) return s;
        while (!(mask & 1u)) { mask >>= 1; ++s; }
        return s;
    };
    int r   = shift_of(img->red_mask);
    int g   = shift_of(img->green_mask);
    int b   = shift_of(img->blue_mask);
    int bpp = img->bits_per_pixel / 8;

    std::vector<unsigned char> rgba(app.w * app.h * 4);
    for (int y = 0; y < app.h; ++y) {
        for (int x = 0; x < app.w; ++x) {
            const unsigned char *src =
                reinterpret_cast<unsigned char *>(img->data)
                + y * img->bytes_per_line + x * bpp;
            unsigned int pix = 0;
            std::memcpy(&pix, src, std::min(bpp, 4));
            int i       = (y * app.w + x) * 4;
            rgba[i + 0] = static_cast<unsigned char>((pix & img->red_mask)   >> r);
            rgba[i + 1] = static_cast<unsigned char>((pix & img->green_mask) >> g);
            rgba[i + 2] = static_cast<unsigned char>((pix & img->blue_mask)  >> b);
            rgba[i + 3] = 255;
        }
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, app.w, app.h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

static bool is_mostly_black(App &app, XImage *img) {
    const int step = 64;
    int total = 0, black = 0;
    for (int y = 0; y < app.h; y += step) {
        for (int x = 0; x < app.w; x += step) {
            unsigned long px = XGetPixel(img, x, y);
            if ((px & 0x00FFFFFF) == 0) ++black;
            ++total;
        }
    }
    return total > 0 && (black * 10 >= total * 9);
}

static XImage *capture_screen(App &app) {
    const int max_attempts = 10;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        XSync(app.dpy, False);

        XImage *img = XGetImage(app.dpy, app.root,
                                0, 0, app.w, app.h, AllPlanes, ZPixmap);
        if (!img) {
            std::printf("XGetImage failed\n");
            return nullptr;
        }

        if (!is_mostly_black(app, img)) {
           if (attempt > 0)
                std::printf("Captured screen after %d retries\n", attempt);
            return img;
        }

        XDestroyImage(img);
        usleep(100000 * (attempt + 1));
    }

    std::printf("WARNING: screen still black after %d attempts, using last capture\n",
                max_attempts);
    return XGetImage(app.dpy, app.root, 0, 0, app.w, app.h, AllPlanes, ZPixmap);
}

static GLuint make_quad() {
    float verts[] = { -1.f, 1.f,  -1.f,-1.f,  1.f, 1.f,  1.f,-1.f };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    return vao;
}

static bool init_glx(App &app) {
    int attr[] = {
        GLX_RGBA, GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
        None
    };
    XVisualInfo *vi = glXChooseVisual(app.dpy, app.scr, attr);
    if (!vi) { std::printf("glXChooseVisual failed\n"); return false; }

    Colormap cmap = XCreateColormap(app.dpy, app.root, vi->visual, AllocNone);
    XSetWindowAttributes swa{};
    swa.colormap          = cmap;
    swa.override_redirect = True;
    swa.event_mask        = KeyPressMask
                          | ButtonPressMask | ButtonReleaseMask
                          | PointerMotionMask;

    app.win = XCreateWindow(app.dpy, app.root, 0, 0, app.w, app.h, 0,
                            vi->depth, InputOutput, vi->visual,
                            CWColormap | CWEventMask | CWOverrideRedirect, &swa);
    XMapRaised(app.dpy, app.win);
    XGrabKeyboard(app.dpy, app.win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    XGrabPointer(app.dpy, app.win, True,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

    app.ctx = glXCreateContext(app.dpy, vi, nullptr, GL_TRUE);
    glXMakeCurrent(app.dpy, app.win, app.ctx);
    XFree(vi);

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::printf("glewInit failed: %s\n", glewGetErrorString(err));
        return false;
    }
    return true;
}

static void render(GLuint prog, GLuint tex, const Camera &cam) {
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(prog, "u_texture"), 0);
    glUniform2f(glGetUniformLocation(prog, "u_offset"), cam.pos.x, cam.pos.y);
    glUniform1f(glGetUniformLocation(prog, "u_scale"),  cam.scale);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

int main() {
    App app;
    app.dpy = XOpenDisplay(nullptr);
    if (!app.dpy) { std::printf("Cannot open display\n"); return 1; }

    app.scr  = DefaultScreen(app.dpy);
    app.root = RootWindow(app.dpy, app.scr);
    app.w    = DisplayWidth(app.dpy, app.scr);
    app.h    = DisplayHeight(app.dpy, app.scr);

    XImage *raw = capture_screen(app);
    if (!raw) return 1;

    if (!init_glx(app)) return 1;

    GLuint tex = upload_image(app, raw);
    XDestroyImage(raw);
    if (!tex) return 1;

    GLuint prog = make_program("./vert.glsl", "./frag.glsl");
    if (!prog) return 1;

    GLuint vao = make_quad();
    glBindVertexArray(vao);
    glClearColor(0.f, 0.f, 0.f, 1.f);

    Camera cam;
    Camera target;
    const float ZOOM_STEP  = 0.1f;
    const float PAN_STEP   = 0.02f;
    const float LERP_SPEED = 14.f;

    bool  running       = true;
    bool  panning       = false;
    int   pan_sx = 0,   pan_sy = 0;
    vec2f pan_start_pos = {};

    while (running) {
        while (XPending(app.dpy)) {
            XEvent ev;
            XNextEvent(app.dpy, &ev);

            if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                switch (ks) {
                case XK_Escape: case XK_q:
                    running = false;
                    break;
                case XK_equal: case XK_plus:
                    target.scale *= 1.f + ZOOM_STEP;
                    break;
                case XK_minus:
                    target.scale /= 1.f + ZOOM_STEP;
                    if (target.scale < 1.f) { target.scale = 1.f; target.pos = {}; }
                    break;
                case XK_0:
                    target.scale = 1.f; target.pos = {};
                    break;
                case XK_h: case XK_Left:
                    target.pos.x -= PAN_STEP / target.scale;
                    break;
                case XK_l: case XK_Right:
                    target.pos.x += PAN_STEP / target.scale;
                    break;
                case XK_k: case XK_Up:
                    target.pos.y -= PAN_STEP / target.scale;
                    break;
                case XK_j: case XK_Down:
                    target.pos.y += PAN_STEP / target.scale;
                    break;
                }

            } else if (ev.type == ButtonPress) {
                int btn = ev.xbutton.button;

                if (btn == Button4 || btn == Button5) {
                    float nx    = ev.xbutton.x / static_cast<float>(app.w);
                    float ny    = ev.xbutton.y / static_cast<float>(app.h);
                    float old_s = target.scale;
                    float new_s = (btn == Button4)
                                  ? old_s * (1.f + ZOOM_STEP)
                                  : old_s / (1.f + ZOOM_STEP);
                    if (new_s < 1.f) { new_s = 1.f; target.pos = {}; }
                    else {
                        target.pos.x += (nx - 0.5f) * (1.f / old_s - 1.f / new_s);
                        target.pos.y += (ny - 0.5f) * (1.f / old_s - 1.f / new_s);
                    }
                    target.scale = new_s;

                } else if (btn == Button1) {
                    float nx = ev.xbutton.x / static_cast<float>(app.w);
                    float ny = ev.xbutton.y / static_cast<float>(app.h);
                    target.pos.x += (nx - 0.5f) / target.scale;
                    target.pos.y += (ny - 0.5f) / target.scale;

                    panning       = true;
                    pan_sx        = ev.xbutton.x;
                    pan_sy        = ev.xbutton.y;
                    pan_start_pos = target.pos;
                }

            } else if (ev.type == ButtonRelease) {
                if (ev.xbutton.button == Button1) panning = false;

            } else if (ev.type == MotionNotify && panning) {
                float dx = (ev.xmotion.x - pan_sx) / static_cast<float>(app.w);
                float dy = (ev.xmotion.y - pan_sy) / static_cast<float>(app.h);
                target.pos.x = pan_start_pos.x - dx / target.scale;
                target.pos.y = pan_start_pos.y - dy / target.scale;
            }
        }

        const float dt    = 1.f / 60.f;
        const float alpha = 1.f - std::exp(-LERP_SPEED * dt);
        cam.pos.x  += (target.pos.x  - cam.pos.x)  * alpha;
        cam.pos.y  += (target.pos.y  - cam.pos.y)  * alpha;
        cam.scale  += (target.scale  - cam.scale)  * alpha;

        render(prog, tex, cam);
        glXSwapBuffers(app.dpy, app.win);

        usleep(16666);
    }

    XUngrabPointer(app.dpy, CurrentTime);
    XUngrabKeyboard(app.dpy, CurrentTime);
    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glXMakeCurrent(app.dpy, None, nullptr);
    glXDestroyContext(app.dpy, app.ctx);
    XDestroyWindow(app.dpy, app.win);
    XCloseDisplay(app.dpy);
    return 0;
}
