# Example Project for GCC ARM

This example is for Cortex M series, but can be adopted for other architectures.

## Design

* All library options are defined in `Header/user_settings.h`.
* The memory map is located in the linker file in `linker.ld`.
* Entry point function is `reset_handler` in `armtarget.c`.
* The RTC and RNG hardware interface needs implemented for real production applications in `armtarget.c`

## Building

1. Make sure you have `gcc-arm-none-eabi` installed.
2. Modify the `Makefile.common`:
  * Use correct toolchain path `TOOLCHAIN`.
  * Use correct architecture 'ARCHFLAGS' (default is cortex-m0 / thumb). See [GCC ARM Options](https://gcc.gnu.org/onlinedocs/gcc-4.7.3/gcc/ARM-Options.html) `-mcpu=name`.
3. Use `make` and it will build the static library and wolfCrypt test/benchmark and wolfSSL TLS client targets as `.elf` and `.hex` in `/Build`.

### Building for Raspberry Pi

Example `Makefile.common` changes for Rasperry Pi with Cortex-A53:

1. Change ARCHFLAGS to `ARCHFLAGS = -mcpu=cortex-a53 -mthumb -mabi=aapcs` to specify Cortex-A53.
2. Comment out `SRC_LD`, since custom memory map is not applicable.
3. Clear `TOOLCHAIN`, so it will use default `gcc`. Set `TOOLCHAIN = `
4. Comment out `LDFLAGS += --specs=nano.specs --specs=nosys.specs` to disable newlib-nano.

Note: To comment out a line in a Makefile use place `#` in front of line.

### Example Build

```
make clean && make

   text	   data	    bss	    dec	    hex   filename
  50076	   2508	     44	  52628	   cd94   ./Build/WolfCryptTest.elf

   text	   data	    bss	    dec	    hex   filename
  39155	   2508	     60	  41723	   a2fb   ./Build/WolfCryptBench.elf

   text	   data	    bss	    dec	    hex	filename
  70368	    464	     36	  70868	  114d4	./Build/WolfSSLClient.elf
```

## Performace Tuning Options

These settings are located in `Header/user_settings.h`.

* `DEBUG_WOLFSSL`: Undefine this to disable debug logging.
* `NO_ERROR_STRINGS`: Disables error strings to save code space.
* `NO_INLINE`: Disabling inline function saves about 1KB, but is slower.
* `WOLFSSL_SMALL_STACK`: Enables stack reduction techniques to allocate stack sections over 100 bytes from heap.
* `USE_FAST_MATH`: Uses stack based math, which is faster than the heap based math.
* `ALT_ECC_SIZE`: If using fast math and RSA/DH you can define this to reduce your ECC memory consumption.
* `FP_MAX_BITS`: Is the maximum math size (key size * 2). Used only with `USE_FAST_MATH`.
* `ECC_TIMING_RESISTANT`: Enables timing resistance for ECC and uses slightly less memory.
* `ECC_SHAMIR`: Doubles heap usage, but slightly faster
* `RSA_LOW_MEM`: Half as much memory but twice as slow. Uses Non-CRT method for private key.
AES GCM: `GCM_SMALL`, `GCM_WORD32` or `GCM_TABLE`: Tunes performance and flash/memory usage.
* `CURVED25519_SMALL`: Enables small versions of Ed/Curve (FE/GE math).
* `USE_SLOW_SHA`: Enables smaller/slower version of SHA.
* `USE_SLOW_SHA256`: About 2k smaller and about 25% slower
* `USE_SLOW_SHA512`: Over twice as small, but 50% slower
* `USE_CERT_BUFFERS_1024` or `USE_CERT_BUFFERS_2048`: Size of RSA certs / keys to test with. 
* `BENCH_EMBEDDED`: Define this if using the wolfCrypt test/benchmark and using a low memory target.
* `ECC_USER_CURVES`: Allows user to defines curve sizes to enable. Default is 256-bit on. To enable others use `HAVE_ECC192`, `HAVE_ECC224`, etc....
