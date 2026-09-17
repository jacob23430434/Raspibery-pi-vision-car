import os, time
import numpy as np
import pyqtgraph as pg
from pyqtgraph.Qt import QtCore

# ============================================================
# 0) 读取你的真实数据（8列），做 white/black 标定 + 软边缘拟合
# ============================================================
WHITE_TXT  = "white.txt"

# 这几个是你上传的“横向平移”数据文件名（你现在目录里就是这些）
LINE_DATA = {
    "left3":  "left 3 space.txt",
    "left2":  "left 2 space.txt",
    "left1":  "left 1 space.txt",
    "center": "center.txt",
    "right1": "right 1 space.txt",
    "right2": "right 2 space.txt",
    "right3": "right 3 space.txt",
}

# 你“space”的实际间距（cm）
# 如果你之后确认不是 1.52cm，把这里改掉即可
SPACE_CM = 1.52

# 这 7 组数据对应的横向偏移（车相对线中心的 y 偏移，cm）
OFFSETS_CM = {
    "left3":  -3 * SPACE_CM,
    "left2":  -2 * SPACE_CM,
    "left1":  -1 * SPACE_CM,
    "center":  0.0,
    "right1":  1 * SPACE_CM,
    "right2":  2 * SPACE_CM,
    "right3":  3 * SPACE_CM,
}

def load_8cols_csv(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip().strip(",")
            if not line:
                continue
            nums = []
            for p in line.split(","):
                p = p.strip()
                if not p:
                    continue
                try:
                    nums.append(float(p))
                except:
                    pass
            if len(nums) >= 8:
                rows.append(nums[:8])
    return np.array(rows, dtype=float)

def file_exists(p): return os.path.exists(p) and os.path.getsize(p) > 0

def calibrate_from_files():
    # fallback（万一你某个文件没放对位置）
    WHITE = np.array([850]*8, dtype=float)
    BLACK = np.array([150]*8, dtype=float)

    if file_exists(WHITE_TXT):
        w = load_8cols_csv(WHITE_TXT)
        # 白底用 90 分位：抗偶发阴影/抖动
        WHITE = np.percentile(w, 90, axis=0)

    # 黑底：用所有“在线数据”的 5 分位，等价于“最黑附近”
    line_all = []
    for k, fn in LINE_DATA.items():
        if file_exists(fn):
            line_all.append(load_8cols_csv(fn))
    if len(line_all) > 0:
        line_all = np.vstack(line_all)
        BLACK = np.percentile(line_all, 5, axis=0)

    # 防止出现 BLACK >= WHITE
    BLACK = np.minimum(BLACK, WHITE - 5.0)
    return WHITE, BLACK

WHITE_LEVEL, BLACK_LEVEL = calibrate_from_files()

# ============================================================
# 1) 轨道：直角弯（带圆角）
# ============================================================
# ============================================================
# 1) 轨道：直线 + 大曲线 + 小曲线 + 直角（硬拐）+ 闭环
# ============================================================
def make_track():
    import numpy as np

    # ---------- 参数（单位：米）----------
    L1 = 1.2      # 起始直线 →
    L2 = 0.9      # 大弯后直线 ↑
    L3 = 1.0      # 小弯后直线 →
    L4 = 0.8      # 直角后直线 ↓

    R_big   = 0.60   # 大曲线半径
    R_small = 0.25   # 小曲线半径

    n_per_m   = 350   # 直线采样
    n_per_rad = 220   # 圆弧采样

    def line(p1, p2):
        p1 = np.array(p1, float)
        p2 = np.array(p2, float)
        d = np.linalg.norm(p2 - p1)
        n = max(2, int(d * n_per_m))
        return np.c_[np.linspace(p1[0], p2[0], n),
                     np.linspace(p1[1], p2[1], n)]

    def arc(c, r, a0, a1):
        n = max(6, int(abs(a1 - a0) * n_per_rad))
        th = np.linspace(a0, a1, n)
        return np.c_[c[0] + r*np.cos(th),
                     c[1] + r*np.sin(th)]

    segs = []

    # ---- 起点 ----
    p0 = (0.0, 0.0)

    # 1) 直线 →
    p1 = (L1, 0.0)
    segs.append(line(p0, p1))

    # 2) 大曲线：左转 90°（→ 变 ↑）
    c1 = (p1[0] - R_big, p1[1] + R_big)
    segs.append(arc(c1, R_big, -np.pi/2, 0.0))
    p2 = (p1[0], p1[1] + R_big)

    # 3) 直线 ↑
    p3 = (p2[0], p2[1] + L2)
    segs.append(line(p2, p3))

    # 4) 小曲线：右转 90°（↑ 变 →）
    c2 = (p3[0] + R_small, p3[1] + R_small)
    segs.append(arc(c2, R_small, np.pi, np.pi/2))
    p4 = (p3[0] + R_small, p3[1])

    # 5) 直线 →
    p5 = (p4[0] + L3, p4[1])
    segs.append(line(p4, p5))

    # 6) 真直角（硬拐）：直接向下 ↓
    p6 = (p5[0], p5[1] - L4)
    segs.append(line(p5, p6))

    # ---------- 闭环（回到起点）----------
    # 水平回到 x=0
    p7 = (0.0, p6[1])
    segs.append(line(p6, p7))

    # 垂直回到 y=0（回到起点）
    segs.append(line(p7, p0))

    track = np.vstack(segs)
    return track

TRACK = make_track()

# ============================================================
# 2) 点到折线最短距离（无符号）
# ============================================================
def point_to_polyline_distance_sq(p, poly):
    x0, y0 = p
    a = poly[:-1]
    b = poly[1:]
    ab = b - a
    ap = np.array([x0, y0]) - a
    ab2 = (ab[:, 0]**2 + ab[:, 1]**2) + 1e-12
    t = (ap[:, 0]*ab[:, 0] + ap[:, 1]*ab[:, 1]) / ab2
    t = np.clip(t, 0.0, 1.0)
    proj = a + (ab.T * t).T
    d2 = (proj[:, 0]-x0)**2 + (proj[:, 1]-y0)**2
    return float(np.min(d2))

# ============================================================
# 3) 你的传感器位置（cm）：W_pos（你给的）
# ============================================================
W_pos_cm = np.array([-7.1903, -3.9097, -3.9097, -1.7343,
                     1.0373,  2.6299,  6.5019,  6.5019], dtype=float)

# ============================================================
# 4) 用横向数据拟合“软边缘”模型：blackness(d_cm)
#    模型：blk = base + (1-base)*exp(-d^2/(2*sigma^2))
# ============================================================
def fit_soft_edge_sigma_base():
    # 如果线数据不全，就回退到一个还不错的默认值
    have_all = all(file_exists(fn) for fn in LINE_DATA.values())
    if not have_all:
        return 1.6, 0.18  # sigma_cm, base

    samples_x = []
    samples_y = []

    # 预先把每组数据做均值，得到每个传感器在该偏移下的“期望读数”
    means = {}
    for name, fn in LINE_DATA.items():
        arr = load_8cols_csv(fn)
        means[name] = arr.mean(axis=0)

    # 用 white/black 做归一化：y=0 代表纯白，y=1 代表纯黑
    for name, off in OFFSETS_CM.items():
        m = means[name]
        y = (WHITE_LEVEL - m) / (WHITE_LEVEL - BLACK_LEVEL)
        y = np.clip(y, 0.0, 1.0)

        # 该组数据里，第 i 个传感器采到的相对横向位置 = W_pos[i] - off
        # （off>0 表示车在“右”，所以线相对传感器是“左”，这里一致即可）
        for i in range(8):
            x = float(W_pos_cm[i] - off)  # cm
            samples_x.append(x)
            samples_y.append(float(y[i]))

    xs = np.array(samples_x, dtype=float)
    ys = np.array(samples_y, dtype=float)

    def loss(sigma, base):
        pred = base + (1.0 - base) * np.exp(-(xs**2) / (2.0 * sigma**2))
        return float(np.mean((pred - ys)**2))

    best = (1e9, 1.6, 0.18)
    for sigma in np.linspace(0.5, 6.0, 200):
        for base in np.linspace(0.0, 0.4, 81):
            l = loss(sigma, base)
            if l < best[0]:
                best = (l, float(sigma), float(base))
    return best[1], best[2]

SIGMA_CM, BASE_BLK = fit_soft_edge_sigma_base()

# 线宽（你原来是 0.010m 半宽 = 1cm 半宽）
LINE_HALF_WIDTH_CM = 1.0

# 噪声：按你 white.txt 的抖动量级来（每个传感器略有不同）
# white.txt 中波动挺大，所以这里取一个“偏真实”的量级
NOISE_STD_PER_CH = np.clip(np.array([30, 30, 28, 25, 25, 28, 35, 45], dtype=float), 10, 80)

def blackness_from_distance_cm(d_cm: float) -> float:
    # 线的中间区域近似“全黑”，边缘是软过渡
    if d_cm <= LINE_HALF_WIDTH_CM:
        return 1.0
    g = np.exp(-(d_cm**2) / (2.0 * SIGMA_CM**2))
    blk = BASE_BLK + (1.0 - BASE_BLK) * g
    return float(np.clip(blk, 0.0, 1.0))

def analog_from_blackness(blk, i):
    w = WHITE_LEVEL[i]
    b = BLACK_LEVEL[i]
    val = w - blk * (w - b) + np.random.normal(0, NOISE_STD_PER_CH[i])
    # 仍然保留 ADC 0..1023 的“物理边界”
    return int(np.clip(val, 0, 1023))

def compute_error_cm_from_analog(analog_vals):
    a = np.array(analog_vals, dtype=float)
    # 以 white 为基准的“黑度增量”
    delta = np.maximum(0.0, WHITE_LEVEL - a)
    s = float(np.sum(delta))
    if s < 1e-9:
        return None, 0.0, delta
    err = float(np.sum(delta * W_pos_cm) / s)
    return err, s, delta

# ============================================================
# 5) 仿真参数（差速 + 电机 0..100，不倒转）
# ============================================================
dt = 0.01

# PID（你可以继续沿用你那组）
Kp, Ki, Kd = 20, 0, 0
I_LIMIT = 80.0

# 电机命令 0..100
BASE_CMD0 = 50
CMD_MIN   = 0
CMD_MAX   = 100
MIN_FWD_CMD = 10

# 误差越大越降速
K_SLOW_CMD = 2.2

# “转向靠差速”的尺度：把 PID 输出映射成差速
TURN_GAIN = 0.35  # 越大转向越猛

# 运动学（把命令映射成 m/s）
V_MAX_MPS = 0.60  # cmd=100 时的速度（随便先定一个，主要影响时间尺度）
L = 0.12          # 轮距（m）
OMEGA_GAIN = 1.0  # 额外增益（如果你觉得转弯不够灵敏就加大）

car_size = 0.10

# 8 传感器阵列（车体坐标 x=前进，y=左）
N = 8
sensor_forward = 0.06
sensor_span_m = 0.14
sensor_y_body = np.linspace(-sensor_span_m/2, sensor_span_m/2, N)

# 丢线判定
LOST_LINE_THRESHOLD = 80.0
LOST_ERROR_CM = 6.0

def world_from_body(px_forward, py_left, x, y, theta):
    c, s = np.cos(theta), np.sin(theta)
    wx = x + c*px_forward - s*py_left
    wy = y + s*px_forward + c*py_left
    return wx, wy

def car_square_world(x, y, theta):
    h = car_size / 2
    corners = np.array([[-h,-h],[h,-h],[h,h],[-h,h],[-h,-h]])
    xs, ys = [], []
    for px, py in corners:
        wx, wy = world_from_body(px, py, x, y, theta)
        xs.append(wx); ys.append(wy)
    return np.array(xs), np.array(ys)

def cmd_to_mps(cmd):
    return (cmd / 100.0) * V_MAX_MPS

# ============================================================
# 6) pyqtgraph UI
# ============================================================
pg.setConfigOptions(antialias=False)
app = pg.mkQApp("Line Follower PID Sim (Realistic IR)")

win = pg.GraphicsLayoutWidget(show=True, title="PID Line Follower Sim (Realistic IR from your data)")
win.resize(1400, 650)

p_scene = win.addPlot(row=0, col=0, title="Track + Car")
p_scene.setAspectLocked(True)
p_scene.showGrid(x=True, y=True, alpha=0.2)
track_item = p_scene.plot(TRACK[:,0], TRACK[:,1], pen=pg.mkPen('w', width=3))
traj_item  = p_scene.plot([], [], pen=pg.mkPen((150,150,255), width=1))
car_item   = p_scene.plot([], [], pen=pg.mkPen((255,255,0), width=2))
head_item  = p_scene.plot([], [], pen=pg.mkPen((255,0,0), width=2))
sensor_scatter = pg.ScatterPlotItem(size=9, brush=pg.mkBrush(0, 200, 255, 180), pen=pg.mkPen(None))
p_scene.addItem(sensor_scatter)

p_sig = win.addPlot(row=0, col=1, title="Signals (windowed view)")
p_sig.showGrid(x=True, y=True, alpha=0.2)
p_sig.addLegend(offset=(10,10))
err_curve = p_sig.plot([], [], pen=pg.mkPen('y', width=2), name="error(cm)")
lc_curve  = p_sig.plot([], [], pen=pg.mkPen('g', width=2), name="leftCmd")
rc_curve  = p_sig.plot([], [], pen=pg.mkPen('r', width=2), name="rightCmd")

p_a = win.addPlot(row=1, col=1, title="8x analogRead (0..1023)")
p_a.setYRange(0, 1023)
p_a.setXRange(-1, N)
p_a.showGrid(x=True, y=True, alpha=0.2)
bars = pg.BarGraphItem(x=np.arange(N), height=np.zeros(N), width=0.8,
                       brush=pg.mkBrush(120,120,255,200))
p_a.addItem(bars)

p_info = win.addPlot(row=1, col=0, title="Info")
p_info.hideAxis('left'); p_info.hideAxis('bottom')
info_text = pg.TextItem(color=(255,255,255), anchor=(0,0))
p_info.addItem(info_text)
p_info.setXRange(0, 1); p_info.setYRange(0, 1)

# ============================================================
# 7) 仿真状态
# ============================================================
x, y = float(TRACK[0,0]), float(TRACK[0,1] + 0.06)
theta = 0.0
integral = 0.0
last_error = 0.0

ts, errs, lcs, rcs = [], [], [], []
traj_x, traj_y = [], []

VIEW_N = 2000
FPS = 60
timer_interval_ms = int(1000 / FPS)

def step_sim():
    global x, y, theta, integral, last_error
    t = (ts[-1] + dt) if ts else 0.0

    # ---- 8 传感器采样（距离 -> blackness(软边缘) -> analog） ----
    sensor_points = np.zeros((N,2), dtype=float)
    analog_vals = np.zeros(N, dtype=int)

    for i, sy in enumerate(sensor_y_body):
        wx, wy = world_from_body(sensor_forward, sy, x, y, theta)
        sensor_points[i] = (wx, wy)

        d_m  = np.sqrt(point_to_polyline_distance_sq((wx, wy), TRACK))
        d_cm = 100.0 * d_m
        blk = blackness_from_distance_cm(d_cm)
        analog_vals[i] = analog_from_blackness(blk, i)

    # ---- error & 丢线 ----
    err, delta_sum, delta = compute_error_cm_from_analog(analog_vals)
    lost_line = (err is None) or (delta_sum < LOST_LINE_THRESHOLD)

    if lost_line:
        error = float(np.sign(last_error) * LOST_ERROR_CM) if last_error != 0 else float(LOST_ERROR_CM)
        error = float(np.clip(error, -3.0, 3.0))
        integral *= 0.95
    else:
        error = float(err)

    # ---- PID ----
    integral += error * dt
    integral = float(np.clip(integral, -I_LIMIT, I_LIMIT))
    derivative = (error - last_error) / dt
    u = Kp*error + Ki*integral + Kd*derivative
    last_error = error

    # ---- 速度规划（0..100）+ 差速，不倒转 ----
    base_cmd = BASE_CMD0 - K_SLOW_CMD * abs(error)
    base_cmd = float(np.clip(base_cmd, CMD_MIN, CMD_MAX))

    # 把 PID 输出映射成差速（单位：cmd）
    turn_cmd = TURN_GAIN * u
    # 限制差速，确保两轮都 >= MIN_FWD_CMD
    max_turn = max(0.0, base_cmd - MIN_FWD_CMD)
    turn_cmd = float(np.clip(turn_cmd, -max_turn, max_turn))

    left_cmd  = float(np.clip(base_cmd - turn_cmd, 0.0, 100.0))
    right_cmd = float(np.clip(base_cmd + turn_cmd, 0.0, 100.0))

    # ---- 运动学 ----
    vl = cmd_to_mps(left_cmd)
    vr = cmd_to_mps(right_cmd)
    v = (vl + vr) / 2.0
    w = OMEGA_GAIN * (vr - vl) / L

    x += v*np.cos(theta) * dt
    y += v*np.sin(theta) * dt
    theta += w * dt

    # ---- 记录 ----
    ts.append(t); errs.append(error); lcs.append(left_cmd); rcs.append(right_cmd)
    traj_x.append(x); traj_y.append(y)

    # ---- 更新显示 ----
    sl0 = max(0, len(ts)-VIEW_N)
    tx = np.asarray(ts[sl0:], dtype=float)

    traj_item.setData(traj_x[sl0:], traj_y[sl0:])
    cx, cy = car_square_world(x, y, theta)
    car_item.setData(cx, cy)
    hx1, hy1 = world_from_body(car_size/2, 0.0, x, y, theta)
    head_item.setData([x, hx1], [y, hy1])

    sensor_scatter.setData(pos=sensor_points)
    err_curve.setData(tx, np.asarray(errs[sl0:], dtype=float))
    lc_curve.setData(tx, np.asarray(lcs[sl0:], dtype=float))
    rc_curve.setData(tx, np.asarray(rcs[sl0:], dtype=float))
    bars.setOpts(height=analog_vals)

    p_scene.setXRange(x - 0.6, x + 0.8, padding=0.0)
    p_scene.setYRange(y - 0.45, y + 0.45, padding=0.0)

    info_text.setText(
        f"t={t:.2f}s\n"
        f"error={error:.2f} cm\n"
        f"delta_sum={delta_sum:.1f}\n"
        f"lost_line={lost_line}\n"
        f"PID u={u:.2f}\n"
        f"base_cmd={base_cmd:.1f}\n"
        f"left_cmd={left_cmd:.1f}, right_cmd={right_cmd:.1f}\n"
        f"WHITE(90p)={np.round(WHITE_LEVEL,1)}\n"
        f"BLACK(5p) ={np.round(BLACK_LEVEL,1)}\n"
        f"soft-edge: SIGMA_CM={SIGMA_CM:.2f}, BASE_BLK={BASE_BLK:.2f}"
    )

timer = QtCore.QTimer()
timer.timeout.connect(step_sim)
timer.start(timer_interval_ms)

if __name__ == "__main__":
    pg.exec()
