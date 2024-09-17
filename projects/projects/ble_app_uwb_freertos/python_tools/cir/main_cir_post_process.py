
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
import glob
import os

PLOT_ENABLED = 0


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
########## parsing data ############################
####################################################

# Constants
CIR_LEN = 300
# CIR_FP_IDX_OFFSET = 700                     # [600:end] fp inside the cir
CIR_BUFFER_LEN = (CIR_LEN * (3 + 3) + 1)    # 3 byte real + 3 byte imag + 1 byte garbage
FRAME_LEN_MAX = 127                         # Set this according to the actual value you use

# Define format string for `data_packet_t`
# We will use 'I H' for header and data_length, and then dynamically handle `data` and `checksum`.
def parse_data_packet(data):
    # Define format for fixed-size part
    fixed_format = 'I H'  # Header (4 bytes) + data_length (2 bytes)
    fixed_size = struct.calcsize(fixed_format)

    # Check if data is at least as large as the fixed-size part
    if len(data) < fixed_size:
        raise ValueError("Data is too short to unpack with the given format!")

    # Unpack fixed-size part
    header, data_length = struct.unpack(fixed_format, data[:fixed_size])

    # Calculate expected size of `data` field
    data_size = data_length  # The size of `data` is defined by `data_length`
    checksum_size = 4  # Checksum is 4 bytes

    # Total size for `data_packet_t`
    total_size = fixed_size + data_size + checksum_size

    # Check if the entire data matches the expected size
    if len(data) != total_size:
        raise ValueError("Data length does not match the expected size!")

    # Extract the `data` and `checksum` parts
    data_field = data[fixed_size:fixed_size + data_size]
    checksum = struct.unpack('I', data[fixed_size + data_size:])[0]

    return {
        'header': header,
        'data_length': data_length,
        'data': data_field,
        'checksum': checksum
    }


# Define the format string for the struct
mhr_data_format = (
    '='    # Native alignment, but no padding
    '2s'   # frame_ctrl[2] (2 bytes)
    'B'    # seq_num (1 byte)
    'H'    # dst_pan_id (2 bytes)
    'H'    # dst_addr (2 bytes)
    'H'    # src_addr (2 bytes)
    'B'    # id (1 byte)
    'I'    # magic (4 bytes)
    'I'    # idx (4 bytes)
    'I'    # tx_timestamp_ms (4 bytes)
    '2s'   # fcs[2] (2 bytes)
)

def parse_mhr_data(data):
    # Ensure the length of data matches the expected size of the structure
    expected_size = struct.calcsize(mhr_data_format)
    if len(data) != expected_size:
        raise ValueError(f"Data size mismatch. Expected {expected_size} bytes, got {len(data)} bytes.")

    # Unpack the binary data
    unpacked_data = struct.unpack(mhr_data_format, data)

    # Structure the parsed values in a dictionary for better readability
    parsed_values = {
        # 'frame_ctrl': unpacked_data[0]),
        'frame_ctrl': int.from_bytes(unpacked_data[0], byteorder='little'),
        'seq_num': unpacked_data[1],
        'dst_pan_id': unpacked_data[2],
        'dst_addr': unpacked_data[3],
        'src_addr': unpacked_data[4],
        'id': unpacked_data[5],
        'magic': unpacked_data[6],
        'idx': unpacked_data[7],
        'tx_timestamp_ms': unpacked_data[8],
        # 'fcs': unpacked_data[9]
        'fcs': int.from_bytes(unpacked_data[9], byteorder='little')
    }

    return parsed_values



# Define the format string for the struct (24byte)
device_event_cnt_format = (
    '='    # Native alignment, but no padding
    'H'    # PHE (2 bytes)
    'H'    # RSL (2 bytes)
    'H'    # CRCG (2 bytes)
    'H'    # CRCB (2 bytes)
    'B'    # ARFE (1 byte)
    'B'    # OVER (1 byte)
    'H'    # SFDTO (2 bytes)
    'H'    # PTO (2 bytes)
    'B'    # RTO (1 byte)
    'H'    # TXF (2 bytes)
    'B'    # HPW (1 byte)
    'B'    # CRCE (1 byte)
    'H'    # PREJ (2 bytes)
    'H'    # SFDD (2 bytes)
    'B'    # STSE (1 byte)
)

def parse_device_event_cnt(data):
    # Ensure the length of data matches the expected size of the structure
    expected_size = struct.calcsize(device_event_cnt_format)
    if len(data) != expected_size:
        raise ValueError(f"Data size mismatch. Expected {expected_size} bytes, got {len(data)} bytes.")

    # Unpack the binary data
    unpacked_data = struct.unpack(device_event_cnt_format, data)

    # Structure the parsed values in a dictionary for better readability
    parsed_values = {
        'PHE': unpacked_data[0],
        'RSL': unpacked_data[1],
        'CRCG': unpacked_data[2],
        'CRCB': unpacked_data[3],
        'ARFE': unpacked_data[4],
        'OVER': unpacked_data[5],
        'SFDTO': unpacked_data[6],
        'PTO': unpacked_data[7],
        'RTO': unpacked_data[8],
        'TXF': unpacked_data[9],
        'HPW': unpacked_data[10],
        'CRCE': unpacked_data[11],
        'PREJ': unpacked_data[12],
        'SFDD': unpacked_data[13],
        'STSE': unpacked_data[14]
    }

    return parsed_values

# Define the format string for the struct (112byte)
dwt_rxdiag_format = (
    '='     # Native alignment, but no padding
    '5s'    # ipatovRxTime[5] (5 bytes)
    'B'     # ipatovRxStatus (1 byte)
    'H'     # ipatovPOA (2 bytes)

    '5s'    # stsRxTime[5] (5 bytes)
    'H'     # stsRxStatus (2 bytes)
    'H'     # stsPOA (2 bytes)
    '5s'    # sts2RxTime[5] (5 bytes)
    'H'     # sts2RxStatus (2 bytes)
    'H'     # sts2POA (2 bytes)

    '6s'    # tdoa[6] (6 bytes)
    'h'     # pdoa (2 bytes)

    'h'     # xtalOffset (2 bytes)
    'I'     # ciaDiag1 (4 bytes)

    'I'     # ipatovPeak (4 bytes)
    'I'     # ipatovPower (4 bytes)
    'I'     # ipatovF1 (4 bytes)
    'I'     # ipatovF2 (4 bytes)
    'I'     # ipatovF3 (4 bytes)
    'H'     # ipatovFpIndex (2 bytes)
    'H'     # ipatovAccumCount (2 bytes)

    'I'     # stsPeak (4 bytes)
    'I'     # stsPower (4 bytes)
    'I'     # stsF1 (4 bytes)
    'I'     # stsF2 (4 bytes)
    'I'     # stsF3 (4 bytes)
    'H'     # stsFpIndex (2 bytes)
    'H'     # stsAccumCount (2 bytes)

    'I'     # sts2Peak (4 bytes)
    'I'     # sts2Power (4 bytes)
    'I'     # sts2F1 (4 bytes)
    'I'     # sts2F2 (4 bytes)
    'I'     # sts2F3 (4 bytes)
    'H'     # sts2FpIndex (2 bytes)
    'H'     # sts2AccumCount (2 bytes)
)

def parse_dwt_rxdiag(data):
    # Ensure the length of data matches the expected size of the structure
    expected_size = struct.calcsize(dwt_rxdiag_format)
    if len(data) != expected_size:
        raise ValueError(f"Data size mismatch. Expected {expected_size} bytes, got {len(data)} bytes.")

    # Unpack the binary data
    unpacked_data = struct.unpack(dwt_rxdiag_format, data)

    # Structure the parsed values in a dictionary for better readability
    parsed_values = {
        'ipatovRxTime': unpacked_data[0],
        'ipatovRxStatus': unpacked_data[1],
        'ipatovPOA': unpacked_data[2],

        'stsRxTime': unpacked_data[3],
        'stsRxStatus': unpacked_data[4],
        'stsPOA': unpacked_data[5],
        'sts2RxTime': unpacked_data[6],
        'sts2RxStatus': unpacked_data[7],
        'sts2POA': unpacked_data[8],

        'tdoa': unpacked_data[9],
        'pdoa': unpacked_data[10],

        'xtalOffset': unpacked_data[11],
        'ciaDiag1': unpacked_data[12],

        'ipatovPeak': unpacked_data[13],
        'ipatovPower': unpacked_data[14],
        'ipatovF1': unpacked_data[15],
        'ipatovF2': unpacked_data[16],
        'ipatovF3': unpacked_data[17],
        'ipatovFpIndex': unpacked_data[18],
        'ipatovAccumCount': unpacked_data[19],

        'stsPeak': unpacked_data[20],
        'stsPower': unpacked_data[21],
        'stsF1': unpacked_data[22],
        'stsF2': unpacked_data[23],
        'stsF3': unpacked_data[24],
        'stsFpIndex': unpacked_data[25],
        'stsAccumCount': unpacked_data[26],

        'sts2Peak': unpacked_data[27],
        'sts2Power': unpacked_data[28],
        'sts2F1': unpacked_data[29],
        'sts2F2': unpacked_data[30],
        'sts2F3': unpacked_data[31],
        'sts2FpIndex': unpacked_data[32],
        'sts2AccumCount': unpacked_data[33]
    }

    return parsed_values

def parse_diag_frame(data):
    # Define format string for diag_frame_t
    diag_frame_fmt = f'=IH{FRAME_LEN_MAX}s{struct.calcsize(device_event_cnt_format)}s{struct.calcsize(dwt_rxdiag_format)}sHH{CIR_BUFFER_LEN}s'

    # Calculate expected size of the data
    rx_timestamp_ms_size = struct.calcsize(f'I')
    fixed_size = struct.calcsize(f'H')
    frame_buffer_size = FRAME_LEN_MAX
    event_cnt_size = struct.calcsize(device_event_cnt_format)
    rxdiag_size = struct.calcsize(dwt_rxdiag_format)
    cir_info_size = struct.calcsize(f'hh')
    cir_buffer_size = CIR_BUFFER_LEN
    total_size = rx_timestamp_ms_size + fixed_size + frame_buffer_size + event_cnt_size + rxdiag_size + cir_info_size + cir_buffer_size

    if len(data) != total_size:
        raise ValueError("Data length does not match the expected size!")

    # Unpack the fixed-size part
    unpacked = struct.unpack(diag_frame_fmt, data)

    rx_timestamp_ms = unpacked[0]
    frame_len = unpacked[1]
    frame_buffer = unpacked[2]
    event_cnt = unpacked[3]
    rxdiag = unpacked[4]
    cir_start_idx = unpacked[5]
    cir_len = unpacked[6]
    cir_buffer = unpacked[7]

    return {
        'rx_timestamp_ms': rx_timestamp_ms,
        'frame_len': frame_len,
        'frame_buffer': frame_buffer,
        'event_cnt': event_cnt,
        'rxdiag': rxdiag,
        'cir_start_idx': cir_start_idx,
        'cir_len': cir_len,
        'cir_buffer': cir_buffer
    }

def parse_cir_buffer(cir_buffer):
    cir_buffer_idx = 1
    complex_values = []

    cir_buffer_idx = 1

    for i in range(CIR_LEN):
        # Extract the 18-bit i_value and q_value from 3 bytes each
        i_value = (cir_buffer[cir_buffer_idx] |
                   (cir_buffer[cir_buffer_idx + 1] << 8) |
                   ((cir_buffer[cir_buffer_idx + 2] & 0x03) << 16))

        q_value = (cir_buffer[cir_buffer_idx + 3] |
                   (cir_buffer[cir_buffer_idx + 4] << 8) |
                   ((cir_buffer[cir_buffer_idx + 5] & 0x03) << 16))

        # Increment index for next set of I/Q values
        cir_buffer_idx += 6

        # Check if the MSB (bit 17) is set (sign bit in 18-bit two's complement)
        if i_value & 0x020000:  # Check the 18th bit for i_value
            i_value -= 0x040000  # Correct for negative values (sign extend)

        if q_value & 0x020000:  # Check the 18th bit for q_value
            q_value -= 0x040000  # Correct for negative values (sign extend)

        # Append as complex number
        complex_values.append(complex(i_value, q_value))

    return np.array(complex_values)


####################################################
##########  read data   ############################
####################################################

def read_dat_files():
    # Get the directory where the script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Create the search pattern for .dat files in the script's directory
    search_pattern = os.path.join(script_dir, '*.dat')

    # Use glob to find all .dat files in the script's directory
    dat_files = glob.glob(search_pattern)

    return dat_files

def thread_read_data(arg, update_plot_data):


    dat_files = read_dat_files()

    # Iterate over the list of .dat files and read their contents
    for file_path in dat_files:
        with open(file_path, 'rb') as file:   # Open file in binary mode

            csv_file_path = os.path.splitext(file_path)[0] + '.csv'
            f = open(csv_file_path, "w")

            while True:
                # Read a chunk of data
                data_chunk = file.read(2084)

                # Break the loop if no more data is read (i.e., end of file)
                if not data_chunk:
                    break

                data_packet = parse_data_packet(data_chunk)
                diag_packet = parse_diag_frame(data_packet["data"])
                if (len(diag_packet["frame_buffer"]) >= struct.calcsize(mhr_data_format)) and diag_packet["frame_len"] == struct.calcsize(mhr_data_format):
                # if (len(diag_packet["frame_buffer"]) >= struct.calcsize(mhr_data_format)) and diag_packet["frame_len"] >= struct.calcsize(mhr_data_format):
                    frame_buffer = diag_packet["frame_buffer"][0:24]
                    mhr_data = parse_mhr_data(frame_buffer)
                    # print(mhr_data["magic"])
                else:
                    continue
                device_event_cnt = parse_device_event_cnt(diag_packet["event_cnt"])
                rxdiag = parse_dwt_rxdiag(diag_packet["rxdiag"])
                cir_cmplx = parse_cir_buffer(diag_packet["cir_buffer"])
                cir_mag = [abs(s) for s in cir_cmplx]

                line = str(mhr_data["tx_timestamp_ms"])
                f.write(line + '\n')


                if PLOT_ENABLED:
                    update_plot_data(cir_mag, diag_packet["cir_start_idx"], diag_packet["cir_len"])
                    time.sleep(0.01)

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
    t.start()

    if PLOT_ENABLED:
        # start window
        app.exec_()

    # wait for exit cir thread
    t.join()

    # exit
    sys.exit(0)
