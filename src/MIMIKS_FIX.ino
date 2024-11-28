#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pitches.h" 
#include "RTClib.h"

// Konfigurasi OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define RELAY_PIN    13  
#define MOISTURE_PIN 36  
const int buzzer = 18;   
#define ADC_MIN 0       
#define ADC_MAX 4095    
#define TEMP_MIN 0     
#define TEMP_MAX 35     
#define POWER_WATER_PIN  17 
#define SIGNAL_WATER_PIN 39 

int valueWater = 0; 
RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

unsigned long previousMillis = 0; // Untuk menyimpan waktu terakhir pembaruan
const long intervalTimeDisplay = 8000; // Interval untuk menampilkan waktu (8 detik)
const long intervalMoistureRelayDisplay = 5000; // Interval untuk menampilkan kelembaban dan relay (5 detik)
bool showTime = true; // Flag untuk menampilkan waktu
bool showMoistureRelay = false; // Flag untuk menampilkan kelembaban dan relay
bool lowWaterLevel = false; // Awalnya false

int melody[] = {
  // Game over sound
  NOTE_C5, NOTE_G4, NOTE_E4,
  NOTE_A4, NOTE_B4, NOTE_A4, NOTE_GS4, NOTE_AS4, NOTE_GS4,
  NOTE_G4, NOTE_D4, NOTE_E4
};

// Durasi masing-masing nada (ms)
int durations[] = {
  //game over sound
  4, 4, 4,
  8, 8, 8, 8, 8, 8,
  8, 8, 2
};

void setup() {
  Serial.begin(9600);
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_BUILTIN, LOW);
  
  pinMode(POWER_WATER_PIN, OUTPUT);   
  digitalWrite(POWER_WATER_PIN, LOW); 
  
  analogSetAttenuation(ADC_11db);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("OLED tidak ditemukan!"));
    for (;;);
  }

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
  while (1);
  }

  rtc.adjust(DateTime(__DATE__, __TIME__));

  DateTime now = rtc.now();
 
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(75,0);
  display.println(now.second(), DEC);
 
  display.setTextSize(2);
  display.setCursor(25,0);
  display.println(":");
 
  display.setTextSize(2);
  display.setCursor(65,0);
  display.println(":");
 
  display.setTextSize(2);
  display.setCursor(40,0);
  display.println(now.minute(), DEC);
 
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println(now.hour(), DEC);
 
  display.setTextSize(2);
  display.setCursor(0,20);
  display.println(now.day(), DEC);
    
  display.clearDisplay();
  display.display();
  
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println(F("ePlants."));
  display.display();
  delay(5000);
}

void playGameOverMelody() {
    int size = sizeof(durations) / sizeof(int);

  for (int note = 0; note < size; note++) {
    //to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int duration = 1000 / durations[note];
    tone(buzzer, melody[note], duration);

    //to distinguish the notes, set a minimum time between them.
    //the note's duration + 30% seems to work well:
    int pauseBetweenNotes = duration * 1.30;
    delay(pauseBetweenNotes);
    
    //stop the tone playing:
    noTone(buzzer);
  }
}


void checkWaterLevel() {
    // Membaca nilai kelembaban dari sensor
    int value = analogRead(MOISTURE_PIN); 
    Serial.print("ADC Value: ");
    Serial.println(value); 

    // Menghidupkan daya ke sensor air
    digitalWrite(POWER_WATER_PIN, HIGH);  
    delay(10); // Delay untuk stabilisasi pembacaan
    valueWater = analogRead(SIGNAL_WATER_PIN); 
    digitalWrite(POWER_WATER_PIN, LOW);   

    float temperature = (value - ADC_MIN) * (TEMP_MAX - TEMP_MIN) / (ADC_MAX - ADC_MIN) + TEMP_MIN;
  
    Serial.print("The water sensor value: ");
    Serial.println(valueWater);
  
    // Mendapatkan waktu saat ini
    DateTime now = rtc.now();
  
    // Peringatan jika air habis
    if (valueWater < 400) {
        lowWaterLevel = true;
        // Jika air habis, tampilkan pesan di Serial dan matikan relay
        Serial.println("Ups airnya habis nih.. saatnya isi ulang :(");
        digitalWrite(RELAY_PIN, LOW); // Matikan relay
        digitalWrite(LED_BUILTIN, LOW); // Matikan LED
        playGameOverMelody(); // Aktifkan buzzer

        display.clearDisplay();
        display.setCursor(0, 0);
        display.setTextSize(1);
        display.println(F("Ups airnya habis nih.."));
        display.setCursor(0, 12);
        display.println(F("saatnya isi ulang :)."));
        display.display();
    } else {
      lowWaterLevel = false;
      // Mengatur status relay berdasarkan waktu
      int currentHour = now.hour();
      int currentMinute = now.minute();

      // Matikan relay jika waktu di luar rentang 06:00 hingga 17:30
      if (currentHour > 17 || currentHour < 6 || (currentHour == 17 && currentMinute >= 30)) {
        digitalWrite(RELAY_PIN, LOW); // Matikan relay
        Serial.println("Relay dimatikan (di luar jam operasi)");
      } else {
        // Nyalakan relay hanya jika kondisi kelembaban memenuhi syarat
        if (temperature > 22) {
          Serial.println("The soil is DRY => Activating pump");
          digitalWrite(RELAY_PIN, HIGH); // Nyalakan relay
          digitalWrite(LED_BUILTIN, HIGH);
        } else {
          Serial.println("The soil is WET => Deactivating pump");
          digitalWrite(RELAY_PIN, LOW); // Matikan relay
          digitalWrite(LED_BUILTIN, LOW);
        }
      }
      noTone(buzzer);
      delay(1000);  
    }
}

void loop() {
  // Membaca nilai kelembaban dari sensor
    int valueMoisture = analogRead(MOISTURE_PIN); 

    unsigned long currentMillis = millis();

    // Cek level air dan perbarui tampilan kelembaban/relay
    checkWaterLevel();

    if (lowWaterLevel) {
        // Prioritaskan tampilan air rendah
        display.clearDisplay();
        display.setCursor(0, 0);
        display.setTextSize(1);
        display.println(F("WARNING!"));
        display.setCursor(0, 12);
        display.println(F("Ups airnya abis nih!"));
        display.setCursor(0, 24);
        display.println(F("saatnya isi ulang ^_^"));
        display.display();
        delay(500); // Atur waktu refresh
        return; // Jangan lanjutkan ke siklus tampilan lainnya
    }

    // Mengatur tampilan
    if (showTime) {
        if (currentMillis - previousMillis >= intervalTimeDisplay) {
            previousMillis = currentMillis; // Simpan waktu terakhir pembaruan
            showTime = false; // Ganti ke tampilan kelembaban/relay
            showMoistureRelay = true; // Aktifkan tampilan kelembaban/relay
        }
    } else if (showMoistureRelay) {
        if (currentMillis - previousMillis >= intervalMoistureRelayDisplay) {
            previousMillis = currentMillis; // Simpan waktu terakhir pembaruan
            showMoistureRelay = false; // Ganti ke tampilan waktu
            showTime = true; // Aktifkan tampilan waktu
        }
    }

    // Menampilkan informasi sesuai dengan flag
    display.clearDisplay();
    display.setCursor(0, 0);

    if (showTime) {
        displayTime(); // Tampilkan waktu
    } else if (showMoistureRelay) {
      DateTime now = rtc.now();
      int currentHour = now.hour();
      int currentMinute = now.minute();
      // Tampilkan status kelembaban dan relay
      float temperature = (valueMoisture - ADC_MIN) * (TEMP_MAX - TEMP_MIN) / (ADC_MAX - ADC_MIN) + TEMP_MIN;
      display.print("== ePlants. ==");
      display.setCursor(0, 7);
      display.print(F("Kelembaban: "));
      display.print(temperature);
      display.println(" C");

      // Menampilkan status kelembaban
      if (temperature > 22) { 
        display.setCursor(0, 16);
        display.print(F("Status: HAUS T_T"));
      } else {
        display.setCursor(0, 15);
        display.print(F("Status: KENYANG ^_^"));
      }

      // Menampilkan status relay
      display.setCursor(0, 24);
      if (currentHour > 17 || currentHour < 6 || (currentHour == 17 && currentMinute >= 30)) {
        display.println(F("Water Pump: MATI")); // Menampilkan status relay
      } else {
        display.println(F("Water Pump: HIDUP")); // Menampilkan status relay
      }
    }

    display.display(); // Perbarui tampilan OLED
    delay(100); // Atur sesuai kebutuhan
}

void displayTime() {
    DateTime now = rtc.now();
    display.setCursor(0, 0);
    display.setTextSize(1.5);
    display.print("== ePlants. ==");
    display.setCursor(0, 11);
    display.setTextSize(1);
    display.print(F("Waktu: "));
    display.print(now.hour());
    display.print(F(":"));
    display.print(now.minute());
    display.print(F(":"));
    display.print(now.second());
    display.println();
    display.setTextSize(1);
    display.setCursor(0,22);
    display.print(daysOfTheWeek[now.dayOfTheWeek()]);
}