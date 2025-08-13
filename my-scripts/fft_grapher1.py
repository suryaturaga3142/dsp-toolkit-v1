import sys
import struct
import numpy as np
import serial
from PySide6.QtWidgets import QApplication, QWidget, QVBoxLayout, QPushButton, QLabel, QHBoxLayout
from PySide6.QtCore import QTimer
import pyqtgraph as pg

COM_PORT = "COM8"
BAUD_RATE = 115200

START_FLAG = 0xAA
END_FLAG = 0x55
DEFAULT_NUM_BINS = 512
DEFAULT_SAMPLE_FREQ = 40000

class FFTSerialReader:
    def __init__(self, port, baudrate):
        self.ser = serial.Serial(port, baudrate=baudrate, timeout=0.01)
        self.buffer = bytearray()
        self.started = False
        self.num_bins = DEFAULT_NUM_BINS
        self.sample_freq = DEFAULT_SAMPLE_FREQ
        self.data = None
        self.packet_len = None

    def read_data(self):
        # Read all available data
        new_bytes = self.ser.read(1024)
        if new_bytes:
            self.buffer.extend(new_bytes)

        while True:
            if not self.started:
                # Wait for start flag
                start_idx = self.buffer.find(START_FLAG.to_bytes(1, 'little'))
                if start_idx < 0:
                    # Start flag not found
                    self.buffer.clear()  # Clear to avoid overflow
                    return False
                # Remove preceding junk bytes
                if start_idx > 0:
                    del self.buffer[:start_idx]
                # Now buffer[0] == START_FLAG
                if len(self.buffer) < 6:
                    # Need minimum 6 bytes to read NUM_BINS and SAMPLE_FREQUENCY
                    return False
                # Parse NUM_BINS (2 bytes little endian)
                self.num_bins = self.buffer[1] | (self.buffer[2] << 8)
                # Parse SAMPLE_FREQUENCY (2 bytes little endian)
                self.sample_freq = self.buffer[3] | (self.buffer[4] << 8)
                # Calculate full expected packet length
                # start_flag (1) + num_bins(2) + sample_freq(2) + data(num_bins*2) + end_flag(1)
                self.packet_len = 1 + 2 + 2 + (self.num_bins * 2) + 1
                self.started = True

            # Wait for full packet
            if len(self.buffer) < self.packet_len:
                return False

            # Check end flag
            if self.buffer[self.packet_len - 1] != END_FLAG:
                # Packet corrupted or misaligned, drop first byte and restart
                del self.buffer[0]
                self.started = False
                continue

            # Extract data
            data_start = 5
            data_end = data_start + self.num_bins * 2
            raw_data = self.buffer[data_start:data_end]

            # Unpack uint16 data little endian
            self.data = np.frombuffer(raw_data, dtype=np.uint16)

            # Remove processed packet from buffer
            del self.buffer[:self.packet_len]

            # Reset for next packet
            self.started = False

            return True

class FFTPlotWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("STM32 FFT Magnitude Plot (Binary DMA Stream)")
        self.resize(900, 500)

        # Serial reader setup
        self.reader = FFTSerialReader(COM_PORT, BAUD_RATE)

        # Plot widget setup
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setLabel('bottom', 'Frequency (Hz)')
        self.plot_widget.setLabel('left', 'Magnitude')
        self.plot_widget.setTitle('FFT Magnitude Plot')
        self.plot_widget.showGrid(x=True, y=True)
        self.curve = self.plot_widget.plot(pen='y')

        # Start/Stop button
        self.btn_start_stop = QPushButton("Stop")
        self.btn_start_stop.setCheckable(True)
        self.btn_start_stop.clicked.connect(self.toggle_plotting)
        self.status_label = QLabel("Plotting: ON")

        h_layout = QHBoxLayout()
        h_layout.addWidget(self.btn_start_stop)
        h_layout.addWidget(self.status_label)

        # Layout assembly
        layout = QVBoxLayout()
        layout.addWidget(self.plot_widget)
        layout.addLayout(h_layout)
        self.setLayout(layout)

        # Plotting control
        self.plotting_enabled = True

        # Timer for periodic reading & plotting ~30 FPS
        self.timer = QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(33)

        # Frequency array (initialized later)
        self.freqs = None

    def toggle_plotting(self):
        if self.btn_start_stop.isChecked():
            self.plotting_enabled = False
            self.btn_start_stop.setText("Start")
            self.status_label.setText("Plotting: OFF (data collection continues)")
        else:
            self.plotting_enabled = True
            self.btn_start_stop.setText("Stop")
            self.status_label.setText("Plotting: ON")

    def update(self):
        if self.reader.read_data():
            # Compute frequency vector when new parameters appear
            if self.freqs is None or len(self.freqs) != self.reader.num_bins:
                self.freqs = (np.arange(self.reader.num_bins) *
                              (self.reader.sample_freq / 2) / self.reader.num_bins)
            if self.plotting_enabled:
                self.curve.setData(self.freqs, self.reader.data)
            else:
                # Do not update plot but keep data internally
                pass


def main():
    app = QApplication(sys.argv)
    win = FFTPlotWindow()
    win.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
