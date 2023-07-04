# UWB nRF5_SDK (DWM3001CDK)

## Prepare (Windows 10)
 * Clone this repository
 * Download and install ARM GNU Toolchain [https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
   * recommended GNU_VERSION 9.3.1: [**`gcc-arm-none-eabi-9-2020-q2-update-win32.zip`**](https://developer.arm.com/downloads/-/gnu-rm/9-2020-q2-update)
   * also works with GNU_VERSION 11.3.1: [**`arm-gnu-toolchain-11.3.rel1-mingw-w64-i686-arm-none-eabi.zip`**](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
 * Set **`GNU_INSTALL_ROOT`** and **`GNU_VERSION`** of ARM GNU Toolchain in nRF5_SDK: **`nRF5_SDK_17.1.0_ddde560\components\toolchain\gcc\Makefile.windows`**
 * Download and install J-Link Software and Documentation Pack [https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack](https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack)
 * Set JLINK Path in Makefile of the ble_app_uwb_freertos project: **`\projects\projects\ble_app_uwb_freertos\dwm3001cdk\s140\armgcc\Makefile`**

## Build & Flash (VS Code)

Open directory with vscode: **`projects\projects\ble_app_uwb_freertos`** or via  **`ble_app_uwb_freertos.code-workspace`**

Application code is inside **`app_main.c`** and called by **`main.c`**


Press on keyboard:

<kbd>cmd + shift + b</kbd>

and choose the task

<kbd>build dwm3001cdk</kbd>

<kbd>clean dwm3001cdk</kbd>

<kbd>flash dwm3001cdk</kbd>

<kbd>erase dwm3001cdk</kbd>

## Logging
Using **J-Link RTT Viewer**
