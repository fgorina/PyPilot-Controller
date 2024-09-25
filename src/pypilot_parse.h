#ifndef PYPILOT_PARSE_H
#define PYPILOT_PARSE_H

#ifdef __cplusplus
extern "C" {
#endif

int  modeToInt(ap_mode_e mode){

  switch(mode){
    case ap_mode_e::COG_TRUE : 
      return 1;

    case ap_mode_e::APP_WIND:
      return 2;

    case ap_mode_e::HEADING_MAG:
      return 0;

    case ap_mode_e::TRUE_WIND:
      return 3;

  }
  return 0;
}

  bool pypilot_parse(WiFiClient& client) {
    bool found = false;
    String dataFeed = client.readStringUntil('\n');
    // ap.heading=164.798
    // ap.heading_command=220.0000
    // ap.enabled=false
    // ap.mode="compass"
    if (dataFeed.length() > 0) {
      found = true;
      if (dataFeed.startsWith("ap.heading=")) {
        float oldHeading = shipDataModel.steering.autopilot.heading.deg;
        shipDataModel.steering.autopilot.heading.deg =
          strtof(dataFeed.substring(strlen("ap.heading="), dataFeed.length()).c_str(), NULL);
        shipDataModel.steering.autopilot.heading.age = millis();
        setStateCharacteristicHeading(shipDataModel.steering.autopilot.heading.deg);
        redraw = redraw || (fabs(oldHeading - shipDataModel.steering.autopilot.heading.deg) >= 0.5 && (shipDataModel.steering.autopilot.ap_state.st == ap_state_e::ENGAGED || rudderMode)) ;

      } else if (dataFeed.startsWith("ap.heading_command=")) {
        float oldCommand = shipDataModel.steering.autopilot.heading.deg;
        shipDataModel.steering.autopilot.command.deg =
        strtof(dataFeed.substring(strlen("ap.heading_command="), dataFeed.length()).c_str(), NULL);
        shipDataModel.steering.autopilot.command.age = millis();
        setStateCharacteristicCommand(shipDataModel.steering.autopilot.command.deg);
        redraw = redraw || (fabs(oldCommand - shipDataModel.steering.autopilot.command.deg) >= 0.5 && shipDataModel.steering.autopilot.ap_state.st == ap_state_e::ENGAGED);
      } else if (dataFeed.startsWith("ap.enabled=true")) {
        ap_state_e oldState = shipDataModel.steering.autopilot.ap_state.st;
        shipDataModel.steering.autopilot.ap_state.st = ap_state_e::ENGAGED;
        shipDataModel.steering.autopilot.ap_state.age = millis();

        // We update all Pilot related values
        
        setStateCharacteristicEnabled(1);
        setStateCharacteristicMode(modeToInt(shipDataModel.steering.autopilot.ap_mode.mode)); 
        setStateCharacteristicCommand(shipDataModel.steering.autopilot.command.deg); 
        redraw = redraw || (oldState != shipDataModel.steering.autopilot.ap_state.st);
      } else if (dataFeed.startsWith("ap.enabled=false")) {
        ap_state_e oldState = shipDataModel.steering.autopilot.ap_state.st;
        shipDataModel.steering.autopilot.ap_state.st = ap_state_e::STANDBY; 
        shipDataModel.steering.autopilot.ap_state.age = millis();
        setStateCharacteristicEnabled(0);
        setStateCharacteristicMode(4);
        redraw = redraw || (oldState != shipDataModel.steering.autopilot.ap_state.st);
      } else if (dataFeed.startsWith("ap.tack.state=\"")) {
        ap_tack_state_e oldState = shipDataModel.steering.autopilot.tack.st;

        String state = dataFeed.substring(strlen("ap.tack.state=\""), dataFeed.length() - 1);
        shipDataModel.steering.autopilot.tack.st = ap_tack_state_e::TACK_NONE;
        shipDataModel.steering.autopilot.tack.age = millis();

        if(state == "begin"){
          shipDataModel.steering.autopilot.tack.st = ap_tack_state_e::TACK_BEGIN;
        }else if (state == "waiting"){
          shipDataModel.steering.autopilot.tack.st = ap_tack_state_e::TACK_WAITING;
        }else if (state == "tacking"){
          shipDataModel.steering.autopilot.tack.st = ap_tack_state_e::TACK_TACKING;
        }
        setStateCharacteristicTackState(shipDataModel.steering.autopilot.tack.st);

        USBSerial.print("Tack State "); USBSerial.print(shipDataModel.steering.autopilot.tack.st); USBSerial.print(" "); USBSerial.println(state);
        redraw = redraw || (oldState != shipDataModel.steering.autopilot.tack.st);
        
      }else if (dataFeed.startsWith("ap.tack.direction=\"")) {
        ap_tack_direction_e oldDirection = shipDataModel.steering.autopilot.tack.direction;
        String direction = dataFeed.substring(strlen("ap.tack.direction=\""), dataFeed.length() - 1);
        shipDataModel.steering.autopilot.tack.direction = ap_tack_direction_e::TACKING_TO_PORT;
        shipDataModel.steering.autopilot.tack.age = millis();

        if(direction == "starboard"){
          shipDataModel.steering.autopilot.tack.direction = ap_tack_direction_e::TACKING_TO_STARBOARD;
        }
        setStateCharacteristicTackDirection(shipDataModel.steering.autopilot.tack.direction);
        //USBSerial.print("Tack Direction "); USBSerial.print(shipDataModel.steering.autopilot.tack.direction); USBSerial.print(" ");USBSerial.println(direction);
        redraw = redraw || (oldDirection != shipDataModel.steering.autopilot.tack.direction);

      }else if (dataFeed.startsWith("ap.mode=\"")) {
        ap_mode_e oldMode = shipDataModel.steering.autopilot.ap_mode.mode;
        String mode = dataFeed.substring(strlen("ap.mode=\""), dataFeed.length() - 1);
        USBSerial.print("Received "); USBSerial.println(mode);
        shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::MODE_NA;
        shipDataModel.steering.autopilot.ap_mode.age = millis();
        int localMode = 0;
        if (mode == "gps") {
          localMode = 1;
          shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::COG_TRUE;
        } else if (mode == "wind") {
          shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::APP_WIND;
          localMode = 2;
        } else if (mode == "compass") {
          shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::HEADING_MAG;
          localMode = 0;
        } else if (mode == "true wind") {
          shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::TRUE_WIND;
          localMode = 3;
        }
        setStateCharacteristicMode(localMode);
        redraw = redraw || (oldMode != shipDataModel.steering.autopilot.ap_mode.mode);
      } else if (dataFeed.startsWith("servo.voltage=")) {
        shipDataModel.steering.autopilot.ap_servo.voltage.volt =
          strtof(dataFeed.substring(strlen("servo.voltage="), dataFeed.length()).c_str(), NULL);
        shipDataModel.steering.autopilot.ap_servo.voltage.age = millis();
        redraw = redraw || detailMode;
      } else if (dataFeed.startsWith("servo.amp_hours=")) {
        shipDataModel.steering.autopilot.ap_servo.amp_hr.amp_hr =
          strtof(dataFeed.substring(strlen("servo.amp_hours="), dataFeed.length()).c_str(), NULL);
        shipDataModel.steering.autopilot.ap_servo.amp_hr.age = millis();
        redraw = redraw || detailMode;
      } else if (dataFeed.startsWith("servo.controller_temp=")) {
        shipDataModel.steering.autopilot.ap_servo.controller_temp.deg_C =
          strtof(dataFeed.substring(strlen("servo.controller_temp="), dataFeed.length()).c_str(), NULL);
        shipDataModel.steering.autopilot.ap_servo.controller_temp.age = millis();
        redraw = redraw || detailMode;
      } else if (dataFeed.startsWith("servo.position=")) {
        float oldServoPos = shipDataModel.steering.autopilot.ap_servo.position.deg;
        shipDataModel.steering.autopilot.ap_servo.position.deg =
          strtof(dataFeed.substring(strlen("servo.position="), dataFeed.length()).c_str(), NULL);
        shipDataModel.steering.autopilot.ap_servo.position.age = millis();
        //redraw = redraw ||  rudderMode;
        // Because it seems servo.position_command does not work

        if(rudderMode && updateRudder){
          sendRudderCommand();
        }
        
      }else if (dataFeed.startsWith("rudder.angle=")) {
        float oldRudderAngle = shipDataModel.steering.rudder_angle.deg;

        shipDataModel.steering.rudder_angle.deg =
          strtof(dataFeed.substring(strlen("rudder.angle="), dataFeed.length()).c_str(), NULL);
        shipDataModel.steering.rudder_angle.age = millis();
        setStateCharacteristicRudder(shipDataModel.steering.rudder_angle.deg);
        redraw = redraw || (fabs(oldRudderAngle - shipDataModel.steering.rudder_angle.deg) >= 1.0);
      }else {
        found = false;
      }      
    }
    return found;
  }

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
