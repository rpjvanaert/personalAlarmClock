#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

//WiFi settings to make use of the NTPClient.
const char *ssid      = "ssidOfThisNetwork";
const char *password  = "notThePasswordOfThisNetwork";

//Initializing udp and timeClient, using nl.pool.ntp.org
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "nl.pool.ntp.org", 3600, 60000);

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

//Defining GPIO for NodeMCUv3 (ESP-12E)
#define BTN_CONFIRM_PIN 14
#define BTN_MODE_PIN 12
#define BTN_PLUS_PIN 13
#define BTN_MIN_PIN 15
#define BUZZER_PIN 0

//Keeping track of Confirm and Mode button to make them rising edge button's.
bool buttonConfirm = false;
bool buttonMode = false;

//Keeping track of hours offset to the ntp time.
int hoursOffset = 1;

//Day array to easily show on LCD.
String day[8] =
{
  "Error #NTP0DAY",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday",
  "Sunday"
};

//State constants for menu on LCD.
const int STATE_TIME_SHOW = 0;
const int STATE_ALARM_SET = 1;
const int STATE_TIME_SET = 2;

int state = 0; //Track current state.

//Currently keeping track of 2 timers (optional more)
const int TIMER_REFRESH_LCD = 0;      //Timer index for refresh rate LCD.
const int TIMER_BUZZER_ALARM = 1;     //Timer index for buzzer on/toggle.
//const int TIMER_BUTTON_PLUS = 2;    //Timer index for optional ramping up button addition.
//const int TIMER_BUTTON_MIN = 3;     //Timer index for optional ramping up button substraction.

//Declaring timer array with current size of 2.
unsigned long timer[2];

//Constant for refresh rate display.
const long REFRESH_DISPLAY_TIME = 5000;    // 5 seconds
//Constant for time buzzer is on at alarm.
const long BUZZER_TOGGLE_TIME_ON = 250;     // 250 milliseconds
//Constant for time buzzer is off at alarm.
const long BUZZER_TOGGLE_TIME_OFF = 750;    // 750 milliseconds

//Optional for ramping up button addition/substraction.
//int buttonPlusCount;
//int buttonMinCount;

//Alarm settings array, keeps track of alarm state.
int alarmSettings[] =
{
  0,  //Enabled/disabled (1/0)
  7,  //Hours
  0,  //Minutes
  1,  //digitalWrite HIGH/LOW state of buzzer (1/0)
  0   //Alarm is running (1/0)
};

void setup() {
  //Setting up serial for debugging.
  Serial.begin(9600);
  
  //Initialise WiFi and try connecting.
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  //If WiFi connected, begin timeClient.
  timeClient.begin();

  //Init LCD, clear and turn on backlight.
  lcd.init();
  lcd.clear();
  lcd.backlight();

  //Initialize buttons to use as input.
  pinMode(BTN_CONFIRM_PIN, INPUT);
  pinMode(BTN_MODE_PIN, INPUT);
  pinMode(BTN_PLUS_PIN, INPUT);
  pinMode(BTN_MIN_PIN, INPUT);
  //Initialize buzzer as output and turning it off (connected to 3v3, not ground).
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);

  //Setting up timers to start from now on. (Checks time by comparing later millis() call)
  timer[TIMER_REFRESH_LCD] = millis();
  timer[TIMER_BUZZER_ALARM] = millis();

  //Posibility for using timer to ramp up plus and minus
  //timer[TIMER_BUTTON_PLUS] = millis();
  //timer[TIMER_BUTTON_MIN] = millis();
}

void loop() {
  //If alarm isn't triggered, do normal menu state sequence, otherwise skip.
  if(!checkAlarm()){
    switch (state){
      case STATE_TIME_SHOW:

        //If mode button is pressed, switch state to alarm set mode.  
        if (buttonAction(BTN_MODE_PIN)){
          state = STATE_ALARM_SET;
          displayAlarm();
          break;
        }

        //Refresh LCD if timer tells to.
        if (timerCheck(TIMER_REFRESH_LCD, REFRESH_DISPLAY_TIME)){
          //Display time with day.
          displayTime(true);
        }
        break;
  
  
  
        
      case STATE_ALARM_SET:

        //If mode button is pressed, switch to time set mode.  
        if (buttonAction(BTN_MODE_PIN)){
          state = STATE_TIME_SET;
          displayTimeSet();
          break;
        }

        //Check buttons 
        if (buttonAction(BTN_CONFIRM_PIN)){
          //If confirm button is pressed, toggle alarm enabled/disabled.
          alarmSettings[0] ^= 1; //togle enable
          displayAlarm(); //Refresh LCD.
          
        } else if (buttonAction(BTN_PLUS_PIN)){
          //If plus button is pressed, add to alarm.
          alarmPlus();
          displayAlarm(); //Refresh LCD.
          
        } else if (buttonAction(BTN_MIN_PIN)){
          //If min button is pressed, substract to alarm.
          alarmMin();
          displayAlarm();//Refresh LCD.
          
        } else if (timerCheck(TIMER_REFRESH_LCD, REFRESH_DISPLAY_TIME)){
          displayAlarm(); //If timer tells, refresh LCD.
        }
  
        break;
  
  
  
        
      case STATE_TIME_SET:

        //If mode button is pressed, switch to time show mode.
        if (buttonAction(BTN_MODE_PIN)){
          state = STATE_TIME_SHOW;
          displayTime(true);
          break;
        }

        //Check buttons
        if (buttonAction(BTN_PLUS_PIN)){
          //If plus button is pressed, add 1 to hours offset.
          ++hoursOffset;
          displayTimeSet(); //Refresh LCD.
          
        } else if (buttonAction(BTN_MIN_PIN)){
          //If min button is pressed, substract 1 to hours offset.
          --hoursOffset;
          displayTimeSet(); //Refresh LCD.
          
        }

        //Loop time around if offset is too much (positive/negative).
        if (hoursOffset > 24){
          hoursOffset = 24;
        } else if (hoursOffset < -24){
          hoursOffset = -24;
        }
  
        if (timerCheck(TIMER_REFRESH_LCD, REFRESH_DISPLAY_TIME)){
          displayTimeSet(); //If timer tells, refresh LCD.
        }
  
        break;
      default:
        //Should do nothing if state is undefined, can't happen.
  
        break;
      
    }
  }
  
    
  delay(50);
}

bool checkAlarm(){
  if (alarmSettings[0] == 1){
    if (buttonAction(BTN_CONFIRM_PIN)){
      alarmSettings[0] = 0;
      digitalWrite(BUZZER_PIN, HIGH);
      alarmSettings[4] = 0;
      Serial.println("Stopping alarm");
      resetTimer(TIMER_REFRESH_LCD);
      if (state == STATE_TIME_SHOW){
        
      }
      return false;
    }

    timeClient.update();
    int hoursCurrent = processTimeOffset(timeClient.getHours());
    int minutesCurrent = timeClient.getMinutes();
    
    if (hoursCurrent == alarmSettings[1] && minutesCurrent == alarmSettings[2]){
      alarmSettings[4] = 1;
      resetTimer(TIMER_REFRESH_LCD);
      displayAlarm(hoursCurrent, minutesCurrent);
      Serial.println("\nAlarm running");
    }

    if (alarmSettings[3] == 1 && timerCheck(TIMER_BUZZER_ALARM, BUZZER_TOGGLE_TIME_ON) && alarmSettings[4] == 1){
      digitalWrite(BUZZER_PIN, HIGH);
      alarmSettings[3] = 0;
      displayAlarm(hoursCurrent, minutesCurrent);
      return true;
      
    } else {
      if (alarmSettings[3] == 0 && timerCheck(TIMER_BUZZER_ALARM, BUZZER_TOGGLE_TIME_OFF) && alarmSettings[4] == 1){
        digitalWrite(BUZZER_PIN, LOW);
        alarmSettings[3] = 1;
        displayAlarm(hoursCurrent, minutesCurrent);
        return true;
      }
    }
  }
  return false;
}


/**
 * alarmPlus
 * 
 * Adds 1 minute to alarm. Handles hours too.
 */
void alarmPlus(){
  //Add 1 to minute of alarm.
  int nextMin = alarmSettings[2] + 1;
  int nextHour = alarmSettings[1];

  //If min gets to/past 60, add 1 to hour and set min to 0.
  if (nextMin >= 60){
    nextMin = 0;
    nextHour += 1;

    //If hour gets to/past 24, set hour to 0.
    if (nextHour >= 24){
      nextHour = 0;
    }
  }

  //Set the new alarm settings.
  alarmSettings[1] = nextHour;
  alarmSettings[2] = nextMin;
}


/**
 * alarmMin
 * 
 * Substracts 1 minute to alarm. Handles hours too.
 */
void alarmMin(){
  //Substract 1 to minute of alarm.
  int nextMin = alarmSettings[2] - 1;
  int nextHour = alarmSettings[1];

  //if min gets under 0, substract to hour and set min to 59.
  if (nextMin < 0){
    nextMin = 59;
    nextHour--;

    //If hour gets under 0, set hour to 23.
    if (nextHour < 0){
      nextHour = 23;
    }
  }

  //Set the new alarm settings.
  alarmSettings[1] = nextHour;
  alarmSettings[2] = nextMin;
}


/**
 * displayAlarm
 * 
 * int hours        Time shown when alarm goes off (hours).
 * int minutes      Time shown when alarm goes off (minutes).
 * 
 * This function displays "ALARM!" on first line and time on second row. Used when alarm goes off.
 */
void displayAlarm(int hours, int minutes){
  //Clear LCD and print "ALARM!" in the middle of the first line.
  lcd.clear();
  lcd.setCursor(5,0);
  lcd.print("ALARM!");

  //Get time String and print this on 
  String timeString = getTimeString(hours, minutes);
  lcd.setCursor(0,1);
  lcd.print(timeString);
}


/**
 * displayTimeSet
 * 
 * Shows menu of time set, to adjust time.
 */
void displayTimeSet(){
  //Clear LCD.
  lcd.clear();

  //Refresh timeClient and get hours and minutes.
  timeClient.update();
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();

  //Format time and print on LCD.
  String timeString = getTimeString(processTimeOffset(hours), minutes);
  lcd.setCursor(0,0);
  lcd.print(timeString);

  //Print "Configure time" on LCD to show adjusting time.
  lcd.setCursor(0,1);
  lcd.print("Configure time");
}


/**
 * displayAlarm
 * 
 * Shows menu of setting alarm.
 */
void displayAlarm(){
  //Clear LCD and show alarm enabled/disabled on first row.
  lcd.clear();
  lcd.setCursor(0,0);

  //Show enabled/disabled based on alarm settings.
  if (alarmSettings[0] == 1){  
    lcd.print("Alarm enabled");
  
  } else {
    lcd.print("Alarm disabled");
  }

  //Format alarm time.
  String alarmTimeString = getTimeString(alarmSettings[1], alarmSettings[2]);

  //Print "at <alarmTime>" on second row.
  lcd.setCursor(0,1);
  lcd.print("at: " + alarmTimeString);
}


/**
 * displayTime
 * 
 * bool day: true to show day on 2nd line
 * 
 * To display time on LCD1602 with optional inclusion of day on 2nd line.
 * NTPClient (timeClient) updates time and gets hours, minutes and day.
 * It then prints this on the LCD.
 * 
 * NOTE:
 *  - Doesn't clear lcd. Chosen for so when updating time using displayTime,
 *    it doesn't blink out and on.
 */
void displayTime(bool day){

  lcd.clear();
  
  //Update NTPClient and get hours, minutes and day.
  timeClient.update();
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int dayIndex = timeClient.getDay();

  //Format time and print on LCD.
  String timeString = getTimeString(processTimeOffset(hours), minutes);
  lcd.setCursor(0,0);
  lcd.print(timeString);

  //If option to show day is on, show day.
  if (day){

    //Format day and print on LCD.
    String dayString = getDayString(dayIndex, hours);
    lcd.setCursor(0,1);
    lcd.print(dayString);
  }
}

/**
 * getTimeString
 * 
 * int hours:   NTPClient getHours.
 * int minutes: NTPClient getMinutes.
 * 
 * Returns formatted time String. Also handles time offset that has been set.
 * Format: "HH : mm".
 */
String getTimeString(int hours, int minutes){

  //Put hours and minutes into String.
  String hoursString = String(hours);
  String minutesString = String(minutes);

  //Format minutes, put '0' in front if 1 digit.
  if (minutesString.length() == 1){
    minutesString = "0" + minutesString;
  }

  //Format hours, put '0' in front if 1 digit.
  if (hoursString.length() == 1){
    hoursString = "0" + hoursString;
  }
  //Return correctly formatted time String.
  return hoursString + " : " + minutesString;
}

/**
 * processTimeOffset
 * 
 * int hours NTPClient's hours to process offset.
 * 
 * Returns processed time according to configured time offset.
 */
int processTimeOffset(int hours){

  //If it's day after, substract 24.
  //f.e.   ( 24:00 -> 00:00 )
  if (isOtherDay(hours) == 1){
    return hours + hoursOffset - 24;

    //If it's day before, add 24.
    //f.e.  ( -1:00 -> 23:00 )
  } else if (isOtherDay(hours) == -1){
    return hours + hoursOffset + 24;
  }

  //If it's the same day, return.
  return hours + hoursOffset;
}

/**
 * isOtherDay
 * 
 * int hours NTPClient's hours to process offset.
 * 
 * Returns an int based on if it's an other day:
 *  ( 0 )   Same day
 *  ( 1 )   Day after
 *  (-1 )   Day before
 */
int isOtherDay(int hours){

  //Add offset of hours
  hours += hoursOffset;

  //If hours tip over, return 1.
  if (hours >= 24){
    return 1;

    //If hours dip under, return -1.
  } else if (hours < 0){
    return -1;
  }

  //Hours match same day, return 0.
  return 0;
}

/**
 * getDayString
 * 
 * int dayIndex   index of the day (1 : 7).
 * int hours      NTPClient's hours.
 * 
 * Returns String of formatted day.
 * Takes into account the hours offset.
 */
String getDayString(int dayIndex, int hours){

  //If day is later, add 1 to index.
  if (isOtherDay(hours) == 1){
    dayIndex++;

    //If day is earlier, substract 1 of index.
  } else if (isOtherDay(hours) ==-1){
    dayIndex--;
  }

  //If day index tips over, set to 1.
  if (dayIndex >= 8){
     dayIndex = 1;

     //If dayIndex dips under, set to 7.
  } else if (dayIndex <=0){
    dayIndex = 7;
  }

  //Return day String from day array.
  return day[dayIndex];
}

/**
 * buttonAction
 * 
 * int pin    pin the button is on.
 * 
 * Returns true if button action should be triggered.
 * There are different type of triggers for the buttons:
 * 
 *  - Button Mode
 *  - Button Confirm
 *  These buttons use rising edge trigger,
 *  thus triggering once if pressed, but unpressed before.
 *  
 *  - Button Plus
 *  - Button Min
 *  These buttons trigger more than once when pressed.
 */
bool buttonAction(int pin){
  //Read out current state of pin given.
  bool currentState = digitalRead(pin);
  
  switch (pin){
    case BTN_CONFIRM_PIN:
      //If confirm button shows rising edge, return true.
      if (buttonConfirm == false && currentState == true){
        buttonConfirm = currentState;
        return true;
      }
      buttonConfirm = currentState; //Make sure current state is now saved.
      break;
      
    case BTN_MODE_PIN:
      //if mode button shows rising edge, return true.
      if (buttonMode == false && currentState == true){
        buttonMode = currentState;
        return true;
      }
      buttonMode = currentState; //Make sure current state is now saved.
      break;
      
    case BTN_PLUS_PIN:
      //If plus button is pressed, return true.
      if (currentState == true){
        return true;
      }
      break;
      
    case BTN_MIN_PIN:
      //If min button is pressed, return true.
      if (currentState == true){
        return true;
      }
      break;
      
    default:
      break;
  }
  //Return false if wrong pin number. (No button defined)
  return false;
}


/**
 * resetTimer
 * 
 * int timerIndex   Resets the timer of index given.
 * 
 * Sets the unsigned long of the array to current millis. Functionally reseting the track of time.
 */
void resetTimer(int timerIndex){
  timer[timerIndex] = millis();
}

/**
 * timerCheck
 * 
 * int timerIndex                   Index of the timer that will be checked.
 * unsigned long timerTriggerMs     Time in ms that has to be passed away for timer to trigger in this check.
 * 
 * Returns true if the timer of the index given, exceeds the time given.
 */

bool timerCheck(int timerIndex, unsigned long timerTriggerMs){
  if (timer[timerIndex] + timerTriggerMs <= millis()){
    resetTimer(timerIndex);
    return true;
  }
  return false;
}
