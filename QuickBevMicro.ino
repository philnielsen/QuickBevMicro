
#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

/* -------------------- NFC Globals -------------------- */
PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

/* -------------------- Flow Meter Globals ------------- */
byte statusLed    = 13;

byte sensorInterrupt = 0;  // 0 = digital pin 2
byte sensorPin       = 2;

// The hall-effect flow sensor outputs approximately 4.5 pulses per second per
// litre/minute of flow.
float calibrationFactor = 140;

volatile byte pulseCount;  

float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;

unsigned long oldTime;

/* -------------------- Solenoid Valve ---------------------- */
//ATMEGA pin 16 = Arduino digital pin 10
byte solenoidValvePin = 10;

void setupNFC(){
    Serial.println("-------Peer to Peer HCE--------");
    
    nfc.begin();
    
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (! versiondata) {
      Serial.print("Didn't find PN53x board");
      while (1); // halt
    }
    
    // Got ok data, print it out!
    Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
    Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
    Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
    
    // Set the max number of retry attempts to read from a card
    // This prevents us from waiting forever for a card, which is
    // the default behaviour of the PN532.
    //nfc.setPassiveActivationRetries(0xFF);
    
    // configure board to read RFID tags
    nfc.SAMConfig();

      // Initialize a serial connection for reporting values to the host

}

/* -------------------- Flow Meter Methods -----------------------------*/

void setupFlowMeter(){
    // Set up the status LED line as an output
    pinMode(statusLed, OUTPUT);
    digitalWrite(statusLed, HIGH);  // We have an active-low LED attached
    
    pinMode(sensorPin, INPUT);
    digitalWrite(sensorPin, HIGH);

    pulseCount        = 0;
    flowRate          = 0.0;
    flowMilliLitres   = 0;
    totalMilliLitres  = 0;
    oldTime           = 0;

    // The Hall-effect sensor is connected to pin 2 which uses interrupt 0.
    // Configured to trigger on a FALLING state change (transition from HIGH
    // state to LOW state)
    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
}


void printResponse(uint8_t *response, uint8_t responseLength) {
  
   String respBuffer;

    for (int i = 0; i < responseLength; i++) {
      
      if (response[i] < 0x10) 
        respBuffer = respBuffer + "0"; //Adds leading zeros if hex value is smaller than 0x10
      
      respBuffer = respBuffer + String(response[i], HEX) + " ";                        
    }

    Serial.print("response: "); Serial.println(respBuffer);
}

String hexToString(uint8_t *response, uint8_t responseLength) {
  
   String respBuffer;

    for (int i = 0; i < responseLength; i++) {
      
      if (response[i] < 0x10) 
        respBuffer = respBuffer + "0"; //Adds leading zeros if hex value is smaller than 0x10
      
      respBuffer = respBuffer + String(response[i], HEX) + " ";                        
    }

    return respBuffer;
}


/*
Insterrupt Service Routine
 */
void pulseCounter()
{
  // Increment the pulse counter
  pulseCount++;
}

void flowMeterBody(){
  if((millis() - oldTime) > 1000) {    // Only process counters once per second 
    detachInterrupt(sensorInterrupt);
          
    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.
    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
    
    // Note the time this processing pass was executed. Note that because we've
    // disabled interrupts the millis() function won't actually be incrementing right
    // at this point, but it will still return the value it was set to just before
    // interrupts went away.
    oldTime = millis();
    
    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
    flowMilliLitres = (flowRate / 60) * 1000;
    
    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;
      
    unsigned int frac;
    
    // Print the flow rate for this second in litres / minute
    Serial.print("Flow rate: ");
    Serial.print(int(flowRate));  // Print the integer part of the variable
    Serial.print(".");             // Print the decimal point
    // Determine the fractional part. The 10 multiplier gives us 1 decimal place.
    frac = (flowRate - int(flowRate)) * 10;
    Serial.print(frac, DEC) ;      // Print the fractional part of the variable
    Serial.print("L/min");
    // Print the number of litres flowed in this second
    Serial.print("  Current Liquid Flowing: ");             // Output separator
    Serial.print(flowMilliLitres);
    Serial.print("mL/Sec");

    // Print the cumulative total of litres flowed since starting
    Serial.print("  Output Liquid Quantity: ");             // Output separator
    Serial.print(totalMilliLitres);
    Serial.println("mL"); 

    // Reset the pulse counter so we can start incrementing again
    pulseCount = 0;
    
    // Enable the interrupt again now that we've finished sending output
    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
  }
}

void openSolenoidValve(){
  digitalWrite(solenoidValvePin, HIGH);
  return;
}
void closeSolenoidValve(){
  digitalWrite(solenoidValvePin, LOW);
  return;
}

uint8_t getOrderAmount(String token){
  /* TODO */
  /* Check firebase */
  /* If order has been processed return 0,
     if token doesn't exist return 0,
     if token exists and hasn't been processed, return amount
  */
  // See if has been processed
  // HTTP GET /Orders/%2D[token]/processed.json returns true or false
  
  // Update to processed
  // HTTP PATCH -d {"processed":true} /Orders/%2D[token].json

  return 200;
}

void processOrder(String token){
  uint8_t amount = getOrderAmount(token);
  if (amount > 0){
    openSolenoidValve();
    while (totalMilliLitres < amount){
      flowMeterBody();
    }
    closeSolenoidValve();
    totalMilliLitres = 0;
  }
  else {
   
  }
  return;
}


/* ------------------ Main Arduino Methods ----------------------------- */
void setup()
{    
    Serial.begin(115200);
    pinMode(solenoidValvePin, OUTPUT);
    closeSolenoidValve();
    setupNFC();
    setupFlowMeter();
}

void loop()
{
  bool success;
  
  uint8_t responseLength = 32;
  
  flowMeterBody();

  Serial.println("Waiting for an ISO14443A card");
  
  // set shield to inListPassiveTarget
  success = nfc.inListPassiveTarget();

  if(success) {
   
    Serial.println("Found something!");
                  
    uint8_t selectApdu[] = { 0x00, /* CLA */
                              0xA4, /* INS */
                              0x04, /* P1  */
                              0x00, /* P2  */
                              0x05, /* Length of AID  */
                              0xF2, 0x22, 0x22, 0x22, 0x22 /* AID defined on Android App */
                               /* Le  */ };
                              
    uint8_t response[32];  
     
    success = nfc.inDataExchange(selectApdu, sizeof(selectApdu), response, &responseLength);

    if(success) {
      
      Serial.print("responseLength: "); Serial.println(responseLength);
       
      nfc.PrintHexChar(response, responseLength);
      uint8_t done = 0;
      do {
        uint8_t apdu[] = {0x80, 0xF0, 0x00, 0x00, 0x00, 0xF2, 0x22, 0x22, 0x22, 0x22};
        uint8_t back[32];
        uint8_t length = 32; 

        success = nfc.inDataExchange(apdu, sizeof(apdu), back, &length);
        
        if(success) {
          done = 1;
          Serial.print("responseLength: "); Serial.println(length); 
          nfc.PrintHexChar(back, length);

          String token = hexToString(response, responseLength);
          
          // Control solenoid valve
          processOrder(token);

          break;
        }
        else {
          
          Serial.println("Broken connection?"); 
        }
      }
      while(success && done == 0);
    }
    else {
     
      Serial.println("Failed sending SELECT AID"); 
    }
  }
  else {
   
    Serial.println("Didn't find anything!");
  }

  delay(1000);
}
