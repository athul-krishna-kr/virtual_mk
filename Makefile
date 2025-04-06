RULES_DIR := /etc/udev/rules.d
TARGET_DIR := /usr/bin

CC = gcc
CFLAGS = -march=x86-64
LDFLAGS = -lm
SRCS = virtual_mk.c keyboard.c mouse.c
TARGET = virtual_mk

LIBEVDEV_CFLAGS = $(shell pkg-config --cflags libevdev)
LIBEVDEV_LIBS = $(shell pkg-config --libs libevdev)
LIBINPUT_LIBS = $(shell pkg-config --libs libinput)

build:
	$(CC) $(CFLAGS) $(LIBEVDEV_CFLAGS) $(SRCS) -o $(TARGET) $(LIBEVDEV_LIBS) $(LIBINPUT_LIBS) $(LDFLAGS)

install:
	@sudo cp 99-virtual_keyboard.rules $(RULES_DIR)/
	@sudo cp 99-virtual_mouse.rules $(RULES_DIR)/
	@sudo udevadm control --reload-rules
	@sudo udevadm trigger
	@sudo cp virtual_mk $(TARGET_DIR)/

uninstall:
	@sudo rm $(RULES_DIR)/99-virtual_keyboard.rules
	@sudo rm $(RULES_DIR)/99-virtual_mouse.rules
	@sudo rm $(TARGET_DIR)/virtual_mk

clean:
	rm -f $(TARGET)