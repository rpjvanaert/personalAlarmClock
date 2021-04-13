#include <LiquidCrystal_I2C.h>

#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

const char *ssid      = "ssidOfThisNetwork";
const char *password  = "notThePasswordOfThisNetwork";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "nl.pool.ntp.org", 3600, 60000);

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

#define BTN_CONFIRM_PIN 14
#define BTN_MODE_PIN 12
#define BTN_PLUS_PIN 13
#define BTN_MIN_PIN 15
#define BUZZER_PIN 0

bool buttonConfirm = false;
bool buttonMode = false;

int hoursOffset = 1;

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

const int STATE_TIME_SHOW = 0;
const int STATE_ALARM_SET = 1;
const int STATE_TIME_SET = 2;

int state = 0;

const int TIMER_REFRESH_LCD = 0;
const int TIMER_BUZZER_ALARM = 1;

//const int TIMER_BUTTON_PLUS = 2;
//const int TIMER_BUTTON_MIN = 3;

unsigned long timer[2];
const long REFRESH_DISPLAY_TIME = 5000;    // 5 seconds

const long BUZZER_TOGGLE_TIME_ON = 250;     // 250 milliseconds
const long BUZZER_TOGGLE_TIME_OFF = 750;    // 750 milliseconds

int buttonPlusCount;
int buttonMinCount;

int alarmSettings[] =
{
  0,  //Enabled/disabled (1/0)
  7,  //Hours
  0,  //Minutes
  1,  //digitalWrite HIGH/LOW state of buzzer (1/0)
  0   //Alarm is running (1/0)
};

void setup() {
  Serial.begin(9600);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  timeClient.begin();
  
  lcd.init();
  lcd.clear();
  lcd.backlight();

  pinMode(BTN_CONFIRM_PIN, INPUT);
  pinMode(BTN_MODE_PIN, INPUT);
  pinMode(BTN_PLUS_PIN, INPUT);
  pinMode(BTN_MIN_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);

  timer[TIMER_REFRESH_LCD] = millis();
  timer[TIMER_BUZZER_ALARM] = millis();

  //Posibility for using timer to ramp up plus and minus
  //timer[TIMER_BUTTON_PLUS] = millis();
  //timer[TIMER_BUTTON_MIN] = millis();
}

void loop() {

  if(!checkAlarm()){
    switch (state){
      case STATE_TIME_SHOW:
        //code
  
        if (buttonAction(BTN_MODE_PIN)){
          state = STATE_ALARM_SET;
          displayAlarm();
          break;
        }
        
        if (timerCheck(TIMER_REFRESH_LCD, REFRESH_DISPLAY_TIME)){
          displayTime(true);
        }
        
        break;
  
  
  
        
      case STATE_ALARM_SET:
        //code
  
        if (buttonAction(BTN_MODE_PIN)){
          state = STATE_TIME_SET;
          displayTimeSet();
          break;
        }
  
        if (buttonAction(BTN_CONFIRM_PIN)){
          alarmSettings[0] ^= 1; //togle enable
          displayAlarm();
        } else if (buttonAction(BTN_PLUS_PIN)){
          alarmPlus();
          displayAlarm();
        } else if (buttonAction(BTN_MIN_PIN)){
          alarmMin();
          displayAlarm();
        } else if (timerCheck(TIMER_REFRESH_LCD, REFRESH_DISPLAY_TIME)){
          displayAlarm();
        }
  
        break;
  
  
  
        
      case STATE_TIME_SET:
        //code
  
        if (buttonAction(BTN_MODE_PIN)){
          state = STATE_TIME_SHOW;
          displayTime(true);
          break;
        }
  
        if (buttonAction(BTN_PLUS_PIN)){
          ++hoursOffset;
          displayTimeSet();
        } else if (buttonAction(BTN_MIN_PIN)){
          --hoursOffset;
          displayTimeSet();
        }
        if (hoursOffset > 24){
          hoursOffset = 24;
        } else if (hoursOffset < -24){
          hoursOffset = -24;
        }
  
        if (timerCheck(TIMER_REFRESH_LCD, REFRESH_DISPLAY_TIME)){
          displayTimeSet();
        }
  
        break;
      default:
        //code
  
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

void alarmPlus(){
  int nextMin = alarmSettings[2] + 1;
  int nextHour = alarmSettings[1];
  if (nextMin >= 60){
    nextMin = 0;
    nextHour += 1;
    if (nextHour >= 24){
      nextHour = 0;
    }
  }
  alarmSettings[1] = nextHour;
  alarmSettings[2] = nextMin;
}

void alarmMin(){
  int nextMin = alarmSettings[2] - 1;
  int nextHour = alarmSettings[1];
  if (nextMin < 0){
    nextMin = 59;
    nextHour--;
    if (nextHour < 0){
      nextHour = 23;
    }
  }
  alarmSettings[1] = nextHour;
  alarmSettings[2] = nextMin;
}

void displayAlarm(int hours, int minutes){
  lcd.clear();
  
  String timeString = getTimeString(hours, minutes);

  lcd.setCursor(5,0);
  lcd.print("ALARM!");

  lcd.setCursor(0,1);
  lcd.print(timeString);
}

void displayTimeSet(){
  lcd.clear();
  
  timeClient.update();
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int dayIndex = timeClient.getDay();

  //Format time and print on LCD.
  String timeString = getTimeString(processTimeOffset(hours), minutes);
  lcd.setCursor(0,0);
  lcd.print(timeString);

  lcd.setCursor(0,1);

  lcd.print("Configure time");
}

void displayAlarm(){

  //TODO FIX ALARM showing
  lcd.clear();

  lcd.setCursor(0,0);
  
  if (alarmSettings[0] == 1){  
    lcd.print("Alarm enabled");
  
  } else {
    lcd.print("Alarm disabled");
  }

  String alarmTimeString = getTimeString(alarmSettings[1], alarmSettings[2]);

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

  if (hoursString.length() == 1){
    hoursString = "0" + hoursString;
  }

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
  bool currentState = digitalRead(pin);
  switch (pin){
    case BTN_CONFIRM_PIN:
      if (buttonConfirm == false && currentState == true){
        buttonConfirm = currentState;
        return true;
      }
      buttonConfirm = currentState;
      break;
      
    case BTN_MODE_PIN:
      if (buttonMode == false && currentState == true){
        buttonMode = currentState;
        return true;
      }
      buttonMode = currentState;
      break;
      
    case BTN_PLUS_PIN:
      if (currentState == true){
        return true;
      }
      break;
      
    case BTN_MIN_PIN:
      if (currentState == true){
        return true;
      }
      break;
      
    default:
      break;
  }
  return false;
}

void resetTimer(int timerIndex){
  timer[timerIndex] = millis();
}

bool timerCheck(int timerIndex, unsigned long timerTriggerMs){
  if (timer[timerIndex] + timerTriggerMs <= millis()){
    resetTimer(timerIndex);
    return true;
  }
  return false;
}
