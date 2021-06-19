*NOTE: This rboot-ota and the TFTP server ota-tftp.h are specific to esp-open-rtos. The below Makefile is from the upstream rboot-ota project and the rboot code is taken from #75ca33b.*

For more details on OTA in esp-open-rtos, see https://github.com/SuperHouse/esp-open-rtos/wiki/OTA-Update-Configuration


rBoot - User API and OTA support for rBoot on the ESP8266
---------------------------------------------------------
by Richard A Burton, richardaburton@gmail.com
http://richard.burtons.org/

See rBoot readme.txt for how to create suitable rom images to update OTA.

This provides a couple of simple APIs for getting rBoot config and one for over
the air updates (OTA):

  rboot_config rboot_get_config();
    Returns rboot_config (defined in rboot.h) allowing you to modify any values
    in it, including the rom layout.

  bool rboot_set_config(rboot_config *conf);
    Saves the rboot_config structure back to sector 2 of the flash, while
    maintaining the contents of the rest of the sector. You can use the rest of
    this sector for your app settings, as long as protect this structure when
    you do so.

  uint8 rboot_get_current_rom();
    Get the currently selected boot rom (the currently running rom, as long as
	you haven't changed it since boot).

  bool rboot_set_current_rom(uint8 rom);
    Set the current boot rom, which will be used when next restarted.

  bool rboot_ota_start(rboot_ota *ota);
    Starts an OTA. Pass it an rboot_ota structure with appropriate options. This
    function works very much like the SDK libupgrade code you may already be
	using, very few changes will be needed to switch to this.
    
    typedef struct {
        uint8 ip[4];
        uint16 port;
        uint8 *request;
        uint8 rom_slot;
        uint32 rom_addr;
        ota_callback callback;
    } rboot_ota;
    
    - ip is the ip of the OTA http server.
    - port is the server port (e.g. 80).
    - request is the http request to send.
    - rom_slot is the rom slot to write to (numbered from zero), or set to
      FLASH_BY_ADDR to flash by address instead of by rom slot.
    - rom_addr is the flash address to write to when using FLASH_BY_ADDR,
      otherwise it is ignored.
    - call back is the user code function to call on completion of OTA.
