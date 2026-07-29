// ESP32-Roboter: cmd_vel-Subscriber + Ultraschall-Publisher
// Ultraschall HC-SR04: Trigger GPIO26, Echo GPIO25 (siehe Beleg 4.2.3)

#include <Arduino.h>
#include <micro_ros_platformio.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <geometry_msgs/msg/twist.h>
#include <std_msgs/msg/float32.h>

#if !defined(MICRO_ROS_TRANSPORT_ARDUINO_SERIAL)
#error serieller Transport noetig
#endif

// ---------- Motoren ----------
HardwareSerial MotorSerial(2);
#define START_BYTE 0xAA
#define MODE_STEPS 0

const uint8_t ID_LINKS  = 1;
const uint8_t ID_RECHTS = 2;
const uint8_t DIR_LINKS_VOR  = 1;
const uint8_t DIR_RECHTS_VOR = 0;

const uint16_t STEPS_MAX = 100;
const float    V_MAX     = 0.20;
const float    RADSTAND  = 0.15;

// ---------- Ultraschall ----------
#define TRIG 26
#define ECHO 25

// ---------- ROS ----------
rcl_subscription_t sub_cmd;
rcl_publisher_t    pub_dist;
geometry_msgs__msg__Twist msg_cmd;
std_msgs__msg__Float32    msg_dist;

rclc_executor_t executor;
rclc_support_t  support;
rcl_allocator_t allocator;
rcl_node_t      node;
rcl_timer_t     timer_motor;
rcl_timer_t     timer_sensor;

#define RCCHECK(fn) { rcl_ret_t rc = fn; if(rc != RCL_RET_OK){ error_loop(); } }
#define RCSOFTCHECK(fn) { rcl_ret_t rc = fn; (void)rc; }

volatile float soll_lin = 0.0f;
volatile float soll_ang = 0.0f;

void error_loop() { while(1) delay(100); }

void sendMotorCmd(uint8_t id, uint8_t mode, uint8_t dir, uint16_t param) {
  uint8_t buf[6] = { START_BYTE, id, mode, dir,
                     (uint8_t)(param >> 8), (uint8_t)(param & 0xFF) };
  MotorSerial.write(buf, 6);
  MotorSerial.flush();
  delay(5);
}

void fahreRad(uint8_t id, uint8_t dir_vor, float v) {
  uint8_t dir = (v >= 0.0f) ? dir_vor : (dir_vor ? 0 : 1);
  float betrag = fabsf(v);
  if (betrag > V_MAX) betrag = V_MAX;
  uint16_t schritte = (uint16_t)((betrag / V_MAX) * STEPS_MAX);
  if (schritte == 0) return;
  sendMotorCmd(id, MODE_STEPS, dir, schritte);
}

// Abstandsmessung: 10us Trigger-Puls, dann Echo-Laufzeit messen.
// Schallgeschwindigkeit 343 m/s, Weg hin und zurueck -> halbieren.
float messeAbstand() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  unsigned long dauer = pulseIn(ECHO, HIGH, 25000);  // Timeout ~4 m
  if (dauer == 0) return -1.0f;                      // nichts empfangen
  return dauer * 0.0001715f;                         // in Metern
}

void timer_motor_cb(rcl_timer_t *t, int64_t last) {
  RCLC_UNUSED(last);
  if (t == NULL) return;
  float v_l = soll_lin - (soll_ang * RADSTAND / 2.0f);
  float v_r = soll_lin + (soll_ang * RADSTAND / 2.0f);
  fahreRad(ID_LINKS,  DIR_LINKS_VOR,  v_l);
  fahreRad(ID_RECHTS, DIR_RECHTS_VOR, v_r);
}

void timer_sensor_cb(rcl_timer_t *t, int64_t last) {
  RCLC_UNUSED(last);
  if (t == NULL) return;
  msg_dist.data = messeAbstand();
  RCSOFTCHECK(rcl_publish(&pub_dist, &msg_dist, NULL));
}

void cmd_vel_cb(const void *msgin) {
  const geometry_msgs__msg__Twist *m = (const geometry_msgs__msg__Twist *)msgin;
  soll_lin = m->linear.x;
  soll_ang = m->angular.z;
}

void setup() {
  Serial.begin(115200);
  set_microros_serial_transports(Serial);

  MotorSerial.begin(9600, SERIAL_8N1, -1, 17);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  delay(2000);

  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "esp32_robot", "", &support));

  RCCHECK(rclc_subscription_init_default(&sub_cmd, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist), "cmd_vel"));

  RCCHECK(rclc_publisher_init_default(&pub_dist, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32), "ultrasonic_range"));

  RCCHECK(rclc_timer_init_default(&timer_motor,  &support, RCL_MS_TO_NS(500), timer_motor_cb));
  RCCHECK(rclc_timer_init_default(&timer_sensor, &support, RCL_MS_TO_NS(200), timer_sensor_cb));

  // 3 Handles: Subscription + 2 Timer
  RCCHECK(rclc_executor_init(&executor, &support.context, 3, &allocator));
  RCCHECK(rclc_executor_add_subscription(&executor, &sub_cmd, &msg_cmd, &cmd_vel_cb, ON_NEW_DATA));
  RCCHECK(rclc_executor_add_timer(&executor, &timer_motor));
  RCCHECK(rclc_executor_add_timer(&executor, &timer_sensor));
}

void loop() {
  delay(10);
  RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)));
}