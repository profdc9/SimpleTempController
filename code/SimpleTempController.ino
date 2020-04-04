/* Simple Temperature Controller */

/*
 * Copyright (c) 2020 Daniel Marks

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
 */

#include <Arduino.h>
#include <EEPROM.h>

#define PINLED1 2
#define PINLED2 5
#define PINLED3 6
#define PINLED4 7

#define PINBUTTON1 10
#define PINBUTTON2 11
#define PINBUTTON3 12

#define PINSPEAKER 9
#define PINRELAY 17

#define ANALOGLM35 A0
#define DIVISOR 500000u

#define EEPROM_CONFIGURATION_STORE_ADDRESS 0 

// These are in units of 0.01 C
#define TEMP_HYSTERESIS 100u
#define TEMP_FAULT 1000u

typedef enum _temp_parameter
{
  TEMP_60C =  1,
  TEMP_65C =  2,
  TEMP_70C =  3,
  TEMP_75C =  4,
  TEMP_80C = 5,
  TEMP_90C = 6,
  TEMP_100C = 7
} temp_parameter;

typedef enum _time_parameter
{
  TIME_10min =  1,
  TIME_20min =  2,
  TIME_30min =  3,
  TIME_45min =  4,
  TIME_60min =  5,
  TIME_90min =  6,
  TIME_120min = 7
} time_parameter;

typedef enum _hysteresis_parameter
{
  HYSTERESIS_1C =  1,
  HYSTERESIS_2C =  2,
  HYSTERESIS_3C =  3,
  HYSTERESIS_4C =  4,
  HYSTERESIS_5C =  5,
  HYSTERESIS_6C =  6,
  HYSTERESIS_7C =  7
} hysteresis_parameter;

const uint8_t temp_states[8] = { 0, 60, 65, 70, 75, 80, 90, 100 };
const uint8_t time_states[8] = { 0, 10, 20, 30, 45, 60, 90, 120 };
const uint8_t hysteresis_states[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
const uint8_t led_states[8] = { 0x00, 0x01, 0x02, 0x03, 0x05, 0x09, 0x0B, 0x0F };

typedef struct _work_settings
{
  temp_parameter temp_parm;
  time_parameter tim_parm;
  hysteresis_parameter hysteresis_parm;
} work_settings;

work_settings settings = { TEMP_70C, TIME_30min, HYSTERESIS_2C };

void updateEEPROM()
{
  EEPROM.put(EEPROM_CONFIGURATION_STORE_ADDRESS, settings);
}

void readEEPROM()
{
  EEPROM.get(EEPROM_CONFIGURATION_STORE_ADDRESS, settings);
  if ((settings.temp_parm < 1) || (settings.temp_parm > (sizeof(temp_states)/sizeof(uint8_t))))
      settings.temp_parm = TEMP_70C;
  if ((settings.tim_parm < 1) || (settings.tim_parm > (sizeof(time_states)/sizeof(uint8_t))))
      settings.tim_parm = TIME_30min;
  if ((settings.hysteresis_parm < 1) || (settings.hysteresis_parm > (sizeof(hysteresis_states)/sizeof(uint8_t))))
      settings.hysteresis_parm = HYSTERESIS_2C;
}
void beep_speaker(int frequency, int duration)
{
  uint16_t waitlen = DIVISOR / frequency;
  uint32_t inittime = millis();
  uint8_t st = 0;
  while (((uint32_t)(millis()-inittime)) < duration)
  {
     st ^= 1;
     digitalWrite(PINSPEAKER,st ? HIGH : LOW);
     delayMicroseconds(waitlen);
  }
  digitalWrite(PINSPEAKER,LOW);
}

volatile uint8_t curStateButton1 = HIGH;
volatile uint8_t curStateButton2 = HIGH;
volatile uint8_t curStateButton3 = HIGH;
volatile uint8_t constStateButton1 = 0;
volatile uint8_t constStateButton2 = 0;
volatile uint8_t constStateButton3 = 0;
volatile uint8_t pushedButton1 = false;
volatile uint8_t pushedButton2 = false;
volatile uint8_t pushedButton3 = false;

bool relayState = false;

ISR(TIMER2_COMPA_vect)
{
  uint8_t state;
  state = digitalRead(PINBUTTON1);
  if (curStateButton1 != state)
  {
    if ((++constStateButton1) >= 10)
    {
        curStateButton1 = state;
        if (state == LOW) pushedButton1 = true;
        constStateButton1 = 0;
    }
  } else constStateButton1 = 0;

  state = digitalRead(PINBUTTON2);
  if (curStateButton2 != state)
  {
    if ((++constStateButton2) >= 10)
    {
        curStateButton2 = state;
        if (state == LOW) pushedButton2 = true;
        constStateButton2 = 0;
    }
  } else constStateButton2 = 0;

  state = digitalRead(PINBUTTON3);
  if (curStateButton3 != state)
  {
    if ((++constStateButton3) >= 10)
    {
        curStateButton3 = state;
        if (state == LOW) pushedButton3 = true;
        constStateButton3 = 0;
    }
  } else constStateButton3 = 0;
}

bool button1Pushed()
{
  if (!pushedButton1) return false;
  pushedButton1 = false;
  return true;
}

bool button2Pushed()
{
  if (!pushedButton2) return false;
  pushedButton2 = false;
  return true;
}

bool button3Pushed()
{
  if (!pushedButton3) return false;
  pushedButton3 = false;
  return true;
}

void clearButtonPushed()
{
  pushedButton1 = false;
  pushedButton2 = false;
  pushedButton3 = false;
}

void setup_timer2(void)
{
  cli();
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2  = 0;
  OCR2A = 200;
  TCCR2A |= (1 << WGM21);
  TCCR2B |= (1 << CS22);   
  TIMSK2 |= (1 << OCIE2A);
  sei();
}

void setup() {
  Serial.begin(9600);

  digitalWrite(PINLED1,HIGH);
  pinMode(PINLED1,OUTPUT);
  digitalWrite(PINLED2,HIGH);
  pinMode(PINLED2,OUTPUT);
  digitalWrite(PINLED3,HIGH);
  pinMode(PINLED3,OUTPUT);
  digitalWrite(PINLED4,HIGH);
  pinMode(PINLED4,OUTPUT);
  digitalWrite(PINSPEAKER,LOW);
  pinMode(PINSPEAKER,OUTPUT);
  digitalWrite(PINRELAY,LOW);
  pinMode(PINRELAY,OUTPUT);

  pinMode(PINBUTTON1,INPUT_PULLUP);
  pinMode(PINBUTTON2,INPUT_PULLUP);
  pinMode(PINBUTTON3,INPUT_PULLUP);
  pinMode(ANALOGLM35,INPUT);

  setup_timer2();
  readEEPROM();
}

void setRelay(bool state)
{
  relayState = state;
  digitalWrite(PINRELAY,state);
}

void indicate_state(uint8_t state)
{
  uint8_t state_vals = led_states[state];
  digitalWrite(PINLED1, state_vals & 0x01 ? LOW : HIGH);
  digitalWrite(PINLED2, state_vals & 0x02 ? LOW : HIGH);
  digitalWrite(PINLED3, state_vals & 0x04 ? LOW : HIGH);
  digitalWrite(PINLED4, state_vals & 0x08 ? LOW : HIGH);
}

bool checkButtonState(volatile uint8_t &state, uint8_t waittime)
{
  for (uint8_t i=0;i<waittime;i++)                 // make the button held down for 2 seconds
  {
    indicate_state(i & 0x01 ? 7 : 0);
    if (state == HIGH) 
    {
      indicate_state(0);
      return false;
    }
    delay(100);
  }
  return true;
}

bool setHysteresis(void)
{
  uint8_t st = 0;
  if (!checkButtonState(curStateButton3,50)) return false;
  setRelay(false);  // turn off heater
  digitalWrite(PINLED1,HIGH);
  digitalWrite(PINLED2,HIGH);
  digitalWrite(PINLED3,HIGH);
  digitalWrite(PINLED4,HIGH);
  for (uint8_t i=0;i<5;i++)
  {
    digitalWrite(PINLED1,LOW);
    beep_speaker(400,300);
    digitalWrite(PINLED1,HIGH);
    digitalWrite(PINLED4,LOW);
    beep_speaker(700,300);
    digitalWrite(PINLED4,HIGH);
    digitalWrite(PINLED2,LOW);
    beep_speaker(500,300);
    digitalWrite(PINLED2,HIGH);
    digitalWrite(PINLED3,LOW);
    beep_speaker(600,300);
    digitalWrite(PINLED3,HIGH);
  }
  digitalWrite(PINLED2,HIGH);
  clearButtonPushed();
  for (;;)
  {
    st ^= 1;
    indicate_state(st ? settings.hysteresis_parm : 0);
    delay(300);
    if (button1Pushed()) 
    {
      beep_speaker(400,300);
      delay(300);
      beep_speaker(400,300);
      indicate_state(0);
      updateEEPROM();
      break;
    }
    if (button2Pushed())
    {
      beep_speaker(600,300);
      settings.hysteresis_parm = ((uint8_t)settings.hysteresis_parm)+1;
      if (settings.hysteresis_parm >= (sizeof(hysteresis_states)/sizeof(uint8_t)))
        settings.hysteresis_parm = 1;
    }
  }
  return true;
}

void setTemp(void)
{
  uint8_t st = 0;
  if (!checkButtonState(curStateButton1,50)) return;
  setRelay(false);  // turn off heater
  for (uint8_t i=0;i<5;i++)
  {
    digitalWrite(PINLED1,LOW);
    beep_speaker(400,300);
    digitalWrite(PINLED1,HIGH);
    digitalWrite(PINLED2,LOW);
    beep_speaker(500,300);
    digitalWrite(PINLED2,HIGH);
    digitalWrite(PINLED3,LOW);
    beep_speaker(600,300);
    digitalWrite(PINLED3,HIGH);
    digitalWrite(PINLED4,LOW);
    beep_speaker(700,300);
    digitalWrite(PINLED4,HIGH);
  }
  digitalWrite(PINLED3,HIGH);
  clearButtonPushed();
  for (;;)
  {
    st ^= 1;
    indicate_state(st ? settings.temp_parm : 0);
    delay(300);
    if (button1Pushed()) 
    {
      beep_speaker(400,300);
      delay(300);
      beep_speaker(400,300);
      indicate_state(0);
      updateEEPROM();
      break;
    }
    if (button2Pushed())
    {
      beep_speaker(600,300);
      settings.temp_parm = ((uint8_t)settings.temp_parm)+1;
      if (settings.temp_parm >= (sizeof(temp_states)/sizeof(uint8_t)))
        settings.temp_parm = 1;
    }
    if (button3Pushed() && setHysteresis()) break;
  }
}

void setDuration(void)
{
  uint8_t st = 0;
  if (!checkButtonState(curStateButton2,50)) return;
  setRelay(false);  // turn off heater
  for (uint8_t i=0;i<5;i++)
  {
    digitalWrite(PINLED1,LOW);
    digitalWrite(PINLED2,HIGH);
    digitalWrite(PINLED3,HIGH);
    digitalWrite(PINLED4,LOW);
    beep_speaker(400,600);
    digitalWrite(PINLED1,HIGH);
    digitalWrite(PINLED2,LOW);
    digitalWrite(PINLED3,LOW);
    digitalWrite(PINLED4,HIGH);
    beep_speaker(500,600);
  }
  digitalWrite(PINLED3,HIGH);
  clearButtonPushed();
  for (;;)
  {
    st ^= 1;
    indicate_state(st ? settings.tim_parm : 0);
    delay(300);
    if (button1Pushed()) 
    {
      beep_speaker(400,300);
      delay(300);
      beep_speaker(400,300);
      indicate_state(0);
      updateEEPROM();
      break;
    }
    if (button2Pushed())
    {
      beep_speaker(500,300);
      settings.tim_parm = ((uint8_t)settings.tim_parm)+1;
      if (settings.tim_parm >= (sizeof(time_states)/sizeof(uint8_t)))
        settings.tim_parm = 1;
    }
  }
}

#define NUMBER_OF_AVERAGES 24
#define AD_MAX_VOLTAGE 50000lu
#define AD_MAX_VALUE 1024lu
#define AD_CELSIUS_PER_VOLT 1lu

// temperature is returned in units of 0.01 deg C, so 
// 20 deg C = 2000 
uint16_t read_temperature_celsius(void)
{
  uint32_t sample = 0;
  for (uint8_t i=0;i<NUMBER_OF_AVERAGES;i++)
      sample += analogRead(ANALOGLM35);
  sample = (sample*(AD_MAX_VOLTAGE*AD_CELSIUS_PER_VOLT))/(AD_MAX_VALUE*NUMBER_OF_AVERAGES);
  return (uint16_t) sample;
}

bool temperature_probe_working(void)
{
  uint16_t temp = read_temperature_celsius();

  // if the temperature is less than 2C or greater than 200C
  // then the temperature probe is not working
  return ((temp >= 200) && (temp < 20000));
}

void indicate_error(uint8_t code, uint16_t freq)
{
  clearButtonPushed();
  while ((!button1Pushed()) && (!button2Pushed()) && (!button3Pushed()))
  {
    indicate_state(code);
    beep_speaker(freq,500);
    indicate_state(0);
    delay(500);
  }
  indicate_state(0);
  clearButtonPushed();
}

void indicate_response(uint8_t code, uint16_t freq)
{
  clearButtonPushed();
  for (uint8_t i=0;i<3;i++)
  {
    indicate_state(code);
    beep_speaker(freq,500);
    indicate_state(0);
    delay(500);
  }
  indicate_state(0);
  clearButtonPushed();
}

bool temperature_probe_error(void)
{
  if (temperature_probe_working()) return false;
  setRelay(false);  // turn off heater
  indicate_error(5,200);
  return true;
}

void doHeating(void)
{
  if (!checkButtonState(curStateButton3,5)) return;
  if (temperature_probe_error()) return;

  uint32_t total_heating_time = ((uint32_t)time_states[settings.tim_parm])*(60lu*1000lu);
      // total heating time milliseconds
  uint16_t temperature_target = ((uint16_t)temp_states[settings.temp_parm])*100u;
  uint16_t temperature_low = ((uint16_t)temp_states[settings.temp_parm])*100u-(100u*hysteresis_states[settings.hysteresis_parm]);
      // do not turn heater off until the temperature is 1 C below target
  uint16_t temperature_high = ((uint16_t)temp_states[settings.temp_parm])*100u+(100u*hysteresis_states[settings.hysteresis_parm]);
      // do not stop heater until the temperature is 1 C above target
  uint16_t temperature_low_error = ((uint16_t)temp_states[settings.temp_parm])*100u-TEMP_FAULT;
      // if temp is less than 10 C below target after heating, there's a problem!
  uint16_t temperature_high_error = ((uint16_t)temp_states[settings.temp_parm])*100u+TEMP_FAULT;
      // if temp is greater than 10 C above target, there's a problem!
  Serial.print("time dur=");
  Serial.print(total_heating_time);
  Serial.print(" temp dur=");
  Serial.println(temperature_target);

  indicate_state(5);
  beep_speaker(400,1000);
  indicate_state(0);
  clearButtonPushed();

  // begin heating cycle
  setRelay(true);          // turn on heater
  uint8_t st = 0;
  for (;;)
  {
    uint16_t current_temp;
    if (temperature_probe_error()) return;
    current_temp = read_temperature_celsius();
    Serial.print("current temp = ");
    Serial.println(current_temp);
    if (current_temp > temperature_high_error)
    {
        setRelay(false);  // turn off heater
        indicate_error(5,200);
        return;
    }
    if (current_temp > temperature_target) break;
    st ^= 1;
    digitalWrite(PINLED1, st ? LOW : HIGH);
    digitalWrite(PINLED2, st ? HIGH : LOW);
    digitalWrite(PINLED3, st ? LOW : HIGH);
    digitalWrite(PINLED4, st ? HIGH : LOW);
    delay(300);
    if (button1Pushed())
    {
      if (checkButtonState(curStateButton1,10))
      {
        setRelay(false); 
        indicate_response(7,300);
        return;
      }
    }
  }
  // reached target temp, now start counting time
  uint32_t start_time = millis();
  for (;;)
  {
    uint16_t current_temp;
    uint32_t elapsed_time = (uint32_t)(millis() - start_time);
    if (temperature_probe_error()) return;

    if (elapsed_time >= total_heating_time) break; // we're done!
    if (button1Pushed())
    {
      if (checkButtonState(curStateButton1,10))
      {
        setRelay(false); 
        indicate_response(7,300);
        return;
      }
    }    
    current_temp = read_temperature_celsius();
    if ((current_temp < temperature_low_error) || (current_temp > temperature_high_error))
    {
        setRelay(false);  // turn off heater
        indicate_error(5,200);
        return;
    }
    if (current_temp > temperature_high) setRelay(false);
    if (current_temp < temperature_low) setRelay(true);

    uint8_t frac = (elapsed_time*4u) / total_heating_time;
    Serial.print("current temp2 = ");
    Serial.print(current_temp);
    Serial.print(" elapsed_time = ");
    Serial.print(elapsed_time);
    Serial.print(" total_heating_time = ");
    Serial.print(total_heating_time);
    Serial.print(" frac = ");
    Serial.println(frac);
    st ^= 1;
    int flashState = relayState ? (st ? HIGH : LOW) : LOW;
    if (frac == 0)
    {
      digitalWrite(PINLED1, flashState);
      digitalWrite(PINLED2, HIGH);
      digitalWrite(PINLED3, HIGH);
      digitalWrite(PINLED4, HIGH);
      
    } else if (frac == 1)
    {
      digitalWrite(PINLED1, LOW);
      digitalWrite(PINLED2, flashState);
      digitalWrite(PINLED3, HIGH);
      digitalWrite(PINLED4, HIGH);
    } else if (frac == 2)
    {
      digitalWrite(PINLED1, LOW);
      digitalWrite(PINLED2, HIGH);
      digitalWrite(PINLED3, flashState);
      digitalWrite(PINLED4, HIGH);
    } else if (frac == 3)
    {
      digitalWrite(PINLED1, LOW);
      digitalWrite(PINLED2, HIGH);
      digitalWrite(PINLED3, HIGH);
      digitalWrite(PINLED4, flashState);
    }
    delay(300);
  }
  setRelay(false); 
  uint8_t i = 0;
  clearButtonPushed();
  for (;;)
  {
    indicate_state(0);
    delay(1000);
    if ((button1Pushed()) || (button2Pushed()) || (button3Pushed())) break;
    indicate_state(7);
    if (i < 10)
    {
        i++;
        beep_speaker(400,1000);
    } else delay(1000);
    if ((button1Pushed()) || (button2Pushed()) || (button3Pushed())) break;
  }    
  clearButtonPushed();
  indicate_state(0);
}

void loop() {
  static uint8_t st = 0;
  st ^= 0x01;
  setRelay(false); 
  digitalWrite(PINLED1,st ? LOW : HIGH);
  digitalWrite(PINLED2,st ? LOW : HIGH);
  digitalWrite(PINLED3,st ? HIGH : LOW);
  digitalWrite(PINLED4,st ? HIGH : LOW);
  if (button1Pushed()) setTemp();
  if (button2Pushed()) setDuration();
  if (button3Pushed()) doHeating();
  delay(300);
}
