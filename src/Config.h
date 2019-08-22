/*############################################## CONFIGURATION ####################################################
 # Comment/uncomment/edit according to your needs.
 ##################################################################################################################*/
#define CONFIG_VERSION 1007 // Changing this will reset eeprom to default values

//########## OPTIONS ###############################################################################################

/* If you have communication problem at 56K , set this on. ( ie APM 2/2.5/2.6/AIO )
 Most Arduino have a +2.18% error at 57600 bd, Teensyduino has a -0.74% error. Booth cumulated are too much.
 Successfull com between Teensy & arduino requires 58824 bauds for Teensy.*/
#define BAUDRATE56K 57600
// #define BAUDRATE56K 58824

#define BARO_ALT // Use Baro for Altitude. Comment for using GPS altitude instead.

//Use Mag+imu for heading or GPS heading if not set ( not used for tracker only osd relay )
#define MAGHEADING 1

// #define MAGDEC -600  // Your local Magnetic Declination in radian. Get it from here: http://magnetic-declination.com/  then convert it in milliradian: http://www.wolframalpha.com/input/?i=%280%C2%B0+5%27%29+in+radians
// only needed if using internal compass.
#define MAGDEC 85

//Minimum distance in meters where it will stop moving servos.
#define DONTTRACKUNDER  5

// Prevent Ghettostation to send packets to the flightcontroler
// Usefull if you're using OSD or a GCS at the same time.
#define PASSIVEMODE 1

// Default tilt angle used when not tracking.
#define DEFAULTELEVATION  15

//Memory bank name to display on LCD (18 char max)
#define BANK1  "1.2 GHZ"
#define BANK2  "5.8 Ghz"
#define BANK3  "Bank 3"
#define BANK4  "Bank 4"

//GS Battery alarm
#define MIN_VOLTAGE1 10.5f // First battery alarm level. Will emit 2 short tones every 10 sec.
#define MIN_VOLTAGE2 10.0f // Second battery alarm level. Will emit 1 short + 1 long tone every 5 sec
#define VOLTAGE_RATIO 600   // Default multiplier for battery voltage reading * 100. This can eb adjustd later from the menu.

// Minimum voltage for lipo alert
//########### GROUND OSD TELEMETRY OUTPUT #########################################################################
// Activate osd output (comment if not needed)
// #define OSD_OUTPUT
//OSD output baudrate ( send data as fast as possible to the osd, no need to have the same baudrate as input one. )
#define OSD_BAUD 57600
//########### LCD ##################################################################################################
//LCD model
// #define LCDLCM1602 // (adress: 0x27 or 0x20) HobbyKing IIC/I2C/TWI Serial 2004 20x4, LCM1602 IIC A0 A1 A2 & YwRobot Arduino LCM1602 IIC V1
//#define LCDGYLCD  // (adress: 0x20) Arduino-IIC-LCD GY-LCD-V1Arduino-IIC-LCD GY-LCD-V1
//#define LCD03I2C  // (adress: 0x63 or  0xc6) LCD03 / LCD05
//#define GLCDEnable // Graphical LCD - Using system5x7 font so its nearly 20x4 size
#define OLEDLCD  // Oled 128x64 i2c LCD (address 0x3C or 0x3D)

// I2C LCD Adress
// #define I2CADRESS 0x27 // LCD03/05 have 0x63 or 0xc6 ( even if it's written 0xc6 when powering the lcd03, in fact it uses 0x63 so try booth)
// LCM1602 uses 0x27 & GY-LCD use 0x20
// OLED_LCD use 0x3d or 0x3d
#define I2CADRESS 0x3C

// used for LCD refresh slowdown during tracking, e.g. value 10 means that LCD is refreshed each 10th cycle
#define LCD_SLOWDOWN_RATE 20
//#################################### SERVOS ENDPOINTS #############################################################
// NO NEED TO EDIT THIS
//. Those are just default values when not configured.
// To prevent burning servo they boot starts at neutral for all values. Adjust them directly from the menu.

#define PAN_MAXPWM  2000     //max pan servo pwm value
#define PAN_MAXANGLE 360     //Max angle clockwise (on the right) relative to PAN_MAXPWM.
#define PAN_MINPWM  1000     //min pan servo pwm valuemin pan servo pwm value
#define PAN_MINANGLE 0       //Max angle counter clockwise (on the left) relative to PAN_MINPWM.

#define TILT_MAXPWM 1500    //max tilt pwm value
#define TILT_MAXANGLE 90    //max tilt angle considering 0° is facing toward.
#define TILT_MINPWM 1000    //min tilt pwm value
#define TILT_MINANGLE 0     //minimum tilt angle. Considering 0 is facing toward, a -10 value would means we can tilt 10° down.

//#################################### ULN2003 driver with BYJ48 Stepper Motors  #####################################
// Activate ULN2003 driver (comment out if not needed)
#define ULN2003

//########################################### BOARDS PINOUTS #########################################################
// DON'T EDIT THIS IF YOU DON'T KNOW WHAT YOU'RE DOINGG

// pinout for Arduino Mega 1280/2560
#define TILT_SERVOPIN    12   // PWM Pin for tilt servo           ---> (PB6) - APM PWM OUT ch. 1
#define PAN_SERVOPIN     11   // PWM Pin for pan servo            ---> (PB5) - APM PWM OUT ch. 2
#define BUZZER_PIN        8   // Any PWM pin (add a 100-150 ohm resistor between buzzer & ground) ---> (PH5) - APM PWM OUT ch. 3
#define LEFT_BUTTON_PIN  7    // Any Digital pin                  ---> (PH4) - APM PWM OUT ch. 4 (button should short to GND)
#define RIGHT_BUTTON_PIN 6    // Any Digital pin                  ---> (PH3) - APM PWM OUT ch. 5 (button should short to GND)
#define ENTER_BUTTON_PIN 3    // Any Digital pin                  ---> (PE5) - APM PWM OUT ch. 6 (button should short to GND)
#define ADC_VOLTAGE      62   // ADC pin used for voltage reading ---> (PK0) - APM A8 port ~ 9th ADC port

// ULN2003 driver with BYJ48 Stepper Motors
#ifdef ULN2003
#define ULN2003_PAN_IN1  54    // IN1 for pan servo  ---> APM A0 (PF0 ~ 54)
#define ULN2003_PAN_IN2  55    // IN2 for pan servo  ---> APM A0 (PF1 ~ 55)
#define ULN2003_PAN_IN3  56    // IN3 for pan servo  ---> APM A0 (PF2 ~ 56)
#define ULN2003_PAN_IN4  57    // IN4 for pan servo  ---> APM A0 (PF3 ~ 57)
#define ULN2003_TILT_IN1 58    // IN1 for pan servo  ---> APM A0 (PF4 ~ 58)
#define ULN2003_TILT_IN2 59    // IN2 for tilt servo ---> APM A0 (PF5 ~ 59)
#define ULN2003_TILT_IN3 60    // IN3 for tilt servo ---> APM A0 (PF6 ~ 60)
#define ULN2003_TILT_IN4 61    // IN4 for tilt servo ---> APM A0 (PF7 ~ 61)
#define ULN2003_RPM          15// RPM of stepper motors
#define ULN2003_AUTO_OFF_MS  15000 // if positive, turns off given stepper after inactivity time to protect overheating; disabled if 0 or negative
#define ULN2003_PAN_REVERSE  0 // reverse pan stepper motor
#define ULN2003_TILT_REVERSE 0 // reverse tilt stepper motor
#endif
//################################################## DEBUG ##########################################################
#define DEBUG

//###############################################END OF CONFIG#######################################################
