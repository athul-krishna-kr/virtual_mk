#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// typedef struct pointer {
//     int type;

//     u_int32_t button;
//     int button_state;

//     uint64_t x;
//     uint64_t y;
// } pointer;

// typedef struct gesture {
//     int type;
//     int value;
//     uint64_t time;
// } gesture;

// struct event {
//     pointer pointer;
//     gesture gesture;
// };

struct virtual_mouse {
    struct libinput *libinput_context;
    struct libevdev_uinput *output_device;
    int libinput_fd;
    int evdev_fd;
    bool grabbed;
};

struct virtual_keyboard {
    struct libevdev *evdev;
    struct libevdev_uinput *output_device;
    int fd;
    bool grabbed;
};

struct virtual_mk {
    struct virtual_mouse *mouse;
    struct virtual_keyboard *keyboard;
    int epoll_fd;
    int signal_fd;
};

int mouse_create(const char *path, struct virtual_mouse *mouse);
void mouse_handle_events(struct virtual_mouse *mouse);
void mouse_grab_global(struct virtual_mouse *mouse, bool grab);
void mouse_close(struct virtual_mouse *mouse);

int keyboard_create(const char *path, struct virtual_keyboard *keyboard);
void keyboard_flush(struct virtual_keyboard *keyboard);
void keyboard_grab_global(struct virtual_keyboard *keyboard, bool flag);
void keyboard_handle_events(struct virtual_keyboard *keyboard);
void keyboard_close(struct virtual_keyboard *keyboard);

static inline double floor_or_ceil(double val) {
    double ret = 0;

    if (val > (double)0)
        ret = ceil(val);
    else
        ret = floor(val);  

    return ret;
}
