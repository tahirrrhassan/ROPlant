
#define BLYNK_MAX_SENDBYTES 255
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <EEPROM.h>

WidgetLED well(V3);
WidgetLED feed(V4);
WidgetLED pressure(V5);
WidgetLED AM(V10);

#define wellPin 32 //D0 16
#define feedPin 33
#define presPin 25
#define intrPin 26
#define am 27

boolean wellPump;
boolean feedPump;
boolean presPump;
boolean amStatus;
boolean err = 0, wellErr = 0, feedErr = 0, presErr = 0, i = 0;

float calibrationFactor = 7.5; //450 pulses / 60 sec
int count = 0;

float wellKW = 2.1;
float feedKW = 1.27;
float presKW = 2.94;

volatile byte pulse = 0;  
float flowRate = 0.0, totalGallons = 0.0, preGallons = 0.0;
float KW = 0.0, KWh = 0.0, preKWh = 0.0;
float GPKW = 0.0;
uint32_t seconds = 0;//oldTime = 0;

char auth[] = "DG-Dmf24TsZoDIuIuUw8b4a2EJNhcpT9";
char ssid[] = "Transworld";//"Infinix Smart 3 Plus";//"Transworld_EXT"; //"HUAWEI nova";//
char pass[] = "Password"; //"mansoors";//"Zubair123";

String body;

BlynkTimer timer;

void intr(){
  pulse++;
}

void EEPROMWrite(int address, uint32_t value)
      {
      //Decomposition from a long to 4 bytes by using bitshift.
      //One = Most significant -> Four = Least significant byte
      byte four = (value & 0xFF);
      byte three = ((value >> 8) & 0xFF);
      byte two = ((value >> 16) & 0xFF);
      byte one = ((value >> 24) & 0xFF);

      //Write the 4 bytes into the eeprom memory.
      EEPROM.write(address, four);
      EEPROM.write(address + 1, three);
      EEPROM.write(address + 2, two);
      EEPROM.write(address + 3, one);
      EEPROM.commit();
      }

//This function will return a 4 byte (32bit) long from the eeprom
//at the specified address to adress + 3.
uint32_t EEPROMRead(int address)
      {
      //Read the 4 bytes from the eeprom memory.
      uint32_t four = EEPROM.read(address);
      uint32_t three = EEPROM.read(address + 1);
      uint32_t two = EEPROM.read(address + 2);
      uint32_t one = EEPROM.read(address + 3);

      //Return the recomposed long by using bitshift.
      return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
      }


void test(){
  Serial.println("---------------------------------------");
  Serial.print("Well Pump : ");
  Serial.println(wellPump);
  Serial.print("Feed Pump : ");
  Serial.println(feedPump);
  Serial.print("Pressure Pump: ");
  Serial.println(presPump);
  Serial.print("FlowRate: ");
  Serial.print(flowRate);
  Serial.println(" GPM");
  Serial.print("Total: ");
  Serial.print(totalGallons);
  Serial.println(" Gallons");
}
void check(){
  if(!Blynk.connected()){
    Blynk.disconnect();
    Blynk.begin(auth, ssid, pass);
    count = 18;
  }
  else{
    if(wellErr && feedErr && presErr){
      if(!i){
        i = 1;
        body = String("AT ""RO PLANT 1.5GPM"", ALL PUMPS i.e. WELL PUMP, FEED PUMP AND HIGH-PRESSURE PUMP ARE STOPPED.");
        Blynk.email("mansoor.samad@patex.com.pk", "Subject: RO PLANT STATUS", body);
        Blynk.notify("RO PLANT 1.5GPM is currently off");
      } else i = 0;
    }
    else if(!feedErr && presErr){
      if(!i){
        i = 1;
        body = String("HIGH-PRESSURE PUMP IS STOPPED, WHILE FEED PUMP IS RUNNING AT ""RO PLANT 1.5GPM"".");
        Blynk.email("mansoor.samad@patex.com.pk", "Subject: RO PLANT STATUS", body);
        Blynk.notify("HIGH-PRESSURE PUMP IS STOPPED, WHILE FEED PUMP IS RUNNING.");
      } else i = 0;
    }
    else if(feedErr && !presErr){
      if(!i){
        i = 1;
        body = String("FEED PUMP IS STOPPED, WHILE HIGH-PRESSURE PUMP IS RUNNING AT ""RO PLANT 1.5GPM"".");
        Blynk.email("mansoor.samad@patex.com.pk", "Subject: RO PLANT STATUS", body);
        Blynk.notify("FEED PUMP IS STOPPED, WHILE HIGH-PRESSURE PUMP IS RUNNING.");
      } else i = 0;
    }

    EEPROMWrite(0, KWh*100);
    EEPROMWrite(5, totalGallons*100);
    EEPROM.commit();
  }
}
void MAIN(){
  wellPump = !digitalRead(wellPin);
  feedPump = digitalRead(feedPin);
  presPump = !digitalRead(presPin);
  amStatus = !digitalRead(am);
  
  //if((millis() - oldTime) > 1000){   
  detachInterrupt(intrPin);
  flowRate = pulse / calibrationFactor;
  //oldTime = millis();
  flowRate = (flowRate * 0.27);
  totalGallons += (flowRate/60);
  pulse = 0;
  attachInterrupt(intrPin, intr, FALLING);
  //}
  //test();

  if(amStatus){
    AM.on();  
  } else{
    AM.off();
  }
  KW = 0.0;
  if(wellPump){
    KW += wellKW;
    wellErr = 0;
    well.on();
  }
  else{
    wellErr = 1;
    well.off();
  }
  
  if(feedPump){
    feedErr = 0;
    KW += feedKW;
    feed.on();
  }
  else{
    feedErr = 1;
    feed.off();
  }
  
  if(presPump){
    presErr = 0;
    KW += presKW;
    pressure.on();
  }
  else{
    presErr = 1;
    pressure.off();
  }
  
  
  KWh += (KW/3600); //adding per second (3600s = 1hr)
  seconds++;
  if(seconds>=2592000){ //monthly seconds
    seconds = 0;
    preKWh = KWh;
    KWh=0;
    preGallons = totalGallons;
    totalGallons = 0;
    GPKW = totalGallons / (KWh/3600); //Gallons per KW
    EEPROMWrite(10, preKWh*100);
    EEPROMWrite(15, preGallons*100);
    EEPROM.commit();
  }
  //////////////////////////////////////////////////

  if(count >= 5){
    count = 0;
    check();
  } else count++;
  
  Blynk.virtualWrite(V6, preKWh);
  Blynk.virtualWrite(V7, preGallons);
  Blynk.virtualWrite(V8, GPKW);
  Blynk.virtualWrite(V9, KWh);
  Blynk.virtualWrite(V0, KW);
  Blynk.virtualWrite(V2, flowRate);
  Blynk.virtualWrite(V1, totalGallons);
  
}


BLYNK_CONNECTED(){
  Blynk.syncVirtual(V11);
}


BLYNK_WRITE(V11){
  if(param.asInt() == 1){
    totalGallons=0;
    KWh=0;
    preKWh=0;
    preGallons=0;
    GPKW=0;
  }
}


void setup() {
  // put your setup code here, to run once:
  EEPROM.begin(20);
  Serial.begin(9600);  
  pinMode(wellPin, INPUT_PULLUP);
  pinMode(feedPin, INPUT_PULLUP);
  pinMode(presPin, INPUT_PULLUP);
  pinMode(intrPin, INPUT_PULLUP);
  pinMode(am, INPUT_PULLUP);
  
  preKWh = (EEPROMRead(10)/100.000);
  preGallons = (EEPROMRead(15)/100.000);
  KWh = (EEPROMRead(0)/100.000);
  KWh += 0.011;
  totalGallons = (EEPROMRead(5)/100.000); 
  totalGallons += 0.034;
  //delay(1000);
  
  Blynk.begin(auth, ssid, pass);
  //Blynk.email("mansoor.samad@patex.com.pk", "Subject: RO PLANT STATUS", """RO PLANT 1.5GPM"" IS SUCCESSFULLY CONNECTED TO THE SERVER.");
  timer.setInterval(1000L, MAIN);
  attachInterrupt(intrPin, intr, FALLING);
}

void loop() {
  if(Blynk.connected()){
    Blynk.run();
    if(!i){
      i=1;
      preKWh = (EEPROMRead(10)/100.000);
      preGallons = (EEPROMRead(15)/100.000);
      KWh = (EEPROMRead(0)/100.000);
      KWh += 0.011;
      totalGallons = (EEPROMRead(5)/100.000); 
      totalGallons += 0.034;
    }
  }
  timer.run();  
}
