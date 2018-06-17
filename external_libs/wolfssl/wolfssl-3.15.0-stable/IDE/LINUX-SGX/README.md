# Static Library: Building libwolfssl.sgx.static.lib.a for use with SGX Enclaves

### Requirements:
This code was created to use Intel's SGX hardware. It is expected that the user has gone through the steps of both turning on the hardware in bios if needed and has installed the necesary software from Intel to make use of the hardware. (https://software.intel.com/en-us/sgx) If these steps have not been done then it is expected that the user is familure with simulation software being used in place of hardware.

### Overview and Build:
This project creates a static library to then link with Enclaves. A simple example of an Enclave linking to the created wolfSSL library can be found in wolfssl-examples on github. This project has been tested with gcc 5.4.0 on Ubuntu 16.04.

To create the static library, simply call make:

`make -f sgx_t_static.mk all`

To clean the static library and compiled objects use the provided clean script:

`clean.sh`

This will create a local static library, libwolfssl.sgx.static.lib.a, that can be linked with SGX enclaves to access wolfSSL APIs using SGX hardware.

### Customization:
    To enable wolfssl debug, add CFLAGS=-DDEBUG_WOLFSSL.
    To enable wolfssl benchmark tests with enclave, specify: HAVE_WOLFSSL_BENCHMARK at build
    To enable wolfcrypt testsuite with enclave, specify: HAVE_WOLFSSL_TEST at build

For example, to enable all three:
`make -f sgx_t_static.mk CFLAGS=-DDEBUG_WOLFSSL HAVE_WOLFSSL_BENCHMARK=1 HAVE_WOLFSSL_TEST=1`

NOTE: This more customized step has been provided for easier execution in the
      script `build.sh`

### Limitations:
    Single Threaded (multiple threaded applications have not been tested)
    AES-NI use with SGX has not been added in yet
