
/* Potential Improvements (For my sanity)
 *  Write a proper library to control the DFPlayer that uses the access mode I like
 *  Separate headers from implementation
 *  Accept that we are relying on Arduino lib for a project just for fun
 *  We aren't using the connection from command module to nano at all
 *  
 *  I really want to break this into a proper project layout, however, Arduino
 *  support in eclipse has been pulled, and the new C++ dev env isn't out yet!
 */

//Because we are using hardware serial for computer interface
#include <SoftwareSerial.h>

//For saving/loading settings
#include <EEPROM.h> 

//Defines
#define BUTTON 2
#define SOFT_TX 6
#define SOFT_RX 7
#define BUSY 3
#define MINUTE_IN_MILLIS 60000
#define VOL_MAX 30

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
#define TWENTY 20
#define THIRTY 21
#define FORTY 22
#define FIFTY 23
#define SYS_FOLDER 1
#define ANNOUNCE_FOLDER 2
#define MAX_ANNOUNCE 10
#define MODE_HEADER 24
#define VOLUME_HEADER 38
#define REMAINING_HEADER 34
#define FIXED_INTERVAL 25
#define RANDOM_AVERAGE 36
#define MAYBE_CONFIG 39
#define PROMPT 35
#define SETTINGS_SAVED 37

//Command bytes
#define PLAY 0x0F
#define DETECT 0x3F
#define STOP 0x16
#define VOLUME 0x06

enum Mode
{
  MODE_FIXED = 0,
  MODE_RANDAVG = 1,
};

// Next announcement
byte hours = 0;
byte minutes = 0;
byte seconds = 0;

unsigned long lastTimerUpdate = 0;

// Config
byte intervalHours = 2;
byte intervalMinutes = 0;
byte volume = 15;
byte mode = MODE_FIXED;

SoftwareSerial ss(SOFT_RX, SOFT_TX);

// Taken from: https://markus-wobisch.blogspot.com/2016/09/arduino-sounds-dfplayer.html
void sendCommand(byte CMD, byte Par1, byte Par2, byte reply=0)  
{   
  # define Start_Byte   0x7E  
  # define Version_Byte  0xFF  
  # define Command_Length 0x06  
  # define End_Byte    0xEF  
  // Calculate the checksum (2 bytes)  
  uint16_t checksum = -(Version_Byte + Command_Length + CMD + reply + Par1 + Par2);  
  // Build the command line  
  uint8_t Command_line[10] = { Start_Byte, Version_Byte, Command_Length, CMD, reply,  
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
  // Initial sync of volume to device
  sendCommand(VOLUME, 0, volume);
  
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
    volume = 15;
  }
  sendCommand(VOLUME, 0, volume);

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

byte debouncedDigitalRead(byte pin)
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

void waitUntilIdle()
{
  delay(500);
  while (!digitalRead(BUSY));
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
    long newMinutes = random(5, intervalMinutes*2);

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
    play(SYS_FOLDER, number);
  }
  else
  {
    byte tens = (byte)(number/10);
    byte ones = number % 10;

    if (tens == 2)
    {
      play(SYS_FOLDER, TWENTY);
    }
    else if (tens == 3)
    {
      play(SYS_FOLDER, THIRTY);
    }
    else if (tens == 4)
    {
      play(SYS_FOLDER, FORTY);
    }
    else if (tens == 5)
    {
      play(SYS_FOLDER, FIFTY);
    }

    if (ones)
    {
      tellNumber(ones);
    }
    
  }
}

void tellTime(byte hour, byte minute)
{
  if (hour)
  {
    tellNumber(hour);
    if (hour == 1)
    {
      play(SYS_FOLDER, HOUR);
    }
    else
    {
      play(SYS_FOLDER, HOURS);
    }
  }

  if (minute)
  {
    tellNumber(minute);
    if (minute == 1)
    {
      play(SYS_FOLDER, MINUTE);
    }
    else
    {
      play(SYS_FOLDER, MINUTES);
    }
  }
}

void doStatusInteraction()
{
  // Mode and interval
  userSetMode(mode);
  tellTime(intervalHours, intervalMinutes);

  // Volume
  userSetVolume(volume);

  // Remaining
  play(SYS_FOLDER, REMAINING_HEADER);

  tellTime(hours, minutes);

  doConfigInteraction();
  
}

bool promptRead(byte button=BUTTON, long promptTime=3000)
{
  play(SYS_FOLDER, PROMPT);
 
  unsigned long startTime = millis();
  while (millis() - startTime < 5000)
  {
    if (!debouncedDigitalRead(BUTTON))
    {
      return true;
    }
  }

  return false;
}

void userSetMode(Mode mode_in)
{
  mode = mode_in;
  play(SYS_FOLDER, MODE_HEADER);
  if (mode == MODE_FIXED)
  {
    play(SYS_FOLDER, FIXED_INTERVAL);
  }
  else if (mode = MODE_RANDAVG)
  {
    play(SYS_FOLDER, RANDOM_AVERAGE);
  }
}

void userSetVolume(byte v)
{
  
  volume = v;
  sendCommand(VOLUME, 0, volume);
  play(SYS_FOLDER, VOLUME_HEADER);

  tellNumber(volume);
}

void doConfigInteraction()
{
  // Maybe do config?
  play(SYS_FOLDER, MAYBE_CONFIG);

  if (!promptRead())
  {
    return;
  }

  // Mode
  while (true)
  {
    // Intro
    userSetMode(mode);

    if (promptRead())
    {
      break; // Accept
    }

    // Toggle
    if (mode == MODE_FIXED)
    {
      mode = MODE_RANDAVG;
    }
    else
    {
      mode = MODE_FIXED;
    }
  }

  // Target Announce Period
  while (true)
  {
    // Intro
    tellTime(intervalHours, intervalMinutes);

    if (promptRead())
    {
      break; // Accept
    }

    // Toggle
    intervalMinutes += 30;
    intervalHours += intervalMinutes / 60;
    intervalMinutes %= 60;

    if (intervalHours == 5)
    {
      intervalHours = 0;
      intervalMinutes = 30;
    }
  }

  // Volume
  while (true)
  {
    // Intro
    userSetVolume(volume);

    if (promptRead())
    {
      break; // Accept
    }

    // Toggle
    if (volume < 30)
    {
      volume += 3;
    }
    else
    {
      volume = 1;
    }
  }

  saveSettings();
  play(SYS_FOLDER, SETTINGS_SAVED);
  
}

void play(byte folder, byte file)
{
  sendCommand(STOP, 0, 0);
  sendCommand(PLAY, folder, file);
  waitUntilIdle();
  sendCommand(STOP, 0, 0);

}

void setup() {
  // Pin states
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(BUSY, INPUT_PULLUP);

  // Initialize the random number generator
  randomSeed(analogRead(0));

  // Open communication
  ss.begin(9600);

  // Init player
  delay(1000);
  sendCommand(DETECT, 0, 0);

  // Load settings from EEPROM
  loadSettings();

  // Pick next announce time
  scheduleNextAnnounce();

  // Play introduction
  sendCommand(STOP, 0, 0);
  play(SYS_FOLDER, INTRO);
  
}

void loop() {
  
  // Handle announcement
  if (!hours && !minutes)
  {
    byte pickedFile = (byte)random(1, MAX_ANNOUNCE);

    play(ANNOUNCE_FOLDER, pickedFile);

    scheduleNextAnnounce();
  }
  
  // Handle button press
  if (!debouncedDigitalRead(BUTTON))
  {
    doStatusInteraction();
  }

  // Update timer
  unsigned long curTime = millis();
  unsigned long elapsedTime = curTime - lastTimerUpdate;
  
  while (elapsedTime > MINUTE_IN_MILLIS)
  {
    elapsedTime -= MINUTE_IN_MILLIS;
    lastTimerUpdate += MINUTE_IN_MILLIS;

    // Decrement the minute
    if (minutes > 0)
    {
      minutes--;
    }
    else if (hours > 0)
    {
      hours--;
      minutes = 59;
    }
    else
    {
      // Minutes and hours are both zero?
      // Just claim the rest
      ;
    }
  }

}
