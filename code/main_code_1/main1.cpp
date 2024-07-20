#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

// OLED display properties
#define SCREEN_WIDTH 128 //OLEDcdisaplay width ,  in pixels
#define SCREEN_HEIGHT 64  // OLED dispaly height ,  in pixels
#define OLED_RESET   -1  // Reset pin  # (or  -1 if  sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // OLED display I2C adress

//Declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

//Global Variables
int days = 0;// 
int hours = 0;
int minutes = 0;
int seconds = 0;

//unsigned timeNow = 0;
//unsigned timeLast = 0;

bool alarm_enabled = true;//turn on alarms 
int n_alarms = 3;//alarm quntitiy to track the alarms
int alarm_hours[] = {0, 1, 2}; //initial alarm hours
int alarm_minutes[] = {1, 0, 0};// Initial alarm minutes
int alarm_triggered[] = {false, false,false}; // Track if alarm is triggered

// Pin definitions
#define BUZZER 5 
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12

#define NTP_SERVER     "pool.ntp.org"  // Time synchronisation using wifi
int UTC_OFFSET = 0;
#define UTC_OFFSET_DST 0

// Music notes_1 frequencies
int n_notes_1 = 8;
int n_notes_2 = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes_1[8] = {C, D, E, F, G, A, B, C_H};
int notes_2[8] = {C, E, G, B, D, F, A, C_H};

// Menu state variables
int current_mode = 0;
int max_modes = 5;
String modes[] = {"1 - Set UTC offset", "2 - Set Alarm 1", "3 - Set Alarm 2","4 - Set Alarm 3", "5 - Disable Alarms" };

void setup() {
  // Setup pin modes
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  // Setup DHT sensor
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  Serial.begin(9600);
  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation faild"));
    for (;;); // Halt if display initialization fails
  }
  //turn on oled display 
  display.display();
  delay(2000); // Pause for 2 seconds

  // Connect to WiFi
  WiFi.begin("Wokwi-GUEST", "", 6); 
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WIFI", 0, 0, 2);//while connecting to wifi
  }
  display.clearDisplay();
  print_line("Connected to WIFI", 0, 0, 2);//when connected to wifi

  // Configure time synchronization
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  /// Display welcome message
  display.clearDisplay();
  print_line("Welcome to medi box!", 10, 20, 2);
  for (int i = 0; i <= n_notes_1; i++) {
    tone(BUZZER, notes_2[i]);//ring the buzzer
      delay(50);
      noTone(BUZZER);
      delay(2);
  }
  //delay(200);
  display.clearDisplay();
}


void loop() {
  update_time_with_check_alarm();//continously cheak the time
  if (digitalRead(PB_OK) == LOW) { 
    delay(200);
    go_to_menu();
  }
  check_temp_and_humidity();//call function to cheak tem and humedity continoiusly
}

// Function to print a text line on the OLED display
void print_line(String text, int column, int row, int text_size) {
  display.setTextSize(text_size); // Noraml 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);
  display.display();
  //delay(2000);
}

// Function to print the current time on the OLED display
void print_time_now(void) {
  display.clearDisplay();
  print_line(String(days), 0, 0, 2);
  print_line(":", 20, 0, 2);
  print_line(String(hours), 30, 0, 2);
  print_line(":", 50, 0, 2);
  print_line(String(minutes), 60, 0, 2);
  print_line(":", 80, 0, 2);
  print_line(String(seconds), 90, 0, 2);
}

// Function to update the time
void update_time() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);// a to i function to convert string to integer

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);// a to i function to convert string to integer

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);// a to i function to convert string to integer

  char timeDay[3];
  strftime(timeDay, sizeof(timeDay), "%d", &timeinfo);
  days = atoi(timeDay);
}

// Function to ring the alarm
void ring_alarm() {
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);//Display when alarm triggered

  digitalWrite(LED_1, HIGH); //ligh up the  LED

  bool break_happen = false;//to stop while statment
  //Ring the BUZZER
  while (break_happen == false && digitalRead(PB_CANCEL) == HIGH) {
    for (int i = 0; i <= n_notes_1; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);//prevent bouncing of the push button 
        break_happen = true;//stop alarm when push cancle
        break;//stop for statement
      }
      tone(BUZZER, notes_1[i]);//ring the buzzer
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}

// Function to update time and check for alarm triggers
void update_time_with_check_alarm(void) {
  update_time();
  print_time_now();

  if (alarm_enabled == true) {
    for (int i = 0; i < n_alarms; i++) {
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}

// Function to wait for button press
int wait_for_button_press() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(200);//delay for debouncing
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);//delay for debouncing
      return PB_DOWN;
    }
    else if (digitalRead(PB_OK) == LOW) {
      delay(200);//delay for debouncing
      return PB_OK;
    }
    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);//delay for debouncing
      return PB_CANCEL;
    }
    update_time();//inside this we have to keep updating the time
  }
}

// Function to navigate through the menu
void go_to_menu() { 
  while (digitalRead(PB_CANCEL) == HIGH) { //cheak if the cancle is press to stay in memnu untill cancle
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);//pass current mode to prinline function
    int pressed = wait_for_button_press();
    if ( pressed == PB_UP) {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_modes;//if the reach max put it back to zero
    }
    else if ( pressed == PB_DOWN) {
      delay(200);
      current_mode -= 1;
      if (current_mode < 0) { //reach bello zero put it back to max mode
        current_mode = max_modes - 1;
      }
    }
    else if ( pressed == PB_OK) {
      delay(200);
      //Serial.println(current_mode);
      run_mode(current_mode);//this function take current mode as the input
    }
    else if ( pressed == PB_CANCEL) {
      delay(200);
      break;//calcle button exit go to menu function
    }
  }
}

// Function to set the time
void set_UTC_offset() { 
  int input_hour = 0;
  while (true) {// to set hours
    display.clearDisplay();
    print_line("Enter hour: " + String(input_hour), 0, 0, 2); //Enter hours
    //change hrs  up down key set - ok  not changing global hours 
    int pressed = wait_for_button_press();
    if ( pressed == PB_UP) {
      delay(200);
      input_hour += 1;
      if (input_hour > 14){
        input_hour = -12;
      }
    }
    else if ( pressed == PB_DOWN) {
      delay(200);
      input_hour -= 1;
      if (input_hour < -12) { //reach bellow zero put it back to max mode
        input_hour = 14;
      }
    }
    else if ( pressed == PB_OK) {
      delay(200);
      //Serial.println(current_mode);
      hours = input_hour;//global varible set local varible value
      break;//stop the loop
    }
    else if ( pressed == PB_CANCEL) {
      delay(200);
      break;//calcle button exit go to menu function
    }
  }
  
  int input_minute = minutes;
  while (true) {//to set minutes
    display.clearDisplay();
    print_line("Enter minute: " + String(input_minute), 0, 0, 2); 
    int pressed = wait_for_button_press();
    if ( pressed == PB_UP) {
      delay(200);
      input_minute += 1;
      input_minute = input_minute % 45;
    }
    else if ( pressed == PB_DOWN) {
      delay(200);
      input_minute -= 1;
      if (input_minute < 0) { //reach bellow zero put it back to max mode
        input_minute = 45;
      }
    }
    else if ( pressed == PB_OK) {
      delay(200);
      //Serial.println(current_mode);
      minutes = input_minute;
      break;//stop the loop
    }
    else if ( pressed == PB_CANCEL) {
      delay(200);
      break;//calcle button exit go to menu function
    }
  }
  UTC_OFFSET = 3600*input_hour + 60*input_minute;
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  display.clearDisplay();//display the msg time set and exit
  print_line("Time is set", 0, 0, 2);
  delay(1000);
}

// Function to set an alarm
void set_alarm(int alarm) {
  int temp_hour = alarm_hours[alarm];//take relavent alarm hrs
  while (true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);
    //change hrs  up down key set - ok  not changing global alarm_hours[alarm] we chnage it when ok play
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;//not to go over 24 hrs or negative numer
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0) { //reach bello zero put it back to max mode
        temp_hour = 23;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      //Serial.println(current_mode);
      alarm_hours[alarm] = temp_hour;
      break;//stop the loop
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;//cancle button exit go to menu function
    }
  }
  //This is for change mi minutes
  int temp_minute = alarm_minutes[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2); 
    //change hrs  up down key set - ok  not changing global alarm_minutes[alarm] we chnage it when ok play
    int pressed = wait_for_button_press();
    if ( pressed == PB_UP) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;//not to go over 24 hrs or negative numer
    }
    else if ( pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0) { //reach bello zero put it back to max mode
        temp_minute = 59;
      }
    }
    else if ( pressed == PB_OK) {
      delay(200);
      //Serial.println(current_mode);
      alarm_minutes[alarm] = temp_minute;
      break;//stop the loop
    }
    else if ( pressed == PB_CANCEL) {
      delay(1000);
      break;//calcle button exit go to menu function
    }
  }
  display.clearDisplay();//display the msg time set and exit
  print_line("Alarm is set", 0, 0, 2);
  delay(1000);
}

// Function to execute a mode based on the menu selection
void run_mode(int mode) { //add separate function to set time
  if (mode == 0) {
    set_UTC_offset();
  }
  //fucntion accept alarm number as input
  else if (mode == 1 || mode == 2 ||mode ==3 ) {
    set_alarm(mode - 1);//alarm number= current mode -1
  }
  else if (mode == 4) {//all alarm 
    alarm_enabled = false;
    display.clearDisplay();
    print_line("Disable all alarms", 0, 0, 2);
    delay(200);
  }
}

// Function to check temperature and humidity
void check_temp_and_humidity() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  display.clearDisplay();
  if (data.temperature > 32 ) {
      print_line("    TEMP HIGH", 3, 40, 1);
  }
  else if (data.temperature < 26 ) {
      print_line("    TEMP LOW", 3, 40, 1);
  }
  if (data.humidity > 80 ) {
    
    print_line("    Humidity HIGH", 3, 50, 1);
  }
  else if (data.humidity < 60 ) {
    
    print_line("    Humidity LOW", 3, 50, 1);
  }
  delay(500);
}


//LED blink
void LED_Blink() {
  digitalWrite(LED_1, HIGH);
  delay(500);
  digitalWrite(LED_1, LOW);
  delay(500);
}
