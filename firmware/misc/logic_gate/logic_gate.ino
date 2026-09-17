// 引脚定义
#define PWM_PIN 5      // D5，PWM输出
#define CTL_PIN 4      // D4，方向控制
#define ENC_A 2        // D2，编码器A
#define ENC_B 3        // D3，编码器B

// 参数设置
const float PULSE_PER_REV = 20.0;       // 编码器每圈脉冲数
const float WHEEL_DIAMETER = 6.5;       // 轮子直径（cm）
const float WHEEL_CIRCUMFERENCE = WHEEL_DIAMETER * 3.1416; // 轮子周长（cm）

volatile long encoderCount = 0;
volatile bool encoderLastA = LOW;
unsigned long lastCalcTime = 0;
long lastEncoderCount = 0;
float speed_cm_s = 0.0;
float distance_cm = 0.0;
int lastPwm = 0;

void setup() {
  pinMode(PWM_PIN, OUTPUT);
  pinMode(CTL_PIN, OUTPUT);
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  Serial.begin(115200);
  attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, CHANGE);
  encoderLastA = digitalRead(ENC_A);

  Serial.println("NAND门电机系统初始化完成");
  lastCalcTime = millis();
}

void loop() {
  // 前进: CTL=0
  digitalWrite(CTL_PIN, LOW);
  sweepPWM();

  delay(500);

  // 后退: CTL=1
  digitalWrite(CTL_PIN, HIGH);
  sweepPWM();

  delay(500);

  updateSpeedDistance(); // 保证无论如何100ms周期更新一次速度
}

// PWM扫频：从0到255再到0
void sweepPWM() {
  for (int pwm = 0; pwm <= 255; pwm += 5) {
    analogWrite(PWM_PIN, pwm);
    lastPwm = pwm; // 记录最新PWM值
    updateSpeedDistance();
    delay(10);
  }
  for (int pwm = 255; pwm >= 0; pwm -= 5) {
    analogWrite(PWM_PIN, pwm);
    lastPwm = pwm;
    updateSpeedDistance();
    delay(10);
  }
  analogWrite(PWM_PIN, 0);
  lastPwm = 0;
}

// 编码器中断
void encoderISR() {
  bool A = digitalRead(ENC_A);
  bool B = digitalRead(ENC_B);

  if (A != encoderLastA) {
    encoderCount += (B != A) ? 1 : -1;
    encoderLastA = A;
  }
}

// 速度距离计算+串口输出
void updateSpeedDistance() {
  unsigned long now = millis();
  unsigned long timeDelta = now - lastCalcTime;
  if (timeDelta >= 100) {
    long countDelta = encoderCount - lastEncoderCount;
    float revDelta = countDelta / PULSE_PER_REV;
    float distanceDelta = revDelta * WHEEL_CIRCUMFERENCE;
    distance_cm += distanceDelta;
    speed_cm_s = distanceDelta * (1000.0 / timeDelta);

    lastEncoderCount = encoderCount;
    lastCalcTime = now;

    Serial.print("CTL=");
    Serial.print(digitalRead(CTL_PIN));
    Serial.print(" | PWM=");
    Serial.print(lastPwm);
    Serial.print(" | 速度=");
    Serial.print(speed_cm_s, 2);
    Serial.print(" cm/s | 距离=");
    Serial.print(distance_cm, 2);
    Serial.print(" cm | 编码器=");
    Serial.println(encoderCount);
  }
}