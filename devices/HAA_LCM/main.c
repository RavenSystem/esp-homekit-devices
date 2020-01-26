/* (c) 2018-2019 HomeAccessoryKid
 * LifeCycleManager dual app
 * use local.mk to turn it into the LCM otamain.bin app or the otaboot.bin app
 */

#include <stdlib.h>  //for UDPLGP and free
#include <stdio.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <espressif/esp_common.h>

#include <wifi_config.h>
#include <string.h>  //for stdcmp
#include <sysparam.h>

#include <ota.h>
#include <udplogger.h>

void ota_task(void *arg) {
    int holdoff_time=1; //32bit, in seconds
    char* user_repo=NULL;
    char* user_version=NULL;
    char* user_file=NULL;
    char*  new_version=NULL;
    char*  ota_version=NULL;
    char*  lcm_version=NULL;
    signature_t signature;
    extern int active_cert_sector;
    extern int backup_cert_sector;
    int file_size; //32bit
    int have_private_key=0;
    int keyid,foundkey=0;
    char keyname[KEYNAMELEN];
    
    if (ota_boot())
        UDPLGP("OTABOOT ");
    else
        UDPLGP("OTAMAIN ");
    
    UDPLGP("VERSION: %s\n", OTAVERSION); //including the compile time makes comparing binaries impossible, so don't

    ota_init();
    
    if (!active_cert_sector) {
        active_cert_sector = HIGHERCERTSECTOR;
        backup_cert_sector = LOWERCERTSECTOR;
        ota_version = ota_get_version(OTAREPO);
        ota_get_file(OTAREPO, ota_version, CERTFILE, active_cert_sector);
        ota_finalize_file(active_cert_sector);
    }
    UDPLGP("active_cert_sector: 0x%05x\n", active_cert_sector);
    file_size=ota_get_pubkey(active_cert_sector);
    
    if (!ota_get_privkey()) { //have private key
        have_private_key = 1;
        UDPLGP("have private key\n");
        if (ota_verify_pubkey()) {
            ota_sign(active_cert_sector, file_size, &signature, "public-1.key");//use this (old) privkey to sign the (new) pubkey
            vTaskDelete(NULL); //upload the signature out of band to github and flash the new private key to backupsector
        }
    }

    if (ota_boot())
        ota_write_status("0.0.0");  //we will have to get user code from scratch if running ota_boot
    if (!ota_load_user_app(&user_repo, &user_version, &user_file)) { //repo/version/file must be configured
        if (ota_boot()) {
            new_version = ota_get_version(user_repo); //check if this repository exists at all
            if (!strcmp(new_version,"404")) {
                UDPLGP("%s so it does not exist! HALTED FOR EVER!\n", user_repo);
                vTaskDelete(NULL);
            }
        }
        
        for (;;) { //escape from this loop by continue (try again) or break (boots into slot 0)
            UDPLGP("--- entering the loop\n");
            //UDPLGP("%d\n",sdk_system_get_time()/1000);
            //need for a protection against an electricity outage recovery storm
            vTaskDelay(holdoff_time * (1000 / portTICK_PERIOD_MS));
            holdoff_time *= HOLDOFF_MULTIPLIER;
            holdoff_time = holdoff_time < HOLDOFF_MAX ? holdoff_time : HOLDOFF_MAX;
            
            //do we still have a valid internet connexion? dns resolve github... should not be private IP
            
            ota_get_pubkey(active_cert_sector); //in case the LCM update is in a cycle
            
            ota_set_verify(0); //should work even without certificates
            if (lcm_version)
                free(lcm_version);
            if (ota_version)
                free(ota_version);
            
            ota_version = ota_get_version(OTAREPO);
            if (ota_get_hash(OTAREPO, ota_version, CERTFILE, &signature)) { //no certs.sector.sig exists yet on server
                if (have_private_key) {
                    ota_sign(active_cert_sector,SECTORSIZE, &signature, CERTFILE); //reports to console
                    vTaskDelete(NULL); //upload the signature out of band to github and start again
                } else {
                    continue; //loop and try again later
                }
            }
            if (ota_verify_hash(active_cert_sector,&signature)) { //seems we need to download certificates
                if (ota_verify_signature(&signature)) { //maybe an update on the public key
                    keyid = 1;
                    while (sprintf(keyname,KEYNAME,keyid), !ota_get_hash(OTAREPO, ota_version, keyname, &signature)) {
                        if (!ota_verify_signature(&signature)) {
                            foundkey = 1;
                            break;
                        }
                        keyid++;
                    }
                    if (!foundkey)
                        break; //leads to boot=0
                    //we found the head of the chain of pubkeys
                    while (--keyid) {
                        ota_get_file(OTAREPO, ota_version,keyname, backup_cert_sector);
                        if (ota_verify_hash(backup_cert_sector, &signature)) {
                            foundkey = 0;
                            break;
                        }
                        ota_get_pubkey(backup_cert_sector); //get one newer pubkey
                        sprintf(keyname, KEYNAME, keyid);
                        if (ota_get_hash(OTAREPO, ota_version, keyname, &signature)) {
                            foundkey = 0;
                            break;
                        }
                        if (ota_verify_signature(&signature)) {
                            foundkey = 0;
                            break;
                        }
                    }
                    if (!foundkey)
                        break; //leads to boot=0
                }
                ota_get_file(OTAREPO, ota_version, CERTFILE, backup_cert_sector); //CERTFILE=public-1.key
                if (ota_verify_hash(backup_cert_sector, &signature))
                    break; //leads to boot=0
                ota_swap_cert_sector();
                ota_get_pubkey(active_cert_sector);
            } //certificates are good now
            ota_set_verify(1); //reject faked server
            
            if (ota_version)
                free(ota_version);
            
            ota_version=ota_get_version(OTAREPO);
            if (ota_get_hash(OTAREPO, ota_version, CERTFILE, &signature)) { //testdownload, if server is fake will trigger
                //report by syslog?  //trouble, so abort
                break; //leads to boot=0
            }
            if (ota_boot()) { //running the ota-boot software now
                //take care our boot code gets a signature by loading it in boot1sector just for this purpose
                if (ota_get_hash(OTAREPO, ota_version, BOOTFILE, &signature)) { //no signature yet
                    if (have_private_key) {
                        file_size = ota_get_file(OTAREPO,ota_version,BOOTFILE,BOOT1SECTOR);
                        if (file_size <= 0)
                            continue; //try again later
                        
                        ota_finalize_file(BOOT1SECTOR);
                        ota_sign(BOOT1SECTOR, file_size, &signature, BOOTFILE); //reports to console
                        vTaskDelete(NULL); //upload the signature out of band to github and start again
                    }
                }
                //switching over to a new repository, called LCM life-cycle-manager
                lcm_version = ota_get_version(LCMREPO);
                //now get the latest ota main software in boot sector 1
                if (ota_get_hash(LCMREPO, lcm_version, MAINFILE, &signature)) { //no signature yet
                    if (have_private_key) {
                        file_size=ota_get_file(LCMREPO, lcm_version, MAINFILE,BOOT1SECTOR);
                        if (file_size <= 0)
                            continue; //try again later
                        ota_finalize_file(BOOT1SECTOR);
                        ota_sign(BOOT1SECTOR, file_size, &signature, MAINFILE); //reports to console
                        vTaskDelete(NULL); //upload the signature out of band to github and start again
                    } else {
                        continue; //loop and try again later
                    }
                } else { //we have a signature, maybe also the main file?
                    if (ota_verify_hash(BOOT1SECTOR, &signature)) { //not yet downloaded
                        file_size=ota_get_file(LCMREPO, lcm_version, MAINFILE,BOOT1SECTOR);
                        if (file_size <= 0)
                            continue; //try again later
                        
                        if (ota_verify_hash(BOOT1SECTOR, &signature)) continue; //download failed
                        ota_finalize_file(BOOT1SECTOR);
                    }
                } //now file is here for sure and matches hash
                //when switching to LCM we need to introduce the latest public key as used by LCM
                //ota_get_file(LCMREPO,lcm_version,CERTFILE,backup_cert_sector);
                //ota_get_pubkey(backup_cert_sector);
                if (ota_verify_signature(&signature))
                    continue; //this should never happen
                
                ota_temp_boot(); //launches the ota software in bootsector 1
            } else {  //running ota-main software now
                UDPLGP("--- running ota-main software\n");
                //if there is a newer version of ota-main...
                if (ota_compare(ota_version,OTAVERSION)>0) { //set OTAVERSION when running make and match with github
                    ota_get_hash(OTAREPO, ota_version, BOOTFILE, &signature);
                    file_size = ota_get_file(OTAREPO,ota_version,BOOTFILE,BOOT0SECTOR);
                    if (file_size <= 0)
                        continue; //something went wrong, but now boot0 is broken so start over
                    
                    if (ota_verify_signature(&signature))
                        continue; //this should never happen
                    
                    if (ota_verify_hash(BOOT0SECTOR, &signature))
                        continue; //download failed
                    
                    ota_finalize_file(BOOT0SECTOR);
                    break; //leads to boot=0 and starts self-updating/otaboot-app
                    
                } //ota code is up to date
                if (new_version)
                    free(new_version);
                
                new_version = ota_get_version(user_repo);
                if (ota_compare(new_version,user_version) > 0) { //can only upgrade
                    UDPLGP("user_repo=\'%s\' new_version=\'%s\' user_file=\'%s\'\n", user_repo, new_version, user_file);
                    ota_get_hash(user_repo, new_version, user_file, &signature);
                    file_size = ota_get_file(user_repo, new_version, user_file,  BOOT0SECTOR);
                    if (file_size <= 0 || ota_verify_hash(BOOT0SECTOR,&signature))
                        continue; //something went wrong, but now boot0 is broken so start over
                    ota_finalize_file(BOOT0SECTOR); //TODO return status and if wrong, continue
                    ota_write_status(new_version); //we have been successful, hurray!
                } //nothing to update
                break; //leads to boot=0 and starts updated user app
            }
        }
    }
    ota_reboot(); //boot0, either the user program or the otaboot app
    vTaskDelete(NULL); //just for completeness sake, would never get till here
}

void on_wifi_ready() {
    xTaskCreate(udplog_send, "logsend", 256, NULL, 2, NULL);
    xTaskCreate(ota_task,"ota", 4096, NULL, 1, NULL);
    UDPLGP("wifiready-done\n");
}

void user_init(void) {
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_disconnect();
    
//    UDPLGP("\n\n\n\n\n\n\nuser-init-start\n");
//    uart_set_baud(0, 74880);
    uart_set_baud(0, 115200);

    ota_new_layout();
        
    wifi_config_init("HAA", NULL, on_wifi_ready); //expanded it with setting repo-details
//    UDPLGP("user-init-done\n");
}
