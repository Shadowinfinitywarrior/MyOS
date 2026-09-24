# myos Drivers

## Existing drivers
- Storage: `ahci`, `ata`, `nvme`, `virtio_blk`
- Network: `ne2k`, `virtio_net`
- Graphics: `framebuffer`, `vga_gfx`, `fbcon`, `fbterm`, `virtio_gpu`
- Input: `keyboard`, `mouse`, `usb_hid`
- Serial / console: `serial`
- Audio: `ac97`, `speaker`
- USB: `usb`, `xhci`, `usb_hid`
- RTC: `rtc`
- PCI: `pci`
- Screen: `screen`

## New stubs added
- `hpet` - High Precision Event Timer
- `ps2` - PS/2 keyboard/mouse controller
- `ide` - Legacy IDE PIO driver
- `i2c` - I2C bus
- `spi` - SPI bus
- `hda` - Intel HD Audio
- `e1000` - Intel e1000 Ethernet
- `sdmmc` - SD/MMC card interface
- `gpio` - GPIO controller
- `wdt` - Watchdog timer

Each stub provides:
- Header with public API
- Init function with `kprintf` log
- TODO placeholders for hardware access

Integrate stubs into `kernel/init_phase8.c` or driver init table as needed.
