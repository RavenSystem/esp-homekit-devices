# Life-Cycle-Manager (LCM)
Initial install, WiFi settings and over the air firmware upgrades for any esp-open-rtos repository on GitHub  
(c) 2018-2019 HomeAccessoryKid

## Version
LCM has arrived at a stable version with plenty of testing and a strategy how to go forward.
By having introduced the latest-pre-release concept, users (and LCM itself) can test new software before exposing it to production devices.
See the 'How to use it' section.

Meanwhile, at https://github.com/HomeACcessoryKid/ota version 0.3.0 is the version that will transfer a 0.1.0 release used by early starters whenever they trigger an update.

https://github.com/HomeACcessoryKid/ota-demo has been upgraded to offer system-parameter editing features which allows for flexible testing of the LCM code.

## Scope
This is a program that allows any simple repository based on esp-open-rtos on esp8266 to solve its life cycle tasks.
- assign a WiFi AccessPoint for internet connectivity
- specify and install the user app without flashing over cable (once the LCM image is in)
- update the user app over the air by using releases and versions on GitHub

## Non-typical solution
The solution is dedicated to a particular set of repositories and devices, which I consider is worth solving.
- Many ESP8266 devices have only 1Mbyte of flash
- Many people have no interest in setting up a software (web)server to solve their upgrade needs
- Many repositories have no ram or flash available to combine the upgrade routines and the user routines
- For repositories that will be applied by MANY people, a scalable software server is needed
- be able to setup wifi securly while not depending on an electrical connection whenever wifi needs setup *)

If all of the above would not be an issue, the typical solution would be to
- combine the usercode and the upgrade code
- load a full new code image side by side with the old proven image
- have a checkpoint in the code that 'proofs' that the upgrade worked or else it will fall back to the old code
- run a server from a home computer at dedicated moments
- setup the wifi password when electrically connected or send it unencrypted and cross fingers no-one is snooping *)

In my opinion, for the target group, the typical solution doesn't work and so LCM will handle it.
Also it turns out that there are no out-of-the-box solutions of the typical case out there so if you are fine with the limitations of LCM, just enjoy it... or roll your own.  
(PS. the balance is much less black and white but you get the gist)  
*) This feature is not yet implemented (it is quite hard), so 'cross your fingers'.

## Benefits
- Having over the air firmware updates is very important to be able to close security holes and prevent others to introduce malicious code
- The user app only requires a few simple lines of code so no increase in RAM usage or complexity and an overall smaller flash footprint
- Through the use of cryptography throughout the life cycle manager, it is not possible for any outside party to squeeze in any malicious code nor to snoop the password of the WiFi AccessPoint *)
- The fact that it is hosted on GitHub means your code is protected by the https certificates from GitHub and that no matter how many devices are out there, it will scale
- The code is publicly visible and can be audited at all times so that security is at its best
- The user could add their own DigitalSignature (ecDSA) although it is not essential. (feature on todolist)
- The producer of hardware could preinstall the LCM code on hardware thereby allowing the final user to select any relevant repository.
- Many off-the-shelf hardware devices have OTA code that can be highjacked and replaced with LCM so no solder iron or mechanical hacks needed (feature on todolist)

## Can I trust you?
If you feel you need 100% control, you can fork this repository, create your own private key and do the life cycle of the LCM yourself.
But since the code of LCM is public, by audit it is unlikely that malicious events will happen. It is up to you. And if you have ideas how to improve on this subject, please share your ideas in the issue #1 that is open for this reason.

## How to use it
User preparation part  
- in an appropriate part of your code, add these two function calls which will trigger an update:
- ```rboot_set_temp_rom(1); //select the OTA main routine```
- ```sdk_system_restart();  //#include <rboot-api.h>```
- there is a bug in the esp SDK such that if you do not power cycle the chip after flashing, restart is unreliable
- compile your own code and create a signature (see below)
- in the shell, `echo -n x.y.z > latest-pre-release`
- commit this to Git and sync it with GitHub
- Start a release from this commit and take care the version is in x.y.z format
- Attach/Upload the binary and the signature and create the release _as a pre-release_ **)
- Now go to the current 'latest release', ie the non-pre-release one you’re about to improve upon, edit its list of assets and either add or remove and replace the `latest-pre-release` file so that we now have a pointer to the pre-release we created above  
**) except the very first time, you must set it as latest release 

Now test your new code by using a device that you enroll to the pre-release versions (a checkbox in the wifi-setup page).

- If fatal errors are found, just start a new version and leave this one as a pre-release.
- Once a version is approved you can mark it as 'latest release'.
- If a 'latest release' is also the latest release overall, a latest-pre-release is not needed, it points to itself.  

User device setup part  
- clone or fork the LCM repository (or download just the otaboot.bin file)
- wipe out the entire flash (not essential, but cleaner)
- upload these three files:
```
0x0    /Volumes/ESPopenHK/esp-open-rtos//bootloader/firmware_prebuilt/rboot.bin
0x1000 /Volumes/ESPopenHK/esp-open-rtos//bootloader/firmware_prebuilt/blank_config.bin \
0x2000 versions/x.y.zv/otaboot.bin
```
- (or otabootbeta.bin if enrolling in the LCM pre-release testing)
- start the code and either use serial input menu or wait till the Wifi AP starts.  
- set the repository you want to use in your device. yourname/repository  and name of binary
- then select your Wifi AP and insert your password
- once selected, it will take up to 5 minutes for the system to download the ota-main software in the second bootsector and the user code in the 1st boot sector
- you can follow progress on the serial port or use the UDPlogger using the terminal command 'nc -kulnw0 45678'

## Creating a user app DigitalSignature
from the directory where `make` is run execute:
```
openssl sha384 -binary -out firmware/main.bin.sig firmware/main.bin
printf "%08x" `cat firmware/main.bin | wc -c`| xxd -r -p >>firmware/main.bin.sig
```

## How it works
This is a bit outdated design from beginning of 2018, but it still serves to read through the code base.
The actual entry point of the process is the self-updater which is called ota-boot and which is flashed by serial cable.

![](https://github.com/HomeACcessoryKid/life-cycle-manager/blob/master/design-v1.png)

### Concepts
```
Main app(0)
v.x
```
The usercode Main app is running in bootslot 0 at version x

```
boot=slot1
baseURL=repo
version=x
```
This represents that in sector1 used by rboot, we will also store the following info
- baseURL: everything is intended to be relative to https://github.com, so this info is the user/repo part
- version: the version that this repo is currently running at

After this we run the OTA code which will try to deposit in boot slot 0 the latest version of the baseURL repo.

```
t
```
This represents an exponential hold-off to prevent excesive hammering on the github servers. It resets at a power-cycle.

```
download certificate signature
certificate update?
Download Certificates
```
This is a file that contains the checksum of the sector containing three certificates/keys
- public key of HomeACessoryKid that signs the certificate/key sector 
- root CA used by GitHub
- root CA used by the DistributedContentProvider (Amazon for now)

First, the file is intended to be downloaded with server certificate verification activated. If this fails, it is downloaded anyway without verification and server is marked as invalid. Once downloaded, the sha256 checksum of the active sector is compared to the checksum in the signature file. If equal, we move on. If not, we download the updated sector file to the standby sector.

```
signature match?
```
From the sector containing up to date certificates the sha256 hash is signed by the private key of HomeACessoryKid.
Using the available public key, the validity is verified

```
server valid?
```
If in the previous steps the server is marked invalid, we return to the main app in boot slot 0 and we report by syslog to a server (to be determinded) so we learn that github has changed its certificate CA provider and HomeACessoryKid can issue a new certificate sector.

```
new OTA version?
self-updater(0) update OTAapp➔1
checksum OK?
```
Now that the downloading from GitHub has been secured, we can trust whatever we download based on a checksum.
We verify if there is an update of this OTA repo itself? If so, we use a self-updater (part of this repo) to 'self update'. After this we have the latest OTA code.

```
OTA app(1) updates Main app➔0
checksum OK?
```
Using the baseURL info and the version as stored in sector1, the latest binary is found and downloaded if needed. If the checksum does not work out, we return to the OTA app start point considering we cannot run the old code anymore.
But normally we boot the new code and the mission is done.

Note that switching from boot=slot1 to boot=slot0 does not require a reflash



## AS-IS disclaimer and License
While I pride myself to make this software error free and backward compatible and otherwise perfect, this is the 
result of a hobby etc. etc. etc.

See the LICENSE file for license information
