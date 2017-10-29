#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <TeensyThreads.h>

//teensy 3.5 with SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11  // not actually used
#define SDCARD_SCK_PIN   13  // not actually used

//teensy audio engine
AudioPlaySdWav           playSdWav1;
AudioMixer4              mixer2;
AudioMixer4              mixer1;
AudioOutputAnalogStereo  dacs1;
AudioConnection          patchCord1(playSdWav1, 0, mixer1, 0);
AudioConnection          patchCord2(playSdWav1, 1, mixer2, 0);
AudioConnection          patchCord3(mixer2, 0, dacs1, 1);
AudioConnection          patchCord4(mixer1, 0, dacs1, 0);

//input processing
//key-in
int key_pins[5] = {6,7,8,9,10};
int mavg(int val);
bool aging(bool val);

//player thread
//thread
void sound_player_thread();
//thread i/o
volatile bool is_play_start_req = false;
volatile bool is_play_stop_req = false;
volatile int note_now = 0;
Threads::Mutex x_snd_player;

////setup

void setup() {

  //debug
  Serial.begin(9600);

  //sd card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

  //key switch pin settings
  for (int i = 0; i < 5; i++) {
    pinMode (key_pins[i], INPUT_PULLUP);
  }

  //audio
  AudioMemory(20);
  mixer1.gain(0,1);
  mixer1.gain(1,0);
  mixer1.gain(2,0);
  mixer1.gain(3,0);
  mixer2.gain(0,1);
  mixer2.gain(1,0);
  mixer2.gain(2,0);
  mixer2.gain(3,0);

  //start audio player thread
  threads.addThread(sound_player_thread);
}

////loop + ()

void loop() {
  static bool is_blowing = false;
  static int key_now = 0;
  static int key_now_prev = 0;
  static bool is_playing = false;

  //check keys
  for (int i = 0; i < 5; i++) {
    if (digitalRead(key_pins[i]) == LOW) { // key pressed
      key_now = i;
      break;
    }
    key_now = -1; // no key
  }

  //check blowin sensor
  int val = mavg(analogRead(0));
  // Serial.print("blow_sense:");
  // Serial.print(val);

  // static bool is_blowing_prev = false;
  is_blowing = aging(val > 50);
  // if (is_blowing_prev != is_blowing) {
  //  Serial.print("is_blowing:");
  //  Serial.println(is_blowing);
  // }
  // is_blowing_prev = is_blowing;

  // Serial.print("key:");
  // Serial.print(key_now);
  // Serial.print(",blowin:");
  // Serial.println(is_blowing);

  //check volume knob
  double knob = (double)analogRead(1)/1023;
  mixer1.gain(0,knob);
  mixer2.gain(0,knob);

  //emit sound play request signals
  if (is_blowing && key_now != -1) {
    if (is_playing == false) {
      is_playing = true;

      // (new) start

      Threads::Scope m(x_snd_player);
      note_now = key_now;
      is_play_start_req = true;
      Serial.print("playstart,note:");
      Serial.println(note_now);
    } else if (key_now != key_now_prev) {

      // re-start

      Threads::Scope m(x_snd_player);
      note_now = key_now;
      is_play_start_req = true;
      Serial.println("play re-start,note:");
      Serial.println(note_now);
    }
  } else {

    // stop

    if (is_playing == true) {
      is_playing = false;
      //
      Threads::Scope m(x_snd_player);
      is_play_stop_req = true;
      Serial.println("play stop.");
    }
  }

  //
  key_now_prev = key_now;

  //
  delay(5);
}

void sound_player_thread()
{
  //sound file list
  char files[5][7] = {"01.WAV", "02.WAV", "03.WAV", "04.WAV", "05.WAV"};

  //thread loop
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  while(1) {
    {
      Threads::Scope m(x_snd_player);

      if (is_play_start_req == true) {
        is_play_start_req = false;

        // if (playSdWav1.isPlaying() == true) {
        //   playSdWav1.stop(); //needed?
        // }
        playSdWav1.play(files[note_now]);
        digitalWrite(13, HIGH);
      }

      if (is_play_stop_req == true) {
        is_play_stop_req = false;
        //
        if (playSdWav1.isPlaying() == true) {
          playSdWav1.stop();
        }
        digitalWrite(13, LOW);

      }
    }

    //
    threads.yield();
  }
}

int mavg(int val) {
  //parameter
  static const int mavg_buff_max = 10;
  //
  static int mavg_buff[mavg_buff_max] = {0, };
  static int mavg_idx = 0;

  //update set
  if (mavg_idx == mavg_buff_max) mavg_idx = 0;
  mavg_buff[mavg_idx] = val;
  mavg_idx++;

  //perform moving average
  int sum = 0;
  for (int i = 0; i < mavg_buff_max; i++) {
    sum = sum + mavg_buff[i];
  }
  return (sum / mavg_buff_max);
}

bool aging(bool val) {
  //parameters
  static const int age_min = 0;
  static const int age_max = 10;
  static const int age_inc = 2;

  static const int age_dec = 1;
  static const int age_thres = 5;
  //
  static int age = 0;

  if (val == true) {
    age = age + age_inc;
    if (age > age_max) age = age_max;
  } else {
    age = age - age_dec;
    if (age < age_min) age = age_min;
  }

  //
  return (age >= age_thres);
}
