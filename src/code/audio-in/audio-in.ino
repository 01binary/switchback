
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1305.h>

const int OLED_CS = 10;
const int OLED_DC = 8;
const int OLED_RESET = 9;
const int AUDIO_LEFT = A0;
const int AUDIO_RIGHT = A1;

const int MIN = 0;
const int MAX = 1024;
const int BIAS = 512;
const int THRESHOLD = -48;
const int WIDTH = 128;
const int ALIAS = 2;
const int SAMPLES = WIDTH * ALIAS;
const int HEIGHT = 32;
const int HALF = HEIGHT / 2;
const int AVG_COUNT = 32;
const double AVG_MUL = 0.1;

Adafruit_SSD1305 display(WIDTH, HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS, 7000000UL);

short aud[SAMPLES] = {0};
int sample = 0;

int minCur = MAX;
short minAvg[AVG_COUNT] = {0};
int minSample = 0;

int maxCur = MIN;
short maxAvg[AVG_COUNT] = {0};
int maxSample = 0;

void setup() {
  pinMode(AUDIO_LEFT, INPUT);
  pinMode(AUDIO_RIGHT, INPUT);

  initDisplay();
}

void loop() {
  int value = analogRead(AUDIO_LEFT) - BIAS;
 
  aud[sample++] = value;

  averageRange(value, &minCur, minAvg, &minSample, true);
  averageRange(value, &maxCur, maxAvg, &maxSample, false);
 
  if (sample >= SAMPLES)
  {
    sample = 0;

    tuneRange();
 
    draw();
  }
}

int averageRange(int value, int* current, short* samples, int* sample, bool minOrMax)
{
  if ((minOrMax && value < *current) || (!minOrMax && value > *current))
    *current = value;

  samples[*sample] = *current;
  *sample = *sample + 1;

  if (*sample >= AVG_COUNT)
  {
    int sum = 0;
   
    for (int n = 0; n < AVG_COUNT; ++n)
    {
      sum += samples[n];
    }

    *current = sum / AVG_COUNT;
    *sample = 0;
  }
}

void tuneRange()
{
  int localMin = MAX;
  int localMax = MIN;
 
  for (int n = 0; n < SAMPLES; ++n)
  {
    if (aud[n] < localMin)
      localMin = aud[n];
    else if (aud[n] > localMax)
      localMax = aud[n];
  }

  if (localMin > minCur)
  {
    minCur += ceil((localMin - minCur) * AVG_MUL);
  }

  if (localMax < maxCur)
  {
    maxCur -= ceil((maxCur - localMax) * AVG_MUL);
  }
}

void draw()
{
  delay(50);
  display.clearDisplay();

  int lastx = 0;
  int lasty = mapSample(aud[0]);
  int smoothy = lasty;
 
  for (int x = 0; x < WIDTH; ++x)
  {
    int sum = 0;
   
    for (int oversample = 0; oversample < ALIAS; ++oversample)
    {
      sum += aud[x];
    }

    int y = (mapSample(sum / ALIAS) + lasty + smoothy) / 3;
   
    display.drawLine(lastx, lasty, x, y, WHITE);

    smoothy = lasty;
    lasty = y;
    lastx = x;
  }

  display.display();
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
}
