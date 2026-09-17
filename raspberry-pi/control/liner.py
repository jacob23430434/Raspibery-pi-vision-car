import pygame
import serial
import time

bt = serial.Serial("COM19", 9600)
time.sleep(6)

pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    print("没有检测到手柄")
    exit()

joy = pygame.joystick.Joystick(0)
joy.init()

print("PS5 Controller:", joy.get_name())
print("蓝牙已连接 → 开始控制")

def send(cmd):
    bt.write(cmd.encode())
    print("发送:", cmd)

while True:
    pygame.event.pump()

    lx = joy.get_axis(0)       # 左摇杆 X
    L2 = joy.get_axis(4)       # 左扳机 (-1~1)
    R2 = joy.get_axis(5)       # 右扳机

    cmd = "S"

    # ========= 右扳机 = 前进 =========
    if R2 > 0.9:
        if lx < -0.6:
            cmd = "Y"  # 左前
        elif lx > 0.6:
            cmd = "X"  # 右前
        else:
            cmd = "F"   # 直线

    # ========= 左扳机 = 后退 =========
    elif L2 > 0.9:
        cmd = "B"

    # ========= 未按扳机 → 就地转 =========
    else:
        if lx < -0.6:
            cmd = "L"
        elif lx > 0.6:
            cmd = "R"
        else:
            cmd = "S"

    send(cmd)
    time.sleep(0.04)  # 25Hz
