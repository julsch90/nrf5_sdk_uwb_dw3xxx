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
import struct
import cir_packet

REC_ENABLED = 1
PLOT_ENABLED = 0

serial_port = "COM220"
# serial_port = "COM149"

####################################################
##########  plot data   ############################
####################################################

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

    def update_plot_data(self, cir_mag, cir_offset, cir_count):

        # self.y_max = self.y_max * 0.98
        # self.y_max = max(cir_mag) + max(cir_mag)/4

        if self.y_max < max(cir_mag):
            self.y_max = max(cir_mag) + max(cir_mag)/4

        self.graphWidget.setYRange(0, self.y_max, padding=0)

        # self.x = list( range(-fp_idx, len(cir_mag)-fp_idx, 1) )
        self.x = list( range(cir_offset, cir_offset+cir_count, 1) )
        self.y = cir_mag

        self.data_line.setData(self.x, self.y)  # Update the data.

####################################################
##########  read data   ############################
####################################################

def thread_read_data(arg, update_plot_data):

    ser = serial.Serial(serial_port, timeout=0.002)

    # set filename
    date_time = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = "cir_rec_" + date_time + ".dat"

    # open rec file (to save)
    if REC_ENABLED:
        f = open(filename, "wb")

    while True:

        data_buffer = ser.read(5000)
        # print(len(data_buffer))

        # receiving packet should match the size of 2086 (packet size)
        if len(data_buffer) != 2086:
            # read empty
            ser.read(5000)
            continue

        date_time = datetime.now()
        formatted_date_time = date_time.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]  # Truncate microseconds to milliseconds
        time_bytes = formatted_date_time.encode('utf-8')

        # Ensure the length is fixed (23 characters in this example)
        if len(formatted_date_time) != 23:
            raise ValueError("Formatted time does not have the expected length")

        # dump to rec file
        if REC_ENABLED:
            f.write(time_bytes+data_buffer)

        data_packet = cir_packet.parse_data_packet(data_buffer)
        diag_packet = cir_packet.parse_diag_frame(data_packet["data"])

        if (len(diag_packet["frame_buffer"]) >= struct.calcsize(cir_packet.mhr_data_format)) and diag_packet["frame_len"] == struct.calcsize(cir_packet.mhr_data_format):     #
        # if (len(diag_packet["frame_buffer"]) >= struct.calcsize(cir_packet.mhr_data_format)) and diag_packet["frame_len"] >= struct.calcsize(cir_packet.mhr_data_format):
            frame_buffer = diag_packet["frame_buffer"][0:24]
            mhr_data = cir_packet.parse_mhr_data(frame_buffer)
            # print(mhr_data["magic"])
        else:
            continue
        device_event_cnt = cir_packet.parse_device_event_cnt(diag_packet["event_cnt"])
        rxdiag = cir_packet.parse_dwt_rxdiag(diag_packet["rxdiag"])
        cir_cmplx = cir_packet.parse_cir_buffer(diag_packet["cir_buffer"])
        cir_mag = [abs(s) for s in cir_cmplx]

        if PLOT_ENABLED:
            update_plot_data(cir_mag, diag_packet["cir_start_idx"], diag_packet["cir_len"])




if __name__ == '__main__':

    update_plot_data = []
    if PLOT_ENABLED:
        # main window
        app = QtWidgets.QApplication(sys.argv)
        w = MainWindow()
        w.show()
        update_plot_data = w.update_plot_data

    # start cir thread
    t = threading.Thread(target=thread_read_data, args=("cir", update_plot_data))
    t.start()

    if PLOT_ENABLED:
        # start window
        app.exec_()

    # wait for exit cir thread
    t.join()

    # exit
    sys.exit(0)