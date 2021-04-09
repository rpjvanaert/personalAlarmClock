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
}

void loop() {
  
  /*
  if (buttonPressed(BTN_CONFIRM_PIN)){
    minutes += 30;
    timeChanged = true;
  }
  if (buttonPressed(BTN_MODE_PIN)){
    minutes -= 30;
    timeChanged = true;
  }
  if (buttonPressed(BTN_PLUS_PIN)){
    minutes++;
    timeChanged = true;
  }
  if (buttonPressed(BTN_MIN_PIN)){
    minutes--;
    timeChanged = true;
  }
  */

  displayTime(true);
  
  delay(250);
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
  
  //Update NTPClient and get hours, minutes and day.
  timeClient.update();
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int dayIndex = timeClient.getDay();

  //Format time and print on LCD.
  String timeString = getTimeString(hours, minutes);
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

  //Process time offset of hours.
  hours = processTimeOffset(hours);

  //Put hours and minutes into String.
  String hoursString = String(hours);
  String minutesString = String(minutes);

  //Format minutes, put '0' in front if 1 digit.
  if (minutesString.length() == 1){
    minutesString = "0" + minutesString;
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
