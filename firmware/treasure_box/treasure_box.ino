/*
 * Liam's Treasure Box — firmware v8.5 / ATmega328P, Arduino Uno profile.
 * Production PCB wiring is retained when the removable MCU is programmed in an Uno.
 *
 * goBILDA Proton 2002-0180-0002: specified range 600–2400 us.
 * 2380 us CLOSED (about 2 degrees off the clockwise limit); 1056 us OPEN.
 * Upside-down mounting does not change these electrical endpoints.
 * A knock or ABCD opens the claw but NEVER disarms the alarm.
 * Armed access: red + green, no blue; battle countdown until the PIN disarms.
 * Disarmed access: anticipation, then reveal on actual lid opening.
 * Tap # to close. ABCD then hold # for 3 s to stage/confirm a new knock.
 * Hold * for 3 s to stage a 4–8 digit PIN; * confirms. Programming lasts 10 s.
 * Staged edits never touch EEPROM until confirmed. Existing v7 settings migrate.
 *
 * All input, programming, alarm, music, and LED feedback use cooperative ticks.
 * Normal moves block for 600 ms; initial closure is monitored for 1500 ms.
 * No servo position feedback: commanded closure is not measured closure.
 * Release serial: s=status (read only). DEBUG adds t=tone, c=reveal.
 */
#include <Servo.h>
#include "audio_driver.h"
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define BENCH_WIRING false
const bool DEBUG = false;
#if BENCH_WIRING
const byte ROW_PINS[4] = {2,3,4,5};
const byte COL_PINS[4] = {6,7,8,9};
const bool SLEEP_AVAILABLE = false;
#else
const byte ROW_PINS[4] = {2,4,5,6};
const byte COL_PINS[4] = {7,8,9,A4};
const bool SLEEP_AVAILABLE = true;
#endif
const byte WAKE_PIN=3, SENS_PWR=A5, SERVO_PIN=10, SPEAKER_PIN=11;
const byte RED_PIN=12, BLUE_PIN=13, GREEN_PIN=A0;
const byte PIEZO_PIN=A1, ARM_BTN=A2, LID_PIN=A3;
const byte CODE_LEN=4, MIN_PIN_LEN=4, MAX_PIN_LEN=8;
const char DEFAULT_PIN[]="1234", DEFAULT_BYPASS[]="ABCD";
const bool LID_CLOSED_IS_LOW=true;
const int LID_ANALOG_THRESHOLD=512;
const uint32_t LID_DEBOUNCE_MS=40, SENSOR_SETTLE_MS=150;
const int KNOCK_THRESHOLD=6, KNOCK_DEBOUNCE=90, PATTERN_TIMEOUT=1500;
const byte MAX_KNOCKS=10;
const float GAP_TOLERANCE=0.35;
const uint32_t MAX_PATTERN_MS=12000, KNOCK_QUIET_MS=350;
const uint32_t COUNTDOWN_MS=10000, PROGRAM_MS=10000, HOLD_MS=3000;
const uint32_t RELATCH_MS=2000, UNLOCK_GRACE_MS=10000, SLEEP_AFTER_MS=300000;
const uint32_t STARTUP_CLOSE_MS=1000, STARTUP_DRIVE_MS=1500, PIN_ENTRY_TIMEOUT_MS=5000;
const byte MAX_TRIES=3;
// Explicit pulses avoid angle/range ambiguity. OPEN is the lower-pulse direction.
// Proton spec: 600–2400 us, 0.098 degrees/us. Gripper assembly step 5 calls
// for the clockwise limit backed off ~2 degrees: 2400 - 20 = 2380 us.
// The old 2444-us close command exceeded this specific servo's maximum.
const int SERVO_CLOSED_US=2380, SERVO_OPEN_US=1056;
const int SERVO_MIN_US=600, SERVO_MAX_US=2400;
static_assert(SERVO_CLOSED_US>=SERVO_MIN_US && SERVO_CLOSED_US<=SERVO_MAX_US,
              "Closed pulse exceeds Proton servo range");
static_assert(SERVO_OPEN_US>=SERVO_MIN_US && SERVO_OPEN_US<=SERVO_MAX_US,
              "Open pulse exceeds Proton servo range");
const uint32_t SERVO_MOVE_MS=600;

// Legacy fields stay in place. Extended PINs use two separately committed slots.
const byte EE_MAGIC_ADDR=0, EE_MAGIC=0x47, EE_KNOCK_N=1, EE_GAPS=2;
const byte EE_PIN=20, EE_BYPASS=24, EE_ARMED=28;
const byte EE_PIN_SLOT0=32, EE_PIN_SLOT1=48, EE_PIN_COMMIT=0xA8;
const byte EE_KNOCK_SLOT0=64, EE_KNOCK_SLOT1=96, EE_KNOCK_COMMIT=0xA9;
const byte EE_THEME=120;
#include "music_themes.h"

const byte MUSIC_OFF=0, MUSIC_BUILD=1, MUSIC_REVEAL=2, MUSIC_BATTLE=3, MUSIC_ALARM=4, MUSIC_FAILURE=5, MUSIC_DISARM=6, MUSIC_THEME=7;
const byte ALARM_IDLE=0, ALARM_CALM=1, ALARM_CHALLENGE=2, ALARM_SOUNDING=3, ALARM_ACCESS=4;
const byte PROGRAM_NONE=0, PROGRAM_KNOCK=1, PROGRAM_PIN=2;

Servo latch;
char secretPin[MAX_PIN_LEN+1], bypassSeq[CODE_LEN+1];
byte pinLength=4;
int8_t activePinSlot=-1;
byte pinGeneration=0;
int8_t activeKnockSlot=-1;
byte knockGeneration=0;
int16_t secretGaps[MAX_KNOCKS-1]={400,200,200,600};
byte secretGapCount=4;
bool armed=false, latched=false, lidWasClosed=true;
bool startupPending=true, startupCloseTiming=false, inputsReady=false, inputsReleaseTiming=false;
bool startupDriving=false, startupDriveAttempted=false;
uint32_t startupCloseAt=0, startupDriveAt=0, inputsReleasedAt=0;
bool lidStableClosed=true, lidLastReading=true;
uint32_t lidChangedAt=0;
bool accessGranted=false, revealPending=false, openedSinceUnlock=false;
bool relatchTimerRunning=false;
uint32_t lidClosedAt=0, lastActivity=0;
byte alarmState=ALARM_IDLE;
uint32_t alarmAt=0, alarmSoundingAt=0;
char pinEntry[MAX_PIN_LEN+1];
byte pinEntryCount=0, wrongTries=0;
uint32_t pinLastDigitAt=0;
byte bypassIndex=0;
uint32_t bypassLastKeyAt=0;
bool bypassRecordEligible=false;

char stableKey=0, keyReading=0, heldKey=0;
uint32_t keyChangedAt=0, keyPressedAt=0;
bool keyHoldHandled=false, hashMayRecord=false, keyInputBlocked=false;
byte armStableState=HIGH, armLastReading=HIGH;
uint32_t armChangedAt=0;
bool armPressActive=false;
byte musicTheme=0;
bool themePicking=false;
uint32_t themePickerAt=0, themeFeedbackAt=0;
byte themeFlashes=0;
byte tiltLastLevel=HIGH;
volatile bool sleepWakeEvent=false;

byte musicMode=MUSIC_OFF, musicStep=0;
uint32_t musicNoteAt=0, musicNoteMs=0;
uint32_t heartbeatEvent=UINT32_MAX, heartbeatCycleAt=0;
uint16_t heartbeatPeriod=1000;
uint32_t lastNoiseAt=0, noiseQuietFor=0;
bool knockActive=false, knockOverflow=false, knockReady=false;
long knockGaps[MAX_KNOCKS-1];
byte knockGapCount=0;
uint32_t knockStartedAt=0, knockLastHit=0;

byte programMode=PROGRAM_NONE;
uint32_t programAt=0;
char candidatePin[MAX_PIN_LEN+1];
byte candidatePinCount=0;
int16_t candidateGaps[MAX_KNOCKS-1];
byte candidateGapCount=0;
bool recordStarted=false, recordComplete=false, recordInvalid=false, recordReady=false;
uint32_t recordLastHit=0;
byte feedbackMode=PROGRAM_NONE, feedbackFlashes=0;
uint32_t feedbackAt=0;
const char KEYS[4][4]={{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};

void playTone(unsigned int frequency, uint32_t duration=0);

ISR(PCINT1_vect) { sleepWakeEvent=true; } // A3 / PCINT11: lid reed
ISR(PCINT2_vect) { sleepWakeEvent=true; } // D3 / PCINT19: tilt, either edge

void setup() {
  byte resetReason=MCUSR;
  MCUSR=0;
  wdt_disable();
  for(byte i=0;i<4;i++) { digitalWrite(ROW_PINS[i],LOW); pinMode(ROW_PINS[i],INPUT); pinMode(COL_PINS[i],INPUT_PULLUP); }
  pinMode(RED_PIN,OUTPUT); pinMode(BLUE_PIN,OUTPUT); pinMode(GREEN_PIN,OUTPUT);
  pinMode(SPEAKER_PIN,OUTPUT); pinMode(ARM_BTN,INPUT_PULLUP);
  armStableState=armLastReading=digitalRead(ARM_BTN);
  pinMode(LID_PIN,INPUT_PULLUP); // definite open level, also backs up the PCB pull-up
  digitalWrite(SERVO_PIN,LOW); pinMode(SERVO_PIN,OUTPUT);
#if !BENCH_WIRING
  pinMode(WAKE_PIN,INPUT_PULLUP); pinMode(SENS_PWR,OUTPUT); digitalWrite(SENS_PWR,HIGH);
  tiltLastLevel=digitalRead(WAKE_PIN);
#endif
  Serial.begin(9600);
  analogReference(INTERNAL);
  analogRead(PIEZO_PIN); analogRead(PIEZO_PIN);
  delay(SENSOR_SETTLE_MS);
  initializeLid();
  loadAll();
  if(resetReason & (1<<WDRF)) Serial.println(F("NOTE: watchdog recovery"));
  // No servo pulse at boot. The startup gate waits for a full closed-lid second.
  startupPending=true; startupCloseTiming=false; startupDriving=false;
  startupDriveAttempted=false; inputsReady=false;
  lidWasClosed=lidStableClosed;
  updateLeds();
  for(byte i=0;i<3;i++) {
    uint16_t duration=pgm_read_word(&THEME_BOOT_LENGTHS[musicTheme][i]);
    playTone(pgm_read_word(&THEME_BOOT_NOTES[musicTheme][i]),duration); delay(duration+20);
  }
  quietKnocks(KNOCK_QUIET_MS);
  Serial.println(F("Treasure box v8.5: Proton servo; waiting for startup close."));
  printStatus();
  lastActivity=millis();
  wdt_enable(WDTO_2S);
}

void loop() {
  wdt_reset();
  serviceTilt();
  if(startupTick()) { handleSerial(); updateLeds(); maybeSleep(); return; }
  serviceLid();
  scanKey(); // detect physical keypad activity before sampling a staged knock
  // Expiration is processed before keys: a confirmation at/after 10 s cannot save.
  programTick();
  themeTick();
  alarmTick();
  if(inputsAreReady()) { keypadTick(); armButtonTick(); }
  serviceLid();
  serviceRelatch();
  pollKnock();
  feedbackTick();
  musicTick();
  ledTick();
  handleSerial();
  maybeSleep();
}

/* =================== LID AND CLAW =================== */
bool startupTick() {
  if(!startupPending) return false;
  bool closed=lidIsClosed();
  if(closed!=lidWasClosed) lastActivity=millis();
  lidWasClosed=closed;
  if(!closed || !rawLidIsClosed()) {
    if(startupDriving) {
      stopLatchDrive(); startupDriving=false;
      Serial.println(F("Startup close stopped: lid opened; waiting again."));
    }
    startupCloseTiming=false;
    return true;
  }
  if(!startupCloseTiming) { startupCloseTiming=true; startupCloseAt=millis(); }
  if(millis()-startupCloseAt<STARTUP_CLOSE_MS) return true;
  if(!startupDriving) {
    stopMusic();
    startLatchDrive(SERVO_CLOSED_US); // unconditional, even if already physically closed
    startupDriving=true; startupDriveAttempted=true; startupDriveAt=millis();
    Serial.println(F("Startup: sending CLOSED 2380us for 1500ms."));
  }
  // Keep generating valid pulses through startup without blocking lid sampling,
  // serial status or the watchdog. This is a drive window, not position sensing.
  if(millis()-startupDriveAt<STARTUP_DRIVE_MS) return true;
  stopLatchDrive(); startupDriving=false;
  finishLockCommand();
  startupPending=false; startupCloseTiming=false; lastActivity=millis();
  inputsReady=false; inputsReleaseTiming=false;
  Serial.println(F("Startup close command complete; ready. Position not sensed."));
  return true;
}

bool inputsAreReady() {
  if(inputsReady) return true;
  if(keyReading || stableKey || keyInputBlocked || digitalRead(ARM_BTN)==LOW) {
    inputsReleaseTiming=false; return false;
  }
  if(!inputsReleaseTiming) { inputsReleaseTiming=true; inputsReleasedAt=millis(); }
  if(millis()-inputsReleasedAt<30) return false;
  heldKey=0; keyHoldHandled=false; bypassIndex=0; pinEntryCount=0;
  armStableState=armLastReading=HIGH; armPressActive=false;
  inputsReady=true;
  return true;
}

int settledAnalogRead(byte pin) {
  analogRead(pin);                     // discard charge from the previous ADC channel
  return analogRead(pin);
}

bool rawLidIsClosed() {
  int v = settledAnalogRead(LID_PIN);
  return LID_CLOSED_IS_LOW ? (v < LID_ANALOG_THRESHOLD) : (v > LID_ANALOG_THRESHOLD);
}

void initializeLid() {
  lidStableClosed = lidLastReading = rawLidIsClosed();
  lidChangedAt = millis();
  uint32_t start = millis();
  // Wait for a stable startup reading, with a bounded wait for a noisy contact.
  do {
    bool reading = rawLidIsClosed();
    if (reading != lidLastReading) {
      lidLastReading = reading;
      lidChangedAt = millis();
    }
    delay(1);
  } while (millis() - lidChangedAt < LID_DEBOUNCE_MS && millis() - start < 1000);
  lidStableClosed = lidLastReading;
}

bool lidIsClosed() {
  bool reading = rawLidIsClosed();
  if (reading != lidLastReading) {
    lidLastReading = reading;
    lidChangedAt = millis();
  }
  if (millis() - lidChangedAt >= LID_DEBOUNCE_MS) lidStableClosed = reading;
  return lidStableClosed;
}

void startLatchDrive(int pulseUs) {
  // attach() enables Timer1. Keep interrupts off until the EXACT pulse is set,
  // avoiding both its default center pulse and pre-attach angle remapping.
  byte oldSREG = SREG;
  cli();
  latch.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  latch.writeMicroseconds(pulseUs);
  SREG = oldSREG;
}

void stopLatchDrive() {
  // Do not truncate a 2380-us closing pulse into a shorter opening command.
  // Wait for LOW with interrupts running, then atomically detach before a new
  // frame can start. Bounded polling also releases a faulty/stuck signal line.
  byte oldSREG=SREG;
  for(uint16_t polls=0;polls<350;polls++) {
    cli();
    if(digitalRead(SERVO_PIN)==LOW) break;
    SREG=oldSREG;
    delayMicroseconds(10);
  }
  cli();
  latch.detach();
  digitalWrite(SERVO_PIN, LOW);
  SREG=oldSREG;
  quietKnocks(KNOCK_QUIET_MS);
}

void moveLatch(int pulseUs) {
  startLatchDrive(pulseUs);
  wdt_reset();
  delay(SERVO_MOVE_MS);
  stopLatchDrive();
  if (DEBUG) { Serial.print(F("commanded servo us=")); Serial.println(pulseUs); }
}


void lockLatch() {
  stopMusic();
  moveLatch(SERVO_CLOSED_US);
  finishLockCommand();
}

void finishLockCommand() {
  latched=true; accessGranted=false; revealPending=false;
  openedSinceUnlock=false; relatchTimerRunning=false;
  bypassRecordEligible=false;
  updateLeds();
}

void unlockLatch() {
  moveLatch(SERVO_OPEN_US);
  latched=false;
  openedSinceUnlock=!lidIsClosed();
  lidClosedAt=millis();
  relatchTimerRunning=!openedSinceUnlock;
  updateLeds();
}

void manualLock() {
  themeFlashes=0;
  // A tap is an explicit claw command, even with the lid physically open.
  // Aborting an unused access prompt is allowed; a real intrusion alarm persists.
  bool cancelPrompt=alarmState==ALARM_ACCESS;
  lockLatch();
  pinEntryCount=0; bypassIndex=0;
  if(cancelPrompt) alarmState=ALARM_IDLE;
  resumeAudio();
  Serial.println(F("# -> claw CLOSED"));
}

void beginChestAccess() {
  if(!accessGranted || latched) {
    unlockLatch();
    accessGranted=true;
    revealPending=true;
  }
  bypassIndex=0; pinEntryCount=0;
  if(armed) {
    if(alarmState==ALARM_IDLE || alarmState==ALARM_CALM) {
      alarmState=ALARM_ACCESS; alarmAt=millis(); wrongTries=0;
      startMusic(MUSIC_BATTLE);
    }
    else resumeAudio(); // repeated ABCD/knocks must not reset the alarm deadline
  } else resumeAudio();
  updateLeds();
}

void serviceLid() {
  bool closed=lidIsClosed();
  if(closed==lidWasClosed) return;
  lidWasClosed=closed; lastActivity=millis();
  if(!closed) {
    if(!latched) openedSinceUnlock=true;
    relatchTimerRunning=false;
    if(armed && (alarmState==ALARM_IDLE || alarmState==ALARM_CALM)) startChallenge();
    else if(!armed && accessGranted && revealPending && musicMode!=MUSIC_DISARM && programMode==PROGRAM_NONE && !feedbackFlashes) {
      revealPending=false; startMusic(MUSIC_REVEAL);
    }
  } else if(!latched) {
    lidClosedAt=millis(); relatchTimerRunning=true;
  }
}

void serviceRelatch() {
  if(latched || programMode!=PROGRAM_NONE || feedbackFlashes) return;
  // The PIN challenge owns its full window. It either fails and locks, or a
  // correct PIN starts a fresh opening window in enterPinDigit().
  if(armed && (alarmState==ALARM_ACCESS || alarmState==ALARM_CALM || alarmState==ALARM_CHALLENGE)) {
    relatchTimerRunning=false; return;
  }
  if(!lidIsClosed()) { relatchTimerRunning=false; return; }
  if(!relatchTimerRunning) { lidClosedAt=millis(); relatchTimerRunning=true; }
  uint32_t wait=openedSinceUnlock?RELATCH_MS:UNLOCK_GRACE_MS;
  if(millis()-lidClosedAt>=wait && rawLidIsClosed()) {
    bool cancelPrompt=alarmState==ALARM_ACCESS;
    lockLatch();
    if(cancelPrompt) alarmState=ALARM_IDLE;
    resumeAudio();
  }
}

/* =================== KEYPAD / HOLD GESTURES =================== */
char scanKey() {
  char found=0; byte count=0;
  for(byte r=0;r<4;r++) {
    digitalWrite(ROW_PINS[r],LOW); pinMode(ROW_PINS[r],OUTPUT); delayMicroseconds(50);
    for(byte c=0;c<4;c++) if(digitalRead(COL_PINS[c])==LOW) { found=KEYS[r][c]; count++; }
    pinMode(ROW_PINS[r],INPUT);
  }
  if(count>1) {
    keyInputBlocked=true; heldKey=0; keyHoldHandled=true; hashMayRecord=false;
    bypassIndex=0; pinEntryCount=0; bypassRecordEligible=false;
    keyReading=0; stableKey=0; keyChangedAt=millis(); return 0;
  }
  if(keyInputBlocked) {
    if(found) keyChangedAt=millis();
    else if(millis()-keyChangedAt>=25) keyInputBlocked=false;
    keyReading=0; stableKey=0; return 0;
  }
  if(found!=keyReading) { keyReading=found; keyChangedAt=millis(); }
  if(millis()-keyChangedAt>=25) stableKey=found;
  return stableKey; // includes held keys and debounced release
}

void keypadTick() {
  if(keyInputBlocked) return;
  char k=stableKey;
  if(k!=heldKey) {
    char released=heldKey;
    bool releaseHandled=keyHoldHandled;
    heldKey=k;
    keyHoldHandled=false;
    if(released && !releaseHandled && programMode==PROGRAM_NONE && !feedbackFlashes && !themePicking && (!themeFlashes || released=='#')) {
      if(released=='#') manualLock();
      if(released=='*') { pinEntryCount=0; bypassIndex=0; }
      if(released=='0') ordinaryKey('0'); // reserve a long hold for theme selection
    }
    if(k) {
      lastActivity=millis(); keyPressedAt=millis();
      hashMayRecord=(k=='#' && bypassRecordEligible);
      if(themePicking) {
        keyHoldHandled=true; themeKey(k);
      } else if(programMode!=PROGRAM_NONE) {
        keyHoldHandled=true; programKey(k);
      } else if(feedbackFlashes || (themeFlashes && k!='#')) {
        keyHoldHandled=true; // a key held through feedback cannot become a later action
      } else if(k!='#' && k!='*' && k!='0') ordinaryKey(k);
    }
  }
  if(heldKey && !keyHoldHandled && programMode==PROGRAM_NONE && !feedbackFlashes && !themePicking && !themeFlashes &&
     millis()-keyPressedAt>=HOLD_MS) {
    keyHoldHandled=true;
    if(heldKey=='*') startProgramming(PROGRAM_PIN);
    else if(heldKey=='#' && hashMayRecord) startProgramming(PROGRAM_KNOCK);
    else if(heldKey=='#') manualLock();
    else if(heldKey=='0') startThemePicker();
  }
}

void ordinaryKey(char k) {
  bypassRecordEligible=false;
  keyBlip(k);
  if(handleBypass(k)) bypassRecordEligible=true;
  else if(armed && k>='0' && k<='9') enterPinDigit(k);
  else pinEntryCount=0;
}

bool handleBypass(char k) {
  if(millis()-bypassLastKeyAt>=10000) bypassIndex=0;
  bypassLastKeyAt=millis();
  char candidate[CODE_LEN+1];
  memcpy(candidate,bypassSeq,bypassIndex); candidate[bypassIndex]=k;
  byte length=bypassIndex+1;
  if(length==CODE_LEN && memcmp(candidate,bypassSeq,CODE_LEN)==0) {
    bypassIndex=0;
    Serial.println(F("Bypass accepted (alarm state unchanged)"));
    beginChestAccess(); return true;
  }
  bypassIndex=0;
  for(byte n=1;n<CODE_LEN && n<=length;n++)
    if(memcmp(candidate+length-n,bypassSeq,n)==0) bypassIndex=n;
  return false;
}

void armButtonTick() {
  byte reading=digitalRead(ARM_BTN);
  if(reading!=armLastReading) { armLastReading=reading; armChangedAt=millis(); }
  if(reading==armStableState || millis()-armChangedAt<30) return;
  armStableState=reading; lastActivity=millis();
  if(reading==LOW) { armPressActive=programMode==PROGRAM_NONE && !feedbackFlashes && !themePicking && !themeFlashes; return; }
  if(!armPressActive) return;
  armPressActive=false;
  if(programMode!=PROGRAM_NONE || feedbackFlashes || themePicking || themeFlashes) return;
  pinEntryCount=0;
  if(!armed) {
    armed=true; saveArmed();
    if(accessGranted) { alarmState=ALARM_ACCESS; alarmAt=millis(); wrongTries=0; startMusic(MUSIC_BATTLE); }
    else if(!lidIsClosed()) startChallenge();
    else { stopMusic(); playTone(themeCue(CUE_ARM),150); }
  } else if(alarmState==ALARM_IDLE || alarmState==ALARM_CALM || alarmState==ALARM_ACCESS) {
    // An explicit disarm-button press starts a fresh PIN-entry window, even
    // after the claw was opened earlier. Preserve ACCESS so timeout locks it.
    if(alarmState!=ALARM_ACCESS) alarmState=ALARM_CALM;
    alarmAt=millis(); startMusic(MUSIC_BATTLE);
  }
  updateLeds();
}

/* =================== ALARM / DISARM =================== */
void startChallenge() {
  if(alarmState==ALARM_CHALLENGE || alarmState==ALARM_SOUNDING || alarmState==ALARM_ACCESS) return;
  if(themePicking) cancelThemePicker(false);
  themeFlashes=0;
  alarmState=ALARM_CHALLENGE; alarmAt=millis(); wrongTries=0; pinEntryCount=0;
  if(programMode==PROGRAM_NONE) startMusic(MUSIC_BATTLE);
  Serial.println(F("Alarm armed: enter PIN within 10 seconds"));
}

void failAccess() {
  alarmState=ALARM_IDLE; pinEntryCount=0;
  lockLatch();                       // includes explicit closure if the lid is open
  startMusic(MUSIC_FAILURE);
  Serial.println(F("Disarm timed out/failed -> closed, still armed"));
}

void soundAlarm() {
  if(themePicking) cancelThemePicker(false);
  themeFlashes=0;
  if(alarmState!=ALARM_SOUNDING) alarmSoundingAt=millis();
  alarmState=ALARM_SOUNDING;
  startMusic(MUSIC_ALARM);
}

void alarmTick() {
  if(!armed || programMode!=PROGRAM_NONE || feedbackFlashes) return;
  if((alarmState==ALARM_ACCESS || alarmState==ALARM_CHALLENGE || alarmState==ALARM_CALM) &&
     millis()-alarmAt>=COUNTDOWN_MS) {
    if(alarmState==ALARM_ACCESS) failAccess();
    else if(alarmState==ALARM_CHALLENGE) {
      soundAlarm(); Serial.println(F("ALARM!"));
    } else { alarmState=ALARM_IDLE; stopMusic(); }
    pinEntryCount=0;
  }
}

void enterPinDigit(char k) {
  if(pinEntryCount && millis()-pinLastDigitAt>=PIN_ENTRY_TIMEOUT_MS) pinEntryCount=0;
  pinLastDigitAt=millis();
  if(pinEntryCount<MAX_PIN_LEN) pinEntry[pinEntryCount++]=k;
  if(pinEntryCount!=pinLength) return;
  pinEntry[pinEntryCount]=0;
  if(strcmp(pinEntry,secretPin)==0) {
    armed=false; saveArmed(); alarmState=ALARM_IDLE; wrongTries=0;
    if(!latched) { lidClosedAt=millis(); relatchTimerRunning=lidIsClosed(); }
    bypassIndex=0; bypassRecordEligible=false;
    Serial.println(F("Alarm disarmed"));
    startMusic(MUSIC_DISARM);
  } else {
    if((alarmState==ALARM_CHALLENGE || alarmState==ALARM_ACCESS) && ++wrongTries>=MAX_TRIES) {
      if(alarmState==ALARM_ACCESS) failAccess();
      else { soundAlarm(); }
    } else playTone(themeCue(CUE_ERROR),180);
  }
  pinEntryCount=0;
}

/* =================== THEME PICKER =================== */
void startThemePicker() {
  pinEntryCount=0; bypassIndex=0; bypassRecordEligible=false;
  if(alarmState!=ALARM_IDLE) { playTone(themeCue(CUE_ERROR),120); return; }
  stopMusic(); armPressActive=false;
  themePicking=true; themePickerAt=millis(); keyHoldHandled=true;
  Serial.println(F("Theme: release 0; 1 Dungeon, 2 Trainer, 3 Arcade; 10 seconds."));
}

void cancelThemePicker(bool cue) {
  themePicking=false; keyHoldHandled=true;
  inputsReady=false; inputsReleaseTiming=false;
  pinEntryCount=0; bypassIndex=0;
  resumeAudio();
  if(cue) playTone(themeCue(CUE_ERROR),120);
}

void themeKey(char k) {
  if(millis()-themePickerAt>=PROGRAM_MS) { cancelThemePicker(true); return; }
  if(k>='1' && k<='3') {
    musicTheme=k-'1'; EEPROM.update(EE_THEME,musicTheme+1);
    themePicking=false; themeFlashes=musicTheme+1; themeFeedbackAt=millis();
    inputsReady=false; inputsReleaseTiming=false; keyHoldHandled=true;
    pinEntryCount=0; bypassIndex=0; lastActivity=millis();
    startMusic(MUSIC_THEME); // selected victory tune; does not open or disarm
    Serial.print(F("Music theme saved: ")); Serial.println(musicTheme+1);
  } else if(k=='#' || k=='*') {
    cancelThemePicker(false);
    if(k=='#') manualLock();
  } else playTone(themeCue(CUE_ERROR),80);
}

void themeTick() {
  if(themeFlashes && millis()-themeFeedbackAt>=themeFlashes*160UL) themeFlashes=0;
  if(themePicking && millis()-themePickerAt>=PROGRAM_MS) cancelThemePicker(true);
}

/* =================== PROGRAMMING (STAGED, EXPLICIT CONFIRM) =================== */
void startProgramming(byte mode) {
  stopMusic(); armPressActive=false;
  programMode=mode; programAt=millis();
  bypassRecordEligible=false; bypassIndex=0; pinEntryCount=0;
  candidatePinCount=0; candidateGapCount=0;
  recordStarted=false; recordComplete=false; recordInvalid=false; recordReady=false;
  // The held initiating key must be released and pressed again to confirm.
  keyHoldHandled=true;
  Serial.println(mode==PROGRAM_KNOCK?F("Knock setup: 10 seconds; # saves"):F("PIN setup: 4-8 digits, * saves within 10 seconds"));
}

void finishProgramming(bool accepted) {
  feedbackMode=programMode;
  programMode=PROGRAM_NONE;
  feedbackFlashes=accepted?5:1; feedbackAt=millis();
  keyHoldHandled=true;
  // Programming owns a full 10 s window; resume a pending challenge afterward.
  alarmAt=millis(); pinEntryCount=0;
  playTone(themeCue(accepted?CUE_SAVED:CUE_ERROR),accepted?120:160);
  Serial.println(accepted?F("Setting saved"):F("Setup canceled; previous setting kept"));
}

void programKey(char k) {
  if(millis()-programAt>=PROGRAM_MS) { finishProgramming(false); return; }
  if(programMode==PROGRAM_PIN) {
    if(k>='0' && k<='9') {
      if(candidatePinCount>=MAX_PIN_LEN) { finishProgramming(false); return; }
      candidatePin[candidatePinCount++]=k;
      playTone(themeCue(CUE_DIGIT),35);
    } else if(k=='*') {
      if(candidatePinCount<MIN_PIN_LEN) { finishProgramming(false); return; }
      candidatePin[candidatePinCount]=0;
      strcpy(secretPin,candidatePin); pinLength=candidatePinCount; savePin();
      finishProgramming(true);
    } else if(k=='#') { finishProgramming(false); manualLock(); }
  } else if(programMode==PROGRAM_KNOCK && k=='#') {
    if(!recordStarted || candidateGapCount<1 || recordInvalid) { finishProgramming(false); return; }
    secretGapCount=candidateGapCount;
    for(byte i=0;i<secretGapCount;i++) secretGaps[i]=candidateGaps[i];
    saveKnock(); finishProgramming(true);
  }
}

void programTick() {
  if(programMode==PROGRAM_NONE) return;
  if(millis()-programAt>=PROGRAM_MS) { finishProgramming(false); return; }
  if(programMode!=PROGRAM_KNOCK || millis()-lastNoiseAt<noiseQuietFor) return;
  // Pressing a keypad key can mechanically excite the piezo. In particular,
  // don't append the confirmation '#' as one more knock during key debounce.
  if(keyInputBlocked || keyReading || stableKey) { recordReady=false; return; }
  int value=settledAnalogRead(PIEZO_PIN);
  uint32_t now=millis();
  if(recordStarted && now-recordLastHit>=PATTERN_TIMEOUT) recordComplete=true;
  if(value<KNOCK_THRESHOLD) { recordReady=true; return; }
  if(!recordReady || (recordStarted && now-recordLastHit<KNOCK_DEBOUNCE)) return;
  recordReady=false;
  if(recordComplete) { recordInvalid=true; return; }
  if(!recordStarted) recordStarted=true;
  else if(candidateGapCount<MAX_KNOCKS-1) candidateGaps[candidateGapCount++]=now-recordLastHit;
  else recordInvalid=true;
  recordLastHit=now;
}

void feedbackTick() {
  if(!feedbackFlashes || millis()-feedbackAt<feedbackFlashes*160UL) return;
  feedbackFlashes=0; feedbackMode=PROGRAM_NONE;
  alarmAt=millis();
  resumeAudio();
}

/* =================== KNOCK ACCESS =================== */
void pollKnock() {
  if(startupPending || !inputsReady || keyInputBlocked || programMode!=PROGRAM_NONE || feedbackFlashes || themePicking || themeFlashes || !latched || !lidIsClosed() ||
     musicMode!=MUSIC_OFF || millis()-lastNoiseAt<noiseQuietFor) {
    knockActive=false; knockReady=false; return;
  }
  uint32_t now=millis();
  int value=settledAnalogRead(PIEZO_PIN);
  if(knockActive && (now-knockLastHit>=PATTERN_TIMEOUT || now-knockStartedAt>=MAX_PATTERN_MS)) {
    bool accepted=!knockOverflow && now-knockStartedAt<MAX_PATTERN_MS && matchPattern(knockGaps,knockGapCount);
    knockActive=false;
    if(accepted) { bypassRecordEligible=false; beginChestAccess(); }
    else playTone(themeCue(CUE_ERROR),180);
    return;
  }
  if(value<KNOCK_THRESHOLD) { knockReady=true; return; }
  if(!knockReady || (knockActive && now-knockLastHit<KNOCK_DEBOUNCE)) return;
  knockReady=false;
  if(!knockActive) { knockActive=true; knockOverflow=false; knockGapCount=0; knockStartedAt=now; }
  else if(knockGapCount<MAX_KNOCKS-1) knockGaps[knockGapCount++]=now-knockLastHit;
  else knockOverflow=true;
  knockLastHit=now; lastActivity=now;
}
bool matchPattern(long gaps[], byte nGaps) {
  if (nGaps != secretGapCount || nGaps == 0 || nGaps >= MAX_KNOCKS) return false;
  long total = 0, secretTotal = 0;
  for (byte i = 0; i < nGaps; i++) {
    if (gaps[i] < KNOCK_DEBOUNCE || gaps[i] >= PATTERN_TIMEOUT || secretGaps[i] <= 0) return false;
    total += gaps[i]; secretTotal += secretGaps[i];
  }
  if (total == 0 || secretTotal == 0) return false;
  for (byte i = 0; i < nGaps; i++) {
    float expected = (float)secretGaps[i] / secretTotal;
    float actual   = (float)gaps[i] / total;
    float allow    = GAP_TOLERANCE * expected;
    if (allow < 0.06) allow = 0.06;
    if (fabs(actual - expected) > allow) return false;
  }
  return true;
}


/* =================== AUDIO =================== */
uint16_t themeCue(byte cue) { return pgm_read_word(&THEME_UI_NOTES[musicTheme][cue]); }
void quietKnocks(uint32_t duration) {
  lastNoiseAt=millis(); noiseQuietFor=duration; knockActive=false; knockReady=false;
}
void playTone(unsigned int frequency, uint32_t duration) {
  if(frequency) audioStart(frequency,duration); else audioStop();
  quietKnocks(duration+KNOCK_QUIET_MS);
}
void stopMusic() { musicMode=MUSIC_OFF; audioStop(); quietKnocks(KNOCK_QUIET_MS); }
void startMusic(byte mode) { stopMusic(); musicMode=mode; musicStep=0; musicNoteMs=0; musicNoteAt=millis(); heartbeatEvent=UINT32_MAX; heartbeatCycleAt=0; heartbeatPeriod=1000; musicTick(); }

void resumeAudio() {
  if(programMode!=PROGRAM_NONE || feedbackFlashes || themePicking) return;
  if(musicMode==MUSIC_THEME && alarmState==ALARM_IDLE) return;
  if(armed) {
    if(alarmState==ALARM_SOUNDING) { if(musicMode!=MUSIC_ALARM) startMusic(MUSIC_ALARM); }
    else if(alarmState==ALARM_ACCESS || alarmState==ALARM_CHALLENGE || alarmState==ALARM_CALM) { if(musicMode!=MUSIC_BATTLE) startMusic(MUSIC_BATTLE); }
    else stopMusic();
  } else if(musicMode==MUSIC_DISARM) return;
  else if(accessGranted && !latched && musicMode==MUSIC_REVEAL) return;
  else if(accessGranted && !latched && revealPending) {
    if(!lidIsClosed()) { revealPending=false; startMusic(MUSIC_REVEAL); }
    else startMusic(MUSIC_BUILD);
  } else stopMusic();
}

void heartbeatTick() {
  uint32_t elapsed=millis()-alarmAt;
  if(elapsed>=COUNTDOWN_MS) return;
  while(elapsed-heartbeatCycleAt>=heartbeatPeriod) {
    heartbeatCycleAt+=heartbeatPeriod;
    uint32_t ramp=heartbeatCycleAt<9000?heartbeatCycleAt:9000;
    heartbeatPeriod=60000UL/(60+ramp*130/9000);
  }
  uint32_t phase=elapsed-heartbeatCycleAt;
  uint16_t firstEnd=heartbeatPeriod*(uint32_t)pgm_read_word(&THEME_HEARTBEAT_SHAPE[musicTheme][0])/100;
  uint16_t secondStart=heartbeatPeriod*(uint32_t)pgm_read_word(&THEME_HEARTBEAT_SHAPE[musicTheme][1])/100;
  uint16_t secondEnd=heartbeatPeriod*(uint32_t)pgm_read_word(&THEME_HEARTBEAT_SHAPE[musicTheme][2])/100;
  byte segment=phase<firstEnd?0:(phase<secondStart?1:(phase<secondEnd?2:3));
  uint32_t event=heartbeatCycleAt*4+segment;
  if(event==heartbeatEvent) return;
  heartbeatEvent=event; musicStep=segment;
  if(segment==0) playTone(pgm_read_word(&THEME_HEARTBEAT_NOTES[musicTheme][0]),firstEnd-phase);
  else if(segment==2) playTone(pgm_read_word(&THEME_HEARTBEAT_NOTES[musicTheme][1]),secondEnd-phase);
  else playTone(0,0);
}

void musicTick() {
  if(musicMode==MUSIC_OFF || programMode!=PROGRAM_NONE || feedbackFlashes || themePicking) return;
  if(musicMode==MUSIC_BATTLE) { heartbeatTick(); return; }
  if(millis()-musicNoteAt<musicNoteMs) return;
  unsigned int frequency=0, duration=0, spacing=25;
  byte duty=50;
  if(musicMode==MUSIC_BUILD) {
    if(armed) { resumeAudio(); return; }
    if(musicStep<24) {
      frequency=pgm_read_word(&THEME_BUILD_NOTES[musicTheme][musicStep]);
      duration=pgm_read_word(&THEME_BUILD_LENGTHS[musicTheme][musicStep++]);
    } else {
      frequency=pgm_read_word(&THEME_BUILD_TAIL[musicTheme][musicStep&1]);
      duration=pgm_read_word(&THEME_BUILD_TAIL_TIMING[musicTheme][0]);
      spacing=pgm_read_word(&THEME_BUILD_TAIL_TIMING[musicTheme][1]);
      musicStep=(musicStep==24)?25:24;
    }
  } else if(musicMode==MUSIC_REVEAL || musicMode==MUSIC_THEME) {
    if(musicMode==MUSIC_REVEAL && armed) { resumeAudio(); return; }
    if(musicStep>=10) { bool preview=musicMode==MUSIC_THEME; stopMusic(); if(preview) resumeAudio(); return; }
    frequency=pgm_read_word(&THEME_REVEAL_NOTES[musicTheme][musicStep]); duration=pgm_read_word(&THEME_REVEAL_LENGTHS[musicTheme][musicStep++]);
  } else if(musicMode==MUSIC_DISARM) {
    if(musicStep>=4) { stopMusic(); resumeAudio(); return; }
    frequency=pgm_read_word(&THEME_DISARM_NOTES[musicTheme][musicStep]); duration=pgm_read_word(&THEME_DISARM_LENGTHS[musicTheme][musicStep++]); spacing=20;
  } else if(musicMode==MUSIC_FAILURE) {
    if(musicStep>=5) { stopMusic(); return; }
    frequency=pgm_read_word(&THEME_FAILURE_NOTES[musicTheme][musicStep]); duration=pgm_read_word(&THEME_FAILURE_LENGTHS[musicTheme][musicStep++]); spacing=60;
  } else {
    uint32_t elapsed=millis()-alarmSoundingAt;
    uint32_t ramp=elapsed<30000UL?elapsed:30000UL;
    unsigned int speed=100-ramp*50/30000UL; // 100% -> 50% note length over 30 s
    duty=12+ramp*38/30000UL; // quieter -> old full-volume drive, never above it
    frequency=pgm_read_word(&THEME_ALARM_NOTES[musicTheme][musicStep]);
    if(elapsed>=10000UL) frequency*=2;
    if(elapsed>=20000UL && musicStep%4==0) frequency*=2;
    duration=(uint32_t)pgm_read_word(&THEME_ALARM_LENGTHS[musicTheme][musicStep])*speed/100;
    spacing=20-ramp*15/30000UL;
    musicStep=(musicStep+1)%24;
  }
  audioStart(frequency,duration,duty); quietKnocks(duration+KNOCK_QUIET_MS);
  musicNoteAt=millis(); musicNoteMs=duration+spacing;
}
void keyBlip(char k) {
  if(musicMode==MUSIC_DISARM || musicMode==MUSIC_THEME) return;
  playTone((k>='0' && k<='9')?pgm_read_word(&THEME_KEY_NOTES[musicTheme][k-'0']):pgm_read_word(&THEME_BUILD_NOTES[musicTheme][0]),40);
}

/* =================== LEDS =================== */
void setLeds(bool r,bool b,bool g) { digitalWrite(RED_PIN,r?HIGH:LOW); digitalWrite(BLUE_PIN,b?HIGH:LOW); digitalWrite(GREEN_PIN,g?HIGH:LOW); }
void updateLeds() { setLeds(armed,false,!startupPending && !latched); }
void ledTick() {
  if(themePicking) {
    uint32_t phase=(millis()-themePickerAt)%2000;
    uint32_t brightness=phase<1000?phase:2000-phase;
    bool on=(micros()%4000)<brightness*4;
    setLeds(false,false,on);
  } else if(themeFlashes) {
    bool on=((millis()-themeFeedbackAt)/80)%2==0;
    setLeds(false,false,on);
  } else if(musicMode==MUSIC_THEME) {
    setLeds(armed,false,true);
  } else if(programMode!=PROGRAM_NONE) {
    uint32_t phase=(millis()-programAt)%2000;
    uint32_t brightness=phase<1000?phase:2000-phase;
    bool on=(micros()%4000)<brightness*4; // software PWM: these LED pins have no usable hardware PWM
    setLeds(programMode==PROGRAM_PIN && on,programMode==PROGRAM_KNOCK && on,on);
  } else if(feedbackFlashes) {
    bool on=((millis()-feedbackAt)/80)%2==0;
    setLeds(feedbackMode==PROGRAM_PIN && on,feedbackMode==PROGRAM_KNOCK && on,on);
  } else if(armed) {
    // Blue stays off until disarmed, including the battle/countdown and alarm.
    bool red=(alarmState==ALARM_SOUNDING)?(millis()/110)%2==0:true;
    setLeds(red,false,!latched);
  } else setLeds(false,(musicMode==MUSIC_BUILD || musicMode==MUSIC_REVEAL) && (musicStep&1),!latched);
}

/* =================== EEPROM =================== */
// Slot: commit, generation, length, 8 digits, checksum-low, checksum-high (13 bytes).
uint16_t pinChecksum(byte generation,byte length,const char* digits) {
  uint16_t h=0x6D31;
  h=(h*31)^generation; h=(h*31)^length;
  for(byte i=0;i<length;i++) h=(h*31)^(byte)digits[i];
  return h;
}

bool readPinSlot(byte address,char* pin,byte &length,byte &generation) {
  if(EEPROM.read(address)!=EE_PIN_COMMIT) return false;
  generation=EEPROM.read(address+1); length=EEPROM.read(address+2);
  if(length<MIN_PIN_LEN || length>MAX_PIN_LEN) return false;
  for(byte i=0;i<length;i++) { pin[i]=EEPROM.read(address+3+i); if(pin[i]<'0' || pin[i]>'9') return false; }
  pin[length]=0;
  uint16_t checksum=EEPROM.read(address+11) | ((uint16_t)EEPROM.read(address+12)<<8);
  return checksum==pinChecksum(generation,length,pin);
}

void savePin() {
  byte slot=activePinSlot==0?1:0;
  byte address=slot==0?EE_PIN_SLOT0:EE_PIN_SLOT1;
  byte generation=pinGeneration+1;
  uint16_t checksum=pinChecksum(generation,pinLength,secretPin);
  EEPROM.update(address,0); // invalidate destination; previous slot remains intact
  EEPROM.update(address+1,generation); EEPROM.update(address+2,pinLength);
  for(byte i=0;i<MAX_PIN_LEN;i++) EEPROM.update(address+3+i,i<pinLength?secretPin[i]:0);
  EEPROM.update(address+11,(byte)checksum); EEPROM.update(address+12,(byte)(checksum>>8));
  EEPROM.update(address,EE_PIN_COMMIT); // commit last
  activePinSlot=slot; pinGeneration=generation;
}

void loadAll() {
  byte savedTheme=EEPROM.read(EE_THEME);
  musicTheme=(savedTheme>=1 && savedTheme<=THEME_COUNT)?savedTheme-1:0;
  if(EEPROM.read(EE_MAGIC_ADDR)!=EE_MAGIC) {
    strcpy(secretPin,DEFAULT_PIN); strcpy(bypassSeq,DEFAULT_BYPASS); pinLength=4; armed=false;
    for(byte i=0;i<4;i++) EEPROM.update(EE_PIN+i,secretPin[i]);
    EEPROM.update(EE_PIN_SLOT0,0); EEPROM.update(EE_PIN_SLOT1,0);
    EEPROM.update(EE_KNOCK_SLOT0,0); EEPROM.update(EE_KNOCK_SLOT1,0);
    musicTheme=0; EEPROM.update(EE_THEME,1);
    saveKnock(); saveBypass(); saveArmed(); savePin();
    EEPROM.update(EE_MAGIC_ADDR,EE_MAGIC);
    Serial.println(F("EEPROM: factory defaults initialized")); return;
  }
  armed=EEPROM.read(EE_ARMED)!=0;
  bool valid=true;
  loadKnock();
  valid=true;
  for(byte i=0;i<CODE_LEN;i++) {
    char c=EEPROM.read(EE_BYPASS+i); bypassSeq[i]=c;
    if(!((c>='0' && c<='9') || (c>='A' && c<='D'))) valid=false;
  }
  bypassSeq[CODE_LEN]=0;
  if(!valid) { strcpy(bypassSeq,DEFAULT_BYPASS); saveBypass(); }
  char first[MAX_PIN_LEN+1],second[MAX_PIN_LEN+1];
  byte len0=0,len1=0,gen0=0,gen1=0;
  bool valid0=readPinSlot(EE_PIN_SLOT0,first,len0,gen0), valid1=readPinSlot(EE_PIN_SLOT1,second,len1,gen1);
  if(valid0 || valid1) {
    bool use1=valid1 && (!valid0 || (int8_t)(gen1-gen0)>0);
    strcpy(secretPin,use1?second:first); pinLength=use1?len1:len0;
    pinGeneration=use1?gen1:gen0; activePinSlot=use1?1:0;
  } else {
    valid=true; pinLength=4;
    for(byte i=0;i<4;i++) { secretPin[i]=EEPROM.read(EE_PIN+i); if(secretPin[i]<'0' || secretPin[i]>'9') valid=false; }
    secretPin[4]=0;
    if(!valid) strcpy(secretPin,DEFAULT_PIN);
    activePinSlot=-1; pinGeneration=0; savePin();
    Serial.println(F("Legacy PIN migrated to v8 slots"));
  }
}
uint16_t knockChecksum(byte generation,byte count,const int16_t* gaps) {
  uint16_t h=(0x51A9*31U)^generation; h=(h*31U)^count;
  for(byte i=0;i<count;i++) { h=(h*31U)^(byte)gaps[i]; h=(h*31U)^(byte)(gaps[i]>>8); }
  return h;
}
bool readKnockSlot(byte address,int16_t* gaps,byte &count,byte &generation) {
  if(EEPROM.read(address)!=EE_KNOCK_COMMIT) return false;
  generation=EEPROM.read(address+1); count=EEPROM.read(address+2);
  if(count<1 || count>=MAX_KNOCKS) return false;
  for(byte i=0;i<count;i++) {
    EEPROM.get(address+3+i*2,gaps[i]);
    if(gaps[i]<KNOCK_DEBOUNCE || gaps[i]>=PATTERN_TIMEOUT) return false;
  }
  uint16_t sum=EEPROM.read(address+21)|((uint16_t)EEPROM.read(address+22)<<8);
  return sum==knockChecksum(generation,count,gaps);
}
void saveKnock() {
  byte slot=activeKnockSlot==0?1:0, address=slot==0?EE_KNOCK_SLOT0:EE_KNOCK_SLOT1;
  byte generation=knockGeneration+1;
  uint16_t sum=knockChecksum(generation,secretGapCount,secretGaps);
  EEPROM.update(address,0);
  EEPROM.update(address+1,generation); EEPROM.update(address+2,secretGapCount);
  for(byte i=0;i<MAX_KNOCKS-1;i++) {
    int16_t gap=i<secretGapCount?secretGaps[i]:0;
    EEPROM.update(address+3+i*2,(byte)gap); EEPROM.update(address+4+i*2,(byte)(gap>>8));
  }
  EEPROM.update(address+21,(byte)sum); EEPROM.update(address+22,(byte)(sum>>8));
  EEPROM.update(address,EE_KNOCK_COMMIT); // commit last; other valid slot survives
  activeKnockSlot=slot; knockGeneration=generation;
}
void loadKnock() {
  int16_t first[MAX_KNOCKS-1],second[MAX_KNOCKS-1];
  byte n0=0,n1=0,g0=0,g1=0;
  bool v0=readKnockSlot(EE_KNOCK_SLOT0,first,n0,g0),v1=readKnockSlot(EE_KNOCK_SLOT1,second,n1,g1);
  if(v0 || v1) {
    bool use1=v1 && (!v0 || (int8_t)(g1-g0)>0);
    secretGapCount=use1?n1:n0; activeKnockSlot=use1?1:0; knockGeneration=use1?g1:g0;
    for(byte i=0;i<secretGapCount;i++) secretGaps[i]=use1?second[i]:first[i];
    return;
  }
  secretGapCount=EEPROM.read(EE_KNOCK_N);
  bool valid=secretGapCount>0 && secretGapCount<MAX_KNOCKS;
  if(valid) for(byte i=0;i<secretGapCount;i++) {
    EEPROM.get(EE_GAPS+i*2,secretGaps[i]);
    if(secretGaps[i]<KNOCK_DEBOUNCE || secretGaps[i]>=PATTERN_TIMEOUT) valid=false;
  }
  if(!valid) { secretGapCount=4; secretGaps[0]=400; secretGaps[1]=200; secretGaps[2]=200; secretGaps[3]=600; }
  activeKnockSlot=-1; knockGeneration=0; saveKnock();
}
void saveBypass() { for(byte i=0;i<CODE_LEN;i++) EEPROM.update(EE_BYPASS+i,bypassSeq[i]); }
void saveArmed() { EEPROM.update(EE_ARMED,armed?1:0); }

/* =================== SLEEP / READ-ONLY STATUS =================== */
void serviceTilt() {
  if(!SLEEP_AVAILABLE) return;
  byte reading=digitalRead(WAKE_PIN);
  if(reading!=tiltLastLevel) { tiltLastLevel=reading; lastActivity=millis(); }
}

void maybeSleep() {
  if(!SLEEP_AVAILABLE || DEBUG || startupDriving || startupCloseTiming || knockActive || heldKey || keyReading || keyInputBlocked || armPressActive ||
     digitalRead(ARM_BTN)==LOW || programMode!=PROGRAM_NONE || feedbackFlashes || themePicking || themeFlashes || musicMode!=MUSIC_OFF ||
     alarmState!=ALARM_IDLE || millis()-lastActivity<SLEEP_AFTER_MS) return;
  byte lidBefore=digitalRead(LID_PIN);
  // Lid wake remains essential to an armed alarm or an unfinished closure.
  // Only a fully initialized, disarmed, closed, commanded-locked box may omit it.
  bool wakeOnLid=armed || startupPending || !latched || !lidStableClosed ||
                 lidBefore!=(LID_CLOSED_IS_LOW?LOW:HIGH);
  Serial.println(wakeOnLid?F("Sleep: tilt + lid wake; weak reed pull-up."):
                          F("Sleep: tilt wake only; reed pull-ups off."));
  Serial.flush();
  audioStop(); setLeds(false,false,false);
  wdt_disable(); byte adcsra=ADCSRA; ADCSRA &= ~(1<<ADEN);
  byte oldSREG=SREG; cli();
  byte oldControl=PCICR, oldMask1=PCMSK1, oldMask2=PCMSK2;
  sleepWakeEvent=false;
  // Check the still-powered reed before changing its circuit. A lid edge during
  // entry must not be mistaken for the deliberate LOW of tilt-only sleep.
  if(digitalRead(WAKE_PIN)==tiltLastLevel && digitalRead(LID_PIN)==lidBefore) {
    PCICR=oldControl & ~((1<<PCIE1)|(1<<PCIE2));
    if(wakeOnLid) {
      // A5 high impedance removes the external 10k pull-up. A3's internal
      // pull-up still supplies a valid OPEN level for asynchronous lid wake.
      pinMode(SENS_PWR,INPUT); digitalWrite(SENS_PWR,LOW);
    } else {
      // With lid wake intentionally disabled, R6 can hold A3 LOW without any
      // pull-up current. Do not read this artificial level as a real lid state.
      pinMode(LID_PIN,INPUT); digitalWrite(LID_PIN,LOW);
      digitalWrite(SENS_PWR,LOW);
    }
    PCMSK1=wakeOnLid?(1<<PCINT11):0; PCMSK2=(1<<PCINT19);
    PCIFR=(1<<PCIF1)|(1<<PCIF2); // discard flags from the circuit transition
    PCICR=(oldControl & ~((1<<PCIE1)|(1<<PCIE2))) |
           (wakeOnLid?(1<<PCIE1):0) | (1<<PCIE2);
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    // Recheck enabled wake inputs after configuration; adjacent SEI/SLEEP
    // avoids losing an edge between this check and actual power-down.
    if(digitalRead(WAKE_PIN)==tiltLastLevel && (!wakeOnLid || digitalRead(LID_PIN)==lidBefore)) {
      sleep_enable(); sei(); sleep_cpu(); sleep_disable();
    }
  }
  cli();
  PCICR=oldControl & ~((1<<PCIE1)|(1<<PCIE2));
  // Restore normal sensing before ADC use or any lid/state-machine updates,
  // including when an input race canceled sleep. Preserve the old lid baseline.
  digitalWrite(SENS_PWR,HIGH); pinMode(SENS_PWR,OUTPUT);
  pinMode(LID_PIN,INPUT_PULLUP);
  PCIFR=(1<<PCIF1)|(1<<PCIF2);
  PCMSK1=oldMask1; PCMSK2=oldMask2; PCICR=oldControl; SREG=oldSREG;
  ADCSRA=adcsra; delay(SENSOR_SETTLE_MS);
  tiltLastLevel=digitalRead(WAKE_PIN);
  quietKnocks(KNOCK_QUIET_MS); wdt_enable(WDTO_2S); updateLeds(); lastActivity=millis();
  Serial.println(F("Awake: rechecking lid."));
}
void printStatus() {
  Serial.print(F("Theme=")); Serial.print(musicTheme+1);
  Serial.print(F(" tilt=")); Serial.print(digitalRead(WAKE_PIN)); Serial.print(F(" "));
  Serial.print(F("Lid raw=")); Serial.print(settledAnalogRead(LID_PIN));
  Serial.print(F(" interpreted=")); Serial.print(lidIsClosed()?F("CLOSED"):F("OPEN"));
  if(startupPending) {
    Serial.print(F(" startup="));
    Serial.print(startupDriving?F("DRIVING_CLOSED_2380us"):
                 (startupCloseTiming?F("WAIT_CLOSED_1s"):F("WAIT_FOR_CLOSED_LID")));
    Serial.print(F(" claw="));
    Serial.print(startupDriving?F("CLOSE_COMMAND_ACTIVE"):
                 (startupDriveAttempted?F("DRIVE_STOPPED_POSITION_UNKNOWN"):F("NOT_COMMANDED")));
    Serial.print(F(" armed=")); Serial.println(armed?F("YES"):F("no")); return;
  }
  Serial.print(F(" claw command=")); Serial.print(latched?F("CLOSED "):F("OPEN "));
  Serial.print(latched?SERVO_CLOSED_US:SERVO_OPEN_US); Serial.print(F("us"));
  Serial.print(F(" armed=")); Serial.println(armed?F("YES"):F("no"));
}
void handleSerial() {
  if(!Serial.available()) return;
  char c=Serial.read();
  if(c=='s') printStatus();
  if(DEBUG && c=='t') playTone(1000,500);
  if(DEBUG && c=='c' && !armed) startMusic(MUSIC_REVEAL);
}
