# UWB nRF5_SDK (DWM3001CDK)

## Prepare (Windows 10)
 * Install VS Code
   * Install Extentions:
     * [cortex-debug](https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug) for debugging
     * [cpptools-extension-pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack) IntelliSense for code navigation
 * Clone this repository
 * Download and install ARM GNU Toolchain [https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
   * recommended GNU_VERSION 9.3.1: [`gcc-arm-none-eabi-9-2020-q2-update-win32.zip`](https://developer.arm.com/downloads/-/gnu-rm/9-2020-q2-update) (debugging not working)
   * GNU_VERSION 11.3.1: [`arm-gnu-toolchain-11.3.rel1-mingw-w64-i686-arm-none-eabi.zip`](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (debugging is working)
 * Set `GNU_INSTALL_ROOT` and `GNU_VERSION` of ARM GNU Toolchain in nRF5_SDK: [`nRF5_SDK_17.1.0_ddde560/components/toolchain/gcc/Makefile.windows`](nRF5_SDK_17.1.0_ddde560/components/toolchain/gcc/Makefile.windows)
 * Download and install J-Link Software and Documentation Pack [https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack](https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack)
 * [`ble_app_uwb_freertos`](projects/projects/ble_app_uwb_freertos) project settings:
   * Set `JLINK` path in Makefile: [`projects/projects/ble_app_uwb_freertos/dwm3001cdk/s140/armgcc/Makefile`](projects/projects/ble_app_uwb_freertos/dwm3001cdk/s140/armgcc/Makefile)
   * Set `compilerPath` in [`projects/projects/ble_app_uwb_freertos/.vscode/tasks.json`](projects/projects/ble_app_uwb_freertos/.vscode/tasks.json) for code navigation (IntelliSense) using [cpptools-extension-pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack) extension
   * Set `armToolchainPath` in [`projects/projects/ble_app_uwb_freertos/.vscode/launch.json`](projects/projects/ble_app_uwb_freertos/.vscode/launch.json) for debugging using [cortex-debug](https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug) extension

## Build & Flash (VS Code)

Open directory with vscode: `projects/projects/ble_app_uwb_freertos` or via  `ble_app_uwb_freertos.code-workspace`

Application code is inside `app_main.c` and called by `main.c`


Press on keyboard:

<kbd>crtl/cmd</kbd> + <kbd>shift</kbd> + <kbd>b</kbd>

and select the task

<kbd>build dwm3001cdk</kbd>

<kbd>clean dwm3001cdk</kbd>

<kbd>flash dwm3001cdk</kbd>

<kbd>erase dwm3001cdk</kbd>

## Logging
Using **J-Link RTT Viewer**
