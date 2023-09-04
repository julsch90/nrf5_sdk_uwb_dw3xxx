
UWB Config: Channel 5, PRF 64M, Preamble Length 128, PAC 8, Preamble code 42, Data Rate 6.8M, (UWB Center Frequency: 6489.6 MHz, UWB Bandwidth: 499.2 MHz)


There 4 devices (4 images/hex files). Each device sending 8 times per second messages and is received by the other devices and print out the cir via usb.
Reading the values via usb/serial on the computer side should be fast otherwise data will be missing (best way dump direct into a file like in the python sctript).

USB serial output of the devices will be in csv format like following lines (every line has 221 cir_values):

idx;timestamp_ms;device_addr;src_addr;rssi;dgc;ipatov_power;ipatov_accum_count;first_path_idx;cir_count;cir_value[0];cir_value[1];cir_value[2], ... , cir_value[cir_count]\r\n


[FIRMWARE UPDATE]

1.) Download "J-Link Software": [https://www.segger.com/products/debug-probes/j-link/technology/flash-download/]
2.) Use "J-Flash LITE" and choose device "NRF52833_xxAA" and the data hex file e.g. "nrf52833_xxaa_cir_addr_1.hex" (using only one image of each board and using hex with addr 1 is as minimum required because it's the clock initiator of the network)
3.) Connect the DWM3001 board to the On-Board J-Link (lower usb port) connection program device.