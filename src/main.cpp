#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA228.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DS3231.h>

#define INA1_ADDR 0x40 // 10000000b
#define INA2_ADDR 0x41 // 10000001b

#define ch1_alert_pin 36
#define ch2_alert_pin 35

#define T_LOG true    // attiva/disattiva gli NTC
#define powerLog true // attiva/disattiva gli INA

#define NTC1_PIN 6
#define NTC2_PIN 4
#define NTC3_PIN 5
#define NTC4_PIN 7

#define I2C_SDA 47
#define I2C_SCL 48

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64 

#define OLED_RESET     -1 
#define SCREEN_ADDRESS 0x3C 

#define startb 14
#define stopb 15

#define stb_time 3000             // tempo di off schermo
#define refresh_time 1000           // tempo di refresh variabili pulsanti

bool century = false;
bool h12Flag;
bool pmFlag;

bool alreadystarted = false;        // display timer (timer startato o no)
unsigned long t;

bool alreadystarted2 = false;       // timer che refresha le variabili pulsante
unsigned long t2;             

bool serial_log = false;

bool displaystatus = true; //acceso

bool startstatus = false;
bool stopstatus = false;

byte year;
byte month;
byte date;
byte dOW;
byte hour;
byte minute;
byte second;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_INA228 ch1 = Adafruit_INA228();
Adafruit_INA228 ch2 = Adafruit_INA228();

DS3231 rtc;

uint8_t mode = 1;                  // numero pagina 


float ntcToTemperature(int adcValue) {

    const float Vcc = 3.3;  
    const int ADC_MAX = 4095;

    const float R_fixed = 10000.0;   // r del partitore
    const float Beta = 3435.0;       //3435.0 default
    const float T0 = 298.15;         
    const float R0 = 10000.0;        //Rntc a 25 gradi 

    // Evita divisioni per zero e valori fuori range
    if (adcValue <= 0) return -273.15;
    if (adcValue >= ADC_MAX) return -273.15;

    float Vout = ((float)adcValue / ADC_MAX) * Vcc; // tensione partitore
    float R_ntc = R_fixed * (Vcc / Vout - 1.0);     // calcolo resistenza NTC
    float tempK = 1.0 / ( (1.0 / T0) + (1.0 / Beta) * log(R_ntc / R0) ); // calcolo Tk
    float tempC = tempK - 273.15; // calcolo gradi 

    return tempC+2.15;               // offset
}

void getDateStuff(byte& year, byte& month, byte& date, byte& dOW, byte& hour, byte& minute, byte& second) {
                  
    // Call this if you notice something coming in on
    // the serial port. The stuff coming in should be in
    // the order YYMMDDwHHMMSS, with an 'x' at the end.
    boolean gotString = false;
    char inChar;
    byte temp1, temp2;
    char inString[20];
    
    byte j=0;
    while (!gotString) {
        if (Serial.available()) {
            inChar = Serial.read();
            inString[j] = inChar;
            j += 1;
            if (inChar == 'x') {
                gotString = true;
            }
        }
    }
    Serial.println(inString);
    // Read year first
    temp1 = (byte)inString[0] -48;
    temp2 = (byte)inString[1] -48;
    year = temp1*10 + temp2;
    // now month
    temp1 = (byte)inString[2] -48;
    temp2 = (byte)inString[3] -48;
    month = temp1*10 + temp2;
    // now date
    temp1 = (byte)inString[4] -48;
    temp2 = (byte)inString[5] -48;
    date = temp1*10 + temp2;
    // now Day of Week
    dOW = (byte)inString[6] - 48;
    // now hour
    temp1 = (byte)inString[7] -48;
    temp2 = (byte)inString[8] -48;
    hour = temp1*10 + temp2;
    // now minute
    temp1 = (byte)inString[9] -48;
    temp2 = (byte)inString[10] -48;
    minute = temp1*10 + temp2;
    // now second
    temp1 = (byte)inString[11] -48;
    temp2 = (byte)inString[12] -48;
    second = temp1*10 + temp2;
}

void screentoggle(){    // standby schermo

  if(!alreadystarted){

    t = millis();
    alreadystarted = true;
  }

  else{

    if((millis()-t) >= stb_time){

      t=0;
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      displaystatus=false;
    }

    else{
      display.ssd1306_command(SSD1306_DISPLAYON);
      displaystatus=true;
    }
  }
}

void var_refresh(){     // ogni tot resetta le variabili dei pulsanti

  if(!alreadystarted2){

    t2=millis();
    alreadystarted2=true;
  }

  else{                                  
    if((millis()-t2) >= refresh_time){    //se è arrivato il momento di refreshare le variabili

      t2 = 0;
      alreadystarted2 = false; //resetta il flag (restarta il timer)
    }
  }
}

/* TODO: SD_log -> implementare log su sd
         menu -> implementare lo switch delle pagine
         */

void setup() {

  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL); 

  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();

  pinMode(startb,INPUT);
  pinMode(stopb,INPUT);

  delay(100);

  while(!digitalRead(startb) && !digitalRead(stopb)){  // config orologio
    
    display.setCursor(0,0);
    display.print("Inviare da terminale (115200 baud) la seguente stringa per impostare data e ora:    YYMMDDwHHMMSSx");
    display.display();

    if (Serial.available()) {
      getDateStuff(year, month, date, dOW, hour, minute, second);
        
      rtc.setClockMode(false);  // set to 24h
        //setClockMode(true); // set to 12h
      rtc.setYear(year);
      rtc.setMonth(month);
      rtc.setDate(date);
      rtc.setDoW(dOW);
      rtc.setHour(hour);
      rtc.setMinute(minute);
      rtc.setSecond(second);
        
    }
    delay(100);
  }
  
  if(T_LOG) { //se gli ntc sono abilitati inizializza i pin

    pinMode(NTC1_PIN, INPUT);
    pinMode(NTC2_PIN, INPUT);
    pinMode(NTC3_PIN, INPUT);
    pinMode(NTC4_PIN, INPUT);
  }
  
  if (powerLog){ // se gli INA sono abilitati li inizializza
    
    pinMode(ch1_alert_pin, INPUT_PULLUP);
    pinMode(ch2_alert_pin, INPUT_PULLUP);

    ch1.begin(INA1_ADDR);
    ch2.begin(INA2_ADDR);

    ch1.setShunt(0.01, 16.384); // shunt usato: 10 mΩ, corrente max 16.384A
    ch2.setShunt(0.01, 16.384);

    ch1.setAveragingCount(INA228_COUNT_16);  //OSR (1,4,16,64,128,256,512,1024)
    ch2.setAveragingCount(INA228_COUNT_16);

    ch1.setVoltageConversionTime(INA228_TIME_150_us);  // tempo di conversione (50us, 84us, 150us, 280us, 540us, 1052us, 2074us, 4120us) -> 2400us una lettura -> 2.4ms
    ch2.setVoltageConversionTime(INA228_TIME_150_us);

    ch1.setCurrentConversionTime(INA228_TIME_280_us);  // tempo di conversione (50us, 84us, 150us, 280us, 540us, 1052us, 2074us, 4120us) -> 4480us una lettura -> 4.48ms
    ch2.setCurrentConversionTime(INA228_TIME_280_us);

    ch1.setAlertType(INA228_ALERT_CONVERSION_READY); // alert quando la conversione è pronta
    ch2.setAlertType(INA228_ALERT_CONVERSION_READY);
  } 
}

void loop() {

  screentoggle();  
  var_refresh();

  if(!digitalRead(startb)){

    alreadystarted=false; // accende lo schermo e starta il timer
    startstatus = true;   // flag di pulsante premuto 

    if(displaystatus && mode<4){
      mode++;
      delay(300);
    }
  }   
  
  if(!digitalRead(stopb)){       

    alreadystarted=false; //  accende lo schermo e starta il timer  
    stopstatus=true;   
      
    if(displaystatus && mode>1){
      mode--;
      delay(300);
    }
  }
  
  switch (mode) {                // modalità di funzionamento (datalogger, test corda, ecc)

    case 1: // power o ntc logger o entrambi

      bool ch1_ready; 
      bool ch2_ready; 

      float busV1;
      float shuntV1;
      float current1;
      float power1;
      float energy1;
      float dieTemp1;

      float busV2;
      float shuntV2;
      float current2;
      float power2;
      float energy2;
      float dieTemp2;

      float temp1;
      float temp2;
      float temp3;
      float temp4;

      ch1_ready = !digitalRead(ch1_alert_pin);  // acquisisce lo stato dell'ina1 (ready= low)
      ch2_ready = !digitalRead(ch2_alert_pin);  // acquisisce lo stato dell'ina2 (ready= low)
     
      if(ch1_ready && powerLog) {            

        busV1 = ch1.readBusVoltage();     // V
        shuntV1 = ch1.readShuntVoltage(); // mV (caduta sullo shunt)
        current1 = ch1.readCurrent();     // mA
        power1 = ch1.readPower();         // mW
        energy1 = ch1.readEnergy();       // mWh
        //float charge1 = ch1.readCharge();     // C
        dieTemp1 = ch1.readDieTemp();     // °C

      }
  
      if(ch2_ready && powerLog) {    
      
        busV2 = ch2.readBusVoltage();
        shuntV2 = ch2.readShuntVoltage();
        current2 = ch2.readCurrent();
        power2 = ch2.readPower();
        energy2 = ch2.readEnergy();
        //float charge2 = ch2.readCharge();
        dieTemp2 = ch2.readDieTemp();
      }

      if(T_LOG) {                    

        temp1 = ntcToTemperature(analogRead(NTC1_PIN));
        temp2 = ntcToTemperature(analogRead(NTC2_PIN));
        temp3 = ntcToTemperature(analogRead(NTC3_PIN));
        temp4 = ntcToTemperature(analogRead(NTC4_PIN));
      }

      display.clearDisplay();
      
      if(powerLog){                  // se il log power è abilitato stampa i dati di potenza

        display.setCursor(0,0);
        display.print(busV1,3);
        display.print("V");

        display.setCursor(0,9);
        display.print(shuntV1);
        display.print("mV");

        display.setCursor(0,18);
        if(current1 < 1000.0){

          display.print(current1);
          display.print("mA");
        }
        else{
          display.print(current1/1000.0);
          display.print("A");
        }

        display.setCursor(0,27);
        if(power1 < 1000.0){

          display.print(power1);
          display.print("mW");
        }
        else{
          display.print(power1/1000.0);
          display.print("W");
        }

        display.setCursor(0,36);
        if(energy1 < 1000.0){

          display.print(energy1);
          display.print("mWh");
        }
        else{
          display.print(energy1/1000.0);
          display.print("Wh");
        }
      
        /*display.setCursor(0,45);
        display.print(dieTemp1);
        display.print("C");*/

        display.setCursor(0,54);
        display.print(rtc.getDate(), DEC);
        display.print("/");
        display.print(rtc.getMonth(century), DEC);
        display.print("/");
        display.print(rtc.getYear(), DEC);
      
        display.print(" ");
        display.print(rtc.getHour(h12Flag, pmFlag), DEC); //24-hr
        display.print(":");

        if(rtc.getMinute() < 10){
          display.print("0");
        }
        
        display.print(rtc.getMinute(), DEC);
        /*display.print(":");
        display.println(rtc.getSecond(), DEC);*/

        display.setCursor(50,0);
        display.print(busV2,3);
        display.print("V");

        display.setCursor(50,9);
        display.print(shuntV2);
        display.print("mV");

        display.setCursor(50,18);
        if (current2<1000.0){

          display.print(current2);
          display.print("mA");
        }
        else{
          display.print(current2/1000.0);
          display.print("A");
        }

        display.setCursor(50,27);
        if(power2 < 1000.0){

          display.print(power2);
          display.print("mW");
        }
        else{
          display.print(power2/1000.0);
          display.print("W");
        }
      
        display.setCursor(50,36);
        if(energy2 < 1000.0){

          display.print(energy2);
          display.print("mWh");
        }
        else{
          display.print(energy2/1000.0);
          display.print("Wh");
        }

        /*display.setCursor(50,45);
        display.print(dieTemp2);
        display.print("C");*/
      }

      if(T_LOG){                     // se il log ntc è abilitato stampa le temperature

        if(temp1 > -35 && temp1 < 100){

        display.setCursor(100,0);
        display.print(temp1,1);
        }
        else{
        display.setCursor(100,0);
        display.print("Err");
        }

        if(temp2 > -35 && temp2 < 100){

        display.setCursor(100,10);
        display.print(temp2,1);
        }
        else{
        display.setCursor(100,10);
        display.print("Err");
        }

        if(temp3 > -35 && temp3 < 100){

        display.setCursor(100,20);
        display.print(temp3,1);
        }
        else{

        display.setCursor(100,20);
        display.print("Err");
        }

        if(temp4 > -35 && temp4 < 100){

        display.setCursor(100,30);
        display.print(temp4,1);
        }
        else{
        display.setCursor(100,30);
        display.print("Err");
        }
      }
      
      if(serial_log){
        
        display.setCursor(100,54);
        display.print("LOG");
      }

      display.display();
      
      if(serial_log){                // se abilitato il log

        Serial.print(rtc.getHour(h12Flag, pmFlag), DEC);
        Serial.print(":");
        Serial.print(rtc.getMinute(), DEC);
        Serial.print(":");
        Serial.print(rtc.getSecond(), DEC);
        Serial.print(",");

        if(powerLog){ 
      
          Serial.print(busV1);
          Serial.print(",");
          Serial.print(shuntV1);
          Serial.print(",");
          Serial.print(current1);
          Serial.print(",");
          Serial.print(power1);
          Serial.print(",");
          Serial.print(energy1);
          Serial.print(",");

          Serial.print(busV2);
          Serial.print(",");
          Serial.print(shuntV2);
          Serial.print(",");
          Serial.print(current2);
          Serial.print(",");
          Serial.print(power2);
          Serial.print(",");
          Serial.print(energy2);
        }
        
        if(T_LOG){

          if(powerLog){      

            Serial.print(",");
          }
          
          Serial.print(temp1);
          Serial.print(",");
          Serial.print(temp2);
          Serial.print(",");
          Serial.print(temp3);
          Serial.print(",");
          Serial.print(temp4);
          Serial.println("");
        }

        else{

          Serial.println("");
        }
      }
    break;


    case 2:                          // test corda

      display.clearDisplay();
      display.setCursor(0,0);
      display.print("pag2");
      display.display();
      
    break;

    case 3:                        

      display.clearDisplay();
      display.setCursor(0,0);
      display.print("pag3");
      display.display();
      
    break;

    case 4:                          // test stocazzo

      display.clearDisplay();
      display.setCursor(0,0);
      display.print("pag4 (andrea gay)");
      display.display();
      
    break;
  }

}