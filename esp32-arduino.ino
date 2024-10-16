#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

#include <ESP32Servo.h>
#include <PubSubClient.h>

// define OLED parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

//Pin Configuration
#define BUZZER 5
#define LED_1 15 
#define PB_CANCEL 26
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 25
#define DHTPIN 12

// Pin Configurations for LDR & Servo
#define LDR_RIGHT 34
#define LDR_LEFT 35
#define SERVO_PIN 23

#define NTP_SERVER    "pool.ntp.org"
#define UTC_OFFSET 0
#define UTC_OFFSET_DST 0

//declarte objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtsensor;

//global variables
int days = 0;
int hours =0;
int minutes =0;
int seconds = 0;

unsigned long timeNow=0;
unsigned long timeLast=0;

bool alarm_enabled = true;
int n_alarms = 3;
int alarm_hours[] = {0,1,2};
int alarm_minutes[] = {1,10,11};
bool alarm_triggered[] = {false, false, false};

int n_notes = 5;
int A =100;
int B = 200;
int C = 300;
int D = 400;
int E = 500;
int notes[] = {A,B,C,D,E};

int current_mode =0;
int max_modes = 5;
String modes[] = {"1 - Set Time zone","2 - Set Alarm 1 "," 3 - Set alarm 2"," 4 - Set Alarm 3"," 5 - disable alarm"};

WiFiClient espClient;
PubSubClient mqttClient(espClient);

Servo servo;

// Servo motor control parameters
int offset_angle = 30;
float control_factor = 0.75;

float intensity_L; 
float intensity_R; 

char temp_array[6];
char LDR_R[4];
char LDR_L[4];
char angleArray[6];

const char* MQTT_SERVER = "test.mosquitto.org";
char* Min_Angle_Servo = "MIN_ANGLE_SERVO";
char* Control_Factor_Servo = "CONTROLLING_FACTOR_SERVO";

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_OK,INPUT);
  pinMode(PB_UP,INPUT);
  pinMode(PB_DOWN,INPUT);
  pinMode(PB_CANCEL,INPUT);
  

  dhtsensor.setup(DHTPIN,DHTesp::DHT22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.display();
  delay(2000);

 WiFi.begin("Wokwi-GUEST", "");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    display.clearDisplay();
    print_line("Connecting to wifi",0,0,2);
  }
  delay(300);
  display.clearDisplay();
  print_line("Connected to wifi",0,0,2);
  delay(500);
  
  configTime(0, UTC_OFFSET_DST, NTP_SERVER);

  display.clearDisplay();
  print_line("WELCOME TO MEDIBOX",10,20,2);
  delay(2000);
  display.clearDisplay();

//Setting up MQTT Communication
  setup_mqtt();
//Attaching Servo motor 
  servo.attach(SERVO_PIN);
}


void loop(){
  if (!mqttClient.connected()) {
    connect_to_broker();
  }

  mqttClient.loop();

  mqttClient.publish("Temperature",temp_array);

  updateLightIntensity();
  intensity();

  mqttClient.publish("LDR_Left",LDR_L);
  delay(100);
  mqttClient.publish("LDR_Right",LDR_R);
  delay(100);

 
 update_time_with_check_alarms();
 if (digitalRead(PB_OK)==LOW){
   delay(200);
   go_to_menu();
 }
 check_temp();
}

void print_line(String text, int column, int row, int text_size){
  

  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column,row);
  
  display.println(text);
  
  display.display();
  
}

void print_time_now(void){
  display.clearDisplay();
  print_line(String(days), 0, 0, 2);
  print_line(":", 20, 0, 2);
  print_line(String(hours), 30, 0, 2);
  print_line(":", 50, 0, 2);
  print_line(String(minutes), 60, 0, 2);
  print_line(":", 80, 0, 2);
  print_line(String(seconds), 90, 0, 2);
}

void update_time(){
 struct tm timeinfo;
 getLocalTime(&timeinfo);
 char timehour[3];
 strftime(timehour,3, "%H", &timeinfo);
 hours = atoi(timehour);

 char timeminute[3];
 strftime(timeminute,3, "%M", &timeinfo);
 minutes = atoi(timeminute);

 char timesecond[3];
 strftime(timesecond,3, "%S", &timeinfo);
 seconds = atoi(timesecond);

 char timeDay[3];
 strftime(timeDay,3, "%d", &timeinfo);
 days = atoi(timeDay);

}

void ring_alarm(){
  display.clearDisplay();
  print_line("Medicine time",0,0,2);

  digitalWrite(LED_1, HIGH);

  bool break_happened = false;

  while (break_happened == false && digitalRead(PB_CANCEL)==HIGH){
      for(int i=0; i<=n_notes; i++ ){
        if (digitalRead(PB_CANCEL)==LOW){
          delay(200);
          break_happened = true;
          break;
          
        }
        tone(BUZZER, notes[i]);
        delay(500);
        noTone(BUZZER);
        delay(2);
    }
  }
  digitalWrite(LED_1, LOW);
  display.clearDisplay();
  
}

void update_time_with_check_alarms(void){
 update_time();
 print_time_now();

 if (alarm_enabled == true){
  for (int i=0; i<n_alarms; i++){
    if (alarm_triggered[i]== false && alarm_hours[i]== hours && alarm_minutes[i]==minutes){
      ring_alarm();
      alarm_triggered[i] = true;
    }
  }
 }
}

int wait_for_button_press(){
  while(true){
    if (digitalRead(PB_UP)==LOW){
      delay(200);
      return PB_UP;
    }
    if (digitalRead(PB_DOWN)==LOW){
      delay(200);
      return PB_DOWN;
    }
    if (digitalRead(PB_OK)==LOW){
      delay(200);
      return PB_OK;
    }
    if (digitalRead(PB_CANCEL)==LOW){
      delay(200);
      return PB_CANCEL;
    }
    update_time();
  }
}

void go_to_menu(){
  while (digitalRead(PB_CANCEL)==HIGH){
    display.clearDisplay();
    print_line(modes[current_mode],0,0,2);

    int pressed = wait_for_button_press();

    if (pressed==PB_UP){
      delay(200);
      current_mode +=1;
      current_mode = current_mode % max_modes;
    }
    if (pressed==PB_DOWN){
      delay(200);
      current_mode -=1;
      if (current_mode<0){
        current_mode = max_modes-1;
      }
    }
    if (pressed==PB_OK){
      delay(200);
      run_mode(current_mode);

    }
    if (pressed==PB_CANCEL){
      delay(200);
      break;
    }

  }
}

void set_time_zone(){
 int zone_hour = 0;

 while(true){

 display.clearDisplay();
 print_line("Enter hour"+String(zone_hour),0,0,2);

 int pressed = wait_for_button_press();

    if (pressed==PB_UP){
      delay(200);
      zone_hour +=1;
      zone_hour = zone_hour % 12;
      Serial.println(zone_hour);
    }
    if (pressed==PB_DOWN){
      delay(200);
      zone_hour -=1;
      if (zone_hour<-12){
        zone_hour = 11;
      }
    }
    if (pressed==PB_OK){
      delay(200);
      zone_hour = zone_hour;
      break;

    }
    if (pressed==PB_CANCEL){
      delay(200);
      break;

}


}

int zone_minute = 0;

while(true){

 display.clearDisplay();
 print_line("Enter minute"+String(zone_minute),0,0,2);

int pressed = wait_for_button_press();

    if (pressed==PB_UP){
      delay(200);
      zone_minute +=1;
      zone_minute = zone_minute % 60;
    }
    if (pressed==PB_DOWN){
      delay(200);
      zone_minute -=1;
      if (zone_minute<0){
        zone_minute = 59;
      }
    }
    if (pressed==PB_OK){
      delay(200);
      minutes = zone_minute;
      break;

    }
    if (pressed==PB_CANCEL){
      delay(200);
      break;

}
}

configTime(zone_hour*3600+ minutes*60, UTC_OFFSET_DST, NTP_SERVER);

display.clearDisplay();
print_line("Time zone Is Set",0,0,2);
delay(1000);

}

void set_alarm(int alarm){

int temp_hour = alarm_hours[alarm];

while(true){

   display.clearDisplay();
 print_line("Enter hour"+String(temp_hour ),0,0,2);

int pressed = wait_for_button_press();

    if (pressed==PB_UP){
      delay(200);
      temp_hour +=1;
      temp_hour = temp_hour % 24;
    }
    if (pressed==PB_DOWN){
      delay(200);
      temp_hour -=1;
      if (temp_hour<0){
        temp_hour = 23;
      }
    }
    if (pressed==PB_OK){
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;

    }
    if (pressed==PB_CANCEL){
      delay(200);
      break;

}


}

int temp_minute = alarm_minutes[alarm];

while(true){

   display.clearDisplay();
 print_line("Enter minute"+String(temp_minute ),0,0,2);

int pressed = wait_for_button_press();

    if (pressed==PB_UP){
      delay(200);
      temp_minute +=1;
      temp_minute = temp_minute % 60;
    }
    if (pressed==PB_DOWN){
      delay(200);
      temp_minute -=1;
      if (temp_minute<0){
        temp_minute = 59;
      }
    }
    if (pressed==PB_OK){
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;

    }
    if (pressed==PB_CANCEL){
      delay(200);
      break;

}
}
display.clearDisplay();
print_line("Alarm Is Set",0,0,2);
delay(1000);

}

void run_mode(int mode){
  if (mode==0){
    set_time_zone();
  }
  if (mode==1 || mode==2 || mode == 3){
    set_alarm(mode-1);

  }
   
  if (mode==4){
    alarm_enabled= false;
    display.clearDisplay();
    print_line("All alarms desabled",0,0,2);
    delay(500);
    alarm_triggered[0] = {false};
    alarm_triggered[1] = {false};
    alarm_triggered[2] = {false};

  }
}

void check_temp(){
  TempAndHumidity data = dhtsensor.getTempAndHumidity();
  //
  String(data.temperature,2).toCharArray(temp_array,6);
  //
  if (data.temperature>32){
    display.clearDisplay();
    print_line("Temp high",0,40,1);
  }
  if (data.temperature<26){
    display.clearDisplay();
    print_line("Temp low",0,40,1);
  }

  if (data.humidity>80){
    display.clearDisplay();
    print_line("Humidity high",0,50,1);
  }
  if (data.humidity<60){
    display.clearDisplay();
    print_line("Humidity low",0,50,1);
  }
}

void setup_mqtt() {
  mqttClient.setServer(MQTT_SERVER, 1883);
  mqttClient.setCallback(receive_callback);
}

//To connect with MQTT Broker
void connect_to_broker() {
  while (!mqttClient.connected()) {
    Serial.println("Trying to establish MQTT Connection...");

    if (mqttClient.connect("ESP32-21060721")) {
      Serial.println("Connected");
      mqttClient.subscribe(Min_Angle_Servo);
      mqttClient.subscribe(Control_Factor_Servo);
    } else {
      Serial.println("Failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void receive_callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message received [" + String(topic) + "]");

  char payload_array[length];
  for (int i = 0; i < length; i++) {
    payload_array[i] = (char) payload[i];
  }

  if (strcmp(topic, Min_Angle_Servo) == 0) {
    offset_angle = atof(payload_array);
    Serial.println("Offset Angle = " + String(offset_angle));
  } else if (strcmp(topic, Control_Factor_Servo) == 0) {
    control_factor = atof(payload_array);
    Serial.println("Controlling Factor = " + String(control_factor));
  }

  calculate_servo_position();
}

void updateLightIntensity() {
  int avL = analogRead(LDR_LEFT);
  int avR = analogRead(LDR_RIGHT);

  intensity_L = map(avL, 0, 4095, 100, 0)/100.00;
  intensity_R = map(avR, 0, 4095, 100, 0)/100.00;

  String(intensity_L).toCharArray(LDR_L,4);
  String(intensity_R).toCharArray(LDR_R,4);
  calculate_servo_position();

}

void calculate_servo_position() {
  float intensityMax = intensity_L;
  float D = 1.5;

  if (intensity_L<intensity_R){
    intensityMax = intensity_R;
    float D = 0.5;

  }
  int angle = offset_angle*D + (180 - offset_angle) * intensityMax * control_factor;
  int servo_angle = min(angle,180);

  Serial.println("Motor Angle = " + String(servo_angle));

  servo.write(servo_angle);

  String(servo_angle).toCharArray(angleArray, 6);

  mqttClient.publish("Motor_Angle_Servo", angleArray);
}
void intensity(){
  Serial.println("Light intensity at Left: "+ String(intensity_L));
  delay(100);
  Serial.println("Light intensity at Right: "+ String(intensity_R));
  delay(100);
}

