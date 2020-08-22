

// Library settings
#define _TASK_SLEEP_ON_IDLE_RUN

// Includes
#include <avr/wdt.h>
#include <SoftwareSerial.h>
#include <EEPROM.h> 
#include <TaskScheduler.h>

//Defines
#define BUTTON 2
#define SOFT_TX 6
#define SOFT_RX 7
#define BUSY 3
#define MINUTE_IN_MICROS 60000000

//EEPROM
#define VOL_ADDR 0
#define MODE_ADDR 1
#define INTERVAL_HOURS_ADDR 2
#define INTERVAL_MINUTES_ADDR 3
#define CHECKSUM_ADDR 4

//File/folder constants
#define INTRO 29
#define HOUR 27
#define HOURS 28
#define MINUTE 32
#define MINUTES 33
#define THIRTY 21
#define FORTY 22
#define FIFTY 23
#define SYS_FOLDER 1
#define ANNOUNCE_FOLDER 2
#define MAX_ANNOUNCE 0
#define MODE_HEADER 24
#define VOLUME_HEADER 38
#define REMAINING_HEADER 34
#define FIXED_INTERVAL 25
#define RANDOM_AVERAGE 36

//Command bytes
#define PLAY 0x06
#define DETECT 0x3F
#define STOP 0x16

// Enums
enum Volume
{
  VOL_LOW = 5,
  VOL_MED = 10,
  VOL_HIGH = 15,
  VOL_MAX = 30,
};

enum Mode
{
  MODE_FIXED = 0,
  MODE_RANDAVG = 1,
};

// Global variables
byte hours = 0;
byte minutes = 0;
byte intervalHours = 2;
byte intervalMinutes = 0;
byte volume = VOL_LOW;
byte mode = MODE_FIXED;

// Forward declarations
void wdtkick();

// Scheduler system
Scheduler ts;
Task wdt(1000, TASK_FOREVER, wdtkick, &ts);

// Software serial
SoftwareSerial ss(SOFT_RX, SOFT_TX);

// Task callbacks
void wdtkick()
{
  wdt_reset();
}

// General functions
// Taken from: https://markus-wobisch.blogspot.com/2016/09/arduino-sounds-dfplayer.html
void sendCommand(byte CMD, byte Par1, byte Par2)  
{   
  # define Start_Byte   0x7E  
  # define Version_Byte  0xFF  
  # define Command_Length 0x06  
  # define Acknowledge  0x00   
  # define End_Byte    0xEF  
  // Calculate the checksum (2 bytes)  
  uint16_t checksum = -(Version_Byte + Command_Length + CMD + Acknowledge + Par1 + Par2);  
  // Build the command line  
  uint8_t Command_line[10] = { Start_Byte, Version_Byte, Command_Length, CMD, Acknowledge,  
         Par1, Par2, highByte(checksum), lowByte(checksum), End_Byte};  
  // Send the command line to DFPlayer  
  for (byte i=0; i<10; i++) ss.write( Command_line[i]);

  // Default processing time
  delay(30);
}

byte getEepromChecksum()
{
  byte checksum = 0;
  for (size_t i=0; i<CHECKSUM_ADDR; i++)
  {
    checksum += EEPROM.read(i);
  }
  return checksum;
}

void loadSettings()
{
  // Verify EEPROM checksum first
  byte checksum = getEepromChecksum();
  if (checksum != EEPROM.read(CHECKSUM_ADDR))
  {
    return;
  }

  // Volume
  volume = EEPROM.read(VOL_ADDR);
  if (volume > VOL_MAX || volume == 0)
  {
    volume = VOL_LOW;
  }

  // Mode
  mode = EEPROM.read(MODE_ADDR);
  if (mode > 1)
  {
    mode = MODE_FIXED;
  }

  // Interval
  intervalHours = EEPROM.read(INTERVAL_HOURS_ADDR);
  intervalMinutes = EEPROM.read(INTERVAL_MINUTES_ADDR);
  
}

void saveSettings()
{
  EEPROM.write(VOL_ADDR, volume);
  EEPROM.write(MODE_ADDR, mode);
  EEPROM.write(INTERVAL_HOURS_ADDR, intervalHours);
  EEPROM.write(INTERVAL_MINUTES_ADDR, intervalMinutes);
  
  byte checksum = getEepromChecksum();
  EEPROM.write(CHECKSUM_ADDR, checksum);
  
}

byte debouncedDigitalRead(Task* task, byte pin)
{
  byte v0 = 0;
  byte v1 = 1;

  while (v0 != v1)
  {
    v0 = digitalRead(pin);
    delay(10);
    v1 = digitalRead(pin);
  }

  return v0;
 
}

void scheduleNextAnnounce()
{
  if (mode == MODE_FIXED)
  {
    hours = intervalHours;
    minutes = intervalMinutes;
  }
  else if (mode == MODE_RANDAVG)
  {
    // Wait the interval time on average, between 0-2X the interval
    long newHours = random(0, intervalHours*2);
    long newMinutes = random(1, intervalMinutes*2);

    if (newMinutes > 59)
    {
      newHours += (long)(minutes / 60);
      newMinutes = minutes % 60;
    }

    hours = (byte)newHours;
    minutes = (byte)newMinutes;
  }
}

void tellNumber(byte number)
{
  if (number <= 20)
  {
    sendCommand(PLAY, SYS_FOLDER, number);
    waitUntilIdle();
  }
  else
  {
    byte tens = (byte)(number/10);
    byte ones = number % 10;

    if (tens == 3)
    {
      sendCommand(PLAY, SYS_FOLDER, THIRTY);
    }
    else if (tens == 4)
    {
      sendCommand(PLAY, SYS_FOLDER, FORTY);
    }
    else if (tens == 5)
    {
      sendCommand(PLAY, SYS_FOLDER, FIFTY);
    }

    delay(1200);

    if (ones)
    {
      tellNumber(ones);
    }
    
  }
}

void setup() {
  // Enable the WDT
  wdt_enable(WDTO_8S);
  
  // Initial task states
  wdt.enable();


}

void loop() {
  ts.execute();

}
