rBoot - User API for rBoot on the ESP8266
------------------------------------------
by Richard A Burton, richardaburton@gmail.com
http://richard.burtons.org/

This provides a few simple APIs for getting & setting rBoot config, writing data
from OTA updates and communicating with rBoot via the RTC data area. API source
files are in the appcode directory.

Actual OTA network code is implementation specific and no longer included in
rBoot itself, see the rBoot sample projects for this code (which you can then
use in your own projects).

  rboot_config rboot_get_config(void);
    Returns rboot_config (defined in rboot.h) allowing you to modify any values
    in it, including the rom layout.

  bool rboot_set_config(rboot_config *conf);
    Saves the rboot_config structure back to sector 2 of the flash, while
    maintaining the contents of the rest of the sector. You can use the rest of
    this sector for your app settings, as long as you protect this structure
    when you do so.

  uint8 rboot_get_current_rom(void);
    Get the currently selected boot rom (the currently running rom, as long as
    you haven't changed it since boot).

  bool rboot_set_current_rom(uint8 rom);
    Set the current boot rom, which will be used when next restarted.

  rboot_write_status rboot_write_init(uint32 start_addr);
    Call once before starting to pass data to write to the flash. start_addr is
    the address on the SPI flash to write from. Returns a status structure which
    must be passed back on each write. The contents of the structure should not
    be modified by the calling code.

  bool rboot_write_end(rboot_write_status *status);
    Call once after the last rboot_write_flash call to ensure any last bytes are
    written to the flash. If you write data that is not a multiple of 4 bytes in
    length the remaining bytes are saved and written on the next call to
    rboot_write_flash automatically. After the last call to the function, if
     there are outstanding bytes, rboot_write_end will ensure they are written.
	
  bool rboot_write_flash(rboot_write_status *status, uint8 *data, uint16 len);
    Call repeatedly to write data to the flash, starting at the address
    specified on the prior call to rboot_write_init. Current write position is
    tracked automatically. This method is likely to be called each time a packet
    of OTA data is received over the network.

  bool rboot_get_rtc_data(rboot_rtc_data *rtc);
    Get rBoot status/control data from RTC data area. Pass a pointer to a
    rboot_rtc_data structure that will be populated. If valid data is stored
    in the RTC data area (validated by checksum) function will return true.

  bool rboot_set_rtc_data(rboot_rtc_data *rtc);
    Set rBoot status/control data in RTC data area, the checksum will be
    calculated for you.

  bool rboot_set_temp_rom(uint8 rom);
    Call to instruct rBoot to temporarily boot the specified rom on the next
    boot. This is does not update the stored rBoot config on the flash, so after
    another reset it will boot back to the original rom.

  bool rboot_get_last_boot_rom(uint8 *rom);
    Call to find the currently running rom, even if booted as a temporary rom.
    Pass a pointer to a uint8 to populate. Returns true if valid rBoot RTC data
    exists, false otherwise (in which case do not use the value of rom).

  bool rboot_get_last_boot_mode(uint8 *mode);
    Call to find the last (current) boot mode, MODE_STANDARD, MODE_GPIO_ROM or
    MODE_TEMP_ROM. Pass a pointer to a uint8 to populate. Returns true if valid
    rBoot RTC data exists, false otherwise (in which case do not use the value
    of mode).

