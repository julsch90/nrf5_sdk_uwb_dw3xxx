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
import glob
import os
import csv

PLOT_ENABLED = 0

PACKET_SIZE = 2090


header = ["date_time",                      # date time of whole message (set by computer)
          "status_reg",                     # status register of dw3000 module after received something (check flags via user manual)
          "payload_len",                    # received payload length
          "payload",                        # received payload content
          "payload_mhr_frame_ctrl",         # mac header: parsed frame ctrl (from payload)
          "payload_mhr_seq_num",            # mac header: parsed frame sequence number (from payload)
          "payload_mhr_dst_pan_id",         # mac header: parsed destination pan id (from payload)
          "payload_mhr_dst_addr",           # mac header: parsed destination addr (from payload)
          "payload_mhr_src_addr",           # mac header: parsed source addr (from payload)
          "payload_data_tx_timestamp_ms",   # local tx_timestamp_ms of sender device (parsed from payload)
          "payload_data_idx",               # index number of sender device incremented every transmit (parsed from payload)
          "payload_data_magic",             # magic number of sender device (parsed from payload)
          "payload_fcs",                    # frame check sequence of payload
          "event_cnt_PHE",
          "event_cnt_RSL",
          "event_cnt_CRCG",
          "event_cnt_CRCB",
          "event_cnt_ARFE",
          "event_cnt_OVER",
          "event_cnt_SFDTO",
          "event_cnt_PTO",
          "event_cnt_RTO",
          "event_cnt_TXF",
          "event_cnt_HPW",
          "event_cnt_CRCE",
          "event_cnt_PREJ",
          "event_cnt_SFDD",
          "event_cnt_STSE",
          "dgc",                            # digital gain control (not valid if not set valid preamble code)
          "rssi",                           # rssi (not valid if not set valid preamble code)
          "cir_start_idx",
          "cir_len",
          "ipatovRxStatus",
          "ipatovPeak",
          "ipatovPower",
          "ipatovF1",
          "ipatovF2",
          "ipatovF3",
          "ipatovFpIndex",
          "cir_cmplx"
        ]

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

# read all *.dat files of current directory
def read_dat_files():
    # Get the directory where the script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Create the search pattern for .dat files in the script's directory
    search_pattern = os.path.join(script_dir, '*.dat')

    # Use glob to find all .dat files in the script's directory
    dat_files = glob.glob(search_pattern)

    return dat_files



def thread_read_data(arg, update_plot_data):

    # read all *.dat files of current directory
    dat_files = read_dat_files()

    # Iterate over the list of .dat files and read their contents
    for file_path in dat_files:

        csv_file_path = os.path.splitext(file_path)[0] + '.csv'

        # Open the .csv file for writing
        with open(csv_file_path, 'w', newline='') as csv_file:
            writer = csv.writer(csv_file, delimiter=';')

            writer.writerow(header)

            with open(file_path, 'rb') as file:   # Open file in binary mode
                while True:
                    # Read a chunk of data
                    date_time_chunk = file.read(23)
                    data_chunk = file.read(PACKET_SIZE)

                    # Break the loop if no more data is read (i.e., end of file)
                    if not data_chunk:
                        break

                    formatted_date_time = date_time_chunk.decode('utf-8')
                    data_packet = cir_packet.parse_data_packet(data_chunk)
                    diag_packet = cir_packet.parse_diag_frame(data_packet["data"])
                    mhr_data = cir_packet.parse_mhr_data(diag_packet["frame_buffer"][0:24])
                    device_event_cnt = cir_packet.parse_device_event_cnt(diag_packet["event_cnt"])
                    rxdiag = cir_packet.parse_dwt_rxdiag(diag_packet["rxdiag"])
                    cir_cmplx = cir_packet.parse_cir_buffer(diag_packet["cir_buffer"])
                    cir_mag = [abs(s) for s in cir_cmplx]

                    cir_cmplx_str = cir_packet.complex_list_to_str(cir_cmplx)
                    frame_buffer_str = ''.join(f'{byte:02X}' for byte in diag_packet["frame_buffer"])

                    writer.writerow((
                        formatted_date_time,
                        diag_packet["status_reg"],      # check via status flags
                        diag_packet["frame_len"],
                        frame_buffer_str,
                        # parsed frame/payload of frame_buffer (check if values are garbage on bad rx)
                        mhr_data["frame_ctrl"],
                        mhr_data["seq_num"],
                        mhr_data["dst_pan_id"],
                        mhr_data["dst_addr"],
                        mhr_data["src_addr"],
                        mhr_data["tx_timestamp_ms"],    # local tx_timestamp_ms of sender
                        mhr_data["idx"],
                        mhr_data["magic"],
                        mhr_data["fcs"],
                        # event counter
                        device_event_cnt["PHE"],
                        device_event_cnt["RSL"],
                        device_event_cnt["CRCG"],
                        device_event_cnt["CRCB"],
                        device_event_cnt["ARFE"],
                        device_event_cnt["OVER"],
                        device_event_cnt["SFDTO"],
                        device_event_cnt["PTO"],
                        device_event_cnt["RTO"],
                        device_event_cnt["TXF"],
                        device_event_cnt["HPW"],
                        device_event_cnt["CRCE"],
                        device_event_cnt["PREJ"],
                        device_event_cnt["SFDD"],
                        device_event_cnt["STSE"],
                        # some diag info
                        diag_packet["dgc"],
                        diag_packet["rssi"],
                        diag_packet["cir_start_idx"],
                        diag_packet["cir_len"],
                        rxdiag["ipatovRxStatus"],
                        rxdiag["ipatovPeak"],
                        rxdiag["ipatovPower"],
                        rxdiag["ipatovF1"],
                        rxdiag["ipatovF2"],
                        rxdiag["ipatovF3"],
                        rxdiag["ipatovFpIndex"],
                        # array of complex values (length of cir_len)
                        cir_cmplx_str
                    ))

                    if PLOT_ENABLED:
                        update_plot_data(cir_mag, diag_packet["cir_start_idx"], diag_packet["cir_len"])
                        time.sleep(0.01) # time to plot



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
