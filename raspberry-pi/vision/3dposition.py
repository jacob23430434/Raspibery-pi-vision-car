import serial
import json
import time
import matplotlib.pyplot as plt

# ------------------ 蓝牙串口 ------------------
PORT = "COM19"  # 改成你的串口
BAUD = 9600

ser = serial.Serial(PORT, BAUD, timeout=0.01)
time.sleep(2)

# ------------------ 运动状态 ------------------
# 小车位置
x = 0.0  # sx: 你 MCU 已经积分好了
y = 0.0  # 这个Python积分
vy = 0.0

last_time = time.time()

# ------------------ 绘图 ------------------
plt.ion()
fig, ax = plt.subplots()
scat = ax.scatter(0, 0, c='red')

# 初始视图
ax.set_xlim(-1, 1)
ax.set_ylim(-1, 1)
ax.set_xlabel("X distance (m)")
ax.set_ylabel("Y distance (m)")
ax.set_title("Car XY real-time position")

print("=== READY ===")

# ------------------ 主循环 ------------------
while True:
    try:
        line = ser.readline().decode().strip()
        if not line:
            continue

        # ------------------ JSON 解析 ------------------
        data = json.loads(line)

        ax_val = data["ax"]
        ay_val = data["ay"]
        vx_val = data["vx"]
        sx_val = data["sx"]

        # ------------------ 时间差 ------------------
        now = time.time()
        dt = now - last_time
        last_time = now

        # X轴：你MCU已经积分 → 直接使用
        x = sx_val

        # Y轴：Python侧积分
        vy += ay_val * dt
        y += vy * dt

        # ------------------ 绘图更新 ------------------
        scat.set_offsets([[x, y]])

        # 视角跟随
        ax.set_xlim(x - 1.5, x + 1.5)
        ax.set_ylim(y - 1.5, y + 1.5)

        plt.pause(0.001)

        # ------------------ Debug输出 ------------------
        print(f"AX={ax_val:.4f}  AY={ay_val:.4f}  X={x:.4f}  Y={y:.4f}")

    except Exception as e:
        print("ERR:", e)
