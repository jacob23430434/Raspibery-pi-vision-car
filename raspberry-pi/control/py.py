import pygame
import serial
import time

# ======= HC06 蓝牙串口 =======
bt = serial.Serial("COM19", 9600)  # 修改为你的端口号
time.sleep(6)

# ======= 初始化手柄 =======
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
    L2 = joy.get_axis(4)       # 左扳机
    R2 = joy.get_axis(5)       # 右扳机

    cmd = "S"  # 默认停止

    # 左右优先
    if lx < -0.6:
        cmd = "L"
    elif lx > 0.6:
        cmd = "R"

    # 前后优先（覆盖左右）
    if R2 > 0.9:
        cmd = "F"
    elif L2 > 0.9:
        cmd = "B"

    send(cmd)
    time.sleep(0.04)  # 25Hz
