import pygame
import serial
import time
import threading
import math

# =================== 蓝牙 ===================
bt = serial.Serial("COM19", 9600, timeout=0.01)
time.sleep(3)

# =================== Pygame / 手柄 ===================
pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    print("没有检测到手柄")
    exit()

joy = pygame.joystick.Joystick(0)
joy.init()

# =================== 屏幕 ===================
WIDTH, HEIGHT = 1000, 1000
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Car HUD")

font = pygame.font.SysFont("consolas", 28)
font_small = pygame.font.SysFont("consolas", 20)

CAR_SPEED = 0.0
LAST_CMD = "S"

smooth_speed = 0.0


# =================== 蓝牙接收线程 ===================
def bt_reader():
    global CAR_SPEED
    buffer = ""
    while True:
        if bt.in_waiting > 0:
            try:
                raw = bt.read().decode('utf-8', errors='ignore')
                if raw == "\n":
                    data = buffer.strip()
                    try:
                        CAR_SPEED = float(data)
                    except:
                        pass
                    buffer = ""
                else:
                    buffer += raw
            except:
                pass
        time.sleep(0.002)


threading.Thread(target=bt_reader, daemon=True).start()


# =================== 蓝牙发送 ===================
def send(cmd):
    global LAST_CMD
    LAST_CMD = cmd
    bt.write(cmd.encode())


# =================== 仪表盘函数 ===================
def draw_gauge(surface, center, radius, speed):
    """
    0–2 m/s 仪表盘
    完全基于 geometry，适配任何分辨率
    """
    global smooth_speed

    # =================== 平滑滤波 ===================
    alpha = 0.12
    smooth_speed = smooth_speed * (1 - alpha) + speed * alpha
    speed = max(0, min(smooth_speed, 2.0))

    cx, cy = center

    # =================== 背景环 ===================
    pygame.draw.circle(surface, (35,35,35), center, radius)
    pygame.draw.circle(surface, (15,15,15), center, int(radius * 0.9))

    # 仪表扫过角度 210° → -30° = 240°
    start_deg = 210
    arc_span = 240
    cur_deg = start_deg + (speed / 2.0) * arc_span

    # ---------------- 彩色弧光 ----------------
    seg = 140
    active = int((speed / 2.0) * seg)

    for i in range(active):
        p = i / seg
        col = (int(255 * p), int(255 * (1 - p)), 60)  # green→yellow→red

        ang = math.radians(start_deg + p * arc_span)

        R1 = radius * 0.88
        R2 = radius * 0.72

        x1 = cx + math.cos(ang) * R1
        y1 = cy + math.sin(ang) * R1
        x2 = cx + math.cos(ang) * R2
        y2 = cy + math.sin(ang) * R2

        pygame.draw.line(surface, col, (x1, y1), (x2, y2), 4)

    # ================ 刻度（21个 0.1m/s） ================
    for i in range(21):
        v = i * 0.1
        a = math.radians(start_deg + (v / 2) * arc_span)

        if i % 5 == 0:
            o = 0.92
            inner = 0.75
            lw = 3
        else:
            o = 0.88
            inner = 0.78
            lw = 2

        x1 = cx + math.cos(a) * radius * o
        y1 = cy + math.sin(a) * radius * o
        x2 = cx + math.cos(a) * radius * inner
        y2 = cy + math.sin(a) * radius * inner

        pygame.draw.line(surface, (180,180,180), (x1, y1), (x2, y2), lw)

    # ================ 数字刻度（0–2） ================
    for i in range(5):
        v = i * 0.5
        a = math.radians(start_deg + (v / 2) * arc_span)

        tx = cx + math.cos(a) * radius * 0.60
        ty = cy + math.sin(a) * radius * 0.60

        label = font_small.render(f"{v:.1f}", True, (220,220,220))
        surface.blit(label, (tx - label.get_width()/2, ty - label.get_height()/2))

    # =================== 指针 ===================
    a = math.radians(cur_deg)
    px = cx + math.cos(a) * radius * 0.70
    py = cy + math.sin(a) * radius * 0.70
    pygame.draw.line(surface, (255,60,60), (cx, cy), (px, py), 8)

    # 中心圆
    pygame.draw.circle(surface, (230,230,230), center, int(radius * 0.05))


# =================== 主循环 ===================
clock = pygame.time.Clock()

while True:

    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            quit()

    pygame.event.pump()

    # 手柄输入
    lx = joy.get_axis(0)
    L2 = joy.get_axis(4)
    R2 = joy.get_axis(5)

    cmd = "S"

    if R2 > 0.9:
        if lx < -0.6: cmd = "Y"
        elif lx > 0.6: cmd = "X"
        else: cmd = "F"
    elif L2 > 0.9:
        cmd = "B"
    else:
        if lx < -0.6: cmd = "L"
        elif lx > 0.6: cmd = "R"
        else: cmd = "S"

    send(cmd)

    # =================== UI 绘制 ===================
    screen.fill((10, 10, 10))

    t1 = font.render(f"CMD: {LAST_CMD}", True, (0,255,120))
    screen.blit(t1, (30, 40))

    t2 = font.render(f"Speed(m/s): {CAR_SPEED:.3f}", True, (255,200,0))
    screen.blit(t2, (30, 90))

    draw_gauge(screen, (500, 560), 360, CAR_SPEED)

    pygame.display.update()
    clock.tick(60)
