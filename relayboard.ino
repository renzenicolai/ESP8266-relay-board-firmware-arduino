#include <DHT.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Time.h>

#define output1_pin 13
#define output2_pin 12
#define button1_pin 2
#define button2_pin 0
#define status_pin 16

#define dht_pin 14
#define dht_type DHT22
DHT dht(dht_pin, dht_type, 20);

float humidity, temperature;

MDNSResponder mdns;
ESP8266WebServer server ( 80 );


char s_ssid[33] = {0};
char s_password[65] = {0};
uint8_t s_state = 0;
uint8_t s_bootval = 0;

bool output1_state = false;
bool output2_state = false;

void settings_load() {
  s_state = EEPROM.read(0);
  for (int i = 0; i<33; i++) {
    s_ssid[i] = EEPROM.read(1+i);
  }
  for (int i = 0; i<65; i++) {
    s_password[i] = EEPROM.read(1+33+i);
  }
  s_bootval = EEPROM.read(1+33+65);
}

void settings_store() {
  EEPROM.write(0,s_state); 
  for (int i = 0; i<33; i++) {
    EEPROM.write(1+i, s_ssid[i]);
  }
  for (int i = 0; i<65; i++) {
    EEPROM.write(1+33+i, s_password[i]);
  }
  EEPROM.write(1+33+65, s_bootval);
  EEPROM.commit();
}

void settings_setup() {
    uint8_t cnt = 0;
    Serial.flush();
    Serial.print("Press 's' to enter serial setup mode");
    while (cnt<100) {
      if (Serial.available()) {
        char input = Serial.read();
        Serial.println(input);
        if (input == 's' || input == 'S') {
          settings_setup_serial();
          break;
        }
      }
      delay(50);
      Serial.print(".");
      cnt++;
    }
    Serial.print("<INFO> Starting in AP mode...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("WiFiClockSetup");
    Serial.print("<INFO> IP address: ");
    Serial.println(WiFi.softAPIP());
    if ( mdns.begin ( "wificlock.local", WiFi.localIP() ) ) {
      Serial.println ( "<INFO> MDNS responder started." );
    }
    server.on ("/", handleSetup);
    server.on ("/setupstore", handleSetupStore);
    server.onNotFound ( handleNotFound );
    server.begin();
    Serial.println("<INFO> HTTP server started in setup mode.");

    digitalWrite(output1_pin, LOW);
    digitalWrite(output2_pin, LOW);
      
    while (1) {
      mdns.update();
      server.handleClient();
      checkbuttons();
    }
}

void settings_setup_serial() {
  ESP.wdtDisable();
  int setupstate = 0;
  while (setupstate < 3) {
    if (setupstate == 0) {
      Serial.flush();
      Serial.print("Please enter the SSID to use: ");
      bool waiting_for_input = true;
      uint8_t pos = 0;
      s_ssid[0] = 0;
      while (waiting_for_input) {
        if (Serial.available()) {
          char input = Serial.read();
          if (input == '\n') {
            waiting_for_input = false;
            break;
          }
          s_ssid[pos] = input;
          s_ssid[pos+1] = 0;
          Serial.print(input);
          pos++;
        }
        if (pos>31) {
          waiting_for_input = false;
          Serial.println("\nMaximum length for SSID reached!");
        }
      }
      Serial.print("\nSSID has been set to \"");
      Serial.print(s_ssid);
      Serial.println("\".");
      Serial.print("Is this correct? (y/N)");
      Serial.flush();
      while (!Serial.available());
      char input = Serial.read();
      Serial.println(input);
      Serial.flush();
      if (input == 'y' || input == 'Y') {
        setupstate = 1;
      }
    } else if (setupstate == 2) {
      delay(100);
      Serial.flush();
      Serial.print("Please give a number (0, 1, 2 or 3) to describe the output state after a reboot: ");
      int intinput = Serial.parseInt();

      if ((intinput>=0) && (intinput<4)) {
        s_bootval = intinput;
      } else {
        Serial.print("\n<ERROR> Input has to be a number in the range 0 to 3.");
      }
      
      Serial.print("\nBootstate has been set to \"");
      Serial.print(s_bootval);
      Serial.println("\".");
      Serial.print("Is this correct? (y/N)");
      Serial.flush();
      while (!Serial.available());
      char input = Serial.read();
      Serial.flush();
      Serial.println(input);
      if (input == 'y' || input == 'Y') {
        setupstate = 3;
      }
    } else if (setupstate == 1) {
      delay(100);
      Serial.flush();
      Serial.print("Please enter the password to use: ");
      bool waiting_for_input = true;
      uint8_t pos = 0;
      s_password[0] = 0;
      while (waiting_for_input) {
        if (Serial.available()) {
          char input = Serial.read();
          if (input == '\n') {
            waiting_for_input = false;
            Serial.print("*");
            break;
          }
          s_password[pos] = input;
          s_password[pos+1] = 0;
          Serial.print(input);
          pos++;
        }
        if (pos>63) {
          waiting_for_input = false;
          Serial.println("\nMaximum length for password reached!");
        }
      }
      Serial.print("\nPassword has been set to \"");
      Serial.print(s_password);
      Serial.println("\".");
      Serial.print("Is this correct? (y/N)");
      Serial.flush();
      while (!Serial.available());
      char input = Serial.read();
      Serial.flush();
      Serial.println(input);
      if (input == 'y' || input == 'Y') {
        setupstate = 2;
      }
    }
  }
  Serial.print("Setup complete. Writing settings to flash...");
  s_state = 1;
  settings_store();
  Serial.print("Rebooting...");
  delay(100);
  ESP.wdtEnable(100);
  ESP.reset();
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void handleSetupStore() {
  String message = "Setup:\n";
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
    if (server.argName(i)=="ssid") {
      message += " SSID set to "+server.arg(i)+"\n";
      String valuestr = String(server.arg(i));
      valuestr.toCharArray(s_ssid, 33);
    }
    if (server.argName(i)=="password") {
      message += " Password set to "+server.arg(i)+"\n";
      String valuestr = String(server.arg(i));
      valuestr.toCharArray(s_password, 65);
    }
    if (server.argName(i)=="bootstate") {
      String valuestr = String(server.arg(i));
      message += " Bootstate set to "+server.arg(i)+"\n";
      int bootstateint = valuestr.toInt();
      if ((bootstateint>=0) && (bootstateint<4)) {
        s_bootval = bootstateint;
      } else {
        Serial.println("<ERROR> bootstate out of bounds.");
        message += "ERROR: bootstate out of bounds\n";
      };
    }
    if (server.argName(i)=="store") {
      s_state = 1;
      message += " Save to flash!\n";
      settings_store();
    }
  }

  server.send ( 200, "text/plain", message );
  delay(1000);
  ESP.reset();
}

int cval = 0;
bool binval = false;
uint8_t bval[4] = {0};
int brightness = 1024;
bool autobrightness = true;

void handleCommand() {
  digitalWrite(status_pin, HIGH);
  String message = "Command:\n";
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
    if (server.argName(i)=="output") {
      message += " Output set to "+server.arg(i)+"\n";
      String valuestr = String(server.arg(i));
      uint8_t input = valuestr.toInt();
      if (input&0x01) {
        digitalWrite(output1_pin, HIGH);
        output1_state = true;
      } else {
        digitalWrite(output1_pin, LOW);
        output1_state = false;
      }
      if (input&0x02) {
        digitalWrite(output2_pin, HIGH);
        output2_state = true;
      } else {
        digitalWrite(output2_pin, LOW);
        output2_state = false;
      }
      Serial.print("<DEBUG> Set output to value '");
      Serial.print(cval);
      Serial.println("'.");
    }
    if (server.argName(i)=="output1") {
      message += " Output 1 set to "+server.arg(i)+"\n";
      String valuestr = String(server.arg(i));
      uint8_t input = valuestr.toInt();
      if (input) {
        digitalWrite(output1_pin, HIGH);
        output1_state = true;
      } else {
        digitalWrite(output1_pin, LOW);
        output1_state = false;
      }
      Serial.print("<DEBUG> Set output 1 to value '");
      Serial.print(cval);
      Serial.println("'.");
    }
    if (server.argName(i)=="output2") {
      message += " Output 2 set to "+server.arg(i)+"\n";
      String valuestr = String(server.arg(i));
      uint8_t input = valuestr.toInt();
      if (input) {
        digitalWrite(output2_pin, HIGH);
        output2_state = true;
      } else {
        digitalWrite(output2_pin, LOW);
        output2_state = false;
      }
      Serial.print("<DEBUG> Set output 2 to value '");
      Serial.print(cval);
      Serial.println("'.");
    }
    if (server.argName(i)=="temperature") {
        char temp[40];
        readDHT();
        dtostrf(temperature, 4, 2, temp);
        message = String(temp);
    }
    if (server.argName(i)=="humidity") {
        char temp[40];
        readDHT();
        dtostrf(humidity, 4, 2, temp);
        message = String(temp);
    }
    if (server.argName(i)=="state1") {
      if (output1_state) {
        message = "1";
      } else {
        message = "0";
      }
    }
    if (server.argName(i)=="state2") {
      if (output2_state) {
        message = "1";
      } else {
        message = "0";
      }
    }
    if (server.argName(i)=="factory") {
      s_state = 0; 
      s_ssid[0] = 0;
      s_password[0] = 0;
      s_bootval = 0;
      settings_store();
    }
  }

  server.send ( 200, "text/plain", message );
  digitalWrite(status_pin, LOW);
}

void handleRoot() {
  digitalWrite(status_pin, HIGH);
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    if (server.argName(i)=="output") {
      String valuestr = String(server.arg(i));
      uint8_t input = valuestr.toInt();
      if (input&0x01) {
        digitalWrite(output1_pin, HIGH);
        output1_state = true;
      } else {
        digitalWrite(output1_pin, LOW);
        output1_state = false;
      }
      if (input&0x02) {
        digitalWrite(output2_pin, HIGH);
        output2_state = true;
      } else {
        digitalWrite(output2_pin, LOW);
        output2_state = false;
      }
      Serial.print("<DEBUG> Set output to value '");
      Serial.print(cval);
      Serial.println("'.");
    }
    if (server.argName(i)=="output1") {
      String valuestr = String(server.arg(i));
      uint8_t input = valuestr.toInt();
      if (input) {
        digitalWrite(output1_pin, HIGH);
        output1_state = true;
      } else {
        digitalWrite(output1_pin, LOW);
        output1_state = false;
      }
      Serial.print("<DEBUG> Set output 1 to value '");
      Serial.print(cval);
      Serial.println("'.");
    }
    if (server.argName(i)=="output2") {
      String valuestr = String(server.arg(i));
      uint8_t input = valuestr.toInt();
      if (input) {
        digitalWrite(output2_pin, HIGH);
        output2_state = true;
      } else {
        digitalWrite(output2_pin, LOW);
        output2_state = false;
      }
      Serial.print("<DEBUG> Set output 2 to value '");
      Serial.print(cval);
      Serial.println("'.");
    }
  }
  
  char temp[1300];

  readDHT();

  char temperature_string[10];
  dtostrf(temperature, 4, 2, temperature_string);
  char humidity_string[10];
  dtostrf(humidity, 4, 2, humidity_string);

  snprintf ( temp, 1300,

"<html>\
  <head>\
    <title>RN+ WiFi switch</title>\
    <style>\
      body { background-color: #363636; font-family: Arial, Helvetica, Sans-Serif; Color: #FFFFFF; }\
      A { color: #FFFFFF; }\
      .logo1 { color: 00AEEF;}\
    </style>\
  </head>\
  <body>\
    <h1><span class='logo1'>R</span>N+ WiFi switch</h1>\
    <hr />\
    Temperature: %s &#176;c<br />\
    Humidity: %s %<br />\
    <hr />\
    <p>Output 1: <a href='/?output1=1'>ON</a> - <a href='/?output1=0'>OFF</a> [Current state: %01d]</p>\
    <p>Output 2: <a href='/?output2=1'>ON</a> - <a href='/?output2=0'>OFF</a> [Current state: %01d]</p>\
    <form action='/' method='post'>Direct control (0,1,2 or 3): <input type='text' name='output' value='0'><input type='submit'></form>\
    <p><a href='/setup'>Go to setup</a></p>\
    <p><a href='/command?reset=true'>Reboot</a></p>\
  </body>\
</html>",

    temperature_string, humidity_string, output1_state, output2_state
  );
  server.send ( 200, "text/html", temp );
  digitalWrite(status_pin, LOW);
}

void handleSetup() {
  digitalWrite(status_pin, HIGH);
  char temp[1200];

  snprintf ( temp, 1200,

"<html>\
  <head>\
    <title>RN+ WiFi switch - Setup</title>\
    <style>\
      body { background-color: #363636; font-family: Arial, Helvetica, Sans-Serif; Color: #FFFFFF; }\
      A { color: #FFFFFF; }\
      .logo1 { color: 00AEEF;}\
    </style>\
  </head>\
  <body>\
    <h1><span class='logo1'>R</span>N+ WiFi switch - Setup</h1>\
    <form action='/setupstore' method='post'>SSID: <input type='text' name='ssid' value=''><br />\
    Password: <input type='text' name='password' value=''><br />\
    Output state at boot (0,1,2 or 3): <input type='text' name='bootstate' value='0'><br />\
    <input type='submit' value='Save configuration'><input type='hidden' name='store' value='yes'></form>\
  </body>\
</html>"
  );
  server.send ( 200, "text/html", temp );
  digitalWrite(status_pin, LOW);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nDouble relay switch\nRN+ 2015\n\nFirmware version: 2.0\n");

  pinMode(output1_pin, OUTPUT);
  pinMode(output2_pin, OUTPUT);
  pinMode(status_pin, OUTPUT);
  pinMode(button1_pin, INPUT);
  pinMode(button2_pin, INPUT);

  digitalWrite(status_pin, LOW);

  EEPROM.begin(sizeof(s_state)+sizeof(s_ssid)+sizeof(s_password)+sizeof(s_bootval));

  //Read state, ssid and password from flash
  settings_load();

  if ((!digitalRead(button1_pin)) && (!digitalRead(button2_pin))) {
    Serial.println("<INFO> Both buttons pressed: wiping settings...");
    s_state = 0; 
    s_ssid[0] = 0;
    s_password[0] = 0;
    s_bootval = 0;
    settings_store();
    ESP.reset();
  }

  if (s_state==0) {
    Serial.println("<INFO> The device has not been configured yet.");
    digitalWrite(status_pin, HIGH);
    settings_setup();
  } else if (s_state>1) {
    Serial.println("<ERROR> Unknown state. Resetting to factory default settings...");
    s_state = 0; 
    s_ssid[0] = 0;
    s_password[0] = 0;
    s_bootval = 0;
    settings_store();
    ESP.reset();
  } else {
    Serial.print("<INFO> Device has been configured to connect to network \"");
    Serial.print(s_ssid);
    Serial.println("\".");
    Serial.print("<DEBUG> WiFi password is \"");
    Serial.print(s_password);
    Serial.println("\".");
    Serial.flush();
    uint8_t cnt = 0;
    Serial.flush();
    Serial.print("Press 's' to enter serial setup mode");
    while (cnt<100) {
      if (Serial.available()) {
        char input = Serial.read();
        Serial.println(input);
        if (input == 's' || input == 'S') {
          settings_setup_serial();
          break;
        }
      }
      delay(10);
      Serial.print(".");
      cnt++;
    }
    Serial.print("\n<INFO> Connecting to the WiFi network");
    WiFi.mode(WIFI_STA);
    WiFi.begin(s_ssid,s_password);
    bool led = 0;
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      digitalWrite(status_pin, led);
      led = ~led;
      delay(30);
    }
    Serial.println(" connected");
    Serial.print("<INFO> IP address: ");
    Serial.println(WiFi.localIP());
  }

  if ( mdns.begin ( "wifiswitch.local", WiFi.localIP() ) ) {
    Serial.println ( "<INFO> MDNS responder started." );
  }

  if (s_bootval&0x01) {
    digitalWrite(output1_pin, HIGH);
    output1_state = true;
  } else {
    digitalWrite(output1_pin, LOW);
    output1_state = false;
  }
  if (s_bootval&0x02) {
    digitalWrite(output2_pin, HIGH);
    output2_state = true;
  } else {
    digitalWrite(output2_pin, LOW);
    output2_state = false;
  }

  Serial.println ( "<INFO> Boot state set." );
  
  server.on ("/", handleRoot);
  server.on ("/command", handleCommand);
  server.on ("/setup", handleSetup);
  server.on ("/setupstore", handleSetupStore);
  server.onNotFound ( handleNotFound );
  server.begin();
  Serial.println("<INFO> HTTP server started.");
}

bool button1_ispressed = false;
bool button2_ispressed = false;
void checkbuttons() {
  if (!digitalRead(button1_pin)) {
    if (!button1_ispressed) {
      button1_ispressed = true;
      output1_state = !output1_state;
      digitalWrite(output1_pin, output1_state);
    }
  } else {
    button1_ispressed = false;
  }
  if (!digitalRead(button2_pin)) {
    if (!button2_ispressed) {
      button2_ispressed = true;
      output2_state = !output2_state;
      digitalWrite(output2_pin, output2_state);
    }
  } else {
    button2_ispressed = false;
  }
}

void readDHT() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature(false);
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("<ERROR> Failed to read from the DHT sensor.");
    humidity = -1;
    temperature = -1;
  } else {
    Serial.print("<INFO> Temperature: ");
    Serial.print(temperature);
    Serial.print(" degrees c, humidity: ");
    Serial.print(humidity);
    Serial.println("%");
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("<ERROR> Lost connection to WiFi network. Rebooting...");
    digitalWrite(status_pin, LOW);
    delay(100);
    ESP.reset();
  }
  mdns.update();
  server.handleClient();
  checkbuttons();
}
