// Ramię robota 4DOF + chwytak na Arduino UNO R3.
// Serwa hobbystyczne, biblioteka Servo, sterowanie tekstem po Serial 115200.
//
// Całą konfigurację mechaniczną zmienia się w sekcji KONFIGURACJA poniżej.
// Opis protokołu, zasilania i wgrywania z Raspberry Pi 5: README.md.
//
// Pętla główna nie używa delay(). Ruch jest rozkładany w czasie przez millis().

#include <Servo.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// KONFIGURACJA — tu zmieniaj piny, pozycję domową i limity kątów.
// ============================================================================
//
// ZAŁOŻENIE (Leszek podał piny i kolejność od dołu, NIE potwierdził roli pinu 7):
//   Kolejne osie ramienia od dołu są na pinach 3, 4, 5, 6, 7.
//   pin 3 = podstawa (obrót)
//   pin 4 = bark
//   pin 5 = łokieć
//   pin 6 = nadgarstek
//   pin 7 = chwytak   <-- rola NIEPOTWIERDZONA
//
// Kąty to stopnie biblioteki Servo (0–180). home musi leżeć w min..max;
// program przy starcie i tak obetnie wartość spoza zakresu.
// Domyślne 0..180 to zakres elektryczny serwa, a NIE zmierzone ograniczenia
// mechaniczne tego ramienia. Zawęź limity, zanim ramię zacznie pracować.
// Numery osi w komendach (1..5) liczą od dołu. To nie są numery pinów.
// ============================================================================

static const uint8_t JOINT_COUNT = 5;
static_assert(JOINT_COUNT == 5, "Protokol i nazwy osi sa zapisane dla 5 serw.");

struct JointConfig {
  uint8_t pin;
  int16_t homeDeg;
  int16_t minDeg;
  int16_t maxDeg;
};

static const JointConfig JOINT_CFG[JOINT_COUNT] = {
    // pin, home, min, max
    {3, 90, 0, 180},  // 1 podstawa (obrót)
    {4, 90, 0, 180},  // 2 bark
    {5, 90, 0, 180},  // 3 łokieć
    {6, 90, 0, 180},  // 4 nadgarstek
    {7, 90, 0, 180},  // 5 chwytak — rola pinu 7 NIEPOTWIERDZONA
};

// true: po resecie serwa dostają sygnał i trzymają pozycję domową (skok!).
// false: start bez sygnału, ruch dopiero po komendzie ATTACH.
static const bool ATTACH_ON_BOOT = true;

// Prędkość początkowa i dopuszczalny zakres komendy SPEED, w stopniach/s.
static const int16_t DEFAULT_SPEED_DPS = 30;
static const int16_t MIN_SPEED_DPS = 1;
static const int16_t MAX_SPEED_DPS = 180;

// Pauza między pozami sekwencji PLAY (nieblokująca, liczona z millis()).
static const unsigned long POSE_DWELL_MS = 500;

// Szerokość impulsu w mikrosekundach (domyślne 544 i 2400).
// Biblioteka Servo trzyma odchyłkę od tych wartości w bajcie ze znakiem,
// z krokiem 4 µs, więc sensowna korekta to rząd ±500 µs, nie kilka tysięcy.
// Ruszaj to dopiero, gdy serwo wyraźnie nie dochodzi do skraju zakresu 0–180.
static const int SERVO_MIN_US = 544;
static const int SERVO_MAX_US = 2400;

// Sloty pozycji trzymane tylko w RAM. Reset UNO kasuje zapis.
static const uint8_t SLOT_COUNT = 8;

static const long SERIAL_BAUD = 115200;

// Najdłuższy akceptowany wiersz komendy (bez znaku końca).
static const uint8_t LINE_MAX = 96;

// ============================================================================
// Stan roboczy. Nie trzeba tego zmieniać przy pierwszym uruchomieniu.
// ============================================================================

struct JointRuntime {
  Servo servo;
  uint8_t pin;
  int16_t homeDeg;
  int16_t minDeg;
  int16_t maxDeg;
  int16_t currentTenth;  // kąt * 10, żeby ruch był płynniejszy niż 1°
  int16_t targetTenth;
  int16_t lastWritten;  // ostatni stopień wysłany do serwa, -1 = brak
  bool attached;
  bool pinOk;
};

struct Pose {
  bool used;
  int16_t deg[JOINT_COUNT];
};

static JointRuntime joint[JOINT_COUNT];
static Pose slots[SLOT_COUNT];

static int16_t speedDps = DEFAULT_SPEED_DPS;
static unsigned long lastMotionMs = 0;
static uint16_t motionRemainder = 0;

static int8_t seqQueue[SLOT_COUNT];
static uint8_t seqLen = 0;
static uint8_t seqPos = 0;
static bool seqActive = false;
static bool seqDwell = false;
static unsigned long dwellStartedMs = 0;

static bool waitPending = false;

static char lineBuf[LINE_MAX];
static uint8_t lineLen = 0;
static bool lineOverflow = false;

// ============================================================================
// Wypisywanie
// ============================================================================

void printAxisId(uint8_t index) {
  switch (index) {
    case 0:
      Serial.print(F("base"));
      break;
    case 1:
      Serial.print(F("shoulder"));
      break;
    case 2:
      Serial.print(F("elbow"));
      break;
    case 3:
      Serial.print(F("wrist"));
      break;
    case 4:
      Serial.print(F("gripper"));
      break;
    default:
      Serial.print(F("?"));
      break;
  }
}

void printTenth(int16_t tenth) {
  if (tenth < 0) {
    Serial.print('-');
    tenth = (int16_t)(-tenth);
  }
  Serial.print(tenth / 10);
  Serial.print('.');
  Serial.print(tenth % 10);
}

int16_t tenthToDeg(int16_t tenth) {
  if (tenth >= 0) {
    return (int16_t)((tenth + 5) / 10);
  }
  return (int16_t)((tenth - 5) / 10);
}

void printDegListFromTargets() {
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    Serial.print(' ');
    Serial.print(tenthToDeg(joint[i].targetTenth));
  }
}

void printDegListFromCurrent() {
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    Serial.print(' ');
    Serial.print(tenthToDeg(joint[i].currentTenth));
  }
}

// ============================================================================
// Parsowanie
// ============================================================================

void asciiLowerInPlace(char* text) {
  if (text == nullptr) {
    return;
  }
  for (char* p = text; *p != '\0'; p++) {
    unsigned char c = (unsigned char)(*p);
    if (c < 128) {
      *p = (char)tolower(c);
    }
  }
}

// Zwraca liczbę tokenów albo 255, gdy komenda ma ich więcej niż maxTok.
uint8_t tokenize(char* line, char** tok, uint8_t maxTok) {
  uint8_t count = 0;
  char* p = line;
  while (*p != '\0') {
    while (*p == ' ' || *p == '\t') {
      p++;
    }
    if (*p == '\0') {
      break;
    }
    if (count >= maxTok) {
      return 255;
    }
    tok[count++] = p;
    while (*p != '\0' && *p != ' ' && *p != '\t') {
      p++;
    }
    if (*p != '\0') {
      *p = '\0';
      p++;
    }
  }
  return count;
}

bool parseLong(const char* text, long* out) {
  if (text == nullptr || *text == '\0' || out == nullptr) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  long value = strtol(text, &end, 10);
  if (end == text || *end != '\0' || errno == ERANGE) {
    return false;
  }
  *out = value;
  return true;
}

// Os 1..5 albo nazwa. Świadomie NIE przyjmujemy numeru pinu:
// "3" byłoby jednocześnie trzecią osią (łokieć) i pinem podstawy.
bool axisFromToken(const char* token, uint8_t* indexOut) {
  if (token == nullptr || indexOut == nullptr) {
    return false;
  }
  if (strcmp(token, "1") == 0 || strcmp(token, "base") == 0 ||
      strcmp(token, "b") == 0 || strcmp(token, "podstawa") == 0) {
    *indexOut = 0;
    return true;
  }
  if (strcmp(token, "2") == 0 || strcmp(token, "shoulder") == 0 ||
      strcmp(token, "s") == 0 || strcmp(token, "bark") == 0) {
    *indexOut = 1;
    return true;
  }
  if (strcmp(token, "3") == 0 || strcmp(token, "elbow") == 0 ||
      strcmp(token, "e") == 0 || strcmp(token, "lokiec") == 0) {
    *indexOut = 2;
    return true;
  }
  if (strcmp(token, "4") == 0 || strcmp(token, "wrist") == 0 ||
      strcmp(token, "w") == 0 || strcmp(token, "nadgarstek") == 0) {
    *indexOut = 3;
    return true;
  }
  if (strcmp(token, "5") == 0 || strcmp(token, "gripper") == 0 ||
      strcmp(token, "g") == 0 || strcmp(token, "chwytak") == 0) {
    *indexOut = 4;
    return true;
  }
  return false;
}

bool slotFromToken(const char* token, uint8_t* indexOut) {
  long value = 0;
  if (!parseLong(token, &value) || value < 1 || value > (long)SLOT_COUNT) {
    return false;
  }
  *indexOut = (uint8_t)(value - 1);
  return true;
}

// ============================================================================
// Serwa i ruch
// ============================================================================

bool pinAllowed(uint8_t pin) {
  // 0 i 1 to UART (Serial) na UNO. Zajęcie ich ucina sterowanie z Pi.
  if (pin == 0 || pin == 1) {
    return false;
  }
  return pin <= 19;
}

bool pinUsedEarlier(uint8_t index) {
  for (uint8_t k = 0; k < index; k++) {
    if (joint[k].pin == joint[index].pin) {
      return true;
    }
  }
  return false;
}

void loadConfig() {
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    int16_t mn = JOINT_CFG[i].minDeg;
    int16_t mx = JOINT_CFG[i].maxDeg;
    int16_t home = JOINT_CFG[i].homeDeg;
    bool clamped = false;

    if (mn < 0) {
      mn = 0;
      clamped = true;
    }
    if (mx > 180) {
      mx = 180;
      clamped = true;
    }
    if (mn > mx) {
      int16_t tmp = mn;
      mn = mx;
      mx = tmp;
      clamped = true;
    }
    if (home < mn) {
      home = mn;
      clamped = true;
    }
    if (home > mx) {
      home = mx;
      clamped = true;
    }

    joint[i].pin = JOINT_CFG[i].pin;
    joint[i].minDeg = mn;
    joint[i].maxDeg = mx;
    joint[i].homeDeg = home;
    joint[i].currentTenth = (int16_t)(home * 10);
    joint[i].targetTenth = joint[i].currentTenth;
    joint[i].lastWritten = -1;
    joint[i].attached = false;
    joint[i].pinOk = pinAllowed(joint[i].pin) && !pinUsedEarlier(i);

    if (clamped) {
      Serial.print(F("warn config_clamped "));
      printAxisId(i);
      Serial.println();
    }
    if (!pinAllowed(joint[i].pin)) {
      Serial.print(F("warn pin_reserved_or_invalid "));
      printAxisId(i);
      Serial.print(' ');
      Serial.println(joint[i].pin);
    } else if (pinUsedEarlier(i)) {
      Serial.print(F("warn pin_duplicated "));
      printAxisId(i);
      Serial.print(' ');
      Serial.println(joint[i].pin);
    }
  }
}

int16_t clampDeg(uint8_t index, long requested, bool* clipped) {
  long value = requested;
  bool cut = false;
  if (value < joint[index].minDeg) {
    value = joint[index].minDeg;
    cut = true;
  } else if (value > joint[index].maxDeg) {
    value = joint[index].maxDeg;
    cut = true;
  }
  if (clipped != nullptr) {
    *clipped = cut;
  }
  return (int16_t)value;
}

void setTargetDeg(uint8_t index, int16_t deg) {
  joint[index].targetTenth = (int16_t)(deg * 10);
}

bool attachJoint(uint8_t index) {
  if (!joint[index].pinOk) {
    return false;
  }
  if (joint[index].attached) {
    return true;
  }
  int16_t deg = tenthToDeg(joint[index].currentTenth);
  if (deg < joint[index].minDeg) {
    deg = joint[index].minDeg;
  }
  if (deg > joint[index].maxDeg) {
    deg = joint[index].maxDeg;
  }
  joint[index].currentTenth = (int16_t)(deg * 10);
  // write() przed attach(), żeby pierwszy impuls był już kątem docelowym,
  // a nie domyślnymi 1500 µs (90°), gdy home jest inny niż 90.
  joint[index].servo.write(deg);
  uint8_t channel = joint[index].servo.attach(joint[index].pin, SERVO_MIN_US, SERVO_MAX_US);
  if (channel == 255) {
    joint[index].attached = false;
    return false;
  }
  joint[index].attached = true;
  joint[index].servo.write(deg);
  joint[index].lastWritten = deg;
  return true;
}

void detachJoint(uint8_t index) {
  if (joint[index].attached) {
    joint[index].servo.detach();
    joint[index].attached = false;
  }
  joint[index].currentTenth = (int16_t)(tenthToDeg(joint[index].currentTenth) * 10);
  joint[index].targetTenth = joint[index].currentTenth;
}

bool allAttached() {
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    if (!joint[i].attached) {
      return false;
    }
  }
  return true;
}

bool isBusy() {
  if (seqActive) {
    return true;
  }
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    if (joint[i].attached && joint[i].currentTenth != joint[i].targetTenth) {
      return true;
    }
  }
  return false;
}

void updateMotion() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastMotionMs;
  lastMotionMs = now;
  if (elapsed == 0) {
    return;
  }
  // Po przerwie (np. długi wydruk HELP) doganiamy płynnie, bez skoku o cały ruch.
  if (elapsed > 200) {
    elapsed = 200;
  }

  int32_t product = (int32_t)speedDps * 10 * (int32_t)elapsed + (int32_t)motionRemainder;
  int32_t step = product / 1000;
  motionRemainder = (uint16_t)(product % 1000);
  if (step <= 0) {
    return;
  }
  if (step > 1800) {
    step = 1800;
  }

  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    if (!joint[i].attached) {
      continue;
    }
    int16_t current = joint[i].currentTenth;
    int16_t target = joint[i].targetTenth;
    if (current == target) {
      continue;
    }
    if (current < target) {
      int32_t next = (int32_t)current + step;
      current = (next >= target) ? target : (int16_t)next;
    } else {
      int32_t next = (int32_t)current - step;
      current = (next <= target) ? target : (int16_t)next;
    }
    joint[i].currentTenth = current;
    int16_t deg = tenthToDeg(current);
    if (deg != joint[i].lastWritten) {
      joint[i].servo.write(deg);
      joint[i].lastWritten = deg;
    }
  }
}

// ============================================================================
// Sekwencja w RAM
// ============================================================================

void cancelSequence() {
  seqActive = false;
  seqLen = 0;
  seqPos = 0;
  seqDwell = false;
}

void applySlot(uint8_t slotIndex) {
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    int16_t deg = slots[slotIndex].deg[i];
    if (deg < joint[i].minDeg) {
      deg = joint[i].minDeg;
    }
    if (deg > joint[i].maxDeg) {
      deg = joint[i].maxDeg;
    }
    setTargetDeg(i, deg);
  }
}

void updateSequence() {
  if (!seqActive) {
    return;
  }
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    if (joint[i].attached && joint[i].currentTenth != joint[i].targetTenth) {
      seqDwell = false;
      return;
    }
  }
  if ((uint8_t)(seqPos + 1) >= seqLen) {
    seqActive = false;
    seqDwell = false;
    return;
  }
  if (!seqDwell) {
    seqDwell = true;
    dwellStartedMs = millis();
    return;
  }
  if ((unsigned long)(millis() - dwellStartedMs) < POSE_DWELL_MS) {
    return;
  }
  seqPos++;
  applySlot((uint8_t)seqQueue[seqPos]);
  seqDwell = false;
}

void serviceWait() {
  if (waitPending && !isBusy()) {
    waitPending = false;
    Serial.println(F("OK wait"));
  }
}

// ============================================================================
// Komendy
// ============================================================================

void printHelp() {
  Serial.println(F("Komendy ASCII, katy calkowite w stopniach, predkosc w stopniach/s."));
  Serial.println(F("Os od dolu: 1 base/b/podstawa, 2 shoulder/s/bark, 3 elbow/e/lokiec,"));
  Serial.println(F("  4 wrist/w/nadgarstek, 5 gripper/g/chwytak."));
  Serial.println(F("Numery 1..5 to osie, NIE piny. Zalozone piny: 3 4 5 6 7."));
  Serial.println(F("Pin 7 jako chwytak jest NIEPOTWIERDZONY."));
  Serial.println(F("SET <os> <kat>          ustaw jedna os (alias MOVE)"));
  Serial.println(F("ALL <b> <s> <e> <w> <g> ustaw wszystkie (alias POSE)"));
  Serial.println(F("HOME                    jedz do pozycji domowej"));
  Serial.println(F("STOP                    zatrzymaj i trzymaj kat (moment zostaje)"));
  Serial.println(F("DETACH [os]             zdejmij sygnal (ramie moze opasc)"));
  Serial.println(F("ATTACH [os]             wlacz sygnal, trzymaj biezacy kat"));
  Serial.println(F("SPEED [1..180]          predkosc albo odczyt"));
  Serial.println(F("SAVE <1..8>             zapisz biezacy kat do RAM"));
  Serial.println(F("LOAD <1..8>             jedz do slotu"));
  Serial.println(F("CLEAR <1..8|ALL>        skasuj slot albo wszystkie"));
  Serial.println(F("LIST                    wypisz sloty"));
  Serial.println(F("PLAY [slot ...]         odtworz sloty po kolei"));
  Serial.println(F("WAIT                    odpowiedz gdy ruch i PLAY sie skoncza"));
  Serial.println(F("STATUS                  stan osi, limitow i slotow"));
  Serial.println(F("PING                    test lacza"));
  Serial.println(F("HELP                    ta lista"));
  Serial.println(F("Poza limitem kat jest obcinany. Odpowiedz konczy sie linia OK albo ERR."));
  Serial.println(F("OK help"));
}

void printStatus() {
  Serial.print(F("speed_dps="));
  Serial.println(speedDps);
  Serial.print(F("dwell_ms="));
  Serial.println(POSE_DWELL_MS);
  Serial.print(F("attach_on_boot="));
  Serial.println(ATTACH_ON_BOOT ? 1 : 0);
  Serial.print(F("moving="));
  Serial.println(isBusy() ? 1 : 0);
  if (!seqActive) {
    Serial.println(F("sequence=idle"));
  } else {
    Serial.print(F("sequence=play "));
    Serial.print(seqPos + 1);
    Serial.print('/');
    Serial.println(seqLen);
  }
  Serial.println(F("assumption pin7_role=gripper_UNCONFIRMED"));
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    Serial.print(F("joint "));
    Serial.print(i + 1);
    Serial.print(' ');
    printAxisId(i);
    Serial.print(F(" pin="));
    Serial.print(joint[i].pin);
    Serial.print(F(" attached="));
    Serial.print(joint[i].attached ? 1 : 0);
    Serial.print(F(" cur="));
    printTenth(joint[i].currentTenth);
    Serial.print(F(" tgt="));
    printTenth(joint[i].targetTenth);
    Serial.print(F(" min="));
    Serial.print(joint[i].minDeg);
    Serial.print(F(" max="));
    Serial.print(joint[i].maxDeg);
    Serial.print(F(" home="));
    Serial.println(joint[i].homeDeg);
  }
  uint8_t used = 0;
  for (uint8_t s = 0; s < SLOT_COUNT; s++) {
    if (!slots[s].used) {
      continue;
    }
    used++;
    Serial.print(F("slot "));
    Serial.print(s + 1);
    for (uint8_t i = 0; i < JOINT_COUNT; i++) {
      Serial.print(' ');
      Serial.print(slots[s].deg[i]);
    }
    Serial.println();
  }
  Serial.print(F("slots_used="));
  Serial.println(used);
  Serial.println(F("OK status"));
}

void printList() {
  for (uint8_t s = 0; s < SLOT_COUNT; s++) {
    Serial.print(F("slot "));
    Serial.print(s + 1);
    if (!slots[s].used) {
      Serial.println(F(" empty"));
      continue;
    }
    for (uint8_t i = 0; i < JOINT_COUNT; i++) {
      Serial.print(' ');
      Serial.print(slots[s].deg[i]);
    }
    Serial.println();
  }
  Serial.println(F("OK list"));
}

void cmdSet(char** tok, uint8_t ntok) {
  if (ntok != 3) {
    Serial.println(F("ERR syntax SET <os> <kat>"));
    return;
  }
  uint8_t index = 0;
  if (!axisFromToken(tok[1], &index)) {
    Serial.print(F("ERR bad_axis "));
    Serial.println(tok[1]);
    return;
  }
  long requested = 0;
  if (!parseLong(tok[2], &requested)) {
    Serial.print(F("ERR bad_angle "));
    Serial.println(tok[2]);
    return;
  }
  if (!joint[index].attached) {
    Serial.println(F("ERR detached"));
    return;
  }
  bool clipped = false;
  int16_t applied = clampDeg(index, requested, &clipped);
  cancelSequence();
  setTargetDeg(index, applied);
  Serial.print(F("OK set "));
  printAxisId(index);
  Serial.print(' ');
  Serial.print(applied);
  if (clipped) {
    Serial.print(F(" clipped_from "));
    Serial.print(requested);
  }
  Serial.println();
}

void cmdAll(char** tok, uint8_t ntok) {
  if (ntok != 1 + JOINT_COUNT) {
    Serial.println(F("ERR syntax ALL <base> <shoulder> <elbow> <wrist> <gripper>"));
    return;
  }
  long requested[JOINT_COUNT];
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    if (!parseLong(tok[i + 1], &requested[i])) {
      Serial.print(F("ERR bad_angle "));
      Serial.println(tok[i + 1]);
      return;
    }
  }
  if (!allAttached()) {
    Serial.println(F("ERR detached"));
    return;
  }
  bool anyClip = false;
  int16_t applied[JOINT_COUNT];
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    bool clipped = false;
    applied[i] = clampDeg(i, requested[i], &clipped);
    anyClip = anyClip || clipped;
  }
  cancelSequence();
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    setTargetDeg(i, applied[i]);
  }
  Serial.print(F("OK all"));
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    Serial.print(' ');
    Serial.print(applied[i]);
  }
  if (anyClip) {
    Serial.print(F(" clipped"));
  }
  Serial.println();
}

void cmdHome() {
  if (!allAttached()) {
    Serial.println(F("ERR detached"));
    return;
  }
  cancelSequence();
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    setTargetDeg(i, joint[i].homeDeg);
  }
  Serial.print(F("OK home"));
  printDegListFromTargets();
  Serial.println();
}

void cmdStop() {
  cancelSequence();
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    int16_t deg = tenthToDeg(joint[i].currentTenth);
    joint[i].currentTenth = (int16_t)(deg * 10);
    joint[i].targetTenth = joint[i].currentTenth;
    if (joint[i].attached && deg != joint[i].lastWritten) {
      joint[i].servo.write(deg);
      joint[i].lastWritten = deg;
    }
  }
  Serial.print(F("OK stop"));
  printDegListFromCurrent();
  Serial.println();
}

void cmdDetach(char** tok, uint8_t ntok) {
  if (ntok > 2) {
    Serial.println(F("ERR syntax DETACH [os]"));
    return;
  }
  cancelSequence();
  if (ntok == 1) {
    for (uint8_t i = 0; i < JOINT_COUNT; i++) {
      detachJoint(i);
    }
    Serial.println(F("OK detach all"));
    return;
  }
  uint8_t index = 0;
  if (!axisFromToken(tok[1], &index)) {
    Serial.print(F("ERR bad_axis "));
    Serial.println(tok[1]);
    return;
  }
  detachJoint(index);
  Serial.print(F("OK detach "));
  printAxisId(index);
  Serial.println();
}

void cmdAttach(char** tok, uint8_t ntok) {
  if (ntok > 2) {
    Serial.println(F("ERR syntax ATTACH [os]"));
    return;
  }
  if (ntok == 1) {
    for (uint8_t i = 0; i < JOINT_COUNT; i++) {
      if (!attachJoint(i)) {
        Serial.print(F("ERR attach_failed "));
        printAxisId(i);
        Serial.println();
        return;
      }
    }
    Serial.println(F("OK attach all"));
    return;
  }
  uint8_t index = 0;
  if (!axisFromToken(tok[1], &index)) {
    Serial.print(F("ERR bad_axis "));
    Serial.println(tok[1]);
    return;
  }
  if (!attachJoint(index)) {
    Serial.print(F("ERR attach_failed "));
    printAxisId(index);
    Serial.println();
    return;
  }
  Serial.print(F("OK attach "));
  printAxisId(index);
  Serial.println();
}

void cmdSpeed(char** tok, uint8_t ntok) {
  if (ntok == 1) {
    Serial.print(F("OK speed "));
    Serial.println(speedDps);
    return;
  }
  if (ntok != 2) {
    Serial.println(F("ERR syntax SPEED [stopnie_na_sekunde]"));
    return;
  }
  long requested = 0;
  if (!parseLong(tok[1], &requested)) {
    Serial.print(F("ERR bad_speed "));
    Serial.println(tok[1]);
    return;
  }
  long applied = requested;
  bool clipped = false;
  if (applied < MIN_SPEED_DPS) {
    applied = MIN_SPEED_DPS;
    clipped = true;
  } else if (applied > MAX_SPEED_DPS) {
    applied = MAX_SPEED_DPS;
    clipped = true;
  }
  speedDps = (int16_t)applied;
  motionRemainder = 0;
  Serial.print(F("OK speed "));
  Serial.print(speedDps);
  if (clipped) {
    Serial.print(F(" clipped_from "));
    Serial.print(requested);
  }
  Serial.println();
}

void cmdSave(char** tok, uint8_t ntok) {
  if (ntok != 2) {
    Serial.println(F("ERR syntax SAVE <1..8>"));
    return;
  }
  uint8_t slot = 0;
  if (!slotFromToken(tok[1], &slot)) {
    Serial.print(F("ERR bad_slot "));
    Serial.println(tok[1]);
    return;
  }
  slots[slot].used = true;
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    slots[slot].deg[i] = tenthToDeg(joint[i].currentTenth);
  }
  Serial.print(F("OK save "));
  Serial.print(slot + 1);
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    Serial.print(' ');
    Serial.print(slots[slot].deg[i]);
  }
  Serial.println();
}

void cmdLoad(char** tok, uint8_t ntok) {
  if (ntok != 2) {
    Serial.println(F("ERR syntax LOAD <1..8>"));
    return;
  }
  uint8_t slot = 0;
  if (!slotFromToken(tok[1], &slot)) {
    Serial.print(F("ERR bad_slot "));
    Serial.println(tok[1]);
    return;
  }
  if (!slots[slot].used) {
    Serial.print(F("ERR empty_slot "));
    Serial.println(slot + 1);
    return;
  }
  if (!allAttached()) {
    Serial.println(F("ERR detached"));
    return;
  }
  cancelSequence();
  applySlot(slot);
  Serial.print(F("OK load "));
  Serial.print(slot + 1);
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    Serial.print(' ');
    Serial.print(slots[slot].deg[i]);
  }
  Serial.println();
}

void cmdClear(char** tok, uint8_t ntok) {
  if (ntok != 2) {
    Serial.println(F("ERR syntax CLEAR <1..8|ALL>"));
    return;
  }
  if (strcmp(tok[1], "all") == 0) {
    cancelSequence();
    for (uint8_t s = 0; s < SLOT_COUNT; s++) {
      slots[s].used = false;
    }
    Serial.println(F("OK clear all"));
    return;
  }
  uint8_t slot = 0;
  if (!slotFromToken(tok[1], &slot)) {
    Serial.print(F("ERR bad_slot "));
    Serial.println(tok[1]);
    return;
  }
  cancelSequence();
  slots[slot].used = false;
  Serial.print(F("OK clear "));
  Serial.println(slot + 1);
}

void cmdPlay(char** tok, uint8_t ntok) {
  if (ntok > 1 + SLOT_COUNT) {
    Serial.println(F("ERR syntax PLAY [slot ...]"));
    return;
  }
  uint8_t queue[SLOT_COUNT];
  uint8_t count = 0;
  if (ntok == 1) {
    for (uint8_t s = 0; s < SLOT_COUNT; s++) {
      if (slots[s].used) {
        queue[count++] = s;
      }
    }
    if (count == 0) {
      Serial.println(F("ERR no_slots"));
      return;
    }
  } else {
    for (uint8_t t = 1; t < ntok; t++) {
      uint8_t slot = 0;
      if (!slotFromToken(tok[t], &slot)) {
        Serial.print(F("ERR bad_slot "));
        Serial.println(tok[t]);
        return;
      }
      if (!slots[slot].used) {
        Serial.print(F("ERR empty_slot "));
        Serial.println(slot + 1);
        return;
      }
      queue[count++] = slot;
    }
  }
  if (!allAttached()) {
    Serial.println(F("ERR detached"));
    return;
  }
  cancelSequence();
  for (uint8_t i = 0; i < count; i++) {
    seqQueue[i] = (int8_t)queue[i];
  }
  seqLen = count;
  seqPos = 0;
  seqActive = true;
  seqDwell = false;
  applySlot(queue[0]);
  Serial.print(F("OK play"));
  for (uint8_t i = 0; i < count; i++) {
    Serial.print(' ');
    Serial.print(queue[i] + 1);
  }
  Serial.println();
}

void cmdWait() {
  if (!isBusy()) {
    Serial.println(F("OK wait"));
    return;
  }
  waitPending = true;
}

void handleLine(char* line) {
  // Nowa komenda unieważnia oczekujące WAIT, żeby odpowiedź nie weszła
  // w środek kolejnej transakcji skryptu.
  waitPending = false;

  asciiLowerInPlace(line);
  char* tok[12];
  uint8_t ntok = tokenize(line, tok, 12);
  if (ntok == 0) {
    return;
  }
  if (ntok == 255) {
    Serial.println(F("ERR syntax too_many_tokens"));
    return;
  }

  if (strcmp(tok[0], "help") == 0 || strcmp(tok[0], "?") == 0) {
    if (ntok != 1) {
      Serial.println(F("ERR syntax HELP"));
      return;
    }
    printHelp();
    return;
  }
  if (strcmp(tok[0], "ping") == 0) {
    if (ntok != 1) {
      Serial.println(F("ERR syntax PING"));
      return;
    }
    Serial.println(F("OK pong"));
    return;
  }
  if (strcmp(tok[0], "status") == 0) {
    if (ntok != 1) {
      Serial.println(F("ERR syntax STATUS"));
      return;
    }
    printStatus();
    return;
  }
  if (strcmp(tok[0], "list") == 0) {
    if (ntok != 1) {
      Serial.println(F("ERR syntax LIST"));
      return;
    }
    printList();
    return;
  }
  if (strcmp(tok[0], "set") == 0 || strcmp(tok[0], "move") == 0) {
    cmdSet(tok, ntok);
    return;
  }
  if (strcmp(tok[0], "all") == 0 || strcmp(tok[0], "pose") == 0) {
    cmdAll(tok, ntok);
    return;
  }
  if (strcmp(tok[0], "home") == 0) {
    if (ntok != 1) {
      Serial.println(F("ERR syntax HOME"));
      return;
    }
    cmdHome();
    return;
  }
  if (strcmp(tok[0], "stop") == 0) {
    if (ntok != 1) {
      Serial.println(F("ERR syntax STOP"));
      return;
    }
    cmdStop();
    return;
  }
  if (strcmp(tok[0], "detach") == 0) {
    cmdDetach(tok, ntok);
    return;
  }
  if (strcmp(tok[0], "attach") == 0) {
    cmdAttach(tok, ntok);
    return;
  }
  if (strcmp(tok[0], "speed") == 0) {
    cmdSpeed(tok, ntok);
    return;
  }
  if (strcmp(tok[0], "save") == 0) {
    cmdSave(tok, ntok);
    return;
  }
  if (strcmp(tok[0], "load") == 0) {
    cmdLoad(tok, ntok);
    return;
  }
  if (strcmp(tok[0], "clear") == 0) {
    cmdClear(tok, ntok);
    return;
  }
  if (strcmp(tok[0], "play") == 0) {
    cmdPlay(tok, ntok);
    return;
  }
  if (strcmp(tok[0], "wait") == 0) {
    if (ntok != 1) {
      Serial.println(F("ERR syntax WAIT"));
      return;
    }
    cmdWait();
    return;
  }

  Serial.print(F("ERR unknown_command "));
  Serial.println(tok[0]);
}

void pollSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      if (lineOverflow) {
        lineOverflow = false;
        lineLen = 0;
        Serial.println(F("ERR line_too_long"));
        continue;
      }
      lineBuf[lineLen] = '\0';
      if (lineLen > 0) {
        handleLine(lineBuf);
      }
      lineLen = 0;
      continue;
    }
    if (lineOverflow) {
      continue;
    }
    if (lineLen >= LINE_MAX - 1) {
      lineOverflow = true;
      lineLen = 0;
      continue;
    }
    lineBuf[lineLen++] = c;
  }
}

void printBoot() {
  Serial.println(F("robot_arm_4dof"));
  Serial.print(F("pins"));
  for (uint8_t i = 0; i < JOINT_COUNT; i++) {
    Serial.print(' ');
    printAxisId(i);
    Serial.print('=');
    Serial.print(joint[i].pin);
  }
  Serial.println();
  Serial.println(F("assumption roles=base,shoulder,elbow,wrist,gripper pin7_role=UNCONFIRMED"));
  Serial.println(F("OK ready"));
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  loadConfig();
  if (ATTACH_ON_BOOT) {
    for (uint8_t i = 0; i < JOINT_COUNT; i++) {
      if (!attachJoint(i)) {
        Serial.print(F("warn attach_failed "));
        printAxisId(i);
        Serial.println();
      }
    }
  }
  lastMotionMs = millis();
  printBoot();
}

void loop() {
  // Brak delay(). Serial jest czytany nieblokująco, a serwa idą małymi krokami.
  pollSerial();
  updateMotion();
  updateSequence();
  serviceWait();
}
