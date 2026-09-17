import os, time, json
import numpy as np
import pyqtgraph as pg
from pyqtgraph.Qt import QtCore
from concurrent.futures import ProcessPoolExecutor


# =========================
# MODE: "train" or "ui"
# =========================
MODE = "ui"   # 改成 "ui" 就看动画
SAVE_BEST_JSON = True
BEST_JSON_PATH = "best_params.json"
LOAD_BEST_IN_UI = True

# ============================================================
# 0) 读取你的真实数据（8列），做 white/black 标定 + 软边缘拟合
# ============================================================
WHITE_TXT  = "white.txt"

LINE_DATA = {
    "left3":  "left 3 space.txt",
    "left2":  "left 2 space.txt",
    "left1":  "left 1 space.txt",
    "center": "center.txt",
    "right1": "right 1 space.txt",
    "right2": "right 2 space.txt",
    "right3": "right 3 space.txt",
}

SPACE_CM = 1.52

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

def file_exists(p): 
    return os.path.exists(p) and os.path.getsize(p) > 0

def calibrate_from_files():
    WHITE = np.array([850]*8, dtype=float)
    BLACK = np.array([150]*8, dtype=float)

    if file_exists(WHITE_TXT):
        w = load_8cols_csv(WHITE_TXT)
        WHITE = np.percentile(w, 90, axis=0)

    line_all = []
    for _, fn in LINE_DATA.items():
        if file_exists(fn):
            line_all.append(load_8cols_csv(fn))
    if len(line_all) > 0:
        line_all = np.vstack(line_all)
        BLACK = np.percentile(line_all, 5, axis=0)

    BLACK = np.minimum(BLACK, WHITE - 5.0)
    return WHITE, BLACK

WHITE_LEVEL, BLACK_LEVEL = calibrate_from_files()

# ============================================================
# 1) 轨道：直线 + 大曲线 + 小曲线 + 直角 + 闭环
# ============================================================
def make_track():
    L1 = 1.2
    L2 = 0.9
    L3 = 1.0
    L4 = 0.8

    R_big   = 0.60
    R_small = 0.25

    n_per_m   = 350
    n_per_rad = 220

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
    p0 = (0.0, 0.0)

    p1 = (L1, 0.0)
    segs.append(line(p0, p1))

    c1 = (p1[0] - R_big, p1[1] + R_big)
    segs.append(arc(c1, R_big, -np.pi/2, 0.0))
    p2 = (p1[0], p1[1] + R_big)

    p3 = (p2[0], p2[1] + L2)
    segs.append(line(p2, p3))

    c2 = (p3[0] + R_small, p3[1] + R_small)
    segs.append(arc(c2, R_small, np.pi, np.pi/2))
    p4 = (p3[0] + R_small, p3[1])

    p5 = (p4[0] + L3, p4[1])
    segs.append(line(p4, p5))

    p6 = (p5[0], p5[1] - L4)
    segs.append(line(p5, p6))

    p7 = (0.0, p6[1])
    segs.append(line(p6, p7))

    segs.append(line(p7, p0))

    track = np.vstack(segs)

    # ✅ 修复1：去掉末尾与起点重复的点，避免 idx 一开始跳到末尾
    if np.linalg.norm(track[-1] - track[0]) < 1e-9:
        track = track[:-1]

    return track

TRACK = make_track()

# ============================================================
# A) 轨迹最近点 index + 距离
# ============================================================
def nearest_track_index_and_dist(p, track):
    px, py = float(p[0]), float(p[1])
    dx = track[:,0] - px
    dy = track[:,1] - py
    d2 = dx*dx + dy*dy
    idx = int(np.argmin(d2))
    return idx, float(np.sqrt(d2[idx]))

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
# 3) 你的传感器位置（cm）
# ============================================================
W_pos_cm = np.array([-7.1903, -3.9097, -3.9097, -1.7343,
                     1.0373,  2.6299,  6.5019,  6.5019], dtype=float)

# ============================================================
# 4) 软边缘拟合
# ============================================================
def fit_soft_edge_sigma_base():
    have_all = all(file_exists(fn) for fn in LINE_DATA.values())
    if not have_all:
        return 1.6, 0.18

    samples_x = []
    samples_y = []

    means = {}
    for name, fn in LINE_DATA.items():
        arr = load_8cols_csv(fn)
        means[name] = arr.mean(axis=0)

    for name, off in OFFSETS_CM.items():
        m = means[name]
        y = (WHITE_LEVEL - m) / (WHITE_LEVEL - BLACK_LEVEL)
        y = np.clip(y, 0.0, 1.0)

        for i in range(8):
            x = float(W_pos_cm[i] - off)
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

LINE_HALF_WIDTH_CM = 1.0
NOISE_STD_PER_CH = np.clip(np.array([30, 30, 28, 25, 25, 28, 35, 45], dtype=float), 10, 80)

def blackness_from_distance_cm(d_cm: float) -> float:
    if d_cm <= LINE_HALF_WIDTH_CM:
        return 1.0
    g = np.exp(-(d_cm**2) / (2.0 * SIGMA_CM**2))
    blk = BASE_BLK + (1.0 - BASE_BLK) * g
    return float(np.clip(blk, 0.0, 1.0))

def analog_from_blackness(blk, i):
    w = WHITE_LEVEL[i]
    b = BLACK_LEVEL[i]
    val = w - blk * (w - b) + np.random.normal(0, NOISE_STD_PER_CH[i])
    return int(np.clip(val, 0, 1023))

def compute_error_cm_from_analog(analog_vals):
    a = np.array(analog_vals, dtype=float)
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

# 初始 PID（UI用；训练会覆盖）
Kp, Ki, Kd = 20, 0.0, 0.8
I_LIMIT = 80.0

BASE_CMD0 = 50
CMD_MIN   = 0
CMD_MAX   = 100
MIN_FWD_CMD = 10

K_SLOW_CMD = 2.2
TURN_GAIN = 0.35

V_MAX_MPS = 0.60
L = 0.12
OMEGA_GAIN = 1.0

car_size = 0.10

N = 8
sensor_forward = 0.06
sensor_span_m = 0.14
sensor_y_body = np.linspace(-sensor_span_m/2, sensor_span_m/2, N)

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
# B) 评估专用：单次仿真（无UI），返回得分与统计
#    ✅ 修复2：必须先离开起点区域再回来，才算完成圈（杜绝 t=0）
# ============================================================
def simulate_once(params, seed=0, T_max=18.0):
    rng = np.random.default_rng(seed)

    V_max = V_MAX_MPS * float(rng.uniform(0.75, 1.10))
    L_eff = L * float(rng.uniform(0.95, 1.05))
    omega_gain = OMEGA_GAIN * float(rng.uniform(0.7, 1.3))
    noise_scale = float(rng.uniform(0.7, 1.5))
    deadzone = float(rng.uniform(0.0, 8.0))

    def cmd_to_mps_local(cmd):
        return (cmd / 100.0) * V_max

    x = float(TRACK[0,0])
    y = float(TRACK[0,1] + 0.06)
    theta = 0.0
    integral = 0.0
    last_error = 0.0

    offtrack_count = 0
    lost_count = 0
    jerk_sum = 0.0
    err_abs_sum = 0.0
    t = 0.0

    last_l = None
    last_r = None

    track_len = len(TRACK)
    start = TRACK[0]

    Kp = float(params["Kp"])
    Ki = float(params["Ki"])
    Kd = float(params["Kd"])
    TURN_GAIN = float(params["TURN_GAIN"])
    K_SLOW_CMD = float(params["K_SLOW_CMD"])
    BASE_CMD0 = float(params["BASE_CMD0"])

    OFFTRACK_DIST = 0.10

    # 完圈必须：先离开起点半径，再回来
    left_start_zone = False
    START_LEAVE_R = 0.25
    START_RETURN_R = 0.12

    while t < T_max:
        analog_vals = np.zeros(N, dtype=int)

        for i, sy in enumerate(sensor_y_body):
            wx, wy = world_from_body(sensor_forward, sy, x, y, theta)
            d_m  = np.sqrt(point_to_polyline_distance_sq((wx, wy), TRACK))
            d_cm = 100.0 * d_m
            blk = blackness_from_distance_cm(d_cm)

            w = WHITE_LEVEL[i]
            b = BLACK_LEVEL[i]
            val = w - blk * (w - b) + rng.normal(0, NOISE_STD_PER_CH[i]*noise_scale)
            analog_vals[i] = int(np.clip(val, 0, 1023))

        err, delta_sum, delta = compute_error_cm_from_analog(analog_vals)
        lost_line = (err is None) or (delta_sum < LOST_LINE_THRESHOLD)

        if lost_line:
            lost_count += 1
            error = float(np.sign(last_error) * LOST_ERROR_CM) if last_error != 0 else float(LOST_ERROR_CM)
            error = float(np.clip(error, -3.0, 3.0))
            integral *= 0.95
        else:
            error = float(err)

        integral += error * dt
        integral = float(np.clip(integral, -I_LIMIT, I_LIMIT))
        derivative = (error - last_error) / dt
        u = Kp*error + Ki*integral + Kd*derivative
        last_error = error

        base_cmd = BASE_CMD0 - K_SLOW_CMD * abs(error)
        base_cmd = float(np.clip(base_cmd, CMD_MIN, CMD_MAX))

        turn_cmd = TURN_GAIN * u
        max_turn = max(0.0, base_cmd - MIN_FWD_CMD)
        turn_cmd = float(np.clip(turn_cmd, -max_turn, max_turn))

        left_cmd  = float(np.clip(base_cmd - turn_cmd, 0.0, 100.0))
        right_cmd = float(np.clip(base_cmd + turn_cmd, 0.0, 100.0))

        if left_cmd < deadzone: left_cmd = 0.0
        if right_cmd < deadzone: right_cmd = 0.0

        if last_l is not None:
            jerk_sum += abs(left_cmd - last_l) + abs(right_cmd - last_r)
        last_l, last_r = left_cmd, right_cmd

        vl = cmd_to_mps_local(left_cmd)
        vr = cmd_to_mps_local(right_cmd)
        v = (vl + vr) / 2.0
        w = omega_gain * (vr - vl) / L_eff

        x += v*np.cos(theta) * dt
        y += v*np.sin(theta) * dt
        theta += w * dt

        err_abs_sum += abs(error)

        d_center = np.sqrt(point_to_polyline_distance_sq((x, y), TRACK))
        if d_center > OFFTRACK_DIST:
            offtrack_count += 1

        idx, dist_to_start = nearest_track_index_and_dist((x, y), TRACK)

        if dist_to_start > START_LEAVE_R:
            left_start_zone = True

        # ✅ 完成圈判定：离开过 + 回来近 + 至少跑了一点时间
        if left_start_zone and (t > 1.0) and (dist_to_start < START_RETURN_R) and (idx > int(track_len * 0.10)):
            time_bonus = max(0.0, (T_max - t))
            score = (
                + 500.0
                + 40.0 * time_bonus
                - 1.2 * err_abs_sum
                - 0.02 * jerk_sum
                - 2.0 * lost_count
                - 8.0 * offtrack_count
            )
            return score, {
                "t": t, "lost": lost_count, "off": offtrack_count,
                "err_abs": err_abs_sum, "jerk": jerk_sum
            }

        t += dt

    progress = idx / max(1, (track_len-1))
    score = (
        + 200.0 * progress
        - 1.5 * err_abs_sum
        - 0.03 * jerk_sum
        - 3.0 * lost_count
        - 10.0 * offtrack_count
    )
    return score, {
        "t": T_max, "lost": lost_count, "off": offtrack_count,
        "err_abs": err_abs_sum, "jerk": jerk_sum, "progress": progress
    }

# ============================================================
# C) 参数搜索：随机搜索
# ============================================================
def random_search(n_trials=300, seeds_per_trial=6):
    best = None

    for k in range(n_trials):
        params = {
            "Kp":        float(np.random.uniform(8, 40)),
            "Ki":        float(np.random.uniform(0.0, 1.0)),
            "Kd":        float(np.random.uniform(0.0, 5.0)),
            "TURN_GAIN": float(np.random.uniform(0.12, 0.75)),
            "K_SLOW_CMD":float(np.random.uniform(0.5, 4.0)),
            "BASE_CMD0": float(np.random.uniform(35, 70)),
        }

        from concurrent.futures import ProcessPoolExecutor

def eval_one(args):
    params, seed = args
    sc, st = simulate_once(params, seed=seed, T_max=18.0)
    return sc, st

def random_search(n_trials=300, seeds_per_trial=6, workers=None):
    best = None
    if workers is None:
        workers = max(1, os.cpu_count() - 1)

    with ProcessPoolExecutor(max_workers=workers) as ex:
        for k in range(n_trials):
            params = {
                "Kp":        float(np.random.uniform(8, 40)),
                "Ki":        float(np.random.uniform(0.0, 1.0)),
                "Kd":        float(np.random.uniform(0.0, 5.0)),
                "TURN_GAIN": float(np.random.uniform(0.12, 0.75)),
                "K_SLOW_CMD":float(np.random.uniform(0.5, 4.0)),
                "BASE_CMD0": float(np.random.uniform(35, 70)),
            }

            jobs = [(params, 1000*k + s) for s in range(seeds_per_trial)]
            results = list(ex.map(eval_one, jobs))

            scores = [r[0] for r in results]
            stats_last = results[-1][1]
            score = float(np.mean(scores))

            if (best is None) or (score > best["score"]):
                best = {"score": score, "params": params, "stats": stats_last}
                print("\n[BEST UPDATE]")
                print("score =", best["score"])
                print("params =", best["params"])
                print("stats  =", best["stats"])

            if (k+1) % 20 == 0:
                print(f"trial {k+1}/{n_trials}, current best={best['score']:.2f}")

    return best


def train_and_save():
    best = random_search(n_trials=300, seeds_per_trial=6)
    print("\n===== FINAL BEST =====")
    print("score =", best["score"])
    print("params =", best["params"])
    print("stats  =", best["stats"])

    if SAVE_BEST_JSON:
        with open(BEST_JSON_PATH, "w", encoding="utf-8") as f:
            json.dump(best, f, ensure_ascii=False, indent=2)
        print(f"[saved] {BEST_JSON_PATH}")
    return best

def maybe_load_best():
    global Kp, Ki, Kd, TURN_GAIN, K_SLOW_CMD, BASE_CMD0
    if not (LOAD_BEST_IN_UI and os.path.exists(BEST_JSON_PATH)):
        return
    try:
        with open(BEST_JSON_PATH, "r", encoding="utf-8") as f:
            obj = json.load(f)
        p = obj.get("params", {})
        Kp = float(p.get("Kp", Kp))
        Ki = float(p.get("Ki", Ki))
        Kd = float(p.get("Kd", Kd))
        TURN_GAIN = float(p.get("TURN_GAIN", TURN_GAIN))
        K_SLOW_CMD = float(p.get("K_SLOW_CMD", K_SLOW_CMD))
        BASE_CMD0 = float(p.get("BASE_CMD0", BASE_CMD0))
        print("[ui] loaded best_params.json:", p)
    except Exception as e:
        print("[ui] failed to load best:", e)

# ============================================================
# UI 部分
# ============================================================
def run_ui():
    maybe_load_best()

    pg.setConfigOptions(antialias=False)
    app = pg.mkQApp("Line Follower PID Sim (Realistic IR)")

    win = pg.GraphicsLayoutWidget(show=True, title="PID Line Follower Sim (Realistic IR from your data)")
    win.resize(1400, 650)

    p_scene = win.addPlot(row=0, col=0, title="Track + Car")
    p_scene.setAspectLocked(True)
    p_scene.showGrid(x=True, y=True, alpha=0.2)
    p_scene.plot(TRACK[:,0], TRACK[:,1], pen=pg.mkPen('w', width=3))
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

    # 仿真状态
    x = float(TRACK[0,0])
    y = float(TRACK[0,1] + 0.06)
    theta = 0.0
    integral = 0.0
    last_error = 0.0

    ts, errs, lcs, rcs = [], [], [], []
    traj_x, traj_y = [], []

    VIEW_N = 2000
    FPS = 60
    timer_interval_ms = int(1000 / FPS)

    def step_sim():
        nonlocal x, y, theta, integral, last_error
        t = (ts[-1] + dt) if ts else 0.0

        sensor_points = np.zeros((N,2), dtype=float)
        analog_vals = np.zeros(N, dtype=int)

        for i, sy in enumerate(sensor_y_body):
            wx, wy = world_from_body(sensor_forward, sy, x, y, theta)
            sensor_points[i] = (wx, wy)

            d_m  = np.sqrt(point_to_polyline_distance_sq((wx, wy), TRACK))
            d_cm = 100.0 * d_m
            blk = blackness_from_distance_cm(d_cm)
            analog_vals[i] = analog_from_blackness(blk, i)

        err, delta_sum, delta = compute_error_cm_from_analog(analog_vals)
        lost_line = (err is None) or (delta_sum < LOST_LINE_THRESHOLD)

        if lost_line:
            error = float(np.sign(last_error) * LOST_ERROR_CM) if last_error != 0 else float(LOST_ERROR_CM)
            error = float(np.clip(error, -3.0, 3.0))
            integral *= 0.95
        else:
            error = float(err)

        integral += error * dt
        integral = float(np.clip(integral, -I_LIMIT, I_LIMIT))
        derivative = (error - last_error) / dt
        u = Kp*error + Ki*integral + Kd*derivative
        last_error = error

        base_cmd = BASE_CMD0 - K_SLOW_CMD * abs(error)
        base_cmd = float(np.clip(base_cmd, CMD_MIN, CMD_MAX))

        turn_cmd = TURN_GAIN * u
        max_turn = max(0.0, base_cmd - MIN_FWD_CMD)
        turn_cmd = float(np.clip(turn_cmd, -max_turn, max_turn))

        left_cmd  = float(np.clip(base_cmd - turn_cmd, 0.0, 100.0))
        right_cmd = float(np.clip(base_cmd + turn_cmd, 0.0, 100.0))

        vl = cmd_to_mps(left_cmd)
        vr = cmd_to_mps(right_cmd)
        v = (vl + vr) / 2.0
        w = OMEGA_GAIN * (vr - vl) / L

        x += v*np.cos(theta) * dt
        y += v*np.sin(theta) * dt
        theta += w * dt

        ts.append(t); errs.append(error); lcs.append(left_cmd); rcs.append(right_cmd)
        traj_x.append(x); traj_y.append(y)

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
            f"Kp={Kp:.2f} Ki={Ki:.3f} Kd={Kd:.2f}\n"
            f"TURN_GAIN={TURN_GAIN:.3f} K_SLOW_CMD={K_SLOW_CMD:.2f} BASE_CMD0={BASE_CMD0:.1f}\n"
            f"soft-edge: SIGMA_CM={SIGMA_CM:.2f}, BASE_BLK={BASE_BLK:.2f}"
        )

    timer = QtCore.QTimer()
    timer.timeout.connect(step_sim)
    timer.start(timer_interval_ms)

    pg.exec()

# ============================================================
# main
# ============================================================
if __name__ == "__main__":
    if MODE.lower() == "train":
        train_and_save()
    else:
        run_ui()
