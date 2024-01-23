#ifndef SHIP_DATA_MODEL_H
#define SHIP_DATA_MODEL_H

#ifdef __cplusplus
extern "C" {
#endif



typedef unsigned long age_t;

  typedef enum {
    STATE_NA = -1,
    STANDBY = 0,
    ENGAGED = 1
  } ap_state_e;

  typedef enum {
    CMD_TYPE_NA = -1,
    FOLLOW_DIR = 0,
    FOLLOW_ROUTE = 1
  } ap_cmd_type_e;

  typedef enum {
    TACKING_TO_PORT = -1,
    TACKING_TO_STARBOARD = 1
  } ap_tack_direction_e;

  typedef enum {
    TACK_NONE = 0,
    TACK_BEGIN = 1,
    TACK_WAITING = 2,
    TACK_TACKING = 3
  }ap_tack_state_e;

typedef struct _ap_tack_t {
    ap_tack_state_e st;
    ap_tack_direction_e direction;
    age_t age = 0U;
  } ap_tack_t;

typedef struct _ap_state_t {
    ap_state_e st;
    age_t age = 0U;
  } ap_state_t;

typedef struct _voltage_V_t {
    float volt;
    age_t age = 0U;
  } voltage_V_t;

typedef struct _servo_position_t {
    float deg = 0.0;
    age_t age = 0U;
  } _servo_position_t;


typedef struct _amp_hr_t {
    float amp_hr;
    age_t age = 0U;
  } amp_hr_t;


  typedef struct _deg_C_t {
    float deg_C;
    age_t age = 0U;
  } deg_C_t;

  typedef struct _angle_deg_t {
    float deg = 330;
    age_t age = 0U;
  } angle_deg_t;


 typedef struct _ap_cmd_type_t {
    ap_cmd_type_e cmd;
    age_t age = 0U;
  } ap_cmd_type_t;

  typedef enum {
    MODE_NA = -1,
    HEADING_MAG = 0,
    HEADING_TRUE = 1,
    APP_WIND = 2,
    APP_WIND_MAG = 3,
    APP_WIND_TRUE = 4,
    TRUE_WIND = 5,
    TRUE_WIND_MAG = 6,
    TRUE_WIND_TRUE = 7,
    GROUND_WIND_MAG = 8,
    GROUND_WIND_TRUE = 9,
    COG_MAG = 10,
    COG_TRUE = 11,
  } ap_mode_e;

  typedef struct _ap_mode_t {
    ap_mode_e mode;
    age_t age = 0U;
  } ap_mode_t;

 typedef struct _ap_servo_t {
    struct _voltage_V_t voltage;
    struct _deg_C_t controller_temp;
    struct _amp_hr_t amp_hr;
    struct _servo_position_t position;
  } ap_servo_t;

typedef struct _autopilot_t {
    struct _ap_state_t ap_state;
    struct _angle_deg_t heading;
    struct _angle_deg_t command;
    struct _ap_cmd_type_t command_type;
    struct _ap_mode_t ap_mode;
    struct _ap_servo_t ap_servo;
    struct _ap_tack_t tack;
  } autopilot_t;



typedef struct _steering_t {
    struct _angle_deg_t rudder_angle;
    struct _autopilot_t autopilot;
  } steering_t;


typedef struct _ship_data_t {   
    struct _steering_t steering;
} ship_data_t;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
