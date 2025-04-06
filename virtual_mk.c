#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <argp.h>

#include <sys/epoll.h>
#include <sys/signalfd.h>

#include <libinput.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#include "config.h"
#include "virtual_mk.h"

static char doc[] = {"A utility to pass touchpad and keyboard as evdev to guest VMs."};

static struct argp_option options[] = {
    {"touchpad", 't', "String", 0, "Touchpad evdev"},
    {"keyboard", 'k', "String", 0, "Keyboard evdev"},
};

struct arguments {
    char *touchpad;
    char *keyboard;
};

static error_t parse_options(int key, char *arg, struct argp_state *state)
{
    struct arguments *a = state->input;
    switch (key)
    {
        case 't':
            a->touchpad = strdup(arg);
            break;
        
        case 'k':
            a->keyboard = strdup(arg);
            break;    
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_options, NULL, doc };

static void signal_handler(struct signalfd_siginfo *signal, struct virtual_mk *v_mk) {
    printf("Interrupted!\n");
    mouse_close(v_mk->mouse);
    keyboard_close(v_mk->keyboard);
    close(v_mk->epoll_fd);
    close(v_mk->signal_fd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    struct epoll_event epoll_event, events[10] = {0};
    struct signalfd_siginfo signal = {0};
    sigset_t mask;
    int epoll_fd, signal_fd, libinput_fd, count = 0, ret = 0;

    struct arguments args = {
        .keyboard = NULL,
        .touchpad = NULL,
    };

    struct virtual_mouse mouse = {
        .libinput_context = NULL,
        .output_device = NULL,
        .libinput_fd = -1,
        .evdev_fd = -1,
    };

    struct virtual_keyboard keyboard = {
        .evdev = NULL,
        .output_device = NULL,
        .fd = -1,
        .grabbed = 0,
    };

    argp_parse(&argp, argc, argv, 0, 0, &args);    
    if (!args.keyboard) {
        fprintf(stderr, "Empty path for keyboard\n");
        return -EINVAL;
    }
    if (!args.touchpad) {
        fprintf(stderr, "Empty path for touchpad\n");
        return -EINVAL;
    }

    struct virtual_mk v_mk = {
        .mouse = &mouse,
        .keyboard = &keyboard,
    };

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    signal_fd = signalfd(-1, &mask, 0);
    if (signal_fd < 0) {
        fprintf(stderr, "Failed to open signal file descriptor: %s\n", strerror(errno));
        return -errno;
    }
    v_mk.signal_fd = signal_fd;

    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        fprintf(stderr, "Failed to open epoll file descriptor: %s\n", strerror(errno));
        ret = -errno;
        goto error_epoll_init;
    }
    v_mk.epoll_fd = epoll_fd;

    ret = mouse_create(args.touchpad, &mouse);
    if (ret < 0) {
        fprintf(stderr, "Failed to create mouse: %s\n", strerror(errno));
        ret = -errno;
        goto error_mouse;
    }

    ret = keyboard_create(args.keyboard, &keyboard);
    if (ret < 0) {
        fprintf(stderr, "Failed to create keyboard: %s\n", strerror(errno));
        ret = -errno;
        goto error_keyboard;
    }

    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = signal_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &epoll_event);

    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = mouse.libinput_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, mouse.libinput_fd, &epoll_event);

    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = keyboard.fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, keyboard.fd, &epoll_event);

    free(args.touchpad);
    free(args.keyboard);

    while(1) {
        count = epoll_wait(epoll_fd, events, 10, -1);
        // printf("No of epoll events: %d\n", count);
        for (int n = 0; n < count; n++) {
            if (events[n].events & EPOLLIN) {
                // printf("events[%d]: %d\n", n,  events[n].data.fd);
                if (events[n].data.fd == signal_fd) {
                    signal_handler(&signal, &v_mk);
                }
                else if (events[n].data.fd == mouse.libinput_fd) {
                    libinput_dispatch(mouse.libinput_context);
                    mouse_handle_events(&mouse);
                }
                else if (events[n].data.fd == keyboard.fd) {
                    keyboard_handle_events(&keyboard);

                    if (keyboard.grabbed && !mouse.grabbed) {
                        // printf("Mouse grab\n");
                        mouse_grab_global(&mouse, true);
                    }
                    else if (!keyboard.grabbed && mouse.grabbed) {
                        // printf("Mouse ungrab\n");
                        mouse_grab_global(&mouse, false);
                    }
                }
            }
        }
    }

error_keyboard:
    mouse_close(&mouse);
error_mouse:
    close(epoll_fd);
error_epoll_init:
    close(signal_fd);
    free(args.touchpad);
    free(args.keyboard);
    return ret;
}