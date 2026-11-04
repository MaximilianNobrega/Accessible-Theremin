// Audio out: D9 (Mozzi STANDARD PWM). Use passive speaker/amp via 220Ω + 10µF (+ toward D9).
// Ultrasonic sensors: trig D11 (shared), echo1 D8 (pitch), echo2 D12 (volume).

#define MOZZI_CONTROL_RATE 64
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <Mozzi.h>
#include <Oscil.h>
#include <EventDelay.h>
#include <tables/sin2048_int8.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------ Synth ------------------
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> osc(SIN2048_DATA);
float frequency = 200.0f;

// ------------------ Ultrasonic pins ------------------
const int trigPin  = 11;
const int echoPin1 = 8;
const int echoPin2 = 12;

// Event timer for alternating sensors
EventDelay pingTimer;

// Distance ranges
const float PITCH_MAX_CM  = 50.0f;
const float VOLUME_MAX_CM = 35.0f;

// Volume smoothing
float amp_f = 0.3f;
int8_t amp_q7 = 60;
float lastGoodAmp = 0.3f;

// LCD update timing + change detection
unsigned long lastLcdMs = 0;
const unsigned long LCD_UPDATE_INTERVAL = 1000; // 1 second
static float lastShownFreq = -9999.0f;
char freqBuf[8];

// ---------------- Helper ----------------
float measureOnce(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  unsigned long dur = pulseIn(echo, HIGH, 10000UL);
  if (dur == 0) return NAN;
  return dur * 0.0343f * 0.5f;
}

inline float fmap(float v, float a, float b, float c, float d) {
  if (v < a) v = a;
  if (v > b) v = b;
  float t = (v - a) / (b - a);
  return c + (d - c) * t;
}

// ---------------- Setup ----------------
void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin1, INPUT);
  pinMode(echoPin2, INPUT);
  digitalWrite(trigPin, LOW);

  osc.setFreq(frequency);
  pingTimer.set(5);
  pingTimer.start();

  Wire.setClock(400000); // Fast I²C mode
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Freq:");

  startMozzi();
}

// ---------------- Control Loop ----------------
void updateControl() {
  static bool readFirst = true;
  static float d1 = NAN, d2 = NAN;

  // Alternating ultrasonic reads
  if (pingTimer.ready()) {
    if (readFirst) d1 = measureOnce(trigPin, echoPin1);
    else           d2 = measureOnce(trigPin, echoPin2);
    readFirst = !readFirst;
    pingTimer.start();
  }

  bool pitch_ok  = (!isnan(d1) && d1 < PITCH_MAX_CM);
  bool volume_ok = (!isnan(d2) && d2 < VOLUME_MAX_CM);

  // Frequency glide
  if (pitch_ok) {
    float targetFreq = fmap(d1, 0, PITCH_MAX_CM, 60, 700);
    frequency += (targetFreq - frequency) * 0.12f;
    osc.setFreq(frequency);
  }

  // Volume smoothing
  float vol = volume_ok ? fmap(d2, 0, VOLUME_MAX_CM, 0, 1) : lastGoodAmp;
  float k = (vol < amp_f) ? 0.06f : 0.20f;
  amp_f += (vol - amp_f) * k;
  if (amp_f < 0.004f) amp_f = 0;
  int a = int(amp_f * 127.0f + 0.5f);
  if (a < 0) a = 0; if (a > 127) a = 127;
  amp_q7 = (int8_t)a;
  lastGoodAmp = amp_f;

  // ---------------- LCD Update ----------------
  unsigned long now = millis();
  if (now - lastLcdMs >= LCD_UPDATE_INTERVAL) {
    lastLcdMs = now;

    // Only update if visually different
    if (fabs(frequency - lastShownFreq) > 0.9f) {
      lastShownFreq = frequency;

      dtostrf(frequency, 5, 1, freqBuf); // Format number
      lcd.setCursor(6, 0);               // After "Freq: "
      lcd.print(freqBuf);
      lcd.print(" ");                    // Clear leftover digits
    }
  }
}

// ---------------- Audio Loop ----------------
AudioOutput updateAudio() {
  int q = ((int)osc.next() * amp_q7) >> 7;
  return MonoOutput::from8Bit((uint8_t)(q + 128));
}

// Main loop
void loop() {
  audioHook();
}
