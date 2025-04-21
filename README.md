# virtual_mk
A utility to pass touchpad and keyboard as uinput evdev to guest VMs.

Passing touchpad evdev to Windows guests does not work. So this utility uses libinput provided touchpad events, convert them to mouse evdev events, i.e, BTN_LEFT, BTN_RIGHT, REL_X, REL_Y etc. events.
Keyboard events are also used to grab/ungrab both touchpad and keyboard, to ensure that uinput subsystem does not send events to linux host. 

This is mainly because converting touchpad scroll events to
mouse scroll/high_res scroll does not yield desirable results. In ungrabbed mode, uinput is essentially disabled.

## Dependencies
* libevdev
* libinput

## Build
`make build` to build `virtual_mk` binary <br/>
`make install` to copy udev rules to `/etc/udev/rules.d` and copy binary to `/usr/bin`

## Usage
`virtual_mk` must be run with root priveleges. Grab touchpad and keyboard using LCTRL+RCTRL, before the VM starts. <br/>
This is important, if not done properly qemu's and virtual_mk's grab state will be out of sync.

* Inputs <br/>
  both inputs are mandatory.
    * > --touchpad, -t: /dev/input/eventX (where eventX is evdev for touchpad)
    * > --keyboard, -k: /dev/input/eventX (where eventX is evdev for keyboard)

* Outputs <br/>
  udev rules will automatically create the sysmlinks.
  
    * > /dev/input/by-id/usb-Virtual_Mouse-event-mouse
    * > /dev/input/by-id/usb-Virtual_Keyboard-event-keyboard
    

