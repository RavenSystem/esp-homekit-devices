# Open Espressif Libs

These are functional recreations of the MIT licensed binary Espressif SDK libraries found in `lib`. They keep the same functionality as the SDK libraries (possibly with bugfixes or other minor tweaks), but are compiled from source.

Most of the reverse engineering work so far has been by Alex Stewart (@foogod).

See http://esp8266-re.foogod.com/wiki/ for more technical details of SDK library internals.

# Disabling

The open ESP libs are compiled in by default, and they automatically replace any binary SDK symbols (functions, etc.) with the same names.

To compile using the binary SDK libraries only, override the COMPONENTS list in parameters.mk to remove the open_esplibs component, or add -DOPEN_ESPLIBS=0 to CPPFLAGS.

To selectively replace some functionality with binary SDK functionality for debugging, edit the header file open_esplibs/include/open_esplibs.h
