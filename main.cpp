#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA228.h>

#define INA1_ADDR 0x40 // 10000000b
#define INA2_ADDR 0x41 // 10000001b

#define ch1_alert_pin 36
#define ch2_alert_pin 35

#define T_LOG true    // attiva/disattiva l'inizializzazione degli NTC
#define powerLog true // attiva/disattiva l'inizializzazione degli INA

#define NTC1_PIN 6
#define NTC2_PIN 4
#define NTC3_PIN 5
#define NTC4_PIN 7

uint8_t mode = 1; // 1 = current logger con o senza temp, 2 = solo temp logger

Adafruit_INA228 ch1 = Adafruit_INA228();
Adafruit_INA228 ch2 = Adafruit_INA228();


float ntcToTemperature(int adcValue) {

    const float Vcc = 3.3;  // misurare poi la vcc
    const int ADC_MAX = 4095;

    const float R_fixed = 10000.0;   // r del partitore
    const float Beta = 3435.0;       
    const float T0 = 298.15;         
    const float R0 = 10000.0;        //Rntc a 25 gradi 

    // Evita divisioni per zero e valori fuori range
    if (adcValue <= 0) return -273.15;
    if (adcValue >= ADC_MAX) return -273.15;

    float Vout = ((float)adcValue / ADC_MAX) * Vcc; // tensione partitore
    float R_ntc = R_fixed * (Vcc / Vout - 1.0);     // calcolo resistenza NTC
    float tempK = 1.0 / ( (1.0 / T0) + (1.0 / Beta) * log(R_ntc / R0) ); // calcolo Tk
    float tempC = tempK - 273.15; // calcolo gradi 

    return tempC;
}


void setup() {

  Serial.begin(115200);
  Wire.begin(); 

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

  switch (mode) {

    case 1: // current logger con o senza temp

      bool ch1_ready = !digitalRead(ch1_alert_pin);  // acquisisce lo stato dell'ina1 (ready= low)
      bool ch2_ready = !digitalRead(ch2_alert_pin);  // acquisisce lo stato dell'ina2 (ready= low)

      if(ch1_ready) {    // se sono pronti i dati del ch1

        float busV1 = ch1.readBusVoltage();     // V
        float shuntV1 = ch1.readShuntVoltage(); // mV (caduta sullo shunt)
        float current1 = ch1.readCurrent();     // mA
        float power1 = ch1.readPower();         // mW
        float energy1 = ch1.readEnergy();       // mWh
        //float charge1 = ch1.readCharge();     // C
        float dieTemp1 = ch1.readDieTemp();     // °C

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
        Serial.print(dieTemp1);
      }
  
      if(ch2_ready) {    // se sono pronti i dati del ch2
      
        float busV2 = ch2.readBusVoltage();
        float shuntV2 = ch2.readShuntVoltage();
        float current2 = ch2.readCurrent();
        float power2 = ch2.readPower();
        float energy2 = ch2.readEnergy();
        //float charge2 = ch2.readCharge();
        float dieTemp2 = ch2.readDieTemp();

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
        Serial.print(",");
        Serial.print(dieTemp2);
      }

      if(T_LOG && ch1_ready && ch2_ready) {      //  se il log ntc è abilitato e i sensori sono pronti aggiunge alla riga dati le temperature, altrimenti non stampare nulla altrimenti crea una riga malformata con temperature non correlate

        float temp1 = ntcToTemperature(analogRead(NTC1_PIN));
        float temp2 = ntcToTemperature(analogRead(NTC2_PIN));
        float temp3 = ntcToTemperature(analogRead(NTC3_PIN));
        float temp4 = ntcToTemperature(analogRead(NTC4_PIN));

        Serial.print(",");
        Serial.print(temp1);
        Serial.print(",");
        Serial.print(temp2);
        Serial.print(",");
        Serial.print(temp3);
        Serial.print(",");
        Serial.print(temp4);
      }

      Serial.println(); // fine riga dati
      break;

    case 2: // solo temp logger
    
      float temp1 = ntcToTemperature(analogRead(NTC1_PIN));
      float temp2 = ntcToTemperature(analogRead(NTC2_PIN));
      float temp3 = ntcToTemperature(analogRead(NTC3_PIN));
      float temp4 = ntcToTemperature(analogRead(NTC4_PIN));

      Serial.print(temp1);
      Serial.print(",");
      Serial.print(temp2);
      Serial.print(",");
      Serial.print(temp3);
      Serial.print(",");
      Serial.print(temp4);
      Serial.println();
      break;
  }
}