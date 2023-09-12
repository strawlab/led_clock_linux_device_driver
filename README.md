# led_clock_linux_device_driver

A kernel-mode device driver to set a GPIO pin to toggle according to the kernel clock.

Developed on Raspberry PI 3B+, Raspbian GNU/Linux 11 (bullseye), Linux 6.1.21-v7+.

## Install prerequisites

```bash
apt install raspberrypi-kernel-headers
```

## Build and test run

```bash
make
insmod ./led_clock.ko
```

## Install

```bash
make
xz led_clock.ko
cp led_clock.ko.xz "/lib/modules/$(uname -r)/"
depmod -a
```

## Run

This inserts the module which has been installed as above.

```bash
modprobe led_clock
```

## Automatically run upon boot

To insert the module on boot, add `led_clock` to the file `/etc/modules`.

## Debug

```bash
dmesg --follow
```

## Stop

```bash
rmmod led_clock
```
