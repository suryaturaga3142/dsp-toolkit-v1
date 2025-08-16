import sys
import struct
import numpy as np
import serial
from PySide6.QtWidgets import QApplication, QWidget, QVBoxLayout, QPushButton, QLabel, QHBoxLayout
from PySide6.QtCore import QTimer
import pyqtgraph as pg

'''
Required format:
- Serial data packets start with a start flag (0xAA)
- Followed by 2 bytes for number of FFT bins (little endian)
- Followed by 2 bytes for sample frequency (little endian)
- Then the FFT magnitude data only for each bin (2 bytes each, little endian)
- End with an end flag (0x55)
'''

# Serial parameters
COM_PORT = "COM8"
BAUD_RATE = 115200

START_FLAG = 0xAA
END_FLAG = 0x55
DEFAULT_NUM_BINS = 512
DEFAULT_SAMPLE_FREQ = 40000

# Maximum columns to show on spectrogram (time window width)
MAX_TIME_COLS = 200

class FFTSpectrogramReader:
    def __init__(self, port, baudrate):
        self.ser = serial.Serial(port, baudrate=baudrate, timeout=0.01)
        self.buffer = bytearray()
        self.started = False
        self.num_bins = DEFAULT_NUM_BINS
        self.sample_freq = DEFAULT_SAMPLE_FREQ
        self.packet_len = None
        self.data = None

    def read_packet(self):
        # Read all available bytes from serial
        new_bytes = self.ser.read(1024)
        if new_bytes:
            self.buffer.extend(new_bytes)

        while True:
            if not self.started:
                start_idx = self.buffer.find(START_FLAG.to_bytes(1, 'little'))
                if start_idx < 0:
                    if len(self.buffer) > 10000:
                        self.buffer.clear()
                    return False
                if start_idx > 0:
                    del self.buffer[:start_idx]
                # Need at least header: start flag(1) + num_bins(2) + sample_freq(2)
                if len(self.buffer) < 5:
                    return False

                # Parse num_bins and sample_freq (both 16-bit little endian)
                self.num_bins = self.buffer[1] | (self.buffer[2] << 8)
                self.sample_freq = self.buffer[3] | (self.buffer[4] << 8)

                # Calculate full packet length: start(1) + num_bins(2) + sample_freq(2) + data(num_bins*2) + end(1)
                self.packet_len = 1 + 2 + 2 + (self.num_bins * 2) + 1
                self.started = True

            if len(self.buffer) < self.packet_len:
                # Wait for full packet
                return False

            # Check end flag
            if self.buffer[self.packet_len - 1] != END_FLAG:
                # Packet corrupted? Remove first byte and reset
                del self.buffer[0]
                self.started = False
                continue

            # Extract FFT data block
            data_start = 5
            data_end = data_start + self.num_bins * 2
            raw_data = self.buffer[data_start:data_end]

            # Unpack uint16 values little endian
            self.data = np.frombuffer(raw_data, dtype=np.uint16)

            # Remove parsed packet from buffer
            del self.buffer[:self.packet_len]

            self.started = False
            return True


class FFTSpectrogramPlotter(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("STM32 FFT Spectrogram (Binary DMA Stream)")
        self.resize(900, 600)

        # Serial FFT data reader
        self.reader = FFTSpectrogramReader(COM_PORT, BAUD_RATE)

        # Layout and plot
        self.layout = QVBoxLayout()
        self.setLayout(self.layout)

        # Spectrogram plot setup
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setLabel('left', 'Frequency (Hz)')
        self.plot_widget.setLabel('bottom', 'Time (frames)')
        self.layout.addWidget(self.plot_widget)

        # ImageItem for heatmap (will display 2D numpy array)
        self.img_item = pg.ImageItem(axisOrder='row-major')
        self.plot_widget.addItem(self.img_item)

        # Color map: blue (low) to yellow (high) over full uint16 range
        colors = [(0, 0, 255), (255, 255, 0)]  # blue to yellow
        cmap = pg.ColorMap(pos=[0.0, 1.0], color=colors)
        lut = cmap.getLookupTable(0.0, 1.0, 256)
        self.img_item.setLookupTable(lut)
        self.img_item.setLevels([0, 65535])

        # Start/Stop button to freeze/unfreeze plot updates
        self.btn_start_stop = QPushButton("Stop")
        self.btn_start_stop.setCheckable(True)
        self.btn_start_stop.clicked.connect(self.toggle_plotting)
        self.status_label = QLabel("Plotting: ON")

        btn_layout = QHBoxLayout()
        btn_layout.addWidget(self.btn_start_stop)
        btn_layout.addWidget(self.status_label)
        self.layout.addLayout(btn_layout)

        self.plotting_enabled = True

        # Spectrogram buffer: 2D array shape (num_bins, time_steps)
        self.spectrogram = None
        self.freqs = None  # frequency axis
        self.time_len = 0  # current number of time columns

        # Timer for plot update ~30 fps
        self.timer = QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(33)

    def toggle_plotting(self):
        if self.btn_start_stop.isChecked():
            self.plotting_enabled = False
            self.btn_start_stop.setText("Start")
            self.status_label.setText("Plotting: OFF (data collection continues)")
        else:
            # Clear spectrogram and start plotting again
            self.plotting_enabled = True
            self.btn_start_stop.setText("Stop")
            self.status_label.setText("Plotting: ON")
            self.spectrogram = None
            self.freqs = None
            self.time_len = 0

    def update(self):
        data_available = False
        while self.reader.read_packet():
            data_available = True
            new_data = self.reader.data
            num_bins = self.reader.num_bins
            sample_freq = self.reader.sample_freq

            # Initialize frequency and spectrogram on first packet or reset
            if self.freqs is None or len(self.freqs) != num_bins:
                self.freqs = (np.arange(num_bins) * (sample_freq / 2) / num_bins)

            if self.spectrogram is None:
                # Initialize empty spectrogram: rows=num_bins, cols=MAX_TIME_COLS
                self.spectrogram = np.zeros((num_bins, MAX_TIME_COLS), dtype=np.uint16)
                self.time_len = 0

            # Shift spectrogram data left by 1 to make room for new column
            if self.time_len < MAX_TIME_COLS:
                self.spectrogram[:, self.time_len] = new_data
                self.time_len += 1
            else:
                self.spectrogram = np.roll(self.spectrogram, -1, axis=1)
                self.spectrogram[:, -1] = new_data

        # If new data arrived and plotting enabled, update image
        if data_available and self.plotting_enabled and self.spectrogram is not None:
            # Show only filled portion if not full yet
            img_to_show = self.spectrogram[:, :self.time_len]

            # img_item expects origin at top-left by default (y=0 top)
            # For frequency axis ascending up, flip vertically to have low freq at bottom
            img_to_show = np.flipud(img_to_show)

            # Set image - pyqtgraph ImageItem expects image shape (rows=y, cols=x)
            self.img_item.setImage(img_to_show, autoLevels=False)

            # Update plot ranges and axis scales
            self.plot_widget.setLimits(xMin=0, xMax=MAX_TIME_COLS, yMin=self.freqs[0], yMax=self.freqs[-1])
            self.plot_widget.setXRange(max(0, self.time_len - MAX_TIME_COLS), self.time_len)
            self.plot_widget.setYRange(self.freqs[0], self.freqs[-1])

            # Set axis scales to map pixels to frequency and time units
            # X axis: time frames (0 to current time_len)
            # Y axis: frequency axis set via setYRange

            # Adjust image transformation so pixel coords match frequency and time ranges:
            # Set ImageItem rect to (x0=0, y0=freq_min, width=cols, height=freq_max - freq_min)
            self.img_item.resetTransform()
            self.img_item.setRect(0, self.freqs[0], self.time_len if self.time_len<MAX_TIME_COLS else MAX_TIME_COLS, self.freqs[-1]-self.freqs[0])

def main():
    app = QApplication(sys.argv)
    win = FFTSpectrogramPlotter()
    win.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
