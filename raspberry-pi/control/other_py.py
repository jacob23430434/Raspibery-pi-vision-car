import pygame
import serial
import time
import threading

# ===================蓝牙===================
bt = serial.Serial("COM19", 9600, timeout=0.01)
time.sleep(4)

# ===================手柄===================
pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    print("没有检测到手柄")
    exit()

joy = pygame.joystick.Joystick(0)
joy.init()

# ===================界面===================
WIDTH, HEIGHT = 500, 260
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Car Control HUD")

font = pygame.font.SysFont("consolas", 28)
font_small = pygame.font.SysFont("consolas", 22)

# 界面数据缓存
CAR_SPEED = "0.000"
LAST_CMD = "S"

# ================== 蓝牙读取线程 ==================
def bt_reader():
    global CAR_SPEED
    buffer = ""
    while True:
        if bt.in_waiting > 0:
            try:
                raw = bt.read().decode('utf-8', errors='ignore')
                if raw == "\n":
                    data = buffer.strip()
                    if data != "":
                        CAR_SPEED = data
                    buffer = ""
                else:
                    buffer += raw
            except:
                pass
        time.sleep(0.002)

threading.Thread(target=bt_reader, daemon=True).start()

# ================== 发送命令 ==================
def send(cmd):
    global LAST_CMD
    LAST_CMD = cmd
    bt.write(cmd.encode())


# =================== 主循环 ===================
clock = pygame.time.Clock()

while True:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            quit()

    pygame.event.pump()

    lx = joy.get_axis(0)    # 左摇杆 X
    L2 = joy.get_axis(4)
    R2 = joy.get_axis(5)

    cmd = "S"

    # ========== 前进 ==========
    if R2 > 0.9:
        if lx < -0.6:
            cmd = "Y"
        elif lx > 0.6:
            cmd = "X"
        else:
            cmd = "F"

    # ========== 后退 ==========
    elif L2 > 0.9:
        cmd = "B"

    # ========== 转向 ==========
    else:
        if lx < -0.6:
            cmd = "L"
        elif lx > 0.6:
            cmd = "R"
        else:
            cmd = "S"

    send(cmd)

    # =========== UI绘制 ============
    screen.fill((20, 20, 20))

    # 左侧控制指令
    t1 = font.render(f"CMD      : {LAST_CMD}", True, (0, 255, 0))
    screen.blit(t1, (30, 40))

    # 右侧车速显示
    t2 = font.render(f"Speed(m/s): {CAR_SPEED}", True, (255, 200, 0))
    screen.blit(t2, (30, 110))

    # 提示
    tip = font_small.render("LT=后退  RT=前进  X轴转向", True, (180, 180, 180))
    screen.blit(tip, (30, 180))

    pygame.display.update()
    clock.tick(25)
