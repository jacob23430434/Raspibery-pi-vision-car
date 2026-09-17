/*
 * Minimal Line Following Controller (extracted from Cyberbotics example)
 * ONLY keeps the Line Following Module (LFM)
 */

#include <stdio.h>
#include <webots/robot.h>
#include <webots/distance_sensor.h>
#include <webots/motor.h>

#define TIME_STEP 5

// ===== Ground sensors =====
#define NB_GROUND_SENS 3
#define GS_LEFT   0
#define GS_CENTER 1
#define GS_RIGHT  2

WbDeviceTag gs[NB_GROUND_SENS];
unsigned short gs_value[NB_GROUND_SENS];

// ===== Motors =====
WbDeviceTag left_motor, right_motor;

// ===== Line Following parameters (原样保留) =====
#define LEFT  0
#define RIGHT 1

#define LFM_FORWARD_SPEED 1000
#define LFM_K_GS_SPEED    5.4

int lfm_speed[2];

// ===== Line Following Module (原封不动) =====
void LineFollowingModule(void) {
  int DeltaS = gs_value[GS_RIGHT] - gs_value[GS_LEFT];

  lfm_speed[LEFT]  = LFM_FORWARD_SPEED - LFM_K_GS_SPEED * DeltaS;
  lfm_speed[RIGHT] = LFM_FORWARD_SPEED + LFM_K_GS_SPEED * DeltaS;
}

// ===== Main =====
int main() {
  wb_robot_init();

  // init ground sensors
  char name[10];
  for (int i = 0; i < NB_GROUND_SENS; i++) {
    sprintf(name, "gs%d", i);
    gs[i] = wb_robot_get_device(name);
    wb_distance_sensor_enable(gs[i], TIME_STEP);
  }

  // init motors
  left_motor  = wb_robot_get_device("left wheel motor");
  right_motor = wb_robot_get_device("right wheel motor");

  wb_motor_set_position(left_motor, INFINITY);
  wb_motor_set_position(right_motor, INFINITY);
  wb_motor_set_velocity(left_motor, 0.0);
  wb_motor_set_velocity(right_motor, 0.0);

  // ===== Main loop =====
  while (wb_robot_step(TIME_STEP) != -1) {

    // read ground sensors
    for (int i = 0; i < NB_GROUND_SENS; i++)
      gs_value[i] = wb_distance_sensor_get_value(gs[i]);

    // line following
    LineFollowingModule();

    // set motor speed
    // 原代码里的比例系数：0.00628
    wb_motor_set_velocity(left_motor,  0.00628 * lfm_speed[LEFT]);
    wb_motor_set_velocity(right_motor, 0.00628 * lfm_speed[RIGHT]);

    // debug
    printf("GS L:%d C:%d R:%d | L:%d R:%d\n",
           gs_value[GS_LEFT],
           gs_value[GS_CENTER],
           gs_value[GS_RIGHT],
           lfm_speed[LEFT],
           lfm_speed[RIGHT]);
  }

  wb_robot_cleanup();
  return 0;
}
