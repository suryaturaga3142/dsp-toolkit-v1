import sys
import struct
import numpy as np
import serial
from PySide6.QtWidgets import QApplication, QWidget, QVBoxLayout, QPushButton, QLabel, QHBoxLayout
from PySide6.QtCore import QTimer
import pyqtgraph as pg

'''
Required Cepstrum Packet Format (from mode_cepstrum.c):
- Serial data packets start with a start flag (0xBB)
- Followed by 2 bytes for number of bins (little endian)
- Followed by 2 bytes for sample frequency (little endian)
- Then the Cepstrum magnitude data for each bin (2 bytes each, little endian)
- End with an end flag (0x44)
'''

COM_PORT = "COM8"
BAUD_RATE = 115200

# Flags specific to the Cepstrum DMA stream
START_FLAG = 0xBB
END_FLAG = 0x44
DEFAULT_NUM_BINS = 512
DEFAULT_SAMPLE_FREQ = 40000

class CepstrumSerialReader:
    def __init__(self, port, baudrate):
        try:
            self.ser = serial.Serial(port, baudrate=baudrate, timeout=0.01)
        except serial.SerialException as e:
            print(f"Error opening serial port {port}: {e}")
            print("Please check the COM port and ensure it is not in use.")
            sys.exit(1)
            
        self.buffer = bytearray()
        self.started = False
        self.num_bins = DEFAULT_NUM_BINS
        self.sample_freq = DEFAULT_SAMPLE_FREQ
        self.data = None
        self.packet_len = None

    def read_data(self):
        # Read all available data from the serial port
        try:
            new_bytes = self.ser.read(2048) # Read a larger chunk
            if new_bytes:
                self.buffer.extend(new_bytes)
        except serial.SerialException as e:
            print(f"Serial port error: {e}")
            return False


        while True:
            if not self.started:
                # Wait for start flag
                start_idx = self.buffer.find(START_FLAG.to_bytes(1, 'little'))
                if start_idx < 0:
                    # Start flag not found in the current buffer
                    return False
                
                # Remove preceding junk bytes
                if start_idx > 0:
                    del self.buffer[:start_idx]
                    
                # Now buffer[0] == START_FLAG. Check if we have the full header.
                if len(self.buffer) < 5: # 1 byte start + 2 bytes bins + 2 bytes freq
                    return False
                    
                # Parse NUM_BINS (2 bytes little endian)
                self.num_bins = struct.unpack('<H', self.buffer[1:3])[0]
                # Parse SAMPLE_FREQUENCY (2 bytes little endian)
                self.sample_freq = struct.unpack('<H', self.buffer[3:5])[0]
                
                # Calculate full expected packet length
                # start_flag(1) + num_bins(2) + sample_freq(2) + data(num_bins*2) + end_flag(1)
                self.packet_len = 1 + 2 + 2 + (self.num_bins * 2) + 1
                self.started = True

            # Wait for full packet to arrive
            if len(self.buffer) < self.packet_len:
                return False

            # Check for the correct end flag
            if self.buffer[self.packet_len - 1] != END_FLAG:
                # Packet corrupted or misaligned. Drop the invalid start byte and restart search.
                del self.buffer[0]
                self.started = False
                continue

            # Extract data payload
            data_start = 5
            data_end = data_start + self.num_bins * 2
            raw_data = self.buffer[data_start:data_end]

            # Unpack uint16 data (little endian) into a NumPy array
            self.data = np.frombuffer(raw_data, dtype=np.uint16)

            # Remove the processed packet from the buffer
            del self.buffer[:self.packet_len]

            # Reset state for the next packet
            self.started = False

            return True

class CepstrumPlotWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("STM32 Cepstrum Plot (Binary DMA Stream)")
        self.resize(900, 500)

        # Serial reader setup
        self.reader = CepstrumSerialReader(COM_PORT, BAUD_RATE)

        # Plot widget setup
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setLabel('bottom', 'Quefrency (ms)')
        # The y-axis unit of a cepstrum is sometimes called "Rahmonics"
        self.plot_widget.setLabel('left', 'Magnitude')
        self.plot_widget.setTitle('Real Cepstrum Plot')
        self.plot_widget.showGrid(x=True, y=True)
        self.curve = self.plot_widget.plot(pen='c') # Cyan pen for cepstrum

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

        # Timer for periodic reading & plotting (~30 FPS)
        self.timer = QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(33)

        # Quefrency array (initialized later)
        self.quefs = None

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
            # Compute quefrency vector if parameters change or are uninitialized
            if self.quefs is None or len(self.quefs) != self.reader.num_bins:
                if self.reader.sample_freq > 0:
                    # Quefrency is a time axis: (bin_index / sample_rate)
                    # We multiply by 1000 to display it in milliseconds (ms)
                    self.quefs = (np.arange(self.reader.num_bins) / self.reader.sample_freq) * 1000.0
                else:
                    # Fallback to bin numbers if sample_freq is zero
                    self.quefs = np.arange(self.reader.num_bins)

            if self.plotting_enabled and self.quefs is not None:
                self.curve.setData(self.quefs, self.reader.data)

def main():
    app = QApplication(sys.argv)
    win = CepstrumPlotWindow()
    win.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()