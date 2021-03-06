/**
 ******************************************************************************
 *
 * @file       GhettoStation.ino
 * @author     Guillaume S
 * @brief      Arduino based antenna tracker & telemetry display for UAV projects.
 * @project    https://code.google.com/p/ghettostation/
 *
 *
 *
 *
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************
 */

#include "Config.h"

#include <avr/pgmspace.h>
#include <Arduino.h>
#ifdef DEBUG
#include <MemoryFree.h>
#endif
#include <PWMServo.h>

#include <Wire.h>

#ifdef ULN2003
#include <CheapStepper.h>
#endif

#include <Metro.h>
#include <MenuSystem.h>
#include <Button.h>
#include <EEPROM.h>
#include <Flash.h>
#include <EEPROM.h>
#include "GhettoStation.h"

#ifdef COMPASS //use additional hmc5883L mag breakout
//HMC5883L i2c mag b
#include <HMC5883L.h>
#endif

#ifdef PROTOCOL_UAVTALK
#include "UAVTalk.cpp"
#endif
#ifdef PROTOCOL_MSP
#include "MSP.cpp"
#endif
#ifdef PROTOCOL_LIGHTTELEMETRY
#include "LightTelemetry.cpp"
#endif
#ifdef PROTOCOL_MAVLINK
#include "Mavlink.cpp"
#endif
#ifdef PROTOCOL_NMEA
#include "GPS_NMEA.cpp"
#endif
#ifdef PROTOCOL_UBLOX
#include "GPS_UBLOX.cpp"
#endif

/*
 * BOF preprocessor bug prevent
 */
#define nop() __asm volatile ("nop")
#if 1
nop();
#endif
/*
 * EOF preprocessor bug prevent
 */

//################################### SETTING OBJECTS ###############################################
// Set the pins on the I2C chip used for LCD connections:
// addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
#ifdef LCD03I2C
  #include <LCD03.h>
LCD03 LCD(I2CADRESS);
#else
#include <LiquidCrystal_I2C.h>
#ifdef LCDLCM1602
LiquidCrystal_I2C LCD(I2CADRESS, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); //   HobbyKing IIC/I2C/TWI Serial 2004 20x4, LCM1602 IIC A0 A1 A2 & YwRobot Arduino LCM1602 IIC V1
#else
LiquidCrystal_I2C LCD(I2CADRESS, 4, 5, 6, 0, 1, 2, 3, 7, NEGATIVE);    //   Arduino-IIC-LCD GY-LCD-V1
  #endif
#endif
#ifdef GLCDEnable
  #include <glcd.h>
  #include "fonts/SystemFont5x7.h"
#endif
#ifdef OLEDLCD
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  #define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);
#endif

//##### LOOP RATES
Metro loop5s = Metro(5000); // 5s loop
Metro loop1hz = Metro(1000); // 1hz loop
Metro loop5hz = Metro(200); //5hz loop
Metro loop10hz = Metro(100); //10hz loop
Metro loop50hz = Metro(20); // 50hz loop
//##### BUTTONS
Button right_button = Button(RIGHT_BUTTON_PIN, BUTTON_PULLUP_INTERNAL);
Button left_button = Button(LEFT_BUTTON_PIN, BUTTON_PULLUP_INTERNAL);
Button enter_button = Button(ENTER_BUTTON_PIN, BUTTON_PULLUP_INTERNAL);

#if defined(COMPASS)
HMC5883L compass;
#endif

// PAN movement case
const uint8_t PAN = 1;
// TILT movement case
const uint8_t TILT = 2;

// stepper mottors with ULN2003 driver
#ifdef ULN2003
CheapStepper stepper_pan (ULN2003_PAN_IN1, ULN2003_PAN_IN2, ULN2003_PAN_IN3, ULN2003_PAN_IN4);
CheapStepper stepper_tilt (ULN2003_TILT_IN1, ULN2003_TILT_IN2, ULN2003_TILT_IN3, ULN2003_TILT_IN4);
#endif

//#################################### SETUP LOOP ####################################################

void setup() {

    //init setup
    init_menu();
    //retrieve configuration from EEPROM
    current_bank = EEPROM.read(0);
    if (current_bank > 3) {
        current_bank = 0;
        EEPROM.write(0, 0);
    }
    EEPROM_read(config_bank[int(current_bank)], configuration);
    // set temp value for servo pwm config
    servoconf_tmp[0] = configuration.pan_minpwm;
    servoconf_tmp[1] = configuration.pan_maxpwm;
    servoconf_tmp[2] = configuration.tilt_minpwm;
    servoconf_tmp[3] = configuration.tilt_maxpwm;
    home_bearing = configuration.bearing; // use last bearing position of previous session.
    voltage_ratio = (float) (configuration.voltage_ratio / 100.0);
    delay(20);
    //clear eeprom & write default parameters if config is empty or wrong
    if (configuration.config_crc != CONFIG_VERSION) {
        clear_eeprom();
        delay(20);
    }
    //init LCD
#ifdef OLEDLCD
        display.begin(SSD1306_SWITCHCAPVCC,I2CADRESS); // initialize with the I2C addr 0x3D (for the 128x64)
        display.display(); // show splashscreen
        delay(2000);
        display.clearDisplay(); // clears the screen and buffer
        // init done
#endif

    init_lcdscreen();
    //start serial com
    init_serial();

    // attach servos
    attach_servo(pan_servo, PAN_SERVOPIN, configuration.pan_minpwm, configuration.pan_maxpwm);
    attach_servo(tilt_servo, TILT_SERVOPIN, configuration.tilt_minpwm, configuration.tilt_maxpwm);

    // move servo to neutral pan & DEFAULTELEVATION tilt at startup
    servoPathfinder(0, DEFAULTELEVATION);


    // stepper mottors with ULN2003 driver - set RPM
#ifdef ULN2003
    stepper_pan.setRpm(ULN2003_RPM);
    stepper_tilt.setRpm(ULN2003_RPM);
#endif

    // setup button callback events
    enter_button.releaseHandler(enterButtonReleaseEvents);
    left_button.releaseHandler(leftButtonReleaseEvents);
    right_button.releaseHandler(rightButtonReleaseEvents);

#if defined(COMPASS)
    compass = HMC5883L(); // Construct a new HMC5883 compass.
    delay(100);
    compass.SetScale(1.3); // Set the scale of the compass.
    compass.SetMeasurementMode(Measurement_Continuous); // Set the measurement mode to Continuous
#endif

    delay(2500); // Wait until osd is initialised

}

//######################################## MAIN LOOP #####################################################################
void loop() {
    smartDelay(0, false);
}

static void smartDelay(unsigned long ms, bool ignore_lcd) {
    unsigned long start = millis();
    do {
        unsigned long start_time = millis();
        unsigned long start_time_micros = micros();
        bool refresh_lcd_flag =  false;

        if (force_refresh_lcd) {
            refresh_lcd_flag = true;
            force_refresh_lcd = false;
        }

        get_telemetry();

        // move stepper mottors with ULN2003 driver
#ifdef ULN2003
        stepper_pan.run();
        stepper_tilt.run();
        if (ULN2003_AUTO_OFF_MS > 0) {
            // check steppers inactivity time - if they are inactive for too long, turn them off
            if (stepper_pan.isTurnedOff() == false && start_time_micros > stepper_pan.getRealLastStepTimeMicros()
                    && start_time_micros - stepper_pan.getRealLastStepTimeMicros() > ((unsigned long) ULN2003_AUTO_OFF_MS) * 1000) {
#ifdef DEBUG
                Serial.println("### turning off PAN stepper due to inactivity");
#endif
                // turn off PAN stepper
                stepper_pan.off();
            }
            if (stepper_tilt.isTurnedOff() == false && start_time_micros > stepper_tilt.getRealLastStepTimeMicros()
                    && start_time_micros - stepper_tilt.getRealLastStepTimeMicros() > ((unsigned long) ULN2003_AUTO_OFF_MS) * 1000) {
#ifdef DEBUG
                Serial.println("### turning off TILT stepper due to inactivity");
#endif
                // turn off TILT stepper
                stepper_tilt.off();
            }
        }
#endif
        if (!ignore_lcd && (current_activity == 1 || current_activity == 18)) {
            // when tracking or setting stepper positions - slow down lcd refresh rate (as it takes too much time)
            if (start_time - last_lcd_refresh_ms > LCD_SLOWDOWN_REFRESH_PERIOD_MS) {
                refresh_lcd_flag = true;
            }
        }

        if (loop5s.check()) {
            //debug output to usb Serial
#if defined(DEBUG)
            debug1();
            debug2();
#endif
        }

        if (loop1hz.check()) {
            read_voltage();

            // calculate avg. loop time each second
            if (loop_time_count > 0) {
                last_avg_loop_time = ((float) loop_time_sum) / ((float) loop_time_count);
                loop_time_sum = 0;
                loop_time_count = 0;
                loop_time_longest = 0;
            }
        }

        if (loop10hz.check() == 1) {
            //update buttons internal states
            enter_button.isPressed();
            left_button.isPressed();
            right_button.isPressed();
#ifdef OSD_OUTPUT
            //pack & send LTM packets to SerialPort2 at 10hz.
            ltm_write();
#endif
            //current activity loop
            check_activity();

            switch (buzzer_status) {
            case 1:
                playTones(1);
                break;
            case 2:
                playTones(2);
                break;
            default:
                break;
            }
        }

        if (loop5hz.check()) {
            if (!ignore_lcd && current_activity != 1 && current_activity != 18) {
                // no tracking, no setting of steppers to init pos. --> refresh LCD at 5Hz
                refresh_lcd_flag = true;
            }
        }

        if (loop50hz.check() == 1) {
            //update servos
            if (current_activity == 1) {
                if ((home_dist / 100) > DONTTRACKUNDER) {
                    servoPathfinder(Bearing, Elevation); // refresh servo
                }
            }
        }

        // update LCD screen if required
        if (refresh_lcd_flag) {
            // Serial.print("<<<<< ");
            // Serial.print(millis());
            // Serial.println(" refresh_lcd()");
            refresh_lcd();
            last_lcd_refresh_ms = millis();
        }

        lcd_data_slowdown_counter++;

        // loop time stats
        loop_time_count++;
        unsigned long loop_time = millis() - start_time;
        if (loop_time_longest < loop_time) {
            loop_time_longest = loop_time;
        }
        loop_time_sum += loop_time;
    } while (millis() - start < ms);
}

//######################################## ACTIVITIES #####################################################################

void check_activity() {
    int stepper_angle;
    if (uav_satellites_visible >= 5) {
        gps_fix = true;
    } else
        gps_fix = false;
    switch (current_activity) {
    case 0:             //MENU
        Bearing = 0;
        Elevation = DEFAULTELEVATION;
        lcddisp_menu();
        if (enter_button.holdTime() >= 1000 && enter_button.held()) { //long press
            displaymenu.back();
        }
        break;
    case 1:            //TRACK
        if ((!home_pos) || (!home_bear)) { // check if home is set before start tracking
            Bearing = 0;
            Elevation = 0;
            current_activity = 2;         // set bearing if not set.
        } else if (home_bear) {
            antenna_tracking();
            if (force_refresh_data_for_lcd || (lcd_data_slowdown_counter % LCD_DATA_SLOWDOWN_RATE == 0)) {
                // unsigned long start = micros();
                // XXX NOTE: call takes about 2ms for LCDLCM1602, 3ms for OLEDLCD!
                if (force_refresh_data_for_lcd) {
                    force_refresh_data_for_lcd = false;
                    force_refresh_lcd = true;
                }
                // Serial.print(">>>>> ");
                // Serial.print(millis());
                // Serial.println(" lcddisp_tracking()");
                lcddisp_tracking();
                // Serial.print("#### lcddisp_tracking: ");
                // Serial.print(micros() - start);
                // Serial.println(" micros");
            }
            if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
                current_activity = 0;
                //telemetry_off();
            }
        }
        break;
    case 2:            //SET HOME
        if (!home_pos)
            lcddisp_sethome();
        else if (home_pos) {
            if (!home_bear) {
                lcddisp_setbearing();
            } else
                lcddisp_homeok();
        }
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            current_activity = 0;
        }
        break;
    case 3:             //PAN_MINPWM
        servoconf_tmp[0] = config_servo(1, 1, servoconf_tmp[0]);
        if (servoconf_tmp[0] != servoconfprev_tmp[0]) {
            detach_servo(pan_servo);
            attach_servo(pan_servo, PAN_SERVOPIN, servoconf_tmp[0], configuration.pan_maxpwm);
        }
        move_servo(PAN, servoconf_tmp[0]);
        servoconfprev_tmp[0] = servoconf_tmp[0];
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            configuration.pan_minpwm = servoconf_tmp[0];
            EEPROM_write(config_bank[int(current_bank)], configuration);
            detach_servo(pan_servo);
            attach_servo(pan_servo, PAN_SERVOPIN, configuration.pan_minpwm, configuration.pan_maxpwm);
            move_servo(PAN, 0, configuration.pan_minangle, configuration.pan_maxangle);
            current_activity = 0;
        }
        break;
    case 4:             //PAN_MINANGLE
        configuration.pan_minangle = config_servo(1, 2, configuration.pan_minangle);
        move_servo(PAN, configuration.pan_minpwm);
        // special case - PAN for stepper - we move to first edge of stepper movement - e.g. 180 degrees
        stepper_angle = configuration.pan_minangle + (360 / 2);
        stepper_angle = stepper_angle % 360;
        stepper_angle = stepper_angle < 0 ? stepper_angle + 360 : stepper_angle;
        move_stepper(PAN, stepper_angle);
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            EEPROM_write(config_bank[int(current_bank)], configuration);
            move_servo(PAN, 0, configuration.pan_minangle, configuration.pan_maxangle);
            move_stepper(PAN, 0);
            current_activity = 0;
        }
        break;
    case 5:             //PAN_MAXPWM
        servoconf_tmp[1] = config_servo(1, 3, servoconf_tmp[1]);
        if (servoconf_tmp[1] != servoconfprev_tmp[1]) {
            detach_servo(pan_servo);
            attach_servo(pan_servo, PAN_SERVOPIN, configuration.pan_minpwm, servoconf_tmp[1]);
        }
        move_servo(PAN, servoconf_tmp[1]);
        servoconfprev_tmp[1] = servoconf_tmp[1];
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            configuration.pan_maxpwm = servoconf_tmp[1];
            EEPROM_write(config_bank[int(current_bank)], configuration);
            detach_servo(pan_servo);
            attach_servo(pan_servo, PAN_SERVOPIN, configuration.pan_minpwm, configuration.pan_maxpwm);
            move_stepper(PAN, 0);
            current_activity = 0;
        }
        break;

    case 6:             //PAN_MAXANGLE
        configuration.pan_maxangle = config_servo(1, 4, configuration.pan_maxangle);
        move_servo(PAN, configuration.pan_maxpwm);
        // special case - PAN for stepper - we move to second edge of stepper movement, e.g. 181 degrees
        stepper_angle = configuration.pan_maxangle + (360 / 2);
        stepper_angle = stepper_angle % 360;
        stepper_angle = stepper_angle < 0 ? stepper_angle + 360 : stepper_angle;
        move_stepper(PAN, stepper_angle);
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            EEPROM_write(config_bank[int(current_bank)], configuration);
            move_servo(PAN, 0, configuration.pan_minangle, configuration.pan_maxangle);
            move_stepper(PAN, 0);
            current_activity = 0;
        }
        break;
    case 7:             //"TILT_MINPWM"
        servoconf_tmp[2] = config_servo(2, 1, servoconf_tmp[2]);
        if (servoconf_tmp[2] != servoconfprev_tmp[2]) {
            detach_servo(tilt_servo);
            attach_servo(tilt_servo, TILT_SERVOPIN, servoconf_tmp[2], configuration.tilt_maxpwm);
        }
        move_servo(TILT, servoconf_tmp[2]);
        servoconfprev_tmp[2] = servoconf_tmp[2];
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            configuration.tilt_minpwm = servoconf_tmp[2];
            EEPROM_write(config_bank[int(current_bank)], configuration);
            detach_servo(tilt_servo);
            attach_servo(tilt_servo, TILT_SERVOPIN, configuration.tilt_minpwm, configuration.tilt_maxpwm);
            move_servo(TILT, 0, configuration.tilt_minangle, configuration.tilt_maxangle);
            current_activity = 0;
        }
        break;
    case 8:             //TILT_MINANGLE
        configuration.tilt_minangle = config_servo(2, 2, configuration.tilt_minangle);
        move_servo(TILT, configuration.tilt_minpwm);
        move_stepper(TILT, configuration.tilt_minangle);
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            EEPROM_write(config_bank[int(current_bank)], configuration);
            move_servo(TILT, 0, configuration.tilt_minangle, configuration.tilt_maxangle);
            move_stepper(TILT, 0);
            current_activity = 0;
        }
        break;
    case 9:             //"TILT_MAXPWM"
        servoconf_tmp[3] = config_servo(2, 3, servoconf_tmp[3]);
        if (servoconf_tmp[3] != servoconfprev_tmp[3]) {
            detach_servo(tilt_servo);
            attach_servo(tilt_servo, TILT_SERVOPIN, configuration.tilt_minpwm, servoconf_tmp[3]);
        }
        move_servo(TILT, servoconf_tmp[3]);
        servoconfprev_tmp[3] = servoconf_tmp[3];
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            configuration.tilt_maxpwm = servoconf_tmp[3];
            EEPROM_write(config_bank[int(current_bank)], configuration);
            detach_servo(tilt_servo);
            attach_servo(tilt_servo, TILT_SERVOPIN, configuration.tilt_minpwm, configuration.tilt_maxpwm);
            move_servo(TILT, 0, configuration.tilt_minangle, configuration.tilt_maxangle);
            current_activity = 0;
        }
        break;
    case 10:                //TILT_MAXANGLE
        configuration.tilt_maxangle = config_servo(2, 4, configuration.tilt_maxangle);
        move_servo(TILT, configuration.tilt_maxpwm);
        move_stepper(TILT, configuration.tilt_maxangle);
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            EEPROM_write(config_bank[int(current_bank)], configuration);
            move_servo(TILT, 0, configuration.tilt_minangle, configuration.tilt_maxangle);
            move_stepper(TILT, 0);
            current_activity = 0;
        }
        break;
    case 11:               //TEST_SERVO
        test_servos();
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            current_activity = 0;
            test_servo_cnt = 360;
            test_servo_step = 1;
            servoPathfinder(0, 0);
        }
        break;

    case 12:                //Configure Telemetry
        lcddisp_telemetry();
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            EEPROM_write(config_bank[int(current_bank)], configuration);
            current_activity = 0;
        }
        break;
    case 13:                //Configure Baudrate
        lcddisp_baudrate();
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            EEPROM_write(config_bank[int(current_bank)], configuration);
            current_activity = 0;
        }
        break;
    case 14:                //Change settings bank
        lcddisp_bank();
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            EEPROM.write(0, current_bank);
            EEPROM_read(config_bank[int(current_bank)], configuration);
            servoconf_tmp[0] = configuration.pan_minpwm;
            servoconf_tmp[1] = configuration.pan_maxpwm;
            servoconf_tmp[2] = configuration.tilt_minpwm;
            servoconf_tmp[3] = configuration.tilt_maxpwm;
            home_sent = 0;
            current_activity = 0;
        }
        break;
    case 15:                //Configure OSD
        lcddisp_osd();
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            EEPROM_write(config_bank[int(current_bank)], configuration);
            home_sent = 0; // force resend an OFrame for osd update
            current_activity = 0;
        }
        break;
    case 16:                //Configure bearing method
        lcddisp_bearing_method();
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            EEPROM_write(config_bank[int(current_bank)], configuration);
            current_activity = 0;
        }
        break;
    case 17:               //Configure voltage multiplier
        lcddisp_voltage_ratio();
        if (enter_button.holdTime() >= 700 && enter_button.held()) { //long press
            configuration.voltage_ratio = (uint16_t) (voltage_ratio * 100.0f);
            EEPROM_write(config_bank[int(current_bank)], configuration);
            current_activity = 0;
        }
        break;
    case 18:                // set steppers to initial positions
        // 0) remember old values for stepN for both steppers
        // 1) move steppers to ZERO positions, show WAIT message
        // 2) when they are both at ZERO, turn off steppers and show user that he can center steppers now
        // 3) when user exits centering process, move steppers to old stepN values from 1)

#ifdef ULN2003
        if (steppers_returning_to_zero_started == false) {
            steppers_returning_to_zero_started = true;
            // 0)
            stepper_pan_step_n = stepper_pan.getStep(); // remember stepN
            stepper_tilt_step_n = stepper_tilt.getStep(); // remember stepN
            // 1)
            stepper_pan.newMoveToWithinOneRound(0, true);
            stepper_tilt.newMoveToWithinOneRound(0, true);
#ifdef DEBUG
                Serial.println("### moving stepper motors to zero positions for later manual centering");
#endif
        }

        if (stepper_pan.getStepsLeft() != 0 && stepper_tilt.getStepsLeft() != 0) {
            // steppers are still moving to their zero positions
            if (force_refresh_data_for_lcd || (lcd_data_slowdown_counter  % LCD_DATA_SLOWDOWN_RATE == 0)) {
                if (force_refresh_data_for_lcd) {
                    force_refresh_data_for_lcd = false;
                    force_refresh_lcd = true;
                }
                // Serial.print(">>>>> ");
                // Serial.print(millis());
                // Serial.println(" lcddisp_init_steppers_wait()");
                lcddisp_init_steppers_wait();
            }
        } else {
            // steppers are at zero postions now - user can move steppers manually
            // 2)
            stepper_pan.off();
            stepper_tilt.off();
            if (force_refresh_data_for_lcd || (lcd_data_slowdown_counter % LCD_DATA_SLOWDOWN_RATE == 0)) {
                if (force_refresh_data_for_lcd) {
                    force_refresh_data_for_lcd = false;
                    force_refresh_lcd = true;
                }
                // Serial.print(">>>>> ");
                // Serial.print(millis());
                // Serial.println(" lcddisp_init_steppers()");
                lcddisp_init_steppers();
            }
        }
#endif
        if (enter_button.holdTime() >= 700 && enter_button.held()) { // long press
#ifdef ULN2003
            // restore steppers to previous steps when initial positions are set
#ifdef DEBUG
            Serial.print("### restoring stepper positions - PAN to: ");
            Serial.print(stepper_pan_step_n);
            Serial.print("\tTILT to: ");
            Serial.println(stepper_tilt_step_n);
#endif
            // restore steppers position
            if (stepper_pan_step_n != 0) {
                stepper_pan.newMoveToWithinOneRound(stepper_pan_step_n, true);
                stepper_pan_step_n = 0;
            }
            if (stepper_tilt_step_n != 0) {
                stepper_tilt.newMoveToWithinOneRound(stepper_tilt_step_n, true);
                stepper_tilt_step_n = 0;
            }
            steppers_returning_to_zero_started = false;
#endif
            current_activity = 0;
        }
        break;
    }
}

//######################################## BUTTONS #####################################################################

void enterButtonReleaseEvents(Button &btn) {
    //Serial.println(current_activity);
    if (enter_button.holdTime() < 700) { // normal press
        if (current_activity == 0) { //button action depends activity state
            displaymenu.select();
        } else if (current_activity == 2) {
            if ((gps_fix) && (!home_pos)) {
                //saving home position
                home_lat = uav_lat;
                home_lon = uav_lon;
                home_alt = uav_alt;
                home_pos = true;
                calc_longitude_scaling(home_lat); // calc lonScaleDown
            } else if ((gps_fix) && (home_pos) && (!home_bear)) {
                //set_bearing();
                switch (configuration.bearing_method) {
                case 1:
                    home_bearing = calc_bearing(home_lon, home_lat, uav_lon, uav_lat); // store bearing relative to north
                    home_bear = true;
                    break;
                case 2:
                case 3:
                case 4:
                    home_bear = true;
                    break;
                default:
                    configuration.bearing_method = 1; // shouldn't happened, restoring default value.
                    break;
                }
                configuration.bearing = home_bearing;
                EEPROM_write(config_bank[int(current_bank)], configuration);
                home_sent = 0; // resend an OFrame to osd
            } else if ((gps_fix) && (home_pos) && (home_bear)) {
                // START TRACKING
                current_activity = 1;
                force_refresh_data_for_lcd = true;
            }
        }

    }

}

void leftButtonReleaseEvents(Button &btn) {
    if (left_button.holdTime() < 700) {
        if (current_activity == 0) {
            displaymenu.prev();
        } else if (current_activity != 0 && current_activity != 1 && current_activity != 2) {
            //We're in a setting area: Left button decrase current value.
            switch (current_activity) {
            case 3:
                servoconf_tmp[0]--;
                break;
            case 4:
                configuration.pan_minangle--;
                break;
            case 5:
                servoconf_tmp[1]--;
                break;
            case 6:
                configuration.pan_maxangle--;
                break;
            case 7:
                servoconf_tmp[2]--;
                break;
            case 8:
                configuration.tilt_minangle--;
                break;
            case 9:
                servoconf_tmp[3]--;
                break;
            case 10:
                configuration.tilt_maxangle--;
                break;
            case 12:
                if (configuration.telemetry > 0)
                    configuration.telemetry -= 1;
                break;
            case 13:
                if (configuration.baudrate > 0)
                    configuration.baudrate -= 1;
                break;
            case 14:
                if (current_bank > 0)
                    current_bank -= 1;
                else
                    current_bank = 3;
                break;
            case 15:
                if (configuration.osd_enabled == 0)
                    configuration.osd_enabled = 1;
                else
                    configuration.osd_enabled = 0;
                break;
            case 16:
                if (configuration.bearing_method > 1)
                    configuration.bearing_method -= 1;
                else
                    configuration.bearing_method = 4;
                break;
            case 17:
                if (voltage_ratio >= 1.0)
                    voltage_ratio -= 0.01;
                break;
            }
        } else if (current_activity == 2) {
            if (configuration.bearing_method == 2) {
                if (home_pos && !home_bear) {
                    home_bearing--;
                    if (home_bearing < 0)
                        home_bearing = 359;
                }
            }
            if (gps_fix && home_pos && home_bear) {
                current_activity = 0;
            }
        } else if (current_activity == 1 && home_pos && home_bear)
            home_bearing--;
    }
}

void rightButtonReleaseEvents(Button &btn) {
    if (right_button.holdTime() < 700) {

        if (current_activity == 0) {
            displaymenu.next();
        } else if (current_activity != 0 && current_activity != 1 && current_activity != 2) {
            //We're in a setting area: Right button decrase current value.
            switch (current_activity) {
            case 3:
                servoconf_tmp[0]++;
                break;
            case 4:
                configuration.pan_minangle++;
                break;
            case 5:
                servoconf_tmp[1]++;
                break;
            case 6:
                configuration.pan_maxangle++;
                break;
            case 7:
                servoconf_tmp[2]++;
                break;
            case 8:
                configuration.tilt_minangle++;
                break;
            case 9:
                servoconf_tmp[3]++;
                break;
            case 10:
                configuration.tilt_maxangle++;
                break;
            case 12:
                if (configuration.telemetry < 5)
                    configuration.telemetry += 1;
                break;
            case 13:
                if (configuration.baudrate < 7)
                    configuration.baudrate += 1;
                break;
            case 14:
                if (current_bank < 3)
                    current_bank += 1;
                else
                    current_bank = 0;
                break;
            case 15:
                if (configuration.osd_enabled == 0)
                    configuration.osd_enabled = 1;
                else
                    configuration.osd_enabled = 0;
                break;
            case 16:
                if (configuration.bearing_method < 5)
                    configuration.bearing_method += 1;
                else
                    configuration.bearing_method = 1;
                break;
            case 17:
                voltage_ratio += 0.01;
                break;
            }
        } else if (current_activity == 2) {
            if (configuration.bearing_method == 2) {
                if (home_pos && !home_bear) {
                    home_bearing++;
                    if (home_bearing > 359)
                        home_bearing = 0;
                }
            }
            if (home_pos && home_bear) {
                // reset home pos
                home_pos = false;
                home_bear = false;
                home_sent = 0;
            }
        } else if (current_activity == 1 && home_pos && home_bear) {
            home_bearing++;
        }
    }
}

//########################################################### MENU #######################################################################################

void init_menu() {
    rootMenu.add_item(&m1i1Item, &screen_tracking); //start track
    rootMenu.add_item(&m1i2Item, &screen_sethome); //set home position
#ifdef ULN2003
    rootMenu.add_item(&m1i5Item, &init_stepper_positions); // init stepper positions
#endif
    rootMenu.add_menu(&m1m3Menu); //configure
    m1m3Menu.add_menu(&m1m3m1Menu); //config servos
    m1m3m1Menu.add_menu(&m1m3m1m1Menu);     //config pan
    m1m3m1m1Menu.add_item(&m1m3m1m1l1Item, &configure_pan_minpwm); // pan min pwm
    m1m3m1m1Menu.add_item(&m1m3m1m1l2Item, &configure_pan_maxpwm); // pan max pwm
    m1m3m1m1Menu.add_item(&m1m3m1m1l3Item, &configure_pan_minangle); // pan min angle
    m1m3m1m1Menu.add_item(&m1m3m1m1l4Item, &configure_pan_maxangle); // pan max angle
    m1m3m1Menu.add_menu(&m1m3m1m2Menu);     //config tilt
    m1m3m1m2Menu.add_item(&m1m3m1m2l1Item, &configure_tilt_minpwm); // tilt min pwm
    m1m3m1m2Menu.add_item(&m1m3m1m2l2Item, &configure_tilt_maxpwm); // tilt max pwm
    m1m3m1m2Menu.add_item(&m1m3m1m2l3Item, &configure_tilt_minangle); // tilt min angle
    m1m3m1m2Menu.add_item(&m1m3m1m2l4Item, &configure_tilt_maxangle); // tilt max angle
    m1m3m1Menu.add_item(&m1m3m1i3Item, &configure_test_servo);
    m1m3Menu.add_menu(&m1m3m2Menu);  //Telemetry
    m1m3m2Menu.add_item(&m1m3m2i1Item, &configure_telemetry); // select telemetry protocol ( Teensy++2 only )
    m1m3m2Menu.add_item(&m1m3m2i2Item, &configure_baudrate); // select telemetry protocol
    m1m3Menu.add_menu(&m1m3m3Menu);  //Others
#ifdef OSD_OUTPUT
    m1m3m3Menu.add_item(&m1m3m3i1Item, &configure_osd);    // enable/disable osd
#endif
    m1m3m3Menu.add_item(&m1m3m3i2Item, &configure_bearing_method); // select tracker bearing reference method
    m1m3m3Menu.add_item(&m1m3m3i3Item, &configure_voltage_ratio); // set minimum voltage
    rootMenu.add_item(&m1i4Item, &screen_bank); //set home position
    displaymenu.set_root_menu(&rootMenu);
}

//menu item callback functions

void screen_tracking(MenuItem *p_menu_item) {
    current_activity = 1;
    force_refresh_data_for_lcd = true;
}

void screen_sethome(MenuItem *p_menu_item) {
    current_activity = 2;
}

void configure_pan_minpwm(MenuItem *p_menu_item) {
    current_activity = 3;
}

void configure_pan_minangle(MenuItem *p_menu_item) {
    current_activity = 4;
}

void configure_pan_maxpwm(MenuItem *p_menu_item) {
    current_activity = 5;
}

void configure_pan_maxangle(MenuItem *p_menu_item) {
    current_activity = 6;
}

void configure_tilt_minpwm(MenuItem *p_menu_item) {
    current_activity = 7;
}

void configure_tilt_minangle(MenuItem *p_menu_item) {
    current_activity = 8;
}

void configure_tilt_maxpwm(MenuItem *p_menu_item) {
    current_activity = 9;
}

void configure_tilt_maxangle(MenuItem *p_menu_item) {
    current_activity = 10;
}

void configure_test_servo(MenuItem *p_menu_item) {
    current_activity = 11;
}

void configure_telemetry(MenuItem *p_menu_item) {
    current_activity = 12;
}

void configure_baudrate(MenuItem *p_menu_item) {
    current_activity = 13;
}

void screen_bank(MenuItem *p_menu_item) {
    current_activity = 14;
}

#ifdef OSD_OUTPUT
void configure_osd(MenuItem *p_menu_item) {
    current_activity = 15;
}
#endif

void configure_bearing_method(MenuItem *p_menu_item) {
    current_activity = 16;
}

void configure_voltage_ratio(MenuItem *p_menu_item) {
    current_activity = 17;
}

void init_stepper_positions(MenuItem *p_menu_item) {
    current_activity = 18;
    force_refresh_data_for_lcd = true;
}

//######################################## TELEMETRY FUNCTIONS #############################################
void init_serial() {
    Serial.begin(115200);
    Serial1.begin(baudrates[configuration.baudrate]);
#ifdef OSD_OUTPUT
    Serial2.begin(OSD_BAUD);
#endif
#ifdef DEBUG
    Serial.println("Serial initialised");
#endif

}

//Preparing adding other protocol
void get_telemetry() {

    if (millis() - lastpacketreceived > 2000) {
        telemetry_ok = false;
    }

#if defined(PROTOCOL_UAVTALK) // OpenPilot / Taulabs
    if (configuration.telemetry == 0) {
        if (uavtalk_read())
            protocol = "UAVT";
    }
#endif

#if defined(PROTOCOL_MSP) // Multiwii
    if (configuration.telemetry == 1) {
        if (!PASSIVEMODE) {
            static unsigned long previous_millis_low = 0;
            static unsigned long previous_millis_high = 0;
            static uint8_t queuedMSPRequests = 0;
            unsigned long currentMillis = millis();
            if ((currentMillis - previous_millis_low) >= 1000) // 1hz
                    {
                setMspRequests();
            }
            if ((currentMillis - previous_millis_low) >= 100) // 10 Hz (Executed every 100ms)
                    {
                blankserialRequest (MSP_ATTITUDE);
                previous_millis_low = millis();
            }
            if ((currentMillis - previous_millis_high) >= 200) // 20 Hz (Executed every 50ms)
                    {
                uint8_t MSPcmdsend;
                if (queuedMSPRequests == 0)
                    queuedMSPRequests = modeMSPRequests;
                uint32_t req = queuedMSPRequests & -queuedMSPRequests;
                queuedMSPRequests &= ~req;
                switch (req) {
                case REQ_MSP_IDENT:
                    // MSPcmdsend = MSP_IDENT;
                    break;
                case REQ_MSP_STATUS:
                    MSPcmdsend = MSP_STATUS;
                    break;
                case REQ_MSP_RAW_GPS:
                    MSPcmdsend = MSP_RAW_GPS;
                    break;
                case REQ_MSP_ALTITUDE:
                    MSPcmdsend = MSP_ALTITUDE;
                    break;
                case REQ_MSP_ANALOG:
                    MSPcmdsend = MSP_ANALOG;
                    break;
                }
                previous_millis_high = millis();
            }
        }
        msp_read();
    }
#endif

#if defined(PROTOCOL_LIGHTTELEMETRY) // Ghettostation light protocol.
    if (configuration.telemetry == 2) {
        ltm_read();
    }
#endif

#if defined(PROTOCOL_MAVLINK) // Ardupilot / PixHawk / Taulabs ( mavlink output ) / Other
    if (configuration.telemetry == 3) {
        if (enable_frame_request == 1) { //Request rate control
            enable_frame_request = 0;
            if (!PASSIVEMODE) {
                request_mavlink_rates();
            }
        }
        read_mavlink();
    }
#endif

#if defined (PROTOCOL_NMEA)
    if (configuration.telemetry == 4) {
        gps_nmea_read();
    }
#endif
#if defined (PROTOCOL_UBLOX)
    if (configuration.telemetry == 5) {
        gps_ublox_read();
    }
#endif
}

//void telemetry_off() {
//  //reset uav data
//  uav_lat = 0;
//  uav_lon = 0;
//  uav_satellites_visible = 0;
//  uav_fix_type = 0;
//  uav_alt = 0;
//  uav_groundspeed = 0;
//  protocol = "";
//  telemetry_ok = false;
//  home_sent = 0;
//  }

//######################################## SERVOS #####################################################################

void move_servo(uint8_t servo_type, int a, int mina, int maxa) {
    if (servo_type == PAN) {
        //convert angle for pan to pan servo reference point: 0° is pan_minangle
        if (a <= 180) {
            a = mina + a;
        } else if ((a > 180) && (a < (360 - mina))) {
            //relevant only for 360° configs
            a = a - mina;
        } else if ((a > 180) && (a > (360 - mina)))
            a = mina - (360 - a);
        // map angle to microseconds
        int microsec = map(a, 0, mina + maxa, configuration.pan_minpwm, configuration.pan_maxpwm);
        move_servo(servo_type, microsec);
    } else if (servo_type == TILT) {
        //map angle to microseconds
        int microsec = map(a, mina, maxa, configuration.tilt_minpwm, configuration.tilt_maxpwm);
        move_servo(servo_type, microsec);
    }
}

void move_servo(uint8_t servo_type, int microsec) {
    switch (servo_type) {
        case PAN:
            pan_servo.writeMicroseconds(microsec);
            break;
        case TILT:
            tilt_servo.writeMicroseconds(microsec);
            break;
        default:
            break;
    }
}

void move_stepper(uint8_t servo_type, int angle_degrees) {
#ifdef ULN2003
    // handle ULN2003 drivers
    int angle_degrees_lim;
    int angle;
    switch (servo_type) {
    case PAN:
        angle_degrees_lim = constrain(angle_degrees, configuration.pan_minangle, configuration.pan_maxangle);
        angle = ULN2003_PAN_REVERSE ? -angle_degrees_lim : angle_degrees_lim;
        stepper_pan.newMoveToDegreeWithinOneRound(angle, true); // movement edge is 180 degrees from init position
//#ifdef DEBUG
//            Serial.print("move_stepper: PAN input angle: ");
//            Serial.print(angle_degrees);
//            Serial.print("\tmoving to angle: ");
//            Serial.print(angle);
//            Serial.print("\t steps left: ");
//            Serial.println(stepper_pan.getStepsLeft());
//#endif
        break;
    case TILT:
        angle_degrees_lim = constrain(angle_degrees, configuration.tilt_minangle, configuration.tilt_maxangle);
        angle = ULN2003_TILT_REVERSE ? -angle_degrees_lim : angle_degrees_lim;
        stepper_tilt.newMoveToDegreeWithinOneRound(angle, false); // we usually move tilt in range 0-90 degrees
//#ifdef DEBUG
//            Serial.print("move_stepper: TILT input angle: ");
//            Serial.print(angle_degrees);
//            Serial.print("\tmoving to angle: ");
//            Serial.print(angle);
//            Serial.print("\t steps left: ");
//            Serial.println(stepper_tilt.getStepsLeft());
//#endif
        break;
    default:
        break;
    }
#endif
}

void servoPathfinder(int angle_b, int angle_a) {   // ( bearing, elevation )
//#ifdef DEBUG
//    Serial.print("# servoPathfinder, angle_b (pan): ");
//    Serial.print(angle_b);
//    Serial.print("\tangle_a (tilt): ");
//    Serial.println(angle_a);
//#endif
//find the best way to move pan servo considering 0° reference face toward
    int angle_b_org = angle_b;
    int angle_a_org = angle_a;
    if (angle_b <= 180) {
        if (configuration.pan_maxangle >= angle_b) {
            //define limits
            if (angle_a <= configuration.tilt_minangle) {
                // checking if we reach the min tilt limit
                angle_a = configuration.tilt_minangle;
            } else if (angle_a > configuration.tilt_maxangle) {
                //shouldn't happend but just in case
                angle_a = configuration.tilt_maxangle;
            }
        } else if (configuration.pan_maxangle < angle_b) {
            //relevant for 180° tilt config only, in case bearing is superior to pan_maxangle
            angle_b = 180 + angle_b;
            if (angle_b >= 360) {
                angle_b = angle_b - 360;
            }
            // invert pan axis
            if (configuration.tilt_maxangle >= (180 - angle_a)) {
                // invert pan & tilt for 180° Pan 180° Tilt config
                angle_a = 180 - angle_a;
            } else if (configuration.tilt_maxangle < (180 - angle_a)) {
                // staying at nearest max pos
                angle_a = configuration.tilt_maxangle;
            }
        }
    } else if (angle_b > 180) {
        if (configuration.pan_minangle > 360 - angle_b) {
            if (angle_a < configuration.tilt_minangle) {
                // checking if we reach the min tilt limit
                angle_a = configuration.tilt_minangle;
            }
        } else if (configuration.pan_minangle <= 360 - angle_b) {
            angle_b = angle_b - 180;
            if (configuration.tilt_maxangle >= (180 - angle_a)) {
                // invert pan & tilt for 180/180 conf
                angle_a = 180 - angle_a;
            } else if (configuration.tilt_maxangle < (180 - angle_a)) {
                // staying at nearest max pos
                angle_a = configuration.tilt_maxangle;
            }
        }
    }
    move_servo(PAN, angle_b, configuration.pan_minangle, configuration.pan_maxangle);
    move_servo(TILT, angle_a, configuration.tilt_minangle, configuration.tilt_maxangle);
#ifdef ULN2003
    // handle ULN2003 drivers
    move_stepper(PAN, angle_b_org);
    move_stepper(TILT, angle_a_org);
#endif
}

void test_servos() {
    lcddisp_testservo();
    switch (test_servo_step) {
    case 1:
        if (test_servo_cnt > 180) {
            servoPathfinder(test_servo_cnt, (360 - test_servo_cnt) / 6);
            test_servo_cnt--;
        } else
            test_servo_step = 2;
        break;
    case 2:
        if (test_servo_cnt < 360) {
            servoPathfinder(test_servo_cnt, (360 - test_servo_cnt) / 6);
            test_servo_cnt++;
        } else {
            test_servo_step = 3;
            test_servo_cnt = 0;
        }
        break;
    case 3:
        if (test_servo_cnt < 360) {
            servoPathfinder(test_servo_cnt, test_servo_cnt / 4);
            test_servo_cnt++;
        } else {
            test_servo_step = 4;
            test_servo_cnt = 0;
        }
        break;
    case 4:
        if (test_servo_cnt < 360) {
            servoPathfinder(test_servo_cnt, 90 - (test_servo_cnt / 4));
            test_servo_cnt++;
        } else {
            // finished
            test_servo_step = 1;
            current_activity = 0;
            servoPathfinder(0, 0);
        }
        break;
    }
}

//######################################## TRACKING #############################################

void antenna_tracking() {
// Tracking general function
    //only move servo if gps has a 3D fix, or standby to last known position.
    if (gps_fix && telemetry_ok) {
        rel_alt = uav_alt - home_alt; // relative altitude to ground in decimeters
        calc_tracking(home_lon, home_lat, uav_lon, uav_lat, rel_alt); //calculate tracking bearing/azimuth
        //set current GPS bearing relative to home_bearing
        if (Bearing >= home_bearing) {
            Bearing -= home_bearing;
        } else
            Bearing += 360 - home_bearing;
    }
}

void calc_tracking(int32_t lon1, int32_t lat1, int32_t lon2, int32_t lat2, int32_t alt) {
    //calculating Bearing & Elevation  in degree decimal
    Bearing = calc_bearing(lon1, lat1, lon2, lat2);
    Elevation = calc_elevation(alt);
}

int16_t calc_bearing(int32_t lon1, int32_t lat1, int32_t lon2, int32_t lat2) {
    float dLat = (lat2 - lat1);
    float dLon = (float) (lon2 - lon1) * lonScaleDown;
    home_dist = sqrt(sq(fabs(dLat)) + sq(fabs(dLon))) * 1.113195; // home dist in cm.
    int16_t b = (int) round(-90 + (atan2(dLat, -dLon) * 57.295775));
    if (b < 0)
        b += 360;
    return b;
}

int16_t calc_elevation(int32_t alt) {
    float at = atan2(alt, home_dist);
    // at = at * 57, 2957795;
    at = at * 57.2957795;
    int16_t e = (int16_t) round(at);
    return e;
}

void calc_longitude_scaling(int32_t lat) {
    float rads = (abs((float)lat) / 10000000.0) * 0.0174532925;
    lonScaleDown = cos(rads);
}

//######################################## COMPASS #############################################

#if defined(COMPASS)
void retrieve_mag() {
// Retrieve the raw values from the compass (not scaled).
    // MagnetometerRaw raw = compass.ReadRawAxis();
// Retrieved the scaled values from the compass (scaled to the configured scale).
    MagnetometerScaled scaled = compass.ReadScaledAxis();
//
// Calculate heading when the magnetometer is level, then correct for signs of axis.
    float heading = atan2(scaled.YAxis, scaled.XAxis);

// Once you have your heading, you must then add your ‘Declination Angle’, which is the ‘Error’ of the magnetic field in your location.
// Find yours here: http://www.magnetic-declination.com/

    float declinationAngle = ((float) MAGDEC) / 1000;
    heading += declinationAngle;

    // Correct for when signs are reversed.
    if (heading < 0)
        heading += 2 * PI;

    // Check for wrap due to addition of declination.
    if (heading > 2 * PI)
        heading -= 2 * PI;

    // Convert radians to degrees for readability.
    home_bearing = (int) round(heading * 180/M_PI);
}
#endif

//######################################## BATTERY ALERT#######################################

void read_voltage() {
    voltage_actual = (analogRead(ADC_VOLTAGE) * 5.0 / 1024.0) * voltage_ratio;
    if (voltage_actual <= MIN_VOLTAGE2)
        buzzer_status = 2;
    else if (voltage_actual <= MIN_VOLTAGE1)
        buzzer_status = 1;
    else
        buzzer_status = 0;
}

void playTones(uint8_t alertlevel) {
    static int toneCounter = 0;
    toneCounter += 1;
    switch (toneCounter) {
    case 1:
        tone(BUZZER_PIN, 1047, 100);
        break;
    case 4:
        if (alertlevel == 1)
            tone(BUZZER_PIN, 1047, 100);
        else if (alertlevel == 2)
            tone(BUZZER_PIN, 1047, 500);
        break;
    case 50:
        if (alertlevel == 2) {
            toneCounter = 0;
        }
        break;
    case 100:
        if (alertlevel == 1) {
            toneCounter = 0;
        }
        break;
    default:
        break;
    }
}

//######################################## DEBUG #############################################

#if defined(DEBUG)
void debug1() {
    Serial.println("==========debug1================");
    Serial.print("memory: ");
    int freememory = freeMem();
    Serial.print(freememory);
    Serial.print("\tcurrent activity: ");
    Serial.print(current_activity);
    Serial.print("\tavg. loop time [ms]: ");
    Serial.print(last_avg_loop_time);
    Serial.print("\tavg. loop [HZ]: ");
    if (last_avg_loop_time > 0) {
        Serial.print(1000 / last_avg_loop_time);
    } else {
        Serial.print(0);
    }
    Serial.print("\tlongest loop time [ms]: ");
    Serial.print(loop_time_longest);
    Serial.print("\tlast gframe processed before [ms]: ");
    if (last_ltm_gframe_time > 0) {
        Serial.print(millis() - last_ltm_gframe_time);
    } else {
        Serial.print("N/A");
    }

#ifdef ULN2003
    float stepper_delay_ms = ((float) stepper_pan.getDelay()) / 1000;
    Serial.print("\tsteppers RPM: ");
    Serial.print(stepper_pan.getRpm());
    Serial.print("\tsteppers delay [ms]: ");
    Serial.print(stepper_delay_ms);
    if (last_avg_loop_time > stepper_delay_ms) {
        Serial.print(" ###");
    }
#endif

    Serial.println();
}

void debug2() {
    Serial.println("==========debug2================");
    Serial.print("Free memory: ");
    int freememory = freeMem();
    Serial.println(freememory);
    Serial.print("Activity: ");
    Serial.println(current_activity);
    Serial.print("Conf. telem.: ");
    Serial.println(configuration.telemetry);
    Serial.print("Baud: ");
    Serial.println(configuration.baudrate);
    Serial.print("Lat: ");
    Serial.println(uav_lat / 10000000.0, 7);
    Serial.print("Lon: ");
    Serial.println(uav_lon / 10000000.0, 7);
    Serial.print("Alt: ");
    Serial.println(uav_alt);
    Serial.print("Relative alt: ");
    Serial.println(rel_alt);
    Serial.print("UAV groundspeed: ");
    Serial.println(uav_groundspeed);
    Serial.print("Home dist: ");
    Serial.println(home_dist);
    Serial.print("Elevation: ");
    Serial.println(Elevation);
    Serial.print("Bearing: ");
    Serial.println(Bearing);
    Serial.print("Home bearing: ");
    Serial.println(home_bearing);
    Serial.print("UAV fix type: ");
    Serial.println(uav_fix_type);
    Serial.print("UAV sats visible: ");
    Serial.println(uav_satellites_visible);
    Serial.print("UAV pitch: ");
    Serial.println(uav_pitch);
    Serial.print("UAV roll: ");
    Serial.println(uav_roll);
    Serial.print("UAV yaw: ");
    Serial.println(uav_heading);
    Serial.print("UAV voltage: ");
    Serial.println(uav_bat);
    Serial.print("UAV current: ");
    Serial.println(uav_amp);
    Serial.print("UAV RSSI: ");
    Serial.println(uav_rssi);
    Serial.print("UAV airspeed: ");
    Serial.println(uav_airspeed);
    Serial.print("UAV arm status: ");
    Serial.println(uav_arm);
    Serial.print("UAV failsafe: ");
    Serial.println(uav_failsafe);
    Serial.print("UAV flightmode: ");
    Serial.println(uav_flightmode);
    Serial.print("Arm failsafe mode: ");
    Serial.println(ltm_armfsmode);
}
#endif
