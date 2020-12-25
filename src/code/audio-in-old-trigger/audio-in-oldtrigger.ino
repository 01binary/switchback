
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1305.h>
#include <splash.h>

const int OLED_CS = 10;
const int OLED_DC = 8;
const int OLED_RESET = 9;
const int AUDIO_LEFT = A0;
const int AUDIO_RIGHT = A1;

const int WIDTH = 128;
const int HEIGHT = 32;
const int HALF = HEIGHT / 2;

const int MIN = -64;
const int MAX = 64;
const int BIAS = 512;
const int THRESHOLD = -3;

const int BUFFER = WIDTH * 2;
const int AVG = 32;
const double AVG_SCALE = 0.1;

const int TRIGGER_ARM_THRESHOLD = -43;
const int TRIGGER_HYSTERESIS = 43;
const int TRIGGER_THRESHOLD = TRIGGER_ARM_THRESHOLD + TRIGGER_HYSTERESIS;

Adafruit_SSD1305 display(WIDTH, HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS, 7000000UL);

signed char aud[BUFFER] = {0};
signed char last[WIDTH] = {0};
int sample = 0;

int minCur = MAX;
signed char minAvg[AVG] = {0};
int minSample = 0;

int maxCur = MIN;
signed char maxAvg[AVG] = {0};
int maxSample = 0;

bool triggerArmed = false;
bool triggered = false;
bool recording = false;
bool paused = false;

char debug[128] = {0};

void setup() {
  pinMode(AUDIO_LEFT, INPUT);
  pinMode(AUDIO_RIGHT, INPUT);

  initDisplay();
}

void loop() {
  int value = sampleAudio();
  
  if (!shouldRecord(value))
    return;

  aud[sample++] = value;

  averageMinMax(value);
  
  if (sample == BUFFER)
  {
    adjustMinMaxFromBuffer();
    draw();
    resetBuffer();
  }
}

inline int sampleAudio()
{
  return constrain(analogRead(AUDIO_LEFT) - BIAS, MIN, MAX);
}

inline bool shouldRecord(int value)
{
  if (!recording)
  {
    if (!evalTrigger(value))
      return false;

    recording = true;
  }

  return recording;
}

void resetBuffer()
{
  sample = 0;
  recording = false;
  memcpy(last, aud, WIDTH);
}

bool evalTrigger(int value)
{
  if (triggerArmed)
  {
    if (triggered)
    {
      triggered = false;
    }
    else if (value > TRIGGER_THRESHOLD)
    {
      triggered = true;
      triggerArmed = false;

      return true;
    }
  }
  else if (value > TRIGGER_ARM_THRESHOLD)
  {
    triggerArmed = true;
  }

  return false;
}

inline void averageMinMax(int value)
{
  averageRange(value, &minCur, minAvg, &minSample, true);
  averageRange(value, &maxCur, maxAvg, &maxSample, false);
}

int averageRange(int value, int* current, signed char* samples, int* sample, bool minOrMax)
{
  if ((minOrMax && value < *current) || (!minOrMax && value > *current))
    *current = value;

  samples[*sample] = *current;
  *sample = *sample + 1;

  if (*sample >= AVG)
  {
    int sum = 0;
    
    for (int n = 0; n < AVG; ++n)
    {
      sum += samples[n];
    }

    *current = sum / AVG;
    *sample = 0;
  }
}

void adjustMinMaxFromBuffer()
{
  int localMin = MAX;
  int localMax = MIN;
  
  for (int n = 0; n < BUFFER; ++n)
  {
    if (aud[n] < localMin)
      localMin = aud[n];
    else if (aud[n] > localMax)
      localMax = aud[n];
  }

  if (localMin > minCur)
  {
    minCur += ceil((localMin - minCur) * AVG_SCALE);
  }

  if (localMax < maxCur)
  {
    maxCur -= ceil((maxCur - localMax) * AVG_SCALE);
  }
}

void draw()
{ 
  if (minCur < THRESHOLD)
  {
    display.clearDisplay();

    int lastx = 0;
    int lasty = mapSample(aud[0]);
    int lasty2 = mapSample(aud[1]);
    int lasty3 = mapSample(aud[2]);

    int lastfy = mapSample(last[0]);
    int lastfy2 = mapSample(last[1]);
    int lastfy3 = mapSample(last[2]);

    for (int x = 0; x < WIDTH; ++x)
    {
      int lastFrameY = (mapSample(last[x]) + lastfy + lastfy2 + lastfy3) / 4;
      int curFrameY = (mapSample(aud[x]) + lasty + lasty2 + lastfy3) / 4;
      int y = 0;

      if (x > 3)
      {
        y = (lastFrameY + curFrameY) / 2;
      }
      else
      {
        y = curFrameY;
      }

      display.drawLine(lastx, lasty, x, y, WHITE);

      lastx = x;

      lasty3 = lasty2;
      lasty2 = lasty;
      lasty = y;

      lastfy3 = lastfy2;
      lastfy2 = lastfy;
      lastfy = lastFrameY;
    }

    paused = false;

    display.display();
  }
  else if (!paused)
  {
    drawSplash();
    paused = true;
  }
}

inline int mapSample(int value)
{
  return max(map(value, minCur, maxCur, HEIGHT - 1, 0), 0);
}

void initDisplay()
{
  Serial.begin(9600);

  if (!display.begin(0x3C) ) {
     Serial.println("Unable to initialize OLED");
     while (1) yield();
  }

  display.display();
  delay(1000);
  display.clearDisplay();

  Serial.println("Initialized");
}

void drawSplash()
{
  display.clearDisplay();
    
  display.drawBitmap(
    (WIDTH - splash2_width) / 2,
    (HEIGHT - splash2_height) / 2,
    splash2_data,
    splash2_width,
    splash2_height,
    1);
    
  display.display();
}
