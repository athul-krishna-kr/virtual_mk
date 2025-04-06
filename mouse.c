#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#include <libinput.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#include "virtual_mk.h"
#include "config.h"

static int setup_virtual_mouse(struct libevdev_uinput **output_device) {
    int ret = 0;
    struct input_id vid = {
        .bustype = BUS_USB,
        .vendor  = 0x1234,
        .product = 0x5678,
        .version = 1,
    };

    struct libevdev *dev = libevdev_new();
    libevdev_set_name(dev, "Virtual Mouse");
    libevdev_set_id_vendor(dev, vid.vendor);
    libevdev_set_id_product(dev, vid.product);
    libevdev_set_id_version(dev, vid.version);
    libevdev_enable_event_type(dev, EV_REL);
    libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
    libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);
    libevdev_enable_event_code(dev, EV_REL, REL_WHEEL, NULL);
    libevdev_enable_event_code(dev, EV_REL, REL_WHEEL_HI_RES, NULL);
    libevdev_enable_event_type(dev, EV_KEY);
    libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_MIDDLE, NULL);

    ret = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, output_device);
    if (ret < 0) {
        fprintf(stderr, "Failed to create uinput device: %s\n", strerror(-ret));
    }

    libevdev_free(dev);
    return ret;
}

static void handle_pointer_motion(struct virtual_mouse *mouse,
    struct libinput_event_pointer *p_event, int type)
{
    struct libevdev_uinput *virt_mouse = mouse->output_device;
    double dx = libinput_event_pointer_get_dx_unaccelerated(p_event);
    double dy = libinput_event_pointer_get_dy_unaccelerated(p_event);

    dx = floor_or_ceil(dx * (double)X_SCALE);
    dy = floor_or_ceil(dy * (double)Y_SCALE);

    libevdev_uinput_write_event(virt_mouse, EV_REL, REL_X, (int)dx);
    libevdev_uinput_write_event(virt_mouse, EV_REL, REL_Y, (int)dy);
    libevdev_uinput_write_event(virt_mouse, EV_SYN, SYN_REPORT, 0);

}

static void handle_pointer_button(struct virtual_mouse *mouse,
    struct libinput_event_pointer *p_event, int type)
{
    struct libevdev_uinput *virt_mouse = mouse->output_device;

    uint32_t button = libinput_event_pointer_get_button(p_event);
    int button_state = libinput_event_pointer_get_button_state(p_event);
    // printf("Button: %x state: %d\n", button, button_state);

    switch (button)
    {
        case BTN_LEFT:
            libevdev_uinput_write_event(virt_mouse, EV_KEY, BTN_LEFT, button_state);
            libevdev_uinput_write_event(virt_mouse, EV_SYN, SYN_REPORT, 0);
            break;
        
        case BTN_RIGHT:
            libevdev_uinput_write_event(virt_mouse, EV_KEY, BTN_RIGHT, button_state);
            libevdev_uinput_write_event(virt_mouse, EV_SYN, SYN_REPORT, 0);
            break;

        default:
            break;
    }
}

static void handle_pointer_scroll(struct virtual_mouse *mouse,
    struct libinput_event_pointer *p_event, int type)
{
    struct libevdev_uinput *virt_mouse = mouse->output_device;
    double y_scroll;
    
    if (libinput_event_pointer_has_axis(p_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
        y_scroll = libinput_event_pointer_get_scroll_value(p_event,
                        LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);

        if (y_scroll != (double)(0)) {
            libevdev_uinput_write_event(virt_mouse, EV_REL, REL_WHEEL, y_scroll > 0 ? -SCROLL : SCROLL);
            libevdev_uinput_write_event(virt_mouse, EV_REL, REL_WHEEL_HI_RES,
                y_scroll > 0 ? -SCROLL_HIGH_RES : SCROLL_HIGH_RES);
            libevdev_uinput_write_event(virt_mouse, EV_SYN, SYN_REPORT, 0);
        }
    }
}

void mouse_handle_events(struct virtual_mouse *mouse) {
    struct libinput *libinput = mouse->libinput_context;
    struct libinput_event *event;
    int event_count = 0;

    while ((event = libinput_get_event(libinput)) != NULL) {
        event_count++;
        int type = libinput_event_get_type(event);
        // printf("Event Type: %d\n", type);
        if (mouse->grabbed) {
            switch (type) {
                case LIBINPUT_EVENT_POINTER_BUTTON:
                    handle_pointer_button(mouse, libinput_event_get_pointer_event(event),
                        LIBINPUT_EVENT_POINTER_BUTTON);
                    break;

                case LIBINPUT_EVENT_POINTER_MOTION:
                    handle_pointer_motion(mouse, libinput_event_get_pointer_event(event),
                        LIBINPUT_EVENT_POINTER_MOTION);
                    break;

                case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
                    handle_pointer_scroll(mouse, libinput_event_get_pointer_event(event),
                        LIBINPUT_EVENT_POINTER_SCROLL_FINGER);
                    break;

                case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
                    break;

                case LIBINPUT_EVENT_GESTURE_HOLD_END:
                    break;

                default:
                    break;
            }
        }
        libinput_event_destroy(event);
    }
    // printf("Event count: %d | call_count :%lld\n", event_count, call_count);
}

static int open_restricted(const char *path, int flags, void *user_data) {
    int fd;
    struct virtual_mouse *mouse = (struct virtual_mouse *)user_data;

    fd = open(path, flags);
    if (fd < 0) {
        fprintf(stderr, "Failed to open file descriptor: %s\n", strerror(errno));
        return -errno;
    }

    mouse->evdev_fd = fd;
    return fd;
}

static void close_restricted(int fd, void *user_data) {
    close(fd);
}

int mouse_create(const char *path, struct virtual_mouse *mouse)
{
    int ret = 0;
    const static struct libinput_interface interface = {
        .open_restricted = open_restricted,
        .close_restricted = close_restricted,
    };

    mouse->libinput_context = libinput_path_create_context(&interface, NULL);
    if (!mouse->libinput_context) {
        fprintf(stderr, "Failed to create libinput context: %s\n", strerror(errno));
        return -errno;
    }
    libinput_set_user_data(mouse->libinput_context, mouse);
    struct libinput_device *device = libinput_path_add_device(mouse->libinput_context, path);
    if (!device) {
        fprintf(stderr, "Failed to add device %s: %s\n", path, strerror(errno));
        ret = -errno;
        goto error_virt_mouse;
    }
    mouse->libinput_fd = libinput_get_fd(mouse->libinput_context);

    ret = setup_virtual_mouse(&mouse->output_device);
    if (ret < 0)
        goto error_virt_mouse;

    libinput_device_config_tap_set_enabled(device, LIBINPUT_CONFIG_TAP_ENABLED);
    // libinput_device_config_tap_set_drag_enabled(device, LIBINPUT_CONFIG_DRAG_ENABLED);
    libinput_device_config_accel_set_speed(device, LIBINPUT_CONFIG_ACCEL_PROFILE_NONE);

    return ret;
error_virt_mouse:
    libinput_unref(mouse->libinput_context);
    return ret;
}

void mouse_close(struct virtual_mouse *mouse)
{
    libevdev_uinput_destroy(mouse->output_device);
    libinput_unref(mouse->libinput_context);
}

void inline mouse_grab_global(struct virtual_mouse *mouse, bool grab)
{
    if (grab)
        ioctl(mouse->evdev_fd, EVIOCGRAB, 1);
    else
        ioctl(mouse->evdev_fd, EVIOCGRAB, 0);

    mouse->grabbed = grab;
}