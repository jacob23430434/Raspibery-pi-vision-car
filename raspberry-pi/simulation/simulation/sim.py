import numpy as np
import matplotlib.pyplot as plt

# =========================
# 1) 固定轨道（黑色曲线）
# =========================
def make_track():
    x = np.linspace(0, 3.0, 900)
    y = 0.10*np.sin(2*np.pi*x/1.2) + 0.04*np.sin(2*np.pi*x/0.45)
    return np.c_[x, y]

TRACK = make_track()

# =========================
# 2) 点到折线最短距离（传感器看到线）
# =========================
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

def analog_from_distance(d,
                        line_half_width=0.010,
                        sigma=0.012,
                        white_level=850,
                        black_level=150,
                        noise_std=12):
    """
    返回 analogRead 0..1023（白底高、黑线低）
    """
    g = np.exp(-(d**2) / (2*sigma**2))
    blackness = 1.0 if d <= line_half_width else 0.25*g
    val = white_level*(1-blackness) + black_level*blackness
    val += np.random.normal(0, noise_std)
    return int(np.clip(val, 0, 1023))

# =========================
# 3) PID + 小车参数
# =========================
Kp, Ki, Kd = 10.0, 0.0, 1.2
I_LIMIT = 80.0

base_speed = 0.35
max_speed  = 0.75

dt = 0.01
T  = 12.0
steps = int(T/dt)

# 差速车
L = 0.12
omega_gain = 1.8

# 正方形小车（不变形）
car_size = 0.10

# 8 传感器阵列（车体坐标：x前进，y向左）
N = 8
sensor_span = 0.14
sensor_forward = 0.06
sensor_y_body = np.linspace(-sensor_span/2, sensor_span/2, N)

# 丢线/溢出继续模拟（不暂停）
LOST_LINE_THRESHOLD = 150   # sum(1023-analog) 太小视为全白
LOST_ERROR_CM = 6.0         # 丢线时用这个最大偏差继续转向找线

# =========================
# 4) 坐标系统：x_body=前进，y_body=左
# =========================
def world_from_body(px_forward, py_left, x, y, theta):
    c, s = np.cos(theta), np.sin(theta)
    wx = x + c*px_forward - s*py_left
    wy = y + s*px_forward + c*py_left
    return wx, wy

def car_square_world(x, y, theta):
    h = car_size / 2
    corners_body = np.array([
        [-h, -h],
        [ h, -h],
        [ h,  h],
        [-h,  h],
        [-h, -h],
    ])
    xs, ys = [], []
    for px, py in corners_body:
        wx, wy = world_from_body(px, py, x, y, theta)
        xs.append(wx); ys.append(wy)
    return np.array(xs), np.array(ys)

def compute_error_from_analog(analog_vals):
    """
    白底高、黑线低 -> 权重 w = 1023 - analog
    返回误差 cm（横向偏移）
    """
    a = np.array(analog_vals, dtype=float)
    w = (1023.0 - a)
    s = np.sum(w)
    if s < 1e-9:
        return None
    center_m = np.sum(w * sensor_y_body) / s
    return center_m * 100.0  # cm

# =========================
# 5) 初始状态
# =========================
x, y = float(TRACK[0,0]), float(TRACK[0,1] + 0.06)
theta = 0.0
integral = 0.0
last_error = 0.0

# =========================
# 6) 实时可视化：三窗口
# =========================
plt.ion()

# Figure 1：左=场景，右=信号
fig = plt.figure(figsize=(13, 5))
ax_sim = fig.add_subplot(1, 2, 1)
ax_sig = fig.add_subplot(1, 2, 2)

ax_sim.set_title("Fixed Track + Square Car (real-time)")
ax_sim.set_xlabel("x"); ax_sim.set_ylabel("y")
ax_sim.plot(TRACK[:,0], TRACK[:,1], linewidth=3, color="k")

traj_line, = ax_sim.plot([], [], linewidth=1)
car_poly,  = ax_sim.plot([], [], linewidth=2)
heading,   = ax_sim.plot([], [], linewidth=2)
sensors_sc = ax_sim.scatter([], [], s=45)

ax_sig.set_title("Signals (FULL data, no downsample)")
ax_sig.set_xlabel("t (s)")
line_err, = ax_sig.plot([], [], label="error (cm)")
line_ls,  = ax_sig.plot([], [], label="leftSpeed")
line_rs,  = ax_sig.plot([], [], label="rightSpeed")
ax_sig.legend(loc="upper right")

# Figure 2：8路 analog
fig2, ax_a = plt.subplots(figsize=(6, 3))
ax_a.set_title("8x analogRead (0..1023) - realtime")
bars = ax_a.bar(np.arange(N), np.zeros(N))
ax_a.set_ylim(0, 1023)
ax_a.set_xlabel("sensor index")
ax_a.set_ylabel("analog")

# 全量数据（每一步都append，不丢点）
ts, errs, lss, rss = [], [], [], []
traj_x, traj_y = [], []

refresh_every = 4  # 只是“画图刷新频率”，数据仍然每步记录

for k in range(steps):
    t = k * dt

    # -------- 传感器采样（每步都算，analog每步都记录）--------
    sensor_points = []
    analog_vals = []
    for sy in sensor_y_body:
        wx, wy = world_from_body(sensor_forward, sy, x, y, theta)
        sensor_points.append((wx, wy))
        d = np.sqrt(point_to_polyline_distance_sq((wx, wy), TRACK))
        analog_vals.append(analog_from_distance(d))
    sensor_points = np.array(sensor_points)

    # -------- error & 丢线处理（继续跑，不暂停）--------
    err = compute_error_from_analog(analog_vals)
    line_strength = float(np.sum(1023 - np.array(analog_vals, dtype=float)))
    lost_line = (line_strength < LOST_LINE_THRESHOLD) or (err is None)

    if lost_line:
        error = float(np.sign(last_error) * LOST_ERROR_CM) if last_error != 0 else float(LOST_ERROR_CM)
        integral *= 0.95  # 防积分爆
    else:
        error = float(err)

    # -------- PID（每步算）--------
    integral += error * dt
    integral = float(np.clip(integral, -I_LIMIT, I_LIMIT))
    derivative = (error - last_error) / dt
    u = Kp*error + Ki*integral + Kd*derivative
    last_error = error

    u_scaled = u / 80.0
    left_speed  = float(np.clip(base_speed - u_scaled, -max_speed, max_speed))
    right_speed = float(np.clip(base_speed + u_scaled, -max_speed, max_speed))

    # -------- 运动学（每步更新）--------
    v = (left_speed + right_speed) / 2.0
    w = omega_gain * (right_speed - left_speed) / L
    x += v*np.cos(theta) * dt
    y += v*np.sin(theta) * dt
    theta += w * dt

    # -------- 全量记录：每一步都append，不省 --------
    ts.append(t)
    errs.append(error)
    lss.append(left_speed)
    rss.append(right_speed)
    traj_x.append(x)
    traj_y.append(y)

    # -------- 画图刷新：可以低频，但曲线用全量数据 --------
    if k % refresh_every == 0:
        # 左图：轨道+车+传感器+轨迹
        traj_line.set_data(traj_x, traj_y)
        cx, cy = car_square_world(x, y, theta)
        car_poly.set_data(cx, cy)

        hx1, hy1 = world_from_body(car_size/2, 0.0, x, y, theta)
        heading.set_data([x, hx1], [y, hy1])

        sensors_sc.set_offsets(sensor_points)
        sensors_sc.set_array(np.array(analog_vals, dtype=float))
        sensors_sc.set_clim(0, 1023)

        ax_sim.set_xlim(x - 0.6, x + 0.8)
        ax_sim.set_ylim(y - 0.45, y + 0.45)

        # 右图：信号全量（从 0 到当前 t，不滑窗）
        line_err.set_data(ts, errs)
        line_ls.set_data(ts, lss)
        line_rs.set_data(ts, rss)
        ax_sig.set_xlim(0, t + 0.05)

        y_min = min(min(errs), min(lss), min(rss))
        y_max = max(max(errs), max(lss), max(rss))
        pad = 0.15 * (abs(y_max - y_min) + 1e-6)
        ax_sig.set_ylim(y_min - pad, y_max + pad)

        # Figure 2：8路 analog 柱状图（每次刷新更新）
        for i, v0 in enumerate(analog_vals):
            bars[i].set_height(v0)

        fig.canvas.draw(); fig.canvas.flush_events()
        fig2.canvas.draw(); fig2.canvas.flush_events()
        plt.pause(0.001)

plt.ioff()
plt.show()
