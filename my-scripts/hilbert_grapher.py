import sys
import numpy as np
import serial
from PySide6.QtWidgets import QApplication, QWidget, QVBoxLayout, QPushButton, QLabel, QHBoxLayout
from PySide6.QtCore import QTimer
import pyqtgraph as pg

'''
Required Hilbert Packet Format (from mode_hilbert.c):
- Start flag: 0xCC
- 2 bytes for number of points (N) (little endian)
- 2 bytes for sample frequency (Fs) (little endian)
- Payload: N * 2 uint16_t values, packed as [amp0, freq0, amp1, freq1, ...]
- End flag: 0x22
'''

COM_PORT = "COM8" 
BAUD_RATE = 115200

# Flags specific to the packed Hilbert DMA stream
START_FLAG = 0xCC
END_FLAG = 0x22
DEFAULT_NUM_POINTS = 1024
DEFAULT_SAMPLE_FREQ = 40000

class HilbertSerialReader:
    def __init__(self, port, baudrate):
        try:
            self.ser = serial.Serial(port, baudrate=baudrate, timeout=0.01)
        except serial.SerialException as e:
            print(f"Error opening serial port {port}: {e}")
            print("Please check the COM port and ensure it is not in use.")
            sys.exit(1)
            
        self.buffer = bytearray()
        self.started = False
        self.num_points = DEFAULT_NUM_POINTS
        self.sample_freq = DEFAULT_SAMPLE_FREQ
        self.amplitude = None
        self.frequency = None
        self.packet_len = None

    def read_data(self):
        try:
            new_bytes = self.ser.read(4096) # Read a larger chunk for this data-heavy stream
            if new_bytes:
                self.buffer.extend(new_bytes)
        except serial.SerialException as e:
            print(f"Serial port error: {e}")
            return False

        while True:
            if not self.started:
                start_idx = self.buffer.find(START_FLAG.to_bytes(1, 'little'))
                if start_idx < 0:
                    return False
                
                if start_idx > 0:
                    del self.buffer[:start_idx]
                    
                if len(self.buffer) < 5: # 1 byte start + 2 bytes points + 2 bytes freq
                    return False
                    
                self.num_points = int.from_bytes(self.buffer[1:3], 'little')
                self.sample_freq = int.from_bytes(self.buffer[3:5], 'little')
                
                # Payload is (num_points * 2) uint16_t values, each 2 bytes long
                payload_bytes = self.num_points * 2 * 2
                self.packet_len = 1 + 2 + 2 + payload_bytes + 1
                self.started = True

            if len(self.buffer) < self.packet_len:
                return False

            if self.buffer[self.packet_len - 1] != END_FLAG:
                del self.buffer[0]
                self.started = False
                continue

            data_start = 5
            data_end = data_start + self.num_points * 2 * 2
            raw_data = self.buffer[data_start:data_end]

            # Unpack all interleaved uint16 data
            all_data = np.frombuffer(raw_data, dtype=np.uint16)

            # De-interleave the data into amplitude and frequency arrays
            self.amplitude = all_data[0::2]  # Even indices are amplitude
            self.frequency = all_data[1::2]  # Odd indices are frequency

            del self.buffer[:self.packet_len]
            self.started = False

            return True

class HilbertPlotWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("STM32 Hilbert Transform Plot (Amplitude & Frequency)")
        self.resize(1200, 800)

        self.reader = HilbertSerialReader(COM_PORT, BAUD_RATE)

        # Use GraphicsLayoutWidget for multiple plots
        self.win = pg.GraphicsLayoutWidget()

        # Top plot for Amplitude
        self.p_amp = self.win.addPlot(row=0, col=0)
        self.p_amp.setLabel('left', 'Amplitude')
        self.p_amp.setTitle('Instantaneous Amplitude')
        self.p_amp.showGrid(x=True, y=True)
        self.curve_amp = self.p_amp.plot(pen='y')

        # Bottom plot for Frequency
        self.p_freq = self.win.addPlot(row=1, col=0)
        self.p_freq.setLabel('left', 'Frequency (Hz)')
        self.p_freq.setLabel('bottom', 'Time (ms)')
        self.p_freq.setTitle('Instantaneous Frequency')
        self.p_freq.showGrid(x=True, y=True)
        self.curve_freq = self.p_freq.plot(pen='c')

        # Link the X axes of the two plots
        self.p_freq.setXLink(self.p_amp)

        # UI Controls
        self.btn_start_stop = QPushButton("Stop")
        self.btn_start_stop.setCheckable(True)
        self.btn_start_stop.clicked.connect(self.toggle_plotting)
        self.status_label = QLabel("Plotting: ON")

        h_layout = QHBoxLayout()
        h_layout.addWidget(self.btn_start_stop)
        h_layout.addWidget(self.status_label)

        layout = QVBoxLayout()
        layout.addWidget(self.win)
        layout.addLayout(h_layout)
        self.setLayout(layout)

        self.plotting_enabled = True

        self.timer = QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(33) # ~30 FPS

        self.time_axis = None

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
            if self.time_axis is None or len(self.time_axis) != self.reader.num_points:
                if self.reader.sample_freq > 0:
                    # Time axis in milliseconds
                    self.time_axis = (np.arange(self.reader.num_points) / self.reader.sample_freq) * 1000.0
                else:
                    self.time_axis = np.arange(self.reader.num_points)

            if self.plotting_enabled and self.time_axis is not None:
                # Update both curves with their respective data
                self.curve_amp.setData(self.time_axis, self.reader.amplitude)
                self.curve_freq.setData(self.time_axis, self.reader.frequency)

def main():
    app = QApplication(sys.argv)
    win = HilbertPlotWindow()
    win.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()