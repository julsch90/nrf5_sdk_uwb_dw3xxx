
import threading
import time
import serial
import sys
import numpy as np
import os
from random import randint
from datetime import datetime


# serial_ports = ["COM146","COM162","COM172","COM173"]
# serial_ports = ["COM146","COM150"]
# serial_ports = ["COM146","COM148"]  # up to max 2 ports working fine
serial_ports = ["COM146"]  # up to max 2 ports working fine


def thread_read_cir_data(port):

    print(port + "_STARTED")

    while True:

        s = serial.Serial(port, timeout=30)

        # set filename
        date_time = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = "cir_rec_" + date_time + "_"  + port + ".csv"

        # open rec file (to save)
        f = open(filename, "w")

        while True:

            read_line = s.readline()

            # print(read_line)
            # continue

            try:

                if len(read_line) <= 0:
                    continue

                # read line: idx;timestamp_ms;device_addr;src_addr;rssi;dgc;ipatov_power;ipatov_accum_count;first_path_idx;cir_count;cir_value[0];cir_value[1];cir_value[2], ... , cir_value[220]\r\n
                read_line = read_line.decode('utf-8').replace("\r\n","\n")

                # split and get values
                read_list = read_line.replace("\n","").split(';')

                idx = float(read_list[0])               # idx/counter
                t = float(read_list[1])                 # timestamp [ms]
                device_addr = int(read_list[2])         # device addr
                src_addr = int(read_list[3])            # addr of transmitter
                rssi = int(read_list[4])                # rssi
                dgc = int(read_list[5])                 # digital gain control [0-6] 6 dBm per step [DW3000 USER MANUAL, 4.7.2, page 48]
                ipatov_power = int(read_list[6])        # ipatov preamble power
                ipatov_accum_count = int(read_list[7])  # ipatov preamble count (accumulated)
                fp_idx = int(read_list[8])              # first path idx inside cir
                cir_count = int(read_list[9])           # number of following cir values

                # continue reading if invalid cir count
                if len(read_list[10:]) != cir_count:
                    print("ERR: invalid cir count:")
                    print(len(read_list[10:]))
                    continue

                cir = [complex(s) for s in read_list[10:]]
                cir_mag = [abs(s) for s in cir]
                # real = [s.real for s in cir]
                # imag = [s.imag for s in cir]

            except ValueError:
                print("ERR:" + ValueError)
                continue


            # add timestamp
            # read_line = datetime.now().strftime("%Y.%m.%d;%H:%M:%S.%f")[:-3] + ";" + read_line
            read_line = datetime.now().strftime("%Y.%m.%d;%H:%M:%S;") + read_line

            # dump to rec file
            f.write(read_line)

            # # save only cir values if valid cir count
            # read_list = read_line.replace("\n","").split(';')
            # cir_count = int(read_list[9])           # number of following cir values
            # if len(read_list[10:]) != cir_count:
            #     continue

            # print(port + "_" + str(read_line.count(';')))





if __name__ == '__main__':

    threads = []

    for port in serial_ports:

        # start cir threads
        t = threading.Thread(target=thread_read_cir_data, args=(port,))
        threads.append(t)
        t.start()



    # wait for all threads to complete
    for t in threads:
        t.join()


    # exit
    sys.exit(0)
