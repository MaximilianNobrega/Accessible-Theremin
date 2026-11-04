// ===================== Ultrasonic Theremin — Efficient, Snapped, Half-Level =====================
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
#include <string.h>  // for strcmp, strcpy

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------ Synth ------------------
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> osc(SIN2048_DATA);
float frequency   = 200.0f;  // current glide frequency
float snappedFreq = 440.0f;  // target freq for current MIDI note

// ------------------ Ultrasonic pins ------------------
const int trigPin  = 11;
const int echoPin1 = 8;   // pitch sensor
const int echoPin2 = 12;  // volume sensor

// Event timer for alternating sensors
EventDelay pingTimer;

// Play zone
const float PITCH_MIN_CM  = 5.0f;
const float PITCH_MAX_CM  = 55.0f;
const float VOLUME_MIN_CM = 5.0f;
const float VOLUME_MAX_CM = 55.0f;

// Volume / envelope
float  amp_f  = 0.0f;  // smoothed amplitude [0,1]
int8_t amp_q7 = 0;     // quantized amplitude [0,127]

// Smoothed distances
float d1_smooth = NAN;
float d2_smooth = NAN;

// ---------------- Note / MIDI helpers ----------------

// MIDI range roughly covering 60–700 Hz
const int LOW_MIDI  = 34; // ~A#1 (≈58 Hz)
const int HIGH_MIDI = 77; // ~F5  (≈698 Hz)

const char* NOTE_NAMES[12] = {
  "C", "C#", "D", "D#", "E", "F",
  "F#", "G", "G#", "A", "A#", "B"
};

int currentMidi = 69; // A4
unsigned long lastNoteChangeMs = 0;
const unsigned long MIN_NOTE_HOLD_MS = 120; // ms: hysteresis for note changes

// Converts a MIDI note number to "A4", "C#5", etc.
void midiToNoteName(int midi, char* buf) {
  if (midi < 0)   midi = 0;
  if (midi > 127) midi = 127;

  int noteIndex = midi % 12;
  int octave    = midi / 12 - 1;

  const char* name = NOTE_NAMES[noteIndex];

  int i = 0;
  buf[i++] = name[0];
  if (name[1] != '\0') {   // sharp
    buf[i++] = name[1];
  }

  if (octave < 0) {
    buf[i++] = '-';
    octave   = -octave;
  }

  buf[i++] = '0' + (octave % 10);
  buf[i]   = '\0';
}

// LCD update timing + change detection
unsigned long lastLcdMs = 0;
const unsigned long LCD_UPDATE_INTERVAL = 1000; // 1 second

static char lastShownNote[6] = ""; // last note string shown
char        noteBuf[6];           // current note string

// ---------------- Helper: one ultrasonic measurement ----------------
float measureOnce(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(3);
  digitalWrite(trig, HIGH);
  delayMicroseconds(15);
  digitalWrite(trig, LOW);

  unsigned long dur = pulseIn(echo, HIGH, 10000UL); // 10 ms timeout
  if (dur == 0) return NAN;
  return dur * 0.0343f * 0.5f;  // cm (speed of sound)
}

// Clamp + map helper
inline float fmap(float v, float a, float b, float c, float d) {
  if (v < a) v = a;
  if (v > b) v = b;
  float t = (v - a) / (b - a);
  return c + (d - c) * t;
}

// ---------------- Setup ----------------
void setup() {
  pinMode(trigPin,  OUTPUT);
  pinMode(echoPin1, INPUT);
  pinMode(echoPin2, INPUT);
  digitalWrite(trigPin, LOW);

  osc.setFreq(frequency);
  pingTimer.set(5);  // control ticks between pings (fast alternating)
  pingTimer.start();

  Wire.setClock(400000); // Fast I²C mode
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Note:");

  startMozzi();
}

// ---------------- Control Loop ----------------
void updateControl() {
  static bool  readFirst = true;
  static float d1_raw = NAN, d2_raw = NAN;

  // Alternating ultrasonic reads to avoid crosstalk
  if (pingTimer.ready()) {
    if (readFirst) d1_raw = measureOnce(trigPin, echoPin1);
    else           d2_raw = measureOnce(trigPin, echoPin2);
    readFirst = !readFirst;
    pingTimer.start();

    // --- Smooth the distances to reduce jitter ---
    const float distAlpha = 0.2f;  // smaller = smoother

    if (!isnan(d1_raw)) {
      if (isnan(d1_smooth)) d1_smooth = d1_raw; // first time
      d1_smooth += (d1_raw - d1_smooth) * distAlpha;
    }

    if (!isnan(d2_raw)) {
      if (isnan(d2_smooth)) d2_smooth = d2_raw;
      d2_smooth += (d2_raw - d2_smooth) * distAlpha;
    }
  }

  // Use smoothed distances for logic
  bool pitch_ok  = (!isnan(d1_smooth) && d1_smooth >= PITCH_MIN_CM  && d1_smooth <= PITCH_MAX_CM);
  bool volume_ok = (!isnan(d2_smooth) && d2_smooth >= VOLUME_MIN_CM && d2_smooth <= VOLUME_MAX_CM);

  if (pitch_ok && volume_ok) {
    // ---------------- Distance → MIDI note (no log) ----------------
    float t = (d1_smooth - PITCH_MIN_CM) / (PITCH_MAX_CM - PITCH_MIN_CM);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    int midiRange    = HIGH_MIDI - LOW_MIDI;
    int midiCandidate = LOW_MIDI + (int)(t * midiRange + 0.5f);

    unsigned long nowMs = millis();

    // Hysteresis: only change note occasionally
    if (midiCandidate != currentMidi &&
        (nowMs - lastNoteChangeMs) > MIN_NOTE_HOLD_MS) {
      currentMidi = midiCandidate;
      lastNoteChangeMs = nowMs;

      // Recompute snappedFreq ONLY when note changes
      snappedFreq = 440.0f * pow(2.0f, (currentMidi - 69) / 12.0f);
    }

    // Glide toward snappedFrequency
    const float PITCH_GLIDE = 0.05f;
    frequency += (snappedFreq - frequency) * PITCH_GLIDE;
    osc.setFreq(frequency);

    // ---------------- Volume (volume hand) ----------------
    float vol = fmap(d2_smooth, VOLUME_MIN_CM, VOLUME_MAX_CM, 1.0f, 0.0f);
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;

    // Smoothing on volume
    float k = (vol < amp_f) ? 0.05f : 0.15f; // slower down, medium up
    amp_f += (vol - amp_f) * k;

    if (amp_f < 0.004f) amp_f = 0.0f;
    if (amp_f > 1.0f)   amp_f = 1.0f;

    int a = int(amp_f * 127.0f + 0.5f);
    if (a < 0)   a = 0;
    if (a > 127) a = 127;
    amp_q7 = (int8_t)a;

  } else {
    // ---------------- Fade-out when either sensor leaves play zone ----------------
    float fadeRate = 0.22f;
    amp_f += (0.0f - amp_f) * fadeRate;

    if (amp_f < 0.004f) amp_f = 0.0f;

    int a = int(amp_f * 127.0f + 0.5f);
    if (a < 0)   a = 0;
    if (a > 127) a = 127;
    amp_q7 = (int8_t)a;
  }

  // ---------------- LCD Update (NOTE NAME) ----------------
  unsigned long now = millis();
  if (now - lastLcdMs >= LCD_UPDATE_INTERVAL) {
    lastLcdMs = now;

    midiToNoteName(currentMidi, noteBuf);

    if (strcmp(noteBuf, lastShownNote) != 0) {
      strcpy(lastShownNote, noteBuf);

      lcd.setCursor(0, 0);
      lcd.print("Note:");

      lcd.setCursor(6, 0); // after "Note: "
      lcd.print("    ");   // clear old text
      lcd.setCursor(6, 0);
      lcd.print(noteBuf);
    }
  }
}

// ---------------- Audio Loop ----------------
AudioOutput updateAudio() {
  // TEST A: reduce overall output level by an extra right shift (>> 8 instead of >> 7)
  int q = ((int)osc.next() * amp_q7) >> 8;  // half the amplitude → less clipping
  return MonoOutput::from8Bit((uint8_t)(q + 128));
}

// ---------------- Main loop ----------------
void loop() {
  audioHook();
}
