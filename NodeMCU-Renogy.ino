#include <OneWire.h>
#include <DallasTemperature.h>
#include <ModbusMaster.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>                https://github.com/tzapu/WiFiManager  says by: tablatronix in the library manager
#include <time.h>

//Function Prototypes
void clearArray(uint16_t (&myArray)[6]);
float getTemperature(DeviceAddress);
void handleRoot();             
void handleNotFound();
#define ONE_WIRE_BUS 12

//Globals
time_t tnow;
char dateTime[18];
String web_content = "";
ModbusMaster node;
SoftwareSerial S(5, 4);                     //GPIO 5 or D1 on NodeMCU is set here for RX, GPIO 4 or D2 is TX
OneWire oneWire(ONE_WIRE_BUS);              //onewire for DS18b20 temp sensor
DallasTemperature temp_sensor(&oneWire);    //temp sensor unit for panel temp
DeviceAddress insideThermometer = { 0x28, 0xD8, 0x11, 0xC3, 0x52, 0x20, 0x01, 0xD0 };  //hard coded, will need to change for your ds18b20 module
unsigned long delayTime;
ESP8266WebServer server(8585);              //http server port 8585


void setup()
{
  Serial.begin(9600);                       //Hardware serial for debug messaging.
  S.begin(9600, SWSERIAL_8N1);              //SoftwareSerial (port S declared above) this will be used for communication with the renogy charge controller (or slave)
  node.begin(1, S);                         // communicate with Modbus slave ID 1 over softwareSerial (port S declared above)
  temp_sensor.begin();
  temp_sensor.setResolution(insideThermometer, 9);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  wifiManager.resetSettings();

  //set custom ip for portal
  wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("Solar-1");
  //or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();
  //if you get here you have connected to the WiFi
  Serial.println("connected... :)");
  server.begin();                           //http server
  server.on("/", handleRoot);               // Call the 'handleRoot' function when a client requests URI "/"
  server.onNotFound(handleNotFound);        // When a client requests an unknown URI (i.e. something other than

  configTime(0, 0, "0.north-america.pool.ntp.org", "1.north-america.pool.ntp.org", "2.north-america.pool.ntp.org");
  Serial.println("\nWaiting for time");
  while (time(nullptr)<= 100000)
  {
    Serial.print(".");
    delay(100);
  }
    
  //Serial.println("");
  tnow = time(nullptr);
  tm *ptm = localtime(&tnow);
  strftime(dateTime, 18, "%Y-%m-%d,%H:%M,", ptm);
  //Serial.println(dateTime);
}


void loop()
{
  
  //**************************************
  // Update Time
  //**************************************
  tnow = time(nullptr);
  tm *ptm = localtime(&tnow);
  strftime(dateTime, 18, "%Y-%m-%d,%H:%M,", ptm);
  Serial.println(dateTime); 
  server.handleClient();
  delay(5000);     //update every 5 seconds
  
  temp_sensor.requestTemperatures();
  uint16_t uptime = 0;
  static uint32_t i;
  uint8_t j, result;
  uint16_t data[6];
  uint8_t charge_percent = 0;
  float battery_voltage = 0.0;
  float controller_temp = 0.0;
  float panel_voltage = 0.0;
  float panel_current = 0.0;
  float min_voltage_of_day = 0.0;
  uint16_t charge_power = 0;
  uint16_t charge_power_of_day = 0;
  uint8_t charging_state = 0;
  uint8_t bat_type = 0;
  uint8_t load_watts = 0;
  char bat_type_str[10];
  char charge_state_str[20];
  float tempF = ((getTemperature(insideThermometer)*1.8)+34.7);  //34.7 instead of 32.  Adjusted, sensor slightly off by 2.7 degrees when tested.  Just don't ask me how I tested it.
  web_content = "";   //clear this out on each pass otherwise it just fills up

  StaticJsonDocument<500> doc;                //500 bytes to store JSON serialized data
  doc["panel_temp"] = tempF;
  clearArray(data);                           //this array holds the bytes that come back from the slave device (renogy charge controller).  It gets reused on each read.
                                              //as the loop rolls back around, clear it out to make sure you start with a zeroed out array.
  doc["date_time"] = dateTime;
  doc["station"] = "solar-1";
  
  //Start reading the holding registers as outlined in renogy "rover modbus.docx" document
  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read uptime
  result = node.readHoldingRegisters(0x115, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
      //""ln(data[j], HEX);
    }
    uptime = data[0];  
    doc["uptime"] = uptime;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////
   
  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read SOC  (state of charge)
  result = node.readHoldingRegisters(0x100, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
      charge_percent = data[0];
    }
    doc["soc"] = charge_percent;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Battery Voltage
  result = node.readHoldingRegisters(0x101, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
      battery_voltage = float(data[0]) / 10;
    }
    doc["battery_voltage"] = battery_voltage;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Controller Temperature
    result = node.readHoldingRegisters(0x103, 1);

    // do something with data if read is successful
    if (result == node.ku8MBSuccess)
    {
      for (j = 0; j < 1; j++)
      {
        data[j] = node.getResponseBuffer(j);
        byte controller_temp1 = data[0] >> 8;
        float temp_value = controller_temp1 & 0x0ff;
        int sign = controller_temp1 >> 7;
        if(sign == 1)
        {
          controller_temp = -(temp_value -128);
        }
        else
        {
          controller_temp = temp_value;
        }    
        int toF = (controller_temp*9/5)+32;
        doc["controller_temp"] = toF;
      }
    }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Panel Voltage
  result = node.readHoldingRegisters(0x107, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
      panel_voltage = data[0] * 0.1;
    }
    doc["panel_voltage"] = panel_voltage;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Load Voltage
  result = node.readHoldingRegisters(0x104, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
      //""(data[j], HEX);
      battery_voltage = data[0] * 0.1;  //reuse
    }
    doc["load_voltage"] = battery_voltage;
    clearArray(data);     //reset
    battery_voltage = 0;  //reset
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Load Current
  result = node.readHoldingRegisters(0x105, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
      //""(data[j], HEX);
      battery_voltage = data[0] * 0.01;  //reuse
    }
    doc["load_current"] = battery_voltage;
    clearArray(data);
    battery_voltage = 0;  //reset
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Load Power
  result = node.readHoldingRegisters(0x106, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
      //""(data[j], HEX);
      load_watts = data[0];
    }
    doc["load_power"] = load_watts;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Panel Current
  result = node.readHoldingRegisters(0x108, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }
    panel_current = data[0] * 0.01;
    doc["panel_current"] = panel_current;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Charging Power
  result = node.readHoldingRegisters(0x109, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
      //""ln(data[j], HEX);
    }
    charge_power = data[0];
    doc["charge_power"] = charge_power;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Minimum Voltage of the Day
  result = node.readHoldingRegisters(0x10B, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }
    min_voltage_of_day = data[0] * 0.1;
    doc["min_voltage_of_day"] = min_voltage_of_day;
    clearArray(data);
    min_voltage_of_day = 0; //reset
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Maximum Voltage of the Day
  result = node.readHoldingRegisters(0x10C, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);

    }
    min_voltage_of_day = data[0] * 0.1;  //reuse save memory probably going to regret
    doc["max_voltage_of_day"] = min_voltage_of_day;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Maximum Charge Current of the Day
  result = node.readHoldingRegisters(0x10D, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }
    min_voltage_of_day = data[0] * 0.01;  //reuse save memory probably going to regret
    doc["max_charge_current_of_day"] = min_voltage_of_day;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Maximum Discharge Current of the Day
  result = node.readHoldingRegisters(0x10E, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }
    min_voltage_of_day = data[0] * 0.01;  //reuse save memory probably going to regret
    doc["max_discharge_current_of_day"] = min_voltage_of_day;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Maximum Charge Power of the Day
  result = node.readHoldingRegisters(0x10F, 1);


  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }
    charge_power_of_day = data[0];  //reuse save memory probably going to regret
    doc["max_charge_power_of_day"] = charge_power_of_day;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read Maximum Discharging Power of the Day
  result = node.readHoldingRegisters(0x110, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }
    charge_power_of_day = data[0];  //reuse save memory probably going to regret
    doc["max_discharge_power_of_day"] = charge_power_of_day;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read charging amp/hours of the Day
  result = node.readHoldingRegisters(0x111, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }
    charge_power_of_day = data[0];  //reuse save memory probably going to regret
    doc["charge_amp-hrs_of_day"] = charge_power_of_day;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read discharging amp/hours of the Day
  result = node.readHoldingRegisters(0x112, 1);


  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }
    charge_power_of_day = data[0];  //reuse save memory probably going to regret
    doc["discharge_amp-hrs_of_day"] = charge_power_of_day;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////
  
  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read power generated current day
  result = node.readHoldingRegisters(0x113, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
      //""ln(data[j], HEX);
    }
    charge_power_of_day = data[0];  //reuse save memory probably going to regret
    doc["power_generated_current_day"] = charge_power_of_day;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read power consumed current day
  result = node.readHoldingRegisters(0x114, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
      //""ln(data[j], HEX);
    }
    charge_power_of_day = data[0];  //reuse save memory probably going to regret
    doc["power_consumed_current_day"] = charge_power_of_day;
    clearArray(data);
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read battery type
  result = node.readHoldingRegisters(0xE004, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }

    switch (data[0])
    {
      case 1:
        strcpy(bat_type_str, "Open");
        break;

      case 2:
        strcpy(bat_type_str, "sealed");
        break;

      case 3:
        strcpy(bat_type_str, "gel");
        break;

      case 4:
        strcpy(bat_type_str, "lithium");
        break;

      case 5:
        strcpy(bat_type_str, "self-customized");
        break;

      default:
        Serial.println("Error in Battery Type subroutine");
        break;
    }
    doc["battery_type"] = bat_type_str;
    clearArray(data);
 
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////////////////////
  // slave: read charging state
  result = node.readHoldingRegisters(0x120, 1);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 1; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }
    charging_state = data[0] & 0x00ff;  //reuse save memory probably going to regret

    switch (charging_state)
    {  
      case 0:
        strcpy(charge_state_str, "Deactivated");
        break;
      
      case 1:
        strcpy(charge_state_str, "Activated");
        break;

      case 2:
        strcpy(charge_state_str, "mppt");
        break;

      case 3:
        strcpy(charge_state_str, "Equalizing");
        break;

      case 4:
        strcpy(charge_state_str, "Boost");
        break;

      case 5:
        strcpy(charge_state_str, "Floating");
        break;

      case 6:
        strcpy(charge_state_str, "Current Limiting");
        break;

      default:
        Serial.println("Error in Charge State Subroutine");
        break;    
    }
    doc["charge_state"] = charge_state_str;
  }
  ///////////////////////////////////////////////////////////////////////////////////////

  //serializeJson(doc, Serial);
  //Serial.println();
  serializeJson(doc, web_content);  //convert from JSON to String for web_content serving.
  //delay(5000);
}
//end of loop

///////////////////////////////////////////////////////////////////////////////////////
//Function definitions
///////////////////////////////////////////////////////////////////////////////////////

void handleRoot() {
  server.send(200, "text/plain", web_content);   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void handleNotFound(){
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

void clearArray(uint16_t (&myArray)[6])
{
  for (int i = 0; i < sizeof(myArray); i++)
  {
    myArray[i] = 0;
  }

}

float getTemperature(DeviceAddress deviceAddress)
{
  float tempC = temp_sensor.getTempC(deviceAddress);
  if(tempC == DEVICE_DISCONNECTED_C) 
  {
    Serial.println("Error: Could not read temperature data");
    return 0;
  }
 return tempC;
}
