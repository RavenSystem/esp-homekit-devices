# SPIFFS ESP8266 File system

This component adds file system support for ESP8266. File system of choice
for ESP8266 is [SPIFFS](https://github.com/pellepl/spiffs). 
It was specifically designed to use with SPI NOR flash on embedded systems.
The main advantage of SPIFFS is wear leveling, which prolongs life time 
of a flash memory.

## Features

 * SPIFFS - embedded file system for NOR flash memory.
 * POSIX file operations.
 * Static files upload to ESP8266 file system within build process.
 * SPIFFS singleton or run-time configuration. Selectable by
`SPIFFS_SINGLETON` variable in Makefile.

## Usage

### Configuration

SPIFFS can be configured in two ways. As a SINGLETON with configuration 
parameters provided at compile-time. And during run-time. The default
configuration is a SINGLETON. The desired configuration can be selected in
program's Makefile with variable `SPIFFS_SINGLETON = 0`.

If SPIFFS is configured in runtime (SPIFFS_SINGLETON = 0) the method 
`esp_spiffs_init` accepts two arguments: address and size. Where address 
and size is the location of SPIFFS region in SPI flash and its size.

In order to use file system in a project the following steps should be made:
 * Add SPIFFS component in a project Makefile `EXTRA_COMPONENTS = extras/spiffs`
 * Specify your flash size in the Makefile `FLASH_SIZE = 32`
 * Specify the start address of file system region on the flash memory
`SPIFFS_BASE_ADDR = 0x200000`. It still needed even for `SPIFFS_SINGLETON = 0`
in order to flash SPIFFS image to the right location during `make flash`.
If no SPIFFS image is going to be flashed this variable can be omitted.
 * If you want to upload files to a file system during flash process specify
the directory with files `$(eval $(call make_spiffs_image,files))`

In the end the Makefile should look like:

```
PROGRAM=spiffs_example
EXTRA_COMPONENTS = extras/spiffs
FLASH_SIZE = 32

SPIFFS_BASE_ADDR = 0x200000
SPIFFS_SIZE = 0x100000

include ../../common.mk

$(eval $(call make_spiffs_image,files))
```

Note: Macro call to prepare SPIFFS image for flashing should go after
`include common.mk`

### Files upload

To upload files to a file system during flash process the following macro is
used:

```
$(eval $(call make_spiffs_image,files))
```

It enables the build of a helper utility **mkspiffs**. This utility creates
an SPIFFS image with files in the specified directory.

The SPIFFS image is created during build stage, after `make` is run.
The image is flashed into the device along with firmware during flash stage,
after `make flash` is run.

**mkspiffs** utility uses the same SPIFFS source code and the same
configuration as ESP8266. So the created image should always be compatible
with SPIFFS on a device.

The build process will catch any changes in files directory and rebuild the
image each time `make` is run.

## Example

### Mount

```
esp_spiffs_init();   // allocate memory buffers
if (esp_spiffs_mount() != SPIFFS_OK) {
    printf("Error mounting SPIFFS\n");
}
```

### Format

Formatting SPIFFS is a little bit awkward. Before formatting SPIFFS must be
mounted and unmounted.
```
esp_spiffs_init();
if (esp_spiffs_mount() != SPIFFS_OK) {
    printf("Error mount SPIFFS\n");
}
SPIFFS_unmount(&fs);  // FS must be unmounted before formating
if (SPIFFS_format(&fs) == SPIFFS_OK) {
    printf("Format complete\n");
} else {
    printf("Format failed\n");
}
esp_spiffs_mount();
```

### POSIX read

Nothing special here.

```
const int buf_size = 0xFF;
uint8_t buf[buf_size];

int fd = open("test.txt", O_RDONLY);
if (fd < 0) {
    printf("Error opening file\n");
}

read(fd, buf, buf_size);
printf("Data: %s\n", buf);

close(fd);
```

### SPIFFS read

SPIFFS interface is intended to be as close to POSIX as possible.

```
const int buf_size = 0xFF;
uint8_t buf[buf_size];

spiffs_file fd = SPIFFS_open(&fs, "other.txt", SPIFFS_RDONLY, 0);
if (fd < 0) {
    printf("Error opening file\n");
}

SPIFFS_read(&fs, fd, buf, buf_size);
printf("Data: %s\n", buf);

SPIFFS_close(&fs, fd);
```

### POSIX write

```
uint8_t buf[] = "Example data, written by ESP8266";

int fd = open("other.txt", O_WRONLY|O_CREAT, 0);
if (fd < 0) {
    printf("Error opening file\n");
}

write(fd, buf, sizeof(buf));

close(fd);
```

## Resources

[SPIFFS](https://github.com/pellepl/spiffs)
