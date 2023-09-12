# led_clock_linux_device_driver

A kernel-mode device driver to set a GPIO pin to toggle according to the kernel clock.

Developed on Raspberry PI 3B, Raspbian GNU/Linux 11 (bullseye), Linux 6.1.21-v7+.

## Build and run

```bash
make
insmod ./led_clock.ko
```

## Debug

```bash
dmesg --follow
```

## Stop

```bash
rmmod led_clock
```