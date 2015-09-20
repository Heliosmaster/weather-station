#include <RF12.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include "DHT.h"

#define DHTPIN 3     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)

Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
DHT dht(DHTPIN, DHTTYPE);


/// Utility class to fill a buffer with string data.
class PacketBuffer : public Print {
 public:
  PacketBuffer () : fill (0) {}

  const byte* buffer() { return buf; }
  byte length() { return fill; }
  void reset() { fill = 0; }
  virtual size_t write(uint8_t ch) {
    if (fill < sizeof buf) {
      buf[fill++] = ch;
      return 1;
    }
    return 0;
  }

 private:
  byte fill, buf[RF12_MAXDATA];
};

class MilliTimer {
  word next;
  byte armed;

 public:
  MilliTimer () : armed (0) {}
  byte poll(word ms =0);
  word remaining() const;
  byte idle() const { return !armed; }
  void set(word ms);
};

byte MilliTimer::poll(word ms) {
  byte ready = 0;
  if (armed) {
    word remain = next - millis();
    if (remain <= 60000)
      return 0;
    ready = -remain;
  }
  set(ms);
  return ready;
}

word MilliTimer::remaining() const {
  word remain = armed ? next - millis() : 0;
  return remain <= 60000 ? remain : 0;
}

void MilliTimer::set(word ms) {
  armed = ms != 0;
  if (armed)
    next = millis() + ms - 1;
}

byte myId;              // remember my own node ID
byte needToSend;        // set when we want to send
word counter;           // incremented each second
MilliTimer sendTimer;   // used to send once a second
PacketBuffer payload;   // temp buffer to send out

LiquidCrystal lcd(7, 8, 9, 4, 5, 6);

void setup () {
  lcd.begin(16, 2);
  Serial.begin(57600);
  myId = rf12_config();
  if(!bmp.begin())
    {
      /* There was a problem detecting the BMP085 ... check your connections */
      Serial.print("Ooops, no BMP085 detected ... Check your wiring or I2C ADDR!");
      while(1);
    }

  dht.begin();
}

void loop () {

  if (rf12_recvDone() && rf12_crc == 0) {
    // a packet has been received
    for (byte i = 0; i < rf12_len; ++i)
      Serial.print((char) rf12_data[i]);
    Serial.println();

    lcd.setCursor(0,0);
    lcd.print("T: ");
    for (int i = 0; i < 2 ; i++){
      lcd.print((char)rf12_data[i]);
    }
    lcd.print(" | ");
    //lcd.print(" C");

    lcd.setCursor(0,1);
    lcd.print("H: ");
    for (int i = 0; i < 2 ; i++){
      lcd.print((char)rf12_data[6+i]);
    }
    lcd.print(" | ");
    //lcd.print(" %");
  }

  // we intend to send once a second
  if (sendTimer.poll(1000)) {

    sensors_event_t event;
    bmp.getEvent(&event);

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t) ) {
      Serial.println("DHT error");
      return;
    }
    lcd.setCursor(8,0);
    lcd.print(String(t,0));
    lcd.print(" | ");
    //lcd.print(" C |");

    lcd.setCursor(8,1);
    lcd.print(String(h,0));
    lcd.print(" |");
    //lcd.print(" C");

    if (event.pressure)
      {
        float temperature;
        bmp.getTemperature(&temperature);
        lcd.setCursor(13,0);
        lcd.print(String(temperature,0));
        lcd.setCursor(12,1);
        lcd.print(String(event.pressure,0));
      }
    else
      {
        Serial.println("BMP error");
      }

    needToSend = 0;
    ++counter;
    //Serial.print(" SEND ");
    //Serial.println(counter);
  }

  // can only send when the RF12 driver allows us to
  if (needToSend && rf12_canSend()) {
    needToSend = 0;
    // fill the packet buffer with text to send
    payload.print("myId = ");
    payload.print(myId, DEC);
    payload.print(", counter = ");
    payload.print(counter);
    payload.print(", millis = ");
    payload.print(millis());
    // send out the packet
    rf12_sendStart(0, payload.buffer(), payload.length());
    payload.reset();
  }
}
