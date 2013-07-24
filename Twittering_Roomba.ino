#include <Roomba.h>
#include <SPI.h>
#include "WiFly.h"
#include "TwitterWiFly.h"
#include "Credentials.h"
#include "sayings.h"

Twitter twitter(twitter_oAuth);
//Client client("twitter.com", 80); //Create a client for the WiFly, built in DNS services
Roomba roomba(&Serial, Roomba::Baud115200); //Create a roomba object on Serial port 1 at 115200 BPS, the default for 500 Series Roombas
Server server(80);

#define MinutesBetweenConnectionTries 5 //Time to wait between connection re-atempts, in minutes
#define DebounceTime 5 //Time to wait before an event can be retweeted. In Seconds
#define DeadBatteryThreshold 20 //Percent

void setup() {
  WiFly.begin(); //Initialize the WiFly
  while (!TryWiFly()) {
    delay(MinutesBetweenConnectionTries*60000); //If the connection failed, wait minutes defined and try again
  }
  randomSeed(analogRead(0)); //Make things really random
  server.begin();
  roomba.start(); //Tell the Roomba to start the OI
  
  pinMode(8, OUTPUT);
}

char* ChargeTypes[6] = {
  "Not charging","Reconditioning Charging","Full Charging","Trickle Charging","Waiting","Charging Fault Condition"};
  
int packetSizes[58] = {
  0,0,0,0,0,0, //1-6
  1,1,1,1,1,1,1,1,1,1,1,1, //7-18
  2,2, //19-20
  1, //21
  2,2, //22-23
  1, //24
  2,2,2,2,2,2,2, //25-31
  1, //32
  2, //33
  1,1,1,1,1, //34-38
  2,2,2,2,2,2, //39-44
  1, //45
  2,2,2,2,2,2, //46-51
  1,1, //52-53
  2,2,2,2, //54-57
  1 //58
};

int getPacketSize(int p) {
 return packetSizes[p-1]; 
}
int getPacketOffset(int p) {
  int i=0;
  for  (int s=1; s<p; s++) {
    i+=getPacketSize(s);
  }
  return i;
}

unsigned long lifted = 0;
unsigned long cliffs = 0;
unsigned long docked = 0;

boolean lowBattery = true; // Acts as a debounce
long battery_Current_mAh = 0;
long battery_Total_mAh = 0;
long battery_percent = 0;
boolean DeadBattery = false;
boolean FullBattery = true;
unsigned long chargingState = 0;
boolean firstRun = true; //We don't want it Tweeting when it is getting the current state
boolean buttonsDown = false;
long CorrectDockCycles = 0;
unsigned long cliffStartTime = 0;

uint8_t buf[52];
void loop(){

  if (roomba.getSensors(6, buf, 52)) {

    int off = 0;
    
    //Wheel drop sensors
    if (bitRead(buf[0], 2) && bitRead(buf[0], 3)){ //check if both wheels are up
      if (isDebounceClear(lifted, DebounceTime)) { //make sure it hasn't tweeted this instance yet
        checkFirstRunAndPost(OnPickedUp[random(0,NumPickedUpStrings)]);
      }
      lifted = millis(); //Update the Debounce time
    }
    
    //Cliff Sensors
    off=2;
    int triggedCliffs=0;
    for (int i=0; i<4; i++)
      triggedCliffs+=buf[off+i];
    if (triggedCliffs>0 && triggedCliffs<4 && isDebounceClear(lifted, DebounceTime)) {
      if (isDebounceClear(lifted, 2)) {
        if (isDebounceClear(cliffs, DebounceTime))
          checkFirstRunAndPost(OnCliffDetect[random(0,NumCliffStrings)]);
        cliffs = millis();
      }
    } else
      cliffStartTime=millis(); //When a cliff condition occurs, this stops updating.
    
    // Battery Checks
    off = getPacketOffset(21);
    chargingState = buf[off+0];
    battery_Current_mAh = buf[off+7]+256*buf[off+6];
    battery_Total_mAh = buf[off+9]+256*buf[off+8];
    if (battery_Total_mAh == 0) battery_Total_mAh=1;
    int nBatPcent = battery_Current_mAh*100/battery_Total_mAh;
    if (nBatPcent > 95 && battery_percent <= 95) {
      checkFirstRunAndPost(OnChargeFinish[random(0,NumChargeFinishStrings)]);
    } else if (nBatPcent < 10 && battery_percent >= 10) {
      checkFirstRunAndPost(OnBatteryLow[random(0,NumLowBatStrings)]);
    }
    battery_percent = nBatPcent;
    
    //Button States
    byte ButtonByte = buf[getPacketOffset(18)];
      if (!buttonsDown) { //Debounce
        if (bitRead(ButtonByte, 0)) { //Clean Button
          if (!DeadBattery) {
            checkFirstRunAndPost(OnClean[random(0,NumCleanStrings)]);
          }else{
            checkFirstRunAndPost(OnCleanOnLowBat[random(0,NumCleanOnLowStrings)]);
          }
        }else if (bitRead(ButtonByte, 1)){ //Spot Button
          
        }else if (bitRead(ButtonByte, 2)){ //Dock Button
          
        }
      }
      if (long(ButtonByte)>0) { //Debounce
        buttonsDown = true;
      }else{
        buttonsDown = false;
      }
    
    //Charging State
    byte ChargersAvailable = buf[getPacketOffset(34)];
    if (bitRead(ChargersAvailable,1)) { //Docked
      CorrectDockCycles +=1;
      if (CorrectDockCycles > 2) { //The docked bit seems to be false frequently. This should make it so we don't tweet the false information
        if (isDebounceClear(docked, DebounceTime)) {
          checkFirstRunAndPost(OnDocked[random(0,NumDockedStrings)]);
        }
        docked = millis();
      }
      if (firstRun) {
        docked = millis(); 
      }
    }else{
      CorrectDockCycles = 0;
    }
    chargingState = buf[getPacketOffset(21)];
    firstRun = false;
  }


  CheckWebServerClients(); //Check for clients to the Webserver
  delay(15); //No need to go faster. Roomba only checks its sensors every 15ms. Going faster will only slow the Roomba down.
}

void CheckWebServerClients() { //Mostly Sample code here
  Client client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean current_line_is_blank = true;
    String requestString = String("");

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        if (requestString.length() < 30) { //read char by char HTTP request
          requestString.concat(c); 
        } //store characters to string

        // if we've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so we can send a reply
        if (c == '\n' && current_line_is_blank) {

          if (requestString.indexOf("/SeekDock") > 0) {
            roomba.coverAndDock();
          }
          if (requestString.indexOf("/BeginClean") > 0) {
            roomba.cover();
          }

          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println();

          client.print("<a style=\"color:rgb(");
          long PercentToByte = map(battery_percent,0,100,0,255); //Fancy bit of code to fade the text color like the Roomba's power LED
          client.print(255-PercentToByte);
          client.print(",");
          client.print(PercentToByte);
          client.print(",0)\">");

          client.print("Battery mAh is ");
          client.print(battery_Current_mAh);
          client.print(" of ");
          client.print(battery_Total_mAh);
          client.print(" (");
          client.print(battery_percent);
          client.print("%)");
          //client.println("<br />");
          client.print("</a>");
          client.println("<br />");
          client.print("Charging State: ");
          client.print(ChargeTypes[chargingState]);
          client.println("<br />");

          client.println("<a href=\"/BeginClean\"> Clean </a>");
          client.println("<a href=\"/SeekDock\"> Dock </a>");
          break;
        }
        if (c == '\n') {
          // we're starting a new line
          current_line_is_blank = true;
        } 
        else if (c != '\r') {
          // we've gotten a character on the current line
          current_line_is_blank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(100);
    client.stop();
  }
}

boolean isDebounceClear(unsigned long firstTime, long seconds) {
  if (firstTime+(1000*seconds)<millis() || millis()<firstTime) { //If it has been DebounceTime OR if the millis() overflowed
    return true; //We have cleared the Debounce time
  }
  return false;
}

void checkFirstRunAndPost(char *Msg) {
  if (!firstRun) {
    stampAndPost(Msg);
  }
}

void stampAndPost(char *Msg) {
  String Stamped = StampString(String(Msg)); //Convert it to the String Object. We do not start with strings because they use more memory
  postToTwitter(Stamped);
}

String StampString(String BaseString) {
  BaseString.concat(String(" ["));
  BaseString.concat(millis());
  BaseString.concat(String("]]"));
  return BaseString;
}

boolean TryWiFly() {
  if (!WiFly.join(ssid, passphrase, isWPA)) { //Try an connect to the router with the SSID and Passphrase defined in Credentials.h
    return false; //We could not connect
  } 
  return true;
}

void postToTwitter(String Msg) {
  char CArray[Msg.length()];
  Msg.toCharArray(CArray,Msg.length()); //Convert the string back into a Char Array
  if (twitter.post(CArray)) {
    int status = twitter.wait();
    if (status == 200) {
      //Serial.println("OK."); //We do not want to be sending nonsense messages to the Roomba
    } else {
      //Serial.print("failed : code ");
      //Serial.println(status);
    }
  } else {
    //Serial.println("connection failed.");
  }
}

