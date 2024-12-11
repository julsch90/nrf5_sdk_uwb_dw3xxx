import threading
import time
import serial
import sys
import numpy as np
import os
from random import randint
from datetime import datetime
import struct
import cir_packet

REC_ENABLED = 1
PLOT_ENABLED = 0

# serial_port = "COM220"
serial_port = "/dev/ttyACM0"

####################################################
##########  plot data   ############################
####################################################

if PLOT_ENABLED:

    import PyQt5
    from PyQt5 import QtWidgets, QtCore
    from pyqtgraph import PlotWidget, plot
    import pyqtgraph as pg

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
##########  handle data   ##########################
####################################################

def handle_packet(data_buffer, f, update_plot_data):

    # print(f"Processing packet of size {len(data_buffer)}")

    date_time = datetime.now()
    formatted_date_time = date_time.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]  # Truncate microseconds to milliseconds
    time_bytes = formatted_date_time.encode('utf-8')

    # Ensure the length is fixed (23 characters in this example)
    if len(formatted_date_time) != 23:
        raise ValueError("Formatted time does not have the expected length")

    try:
        data_packet = cir_packet.parse_data_packet(data_buffer)
        diag_packet = cir_packet.parse_diag_frame(data_packet["data"])
        mhr_data = cir_packet.parse_mhr_data(diag_packet["frame_buffer"][0:24])
        device_event_cnt = cir_packet.parse_device_event_cnt(diag_packet["event_cnt"])
        rxdiag = cir_packet.parse_dwt_rxdiag(diag_packet["rxdiag"])
        cir_cmplx = cir_packet.parse_cir_buffer(diag_packet["cir_buffer"])
        cir_mag = [abs(s) for s in cir_cmplx]
    except:
        print("Error parsing packet (skipping packet)")
        return

    # print(f'seq_num={mhr_data["seq_num"]}')

    # dump to rec file
    if REC_ENABLED:
        f.write(time_bytes+data_buffer)

    if PLOT_ENABLED:
        update_plot_data(cir_mag, diag_packet["cir_start_idx"], diag_packet["cir_len"])


####################################################
##########  read data   ############################
####################################################

stop_threads = False

def is_valid_packet(packet):
    """
    Placeholder for packet validation logic.
    Replace this with actual logic to check if the packet is valid.
    """
    # check for a specific header
    HEADER = b'\xAA\xAA\xAA\xAA'
    return packet.startswith(HEADER)

def find_packet_start(buffer):
    """
    Find the start of the next valid packet in the buffer.
    Replace this with a condition that uniquely identifies the start of your packet.
    """
    for i in range(len(buffer) - cir_packet.PACKET_SIZE):
        if is_valid_packet(buffer[i:i + cir_packet.PACKET_SIZE]):
            return i
    return -1

def thread_read_data(arg, update_plot_data):

    ser = serial.Serial(serial_port, timeout=0.005)

    # set filename
    date_time = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = "cir_rec_" + date_time + ".dat"

    # open rec file (to save)
    if REC_ENABLED:
        f = open(filename, "wb")

    data_buffer = b""

    while not stop_threads:

        new_data = ser.read(cir_packet.PACKET_SIZE)
        data_buffer += new_data

        # if len(new_data):
        #     print(f"len(data_buffer)={len(data_buffer)} added {len(new_data)}")

        # sync packet stream
        if len(data_buffer) >= 2*cir_packet.PACKET_SIZE:
            # Find the start of the next packet
            sync_index = find_packet_start(data_buffer)
            # print(f"check sync packets alignment ({sync_index})")
            if sync_index == -1:
                # Discard data if no valid start found
                print("Dropping data to recover sync.")
                data_buffer = b""
                continue
            data_buffer = data_buffer[sync_index:]

            # Process all complete packets in the data_buffer
            while len(data_buffer) >= cir_packet.PACKET_SIZE:
                handle_packet(data_buffer[:cir_packet.PACKET_SIZE], f, update_plot_data)
                data_buffer = data_buffer[cir_packet.PACKET_SIZE:]

    if REC_ENABLED:
        f.close()



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
    t.daemon = True
    t.start()

    if PLOT_ENABLED:
        # start window
        app.exec_()
    else:
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass

    stop_threads = True
    # wait for exit cir thread
    t.join()

    # exit
    sys.exit(0)
