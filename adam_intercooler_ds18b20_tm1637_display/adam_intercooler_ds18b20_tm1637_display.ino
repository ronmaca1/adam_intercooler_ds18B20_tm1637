#include <SerialCommand.h>
#include <EEPROM.h>
#include <stdio.h>
#include <SPI.h>
#include <TM1637Display.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Module connection pins (Digital Pins)
#define CLK 3
#define DIO 2

#define erased 0
#define eepromparamsize 2 // EEPROM storage allocation units in bytes == sizeof(int)
#define sigaddr 1         // first active eeprom addr. skipped addr 0.
#define setcalAddr (sigaddr + eepromparamsize)
#define setr1Addr (setcalAddr + eepromparamsize)
#define setcelorfahrAddr (setr1Addr + eepromparamsize)
#define setbrightAddr (setcelorfahrAddr + eepromparamsize)
// (use for MCP4901 single channel DAC)
// VREF gain 1(EXT. Vref)
#define MCP4901ACONFIGBITS 0x30

typedef unsigned int storedparamtype;
// <settable parameters>

storedparamtype brightval;
storedparamtype fahrenheit;

// </end of settable parameters>
byte dacoutL = 0;
byte dacoutH = 0;
byte calval = 0;
const byte ONE_WIRE_BUS = A4;
const byte dacsel = 10;
const byte ldac = 9;
TM1637Display display(CLK, DIO);

SerialCommand sCmd;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
void caldacset(void);

void get_stored_params(void)
{
  EEPROM.get(setcelorfahrAddr, fahrenheit);
  EEPROM.get(setbrightAddr, brightval);
}

void setup()
{
  digitalWrite(ldac, HIGH);
  pinMode(ldac, OUTPUT);
  digitalWrite(dacsel, HIGH);
  pinMode(dacsel, OUTPUT);
  calval = 255;
  caldacset();
  Serial.begin(9600);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV8);
  SPI.begin();
  // gets parame
  sensors.begin();
  sensors.setResolution(0x3F); // 10 bit resolution, < 300mSec to convert

  get_stored_params();

  display.clear();
  EEPROM.get(setbrightAddr, brightval);
  display.setBrightness(brightval);

  // Setup callbacks for SerialCommand commands
  sCmd.addCommand("INIT", init_eeprom); //1
  sCmd.addCommand("SETF", set_fahrenheit);
  sCmd.addCommand("SETC", set_celsius);
  sCmd.addCommand("SETBRIGHT", set_bright);
  sCmd.addCommand("HELP", show_help);
  sCmd.addCommand("PSET", print_current_settings);
  sCmd.setDefaultHandler(unrecognized);
} ///////////////////////////// end of setup //////////////////////////////////

void loop()
{
  // <check for incoming command>
  sCmd.readSerial();

  // Celsius
  if (fahrenheit == false) // output degrees in Celsius
  {
    sensors.requestTemperatures(); // Send the command to get temperatures

    display.showNumberDecEx(((sensors.getTempCByIndex(0)) * 10), 0b00100000, false);
    // num to display, D.P. position, disp. leading zeros
  }

  // Fahrenheit
  if (fahrenheit == true) // output degrees in Fahrenheit
  {
    sensors.requestTemperatures();

    display.showNumberDecEx((((sensors.getTempFByIndex(0)) - 2) * 10), 0b00100000, false);
  }
}
//////////////////////////////////// end of loop ///////////////////////////////////

// <helper functions for settings>

storedparamtype eepromreturn(unsigned int addr)
{
  storedparamtype eepromdata;
  EEPROM.get(addr, eepromdata);
  return eepromdata;
}

void erase_eeprom(void)
{
  for (int i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.put(i, erased);
  }
}

void set_defaults(void)
{
  EEPROM.put(sigaddr, 0xAA55);
  EEPROM.put(setcelorfahrAddr, true); // display fahrenheit by default
  EEPROM.put(setbrightAddr, 3);       // set display at half brightness
}

void init_eeprom()
{
  String arg;
  arg = sCmd.next();

  if (arg != NULL)
  {
    if (arg == "+++")
    {
      erase_eeprom();
      set_defaults();
    }
    else
    {
      Serial.println("");
      Serial.println(F("OOPSIE. wrong password"));
      Serial.println("");
    }
  }
  else
  {
    Serial.println("");
    Serial.println(F("No secret code given, nothing has been done. Bye :-)"));
    Serial.println("");
    return;
  }
}

void set_fahrenheit(void)
{
  EEPROM.put(setcelorfahrAddr, (storedparamtype)(true));
  EEPROM.get(setcelorfahrAddr, fahrenheit);
}
void set_celsius(void)
{
  EEPROM.put(setcelorfahrAddr, (storedparamtype)(false));
  EEPROM.get(setcelorfahrAddr, fahrenheit);
}

void set_bright(void)
{
  char *arg;

  arg = sCmd.next();
  if (arg != NULL)
  {
    brightval = atoi(arg);
    if (brightval >= 0 && brightval <= 7)
    {
      EEPROM.put(setbrightAddr, (storedparamtype)(brightval));
      EEPROM.get(setbrightAddr, brightval);
      display.setBrightness(brightval);
    }
    else
    {
      Serial.println(F(" setbright cannot be less than 0 or "));
      Serial.println(F(" more than 7. Nothing was changed"));
    }
  }
}

void show_parameters(void)
{
  Serial.println();
  Serial.println("debugging info");
  Serial.println(fahrenheit);
  Serial.println(brightval);
  Serial.println("end of debugging info");
  Serial.println();
}

void show_help()
{
  Serial.println("");
  Serial.println(F("\"INIT +++\" sets the stored settings to default"));
  Serial.println("");

  Serial.println(F("\"PSET\" Shows current settings"));
  Serial.println("");

  Serial.println(F("\"SETF\" sets the temperature"));
  Serial.println(F(" display to Fahrenheit "));
  Serial.println("");

  Serial.println(F("\"SETC\" sets the temperature"));
  Serial.println(F(" display to Celsius "));
  Serial.println("");

  Serial.println(F("\"SETBRIGHT\" sets the display "));
  Serial.println(F(" brightnes in steps from  0 to 7 "));
  Serial.println(F("values lower or higher will be ignored"));
  Serial.println("");
}
void print_current_settings()
{
  Serial.println("");

  Serial.print(F("EEPROM signature is: "));
  Serial.println((eepromreturn(sigaddr)), HEX);
  Serial.println("");

  Serial.print(F("setbright is: "));
  Serial.println((eepromreturn(setbrightAddr)), DEC);
  Serial.println("");

  Serial.print(F("setfahr diplay is: "));
  if (eepromreturn(setcelorfahrAddr) == false)
  {
    Serial.println(F(" in Centigrade"));
  }
  else if (eepromreturn(setcelorfahrAddr) == true)
  {
    Serial.println(F(" in Fahrenheit"));
  }

  else
  {
    Serial.println(F(" oops."));
    Serial.println("");
  }
}

void unrecognized(const char *command)
{
  Serial.println("");
  Serial.println(F("Unknown command"));
  Serial.println("");
}

void caldacset(void)
{
  dacoutH = MCP4901ACONFIGBITS | ((calval >> 4) & 0x0F);
  dacoutL = (calval << 4) & 0xF0;
  digitalWrite(dacsel, LOW);
  delayMicroseconds(10); // let the DAC get ready
  SPI.transfer(dacoutH);
  SPI.transfer(dacoutL);
  delayMicroseconds(10); // let the DAC settle
  digitalWrite(dacsel, HIGH);
  digitalWrite(ldac, LOW);
  delayMicroseconds(10);
  digitalWrite(ldac, HIGH);
}