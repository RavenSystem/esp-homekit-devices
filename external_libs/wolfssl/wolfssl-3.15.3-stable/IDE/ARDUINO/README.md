### wolfSSL with Arduino

##### Reformatting wolfSSL as a compatible Arduino Library
This is a shell script that will re-organize the wolfSSL library to be 
compatible with Arduino projects. The Arduino IDE requires a library's source
files to be in the library's root directory with a header file in the name of 
the library. This script moves all src/ files to the `IDE/ARDUINO/wolfSSL`
directory and creates a stub header file called `wolfssl.h`.

Step 1: To configure wolfSSL with Arduino, enter the following from within the
wolfssl/IDE/ARDUINO directory:

        `./wolfssl-arduino.sh`


Step 2: Edit `<wolfssl-root>/IDE/ARDUINO/wolfSSL/wolfssl/wolfcrypt/settings.h` uncomment the define for `WOLFSSL_ARDUINO`
If building for Intel Galileo platform also uncomment the define for `INTEL_GALILEO`.

#####Including wolfSSL in Arduino Libraries (for Arduino version 1.6.6)

1. In the Arduino IDE:
    - In `Sketch -> Include Library -> Add .ZIP Library...` and choose the
        `IDE/ARDUNIO/wolfSSL` folder.
    - In `Sketch -> Include Library` choose wolfSSL.

An example wolfSSL client INO sketch exists here: `sketches/wolfssl_client/wolfssl_client.ino`
