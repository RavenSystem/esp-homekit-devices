# mkspiffs Create spiffs image

mkspiffs is a command line utility to create an image of SPIFFS in order
to write to flash.

## Usage

mkspiffs will be built automatically if you include the following line in your
makefile:

```
$(eval $(call make_spiffs_image,files))
```

where *files* is the directory with files that should go into SPIFFS image.

mkspiffs can be built separately. Simply run `make` in the mkspiffs directory.

To manually generate SPIFFS image from a directory SPIFFS configuration must be
provided as command line arguments.

Arguments:
 * -D Directory with files that will be put in SPIFFS image.
 * -f SPIFFS image file name.
 * -s SPIFFS size.
 * -p Logical page size.
 * -b Logical block size.

All arguments are mandatory.

For example:

```
mkspiffs -D ./my_files -f spiffs.img -s 0x10000 -p 256 -b 8192
```
