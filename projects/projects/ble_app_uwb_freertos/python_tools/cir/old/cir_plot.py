
import threading
import time
import serial
import sys
import numpy as np
import PyQt5
from PyQt5 import QtWidgets, QtCore
from pyqtgraph import PlotWidget, plot
import pyqtgraph as pg
import os
from random import randint
from datetime import datetime

REC_ENABLED = 0

serial_port = "COM146"
# serial_port = "COM149"


class MainWindow(QtWidgets.QMainWindow):

    def __init__(self, *args, **kwargs):
        super(MainWindow, self).__init__(*args, **kwargs)

        self.graphWidget = pg.PlotWidget()
        self.setCentralWidget(self.graphWidget)

        self.y_max = 0

        self.x = [0]
        self.y = [0]

        self.graphWidget.setTitle("Channel impulse response", size="18pt")
        self.graphWidget.setBackground('w')

        self.graphWidget.showGrid(x=True, y=True)

        pen = pg.mkPen(color='k', width=1)
        self.data_line = self.graphWidget.plot(self.x, self.y, pen=pen)

        # self.timer = QtCore.QTimer()
        # self.timer.setInterval(10)
        # self.timer.timeout.connect(self.update_plot_data)
        # self.timer.start()


    def update_plot_data(self, cir_mag, fp_idx, cir_count):


        # self.y_max = self.y_max * 0.98
        # self.y_max = max(cir_mag) + max(cir_mag)/4

        if self.y_max < max(cir_mag):
            self.y_max = max(cir_mag) + max(cir_mag)/4

        self.graphWidget.setYRange(0, self.y_max, padding=0)

        # self.x = list( range(-fp_idx, len(cir_mag)-fp_idx, 1) )
        self.x = list( range(0, cir_count, 1) )
        self.y = cir_mag

        self.data_line.setData(self.x, self.y)  # Update the data.



def thread_read_cir_data(arg, update_plot_data):

    s = serial.Serial(serial_port, timeout=30)

    # set filename
    date_time = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = "cir_rec_" + date_time + ".csv"

    # open rec file (to save)
    if REC_ENABLED:
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


        # print(fp_idx)
        # print(int(idx))
        # # calc gain factor (TODO: proof of correctness)
        # dgc_factor =  (4**(6-dgc))      #(4**(6-dgc))/10
        # cir_mag = np.array(cir_mag) * (1/dgc_factor)
        # cir_mag = list(cir_mag)

        update_plot_data(cir_mag, fp_idx, cir_count)

        # dump to rec file
        if REC_ENABLED:
            f.write(read_line)

        # print(t)
        # print(cir_mag)
        global stop_threads
        if stop_threads:
            break



if __name__ == '__main__':

    stop_threads = False

    # main window
    app = QtWidgets.QApplication(sys.argv)
    w = MainWindow()
    w.show()

    # start cir thread
    t = threading.Thread(target=thread_read_cir_data, args=("cir", w.update_plot_data))
    t.start()

    # start window
    app.exec_()

    # wait for exit cir thread
    t.join()

    # exit
    sys.exit(0)
