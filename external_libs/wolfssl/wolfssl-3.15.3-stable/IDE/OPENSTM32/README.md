# wolfSSL STM32F2/F4 Example for Open STM32 Tools System Workbench


## Requirements

* STM32CubeMX: STM32 CubeMX HAL code generation tool - [http://www.st.com/en/development-tools/stm32cubemx.html](http://www.st.com/en/development-tools/stm32cubemx.html)
* SystemWorkbench for STM32 - [http://www.st.com/en/development-tools/sw4stm32.html](http://www.st.com/en/development-tools/sw4stm32.html)

## Setup

1. Using the STM32CubeMX tool, load the `<wolfssl-root>/IDE/OPENSTM32/wolfSTM32.ioc` file.
2. Adjust the HAL options based on your specific micro-controller.
3. Generate source code.
4. Run `SystemWorkbench` and choose a new workspace location for this project.
5. Import `wolfSTM32` project from `<wolfssl-root>/IDE/OPENSTM32/`.
6. Adjust the micro-controller define in `Project Settings -> C/C++ General -> Paths and Symbols -> Symbols -> GNU C`. Example uses `STM32F437xx`, but should be changed to reflect your micro-controller type.
7. Build and Run

Note: You may need to manually copy over the CubeMX HAL files for `stm32f4xx_hal_cryp.c`, `stm32f4xx_hal_cryp_ex.c`, `stm32f4xx_hal_cryp.h`, `stm32f4xx_hal_cryp_ex.h`. Also uncomment the `#define HAL_CRYP_MODULE_ENABLED` line in `stm32f4xx_hal_conf.h`.

## Configuration

The settings for the wolfSTM32 project are located in `<wolfssl-root>/IDE/OPENSTM32/Inc/user_settings.h`.

## Support

For questions please email [support@wolfssl.com](mailto:support@wolfssl.com)
