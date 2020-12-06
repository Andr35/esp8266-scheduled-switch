
#include "Arduino.h"
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// ----------------------------------------------------------------------------

#define WIFI_SSID "###"     // TODO set var
#define WIFI_PASSWORD "###" // TODO set var

#define TIME_OFFSET 3600 // NTP offset

// Set hour of the day when light should be turned on
#define LIGHT_ON_HOUR 19
#define LIGHT_ON_MIN 30
// Set hour of the day when light should be turned off
#define LIGHT_OFF_HOUR 23
#define LIGHT_OFF_MIN 0

// Pin used as switch
#define SWITCH_PIN D2 // LED_BUILTIN

// ----------------------------------------------------------------------------

void init_wifi_connection();
void init_program();
void evaluate_alarm();
void do_task();

void set_alarm_at(uint8_t hours, uint8_t minutes);
long start_of_day(long timestamp);

void job_light_on();
void job_light_off();

// ----------------------------------------------------------------------------

enum State
{
  LIGHTS_ON,
  LIGHTS_OFF
};

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// ----------------------------------------------------------------------------

bool led_status = 0;
State next_desidered_state = LIGHTS_ON;
long next_alarm_time = 0;

// ----------------------------------------------------------------------------

void setup()
{

  pinMode(SWITCH_PIN, OUTPUT);

  Serial.begin(115200);

  init_wifi_connection();

  timeClient.setTimeOffset(TIME_OFFSET);
  timeClient.begin();

  init_program();

  delay(1000);
}

void loop()
{
  // Reset WiFi connection
  init_wifi_connection();
  // Check for jobs to run
  evaluate_alarm();
}

// ----------------------------------------------------------------------------

void init_wifi_connection()
{

  bool is_connected = WiFi.status() == WL_CONNECTED;

  if (!is_connected)
  {
    Serial.println("Connect to WiFi");

    WiFi.forceSleepWake();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
  }
}

void init_program()
{
  timeClient.update();
  // Init with light off
  job_light_off();
}

void evaluate_alarm()
{

  timeClient.update();

  long now = timeClient.getEpochTime(); // UNIX seconds

  bool trigger_task = now > next_alarm_time; // seconds

  if (trigger_task)
  {
    // Run task
    do_task();
  }
  else
  {
    long remaining_time = next_alarm_time - now; // seconds
    Serial.printf("Set alarm in %ld seconds\n", remaining_time);

    // long alarm_duration = ((unsigned long)remaining_time) > ((ESP.deepSleepMax() / 1000) / 1000) ? ESP.deepSleepMax() : remaining_time * 1000 * 1000;
    long alarm_duration_ms = remaining_time * 1000;

    WiFi.forceSleepBegin(); // Turn off WiFi
    delay(alarm_duration_ms);

    // ESP.deepSleep(alarm_duration);
  }
}

void do_task()
{

  switch (next_desidered_state)
  {

  case LIGHTS_ON:
    job_light_on();
    break;

  case LIGHTS_OFF:
    job_light_off();
    break;

  default:
    Serial.println("Unknown state!");
    break;
  }
}

void set_alarm_at(uint8_t hours, uint8_t minutes)
{

  Serial.printf("Set next alarm at %02u:%02u\n", hours, minutes);

  long now = timeClient.getEpochTime(); // UNIX seconds
  long now_start_of_day = start_of_day(now);

  long requested_date = now_start_of_day + (hours * 60 * 60) + (minutes * 60);

  if (requested_date < now)
  { // Time is passed -> Add a day
    requested_date += 24 * 60 * 60;
  }

  next_alarm_time = requested_date;

  Serial.printf("[now: %lu] Next alarm is set at %ld\n", now, next_alarm_time);
}

long start_of_day(long timestamp)
{
  return timestamp - (timestamp % 86400); // 86400 = seconds in a day
}

// Jobs -----------------------------------------------------------------------

void job_light_on()
{
  Serial.println("Turn light on!");

  // Turn on
  digitalWrite(SWITCH_PIN, HIGH);
  // Prepare next alarm and next desidered state
  set_alarm_at(LIGHT_OFF_HOUR, LIGHT_OFF_MIN);
  next_desidered_state = LIGHTS_OFF;
}

void job_light_off()
{

  Serial.println("Turn light off!");

  // Turn off
  digitalWrite(SWITCH_PIN, LOW);
  // Prepare next alarm and next desidered state
  set_alarm_at(LIGHT_ON_HOUR, LIGHT_ON_MIN);
  next_desidered_state = LIGHTS_ON;
}