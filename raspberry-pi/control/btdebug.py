import json
import time
import csv
from collections import deque

import serial
import serial.tools.list_ports

from PyQt5 import QtCore, QtWidgets
import pyqtgraph as pg


def list_serial_ports():
    ports = []
    for p in serial.tools.list_ports.comports():
        ports.append((p.device, p.description))
    return ports


class SerialWorker(QtCore.QThread):
    tele_received = QtCore.pyqtSignal(dict)
    status_received = QtCore.pyqtSignal(dict)
    ok_received = QtCore.pyqtSignal(dict)
    err_received = QtCore.pyqtSignal(dict)
    log_received = QtCore.pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.ser = None
        self._running = False
        self._write_queue = deque()

    def connect_port(self, port, baud=9600):
        self.ser = serial.Serial(port, baudrate=baud, timeout=0.1)

    def close_port(self):
        if self.ser:
            try:
                self.ser.close()
            except:
                pass
        self.ser = None

    def send_line(self, line: str):
        self._write_queue.append(line.strip() + "\n")

    def run(self):
        self._running = True
        buf = ""
        while self._running:
            # write pending
            if self.ser and self.ser.is_open and self._write_queue:
                try:
                    s = self._write_queue.popleft()
                    self.ser.write(s.encode("utf-8", errors="ignore"))
                except Exception as e:
                    self.log_received.emit(f"[SERIAL WRITE ERROR] {e}")

            # read
            if self.ser and self.ser.is_open:
                try:
                    data = self.ser.read(4096)
                    if data:
                        buf += data.decode("utf-8", errors="ignore")
                        while "\n" in buf:
                            line, buf = buf.split("\n", 1)
                            line = line.strip()
                            if not line:
                                continue
                            # Arduino校准时会直接发数字行，也会发JSON行
                            if line.startswith("{") and line.endswith("}"):
                                try:
                                    msg = json.loads(line)
                                    t = msg.get("type")
                                    if t == "tele":
                                        self.tele_received.emit(msg)
                                    elif t == "status":
                                        self.status_received.emit(msg)
                                    elif t == "ok":
                                        self.ok_received.emit(msg)
                                    elif t == "err":
                                        self.err_received.emit(msg)
                                    else:
                                        self.log_received.emit(f"[JSON] {msg}")
                                except Exception:
                                    self.log_received.emit(f"[BAD JSON] {line}")
                            else:
                                # 例如校准输出 i
                                self.log_received.emit(line)
                except Exception as e:
                    self.log_received.emit(f"[SERIAL READ ERROR] {e}")
            time.sleep(0.005)

    def stop(self):
        self._running = False
        self.wait(500)


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Line Follower Debug GUI (BT)")

        self.worker = SerialWorker()
        self.worker.tele_received.connect(self.on_tele)
        self.worker.status_received.connect(self.on_status)
        self.worker.ok_received.connect(lambda m: self.append_log(f"[OK] {m.get('msg','')}"))
        self.worker.err_received.connect(lambda m: self.append_log(f"[ERR] {m.get('msg','')}"))
        self.worker.log_received.connect(self.append_log)

        self.recording = False
        self.csv_file = None
        self.csv_writer = None

        # buffers
        self.N = 400
        self.ts = deque(maxlen=self.N)
        self.err = deque(maxlen=self.N)
        self.lost = deque(maxlen=self.N)
        self.sensors = [deque(maxlen=self.N) for _ in range(8)]
        self.Lout = deque(maxlen=self.N)
        self.Rout = deque(maxlen=self.N)

        self.build_ui()
        self.refresh_ports()

        # UI timer for plot refresh
        self.plot_timer = QtCore.QTimer()
        self.plot_timer.timeout.connect(self.refresh_plots)
        self.plot_timer.start(50)

    def build_ui(self):
        cw = QtWidgets.QWidget()
        self.setCentralWidget(cw)
        layout = QtWidgets.QGridLayout(cw)

        # --- connection panel ---
        self.portCombo = QtWidgets.QComboBox()
        self.refreshBtn = QtWidgets.QPushButton("Refresh Ports")
        self.connectBtn = QtWidgets.QPushButton("Connect")
        self.disconnectBtn = QtWidgets.QPushButton("Disconnect")
        self.disconnectBtn.setEnabled(False)

        self.baudCombo = QtWidgets.QComboBox()
        self.baudCombo.addItems(["9600", "115200"])
        self.baudCombo.setCurrentText("9600")

        layout.addWidget(QtWidgets.QLabel("Port:"), 0, 0)
        layout.addWidget(self.portCombo, 0, 1)
        layout.addWidget(QtWidgets.QLabel("Baud:"), 0, 2)
        layout.addWidget(self.baudCombo, 0, 3)
        layout.addWidget(self.refreshBtn, 0, 4)
        layout.addWidget(self.connectBtn, 0, 5)
        layout.addWidget(self.disconnectBtn, 0, 6)

        self.refreshBtn.clicked.connect(self.refresh_ports)
        self.connectBtn.clicked.connect(self.connect_serial)
        self.disconnectBtn.clicked.connect(self.disconnect_serial)

        # --- control buttons ---
        self.runBtn = QtWidgets.QPushButton("RUN")
        self.stopBtn = QtWidgets.QPushButton("STOP")
        self.getBtn = QtWidgets.QPushButton("GET STATUS")
        self.loadBtn = QtWidgets.QPushButton("LOAD EEPROM")
        self.saveBtn = QtWidgets.QPushButton("SAVE EEPROM")
        self.calBtn = QtWidgets.QPushButton("CAL 50 (and Save)")

        self.streamChk = QtWidgets.QCheckBox("Stream ON")
        self.streamChk.setChecked(True)
        self.rateSpin = QtWidgets.QSpinBox()
        self.rateSpin.setRange(5, 1000)
        self.rateSpin.setValue(50)
        self.rateSpin.setSuffix(" ms")

        self.runBtn.clicked.connect(lambda: self.send("RUN 1"))
        self.stopBtn.clicked.connect(lambda: self.send("RUN 0"))
        self.getBtn.clicked.connect(lambda: self.send("GET"))
        self.loadBtn.clicked.connect(lambda: self.send("LOAD"))
        self.saveBtn.clicked.connect(lambda: self.send("SAVE"))
        self.calBtn.clicked.connect(lambda: self.send("CAL 50"))
        self.streamChk.toggled.connect(lambda v: self.send(f"STREAM {1 if v else 0}"))
        self.rateSpin.valueChanged.connect(lambda v: self.send(f"RATE {v}"))

        row = 1
        layout.addWidget(self.runBtn, row, 0)
        layout.addWidget(self.stopBtn, row, 1)
        layout.addWidget(self.getBtn, row, 2)
        layout.addWidget(self.loadBtn, row, 3)
        layout.addWidget(self.saveBtn, row, 4)
        layout.addWidget(self.calBtn, row, 5)
        layout.addWidget(self.streamChk, row, 6)
        layout.addWidget(self.rateSpin, row, 7)

        # --- parameter controls ---
        self.kpSpin = QtWidgets.QDoubleSpinBox(); self.kpSpin.setRange(0, 200); self.kpSpin.setDecimals(4); self.kpSpin.setSingleStep(0.1)
        self.kiSpin = QtWidgets.QDoubleSpinBox(); self.kiSpin.setRange(0, 50);  self.kiSpin.setDecimals(4); self.kiSpin.setSingleStep(0.01)
        self.kdSpin = QtWidgets.QDoubleSpinBox(); self.kdSpin.setRange(0, 200); self.kdSpin.setDecimals(4); self.kdSpin.setSingleStep(0.1)
        self.baseSpin = QtWidgets.QSpinBox(); self.baseSpin.setRange(0, 100)
        self.maxSpin  = QtWidgets.QSpinBox(); self.maxSpin.setRange(0, 100)
        self.lineSpin = QtWidgets.QSpinBox(); self.lineSpin.setRange(0, 1000)
        self.turnSpin = QtWidgets.QSpinBox(); self.turnSpin.setRange(0, 100)

        self.applyBtn = QtWidgets.QPushButton("Apply Params")
        self.applyBtn.clicked.connect(self.apply_params)

        pr = 2
        layout.addWidget(QtWidgets.QLabel("Kp"), pr, 0); layout.addWidget(self.kpSpin, pr, 1)
        layout.addWidget(QtWidgets.QLabel("Ki"), pr, 2); layout.addWidget(self.kiSpin, pr, 3)
        layout.addWidget(QtWidgets.QLabel("Kd"), pr, 4); layout.addWidget(self.kdSpin, pr, 5)
        pr += 1
        layout.addWidget(QtWidgets.QLabel("Base"), pr, 0); layout.addWidget(self.baseSpin, pr, 1)
        layout.addWidget(QtWidgets.QLabel("Max"),  pr, 2); layout.addWidget(self.maxSpin,  pr, 3)
        layout.addWidget(QtWidgets.QLabel("LINE_TH"), pr, 4); layout.addWidget(self.lineSpin, pr, 5)
        layout.addWidget(QtWidgets.QLabel("Turn"), pr, 6); layout.addWidget(self.turnSpin, pr, 7)
        pr += 1
        layout.addWidget(self.applyBtn, pr, 0, 1, 2)

        # --- plots ---
        self.plotWidget1 = pg.PlotWidget(title="Sensors (8 channels)")
        self.plotWidget2 = pg.PlotWidget(title="Error / Lost / Motor Outputs")

        layout.addWidget(self.plotWidget1, 5, 0, 1, 8)
        layout.addWidget(self.plotWidget2, 6, 0, 1, 8)

        self.sensorCurves = [self.plotWidget1.plot(pen=pg.mkPen(width=1)) for _ in range(8)]
        self.errCurve = self.plotWidget2.plot(pen=pg.mkPen(width=2), name="err")
        self.lostCurve = self.plotWidget2.plot(pen=pg.mkPen(width=1), name="lost*10")
        self.LCurve = self.plotWidget2.plot(pen=pg.mkPen(width=1), name="L")
        self.RCurve = self.plotWidget2.plot(pen=pg.mkPen(width=1), name="R")

        # --- record ---
        self.recBtn = QtWidgets.QPushButton("Start Recording CSV")
        self.stopRecBtn = QtWidgets.QPushButton("Stop Recording")
        self.stopRecBtn.setEnabled(False)

        self.recBtn.clicked.connect(self.start_record)
        self.stopRecBtn.clicked.connect(self.stop_record)

        layout.addWidget(self.recBtn, 7, 0, 1, 2)
        layout.addWidget(self.stopRecBtn, 7, 2, 1, 2)

        # --- log ---
        self.log = QtWidgets.QPlainTextEdit()
        self.log.setReadOnly(True)
        layout.addWidget(self.log, 8, 0, 1, 8)

    def append_log(self, s):
        self.log.appendPlainText(s)

    def refresh_ports(self):
        self.portCombo.clear()
        ports = list_serial_ports()
        for dev, desc in ports:
            self.portCombo.addItem(f"{dev}  |  {desc}", userData=dev)

    def connect_serial(self):
        if self.worker.isRunning():
            return
        dev = self.portCombo.currentData()
        if not dev:
            self.append_log("[INFO] No port selected.")
            return
        baud = int(self.baudCombo.currentText())
        try:
            self.worker.connect_port(dev, baud=baud)
            self.worker.start()
            self.append_log(f"[INFO] Connected {dev} @ {baud}")
            self.connectBtn.setEnabled(False)
            self.disconnectBtn.setEnabled(True)
            # 拉一次状态
            QtCore.QTimer.singleShot(200, lambda: self.send("GET"))
            QtCore.QTimer.singleShot(250, lambda: self.send(f"STREAM {1 if self.streamChk.isChecked() else 0}"))
            QtCore.QTimer.singleShot(300, lambda: self.send(f"RATE {self.rateSpin.value()}"))
        except Exception as e:
            self.append_log(f"[CONNECT ERROR] {e}")

    def disconnect_serial(self):
        try:
            if self.worker.isRunning():
                self.worker.stop()
            self.worker.close_port()
        finally:
            self.connectBtn.setEnabled(True)
            self.disconnectBtn.setEnabled(False)
            self.append_log("[INFO] Disconnected")

    def send(self, line):
        if not self.worker.isRunning():
            self.append_log("[INFO] Not connected.")
            return
        self.worker.send_line(line)

    def apply_params(self):
        self.send(f"SET KP {self.kpSpin.value():.4f}")
        self.send(f"SET KI {self.kiSpin.value():.4f}")
        self.send(f"SET KD {self.kdSpin.value():.4f}")
        self.send(f"SET BASE {self.baseSpin.value()}")
        self.send(f"SET MAX {self.maxSpin.value()}")
        self.send(f"SET LINE {self.lineSpin.value()}")
        self.send(f"SET TURN {self.turnSpin.value()}")

    def on_status(self, msg):
        # 同步到界面
        try:
            self.kpSpin.setValue(float(msg.get("kp", self.kpSpin.value())))
            self.kiSpin.setValue(float(msg.get("ki", self.kiSpin.value())))
            self.kdSpin.setValue(float(msg.get("kd", self.kdSpin.value())))
            self.baseSpin.setValue(int(msg.get("base", self.baseSpin.value())))
            self.maxSpin.setValue(int(msg.get("max", self.maxSpin.value())))
            self.lineSpin.setValue(int(msg.get("line", self.lineSpin.value())))
            self.turnSpin.setValue(int(msg.get("turn", self.turnSpin.value())))
            self.streamChk.setChecked(bool(msg.get("stream", 1)))
            self.rateSpin.setValue(int(msg.get("rate", self.rateSpin.value())))
        except Exception as e:
            self.append_log(f"[STATUS PARSE ERROR] {e}")

    def on_tele(self, msg):
        t_ms = int(msg.get("t", 0))
        s = msg.get("s", [0]*8)
        err = float(msg.get("err", 0.0))
        lost = int(msg.get("lost", 0))
        L = int(msg.get("L", 0))
        R = int(msg.get("R", 0))

        self.ts.append(t_ms / 1000.0)
        self.err.append(err)
        self.lost.append(lost)
        self.Lout.append(L)
        self.Rout.append(R)
        for i in range(8):
            self.sensors[i].append(int(s[i]) if i < len(s) else 0)

        # record
        if self.recording and self.csv_writer:
            row = [t_ms] + [self.sensors[i][-1] for i in range(8)] + [err, lost, L, R]
            self.csv_writer.writerow(row)

    def refresh_plots(self):
        if len(self.ts) < 2:
            return
        x = list(self.ts)

        for i in range(8):
            self.sensorCurves[i].setData(x, list(self.sensors[i]))

        # 第二张图：把 lost 放大一点方便看
        self.errCurve.setData(x, list(self.err))
        self.lostCurve.setData(x, [v * 10.0 for v in self.lost])
        self.LCurve.setData(x, list(self.Lout))
        self.RCurve.setData(x, list(self.Rout))

    def start_record(self):
        fn, _ = QtWidgets.QFileDialog.getSaveFileName(self, "Save CSV", "linefollow_log.csv", "CSV Files (*.csv)")
        if not fn:
            return
        self.csv_file = open(fn, "w", newline="", encoding="utf-8")
        self.csv_writer = csv.writer(self.csv_file)
        header = ["t_ms"] + [f"s{i}" for i in range(8)] + ["err", "lost", "L", "R"]
        self.csv_writer.writerow(header)
        self.recording = True
        self.recBtn.setEnabled(False)
        self.stopRecBtn.setEnabled(True)
        self.append_log(f"[REC] Recording to {fn}")

    def stop_record(self):
        self.recording = False
        self.recBtn.setEnabled(True)
        self.stopRecBtn.setEnabled(False)
        try:
            if self.csv_file:
                self.csv_file.flush()
                self.csv_file.close()
        except:
            pass
        self.csv_file = None
        self.csv_writer = None
        self.append_log("[REC] Stopped")

    def closeEvent(self, e):
        try:
            self.stop_record()
            self.disconnect_serial()
        except:
            pass
        e.accept()


if __name__ == "__main__":
    app = QtWidgets.QApplication([])
    w = MainWindow()
    w.resize(1200, 800)
    w.show()
    app.exec_()
