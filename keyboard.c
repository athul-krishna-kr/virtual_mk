#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#include "virtual_mk.h"

#define MAX_EVENTS 16

#define toggle_grab(keyboard) ({ \
    libevdev_uinput_write_event((keyboard)->output_device, EV_KEY, KEY_LEFTCTRL, 1); \
    libevdev_uinput_write_event((keyboard)->output_device, EV_SYN, SYN_REPORT, 0); \
    libevdev_uinput_write_event((keyboard)->output_device, EV_KEY, KEY_RIGHTCTRL, 1); \
    libevdev_uinput_write_event((keyboard)->output_device, EV_SYN, SYN_REPORT, 0); \
    libevdev_uinput_write_event((keyboard)->output_device, EV_KEY, KEY_RIGHTCTRL, 0); \
    libevdev_uinput_write_event((keyboard)->output_device, EV_SYN, SYN_REPORT, 0); \
    libevdev_uinput_write_event((keyboard)->output_device, EV_KEY, KEY_LEFTCTRL, 0); \
    libevdev_uinput_write_event((keyboard)->output_device, EV_SYN, SYN_REPORT, 0); \
})

static struct grab_context {
    bool left_ctrl;
    bool right_ctrl;
    bool grab;
} grab = {
    .left_ctrl = 0,
    .right_ctrl = 0,
    .grab = 0,
};

static void inline __keyboard_grab(struct virtual_keyboard *keyboard, struct input_event *event)
{
    unsigned int i;

    if (grab.left_ctrl && grab.right_ctrl) {
        if (keyboard->grabbed)
            grab.grab = 0;
        else
            grab.grab = 1;
    }
    else if (!grab.left_ctrl && !grab.right_ctrl) {
        if (grab.grab) {
            keyboard->grabbed = 1;
            toggle_grab(keyboard);
            libevdev_grab(keyboard->evdev, LIBEVDEV_GRAB);
            // printf("Keyboard grabbed\n");
        }
        else {
            keyboard->grabbed = 0;
            libevdev_uinput_write_event(keyboard->output_device, EV_KEY, event->code, event->value);
            libevdev_uinput_write_event(keyboard->output_device, EV_SYN, SYN_REPORT, 0);
            libevdev_grab(keyboard->evdev, LIBEVDEV_UNGRAB);
            // printf("Keyboard ungrabbed\n");
        }
        return;
    }
}
static int inline keyboard_grab(struct virtual_keyboard *keyboard, struct input_event *event)
{
    int code = event->code;

    if (event->type == EV_KEY && event->value != 2) {
        switch (code)
        {
            case KEY_LEFTCTRL:
                grab.left_ctrl = event->value;
                __keyboard_grab(keyboard, event);
                break;
            
            case KEY_RIGHTCTRL:
                grab.right_ctrl = event->value;
                __keyboard_grab(keyboard, event);
                
                break;
            default:
                break;
        }
    }

    return keyboard->grabbed;
}

static void __always_inline keyboard_write(struct virtual_keyboard *keyboard, struct input_event *event)
{
    if (keyboard_grab(keyboard, event)) {
        libevdev_uinput_write_event(keyboard->output_device, event->type, event->code, event->value);
        // printf("keyboard: type: %x, code: %x, value: %d\n", event->type, event->code, event->value);
    }
}

static int setup_keyboard(struct libevdev_uinput **output_device)
{
    int ret;
    struct input_id vid = {
        .bustype = BUS_USB,
        .vendor  = 0x1234,
        .product = 0x5679,
        .version = 1,
    };

    struct libevdev *dev = libevdev_new();
    libevdev_set_name(dev, "Virtual Keyboard");
    libevdev_set_id_vendor(dev, vid.vendor);
    libevdev_set_id_product(dev, vid.product);
    libevdev_set_id_version(dev, vid.version);

    for (int code = 1; code <= 248; code++) {
        libevdev_enable_event_code(dev, EV_KEY, code, NULL);
    }

    ret = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, output_device);
    libevdev_free(dev);

    return ret;
}

void keyboard_handle_events(struct virtual_keyboard *keyboard)
{
    struct input_event events[MAX_EVENTS];
    int count = 0, ret, flag,  sync = false;

    while (count < (int)MAX_EVENTS) {
        if (sync)
            flag = LIBEVDEV_READ_FLAG_SYNC;
        else
            flag = LIBEVDEV_READ_FLAG_NORMAL;

        ret = libevdev_next_event(keyboard->evdev, flag, &events[count]);
        if (ret == LIBEVDEV_READ_STATUS_SUCCESS || ret == LIBEVDEV_READ_STATUS_SYNC) {
            sync = (ret == LIBEVDEV_READ_STATUS_SYNC);
        } 
        else if (ret == -EAGAIN || ret == -EINTR) {
            break;
        }
        count++;
    }

    if (count == (int)MAX_EVENTS) {
        fprintf(stderr, "Events from evdev: %s exceeded limit: %d. This is an application bug!\n", libevdev_get_name(keyboard->evdev), MAX_EVENTS);
    }

    for (int i = 0; i < count; i++)
        keyboard_write(keyboard, &events[i]);
}

void keyboard_flush(struct virtual_keyboard *keyboard)
{
    struct input_event ev;
    int ret = 1;
    while (ret > 0 ) {
        ret = libevdev_next_event(keyboard->evdev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    }
}

int keyboard_create(const char *path, struct virtual_keyboard *keyboard)
{
    int fd, ret;

    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "Failed to open fd for keyboard %s: %s\n", path, strerror(errno));
        ret = -errno;\
        goto err_open;
    }

    ret = libevdev_new_from_fd(fd, &keyboard->evdev);
    if (ret < 0) {
        fprintf(stderr, "Failed to init libevdev for keyboard: %s\n", strerror(-ret));
        goto err_evdev;
    }
    libevdev_set_clock_id(keyboard->evdev, CLOCK_MONOTONIC);

    ret = setup_keyboard(&keyboard->output_device);
    if (ret < 0) {
        fprintf(stderr, "Failed to create uinput device: %s\n", strerror(-ret));
        goto err_uinput;
    }

    keyboard->fd = fd;

    return ret;

err_uinput:
    libevdev_free(keyboard->evdev);
err_evdev:
    close(fd);
err_open:
    return ret;
}

void keyboard_close(struct virtual_keyboard *keyboard)
{
    libevdev_uinput_destroy(keyboard->output_device);
    libevdev_free(keyboard->evdev);
    close(keyboard->fd);
}

void inline keyboard_grab_global(struct virtual_keyboard *keyboard, bool flag)
{
    if (flag)
        libevdev_grab(keyboard->evdev, LIBEVDEV_GRAB);
    else
        libevdev_grab(keyboard->evdev, LIBEVDEV_UNGRAB);
}