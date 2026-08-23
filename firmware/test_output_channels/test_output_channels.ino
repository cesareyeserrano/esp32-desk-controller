// test_output_channels — proves the four actuation channels are safe BEFORE
// anything is soldered to the handset.
//
// Nothing here touches the desk. The optocouplers are wired on the breadboard
// with their output side connected to NOTHING, so the worst this sketch can do
// is light an infrared LED inside a chip.
//
// What it is here to prove, in order of importance:
//
//   1. The pins are LOW at reset and stay LOW through boot. Everything in
//      ADR-024 rests on this: the watchdog protects us by resetting the chip,
//      and a reset only helps if a reset leaves the channels open. If a pin
//      came up HIGH during boot, the watchdog would ACTIVATE a button instead
//      of releasing it — the exact opposite of the protection.
//   2. A commanded pulse lasts what it should and not a millisecond more.
//   3. Only one channel is ever active at a time.
//
// Wiring, per channel (see docs/SEGURIDAD.md):
//
//     GPIO --[330 ohm]--|>|-- PC817 pin 1 (anode)      (320 ohm is fine too:
//                             the value is not critical, see below)
//                             PC817 pin 2 (cathode) -- GND
//       |
//     [10k]        pull-down: without it, a weak internal pull-up at boot
//       |          (~45 kohm) would push ~47 uA through the LED, which is
//      GND         uncomfortably close to the ~90 uA the handset needs to see
//                  a key. With it, that leakage sits at 0.6 V — far below the
//                  1.2 V the LED needs to conduct at all.
//
//     Multimeter across PC817 pins 3 and 4 (the output) to watch the channel.
//
// The series resistor is deliberately uncritical. It sets the LED current, and
// the LED only has to switch the ~90 uA the handset's key matrix carries. With
// a CTR of 200% even 0.2 mA through the LED gives four times the margin needed,
// so anything from 300 ohm to about 2 kohm works. 330 ohm gives 6.4 mA and 320
// gives 6.6 mA — a 3% difference that changes nothing.

#include <Arduino.h>

// Four free pins, consecutive on the right-hand terminal block. None of them is
// a strapping pin (0, 2, 5, 12, 15), none belongs to the flash (6..11), and all
// four can drive an output — unlike P34, P35, SVN and SVP on that same block,
// which are input-only.
static const uint8_t CH_PIN[4] = {27, 26, 25, 33};
static const char *CH_NAME[4] = {"1", "2", "3", "4"};

// ADR-023: no pulse may ever outlast this. 300 ms sits between the 160 ms the
// AiP650E needs to register a press at all and the 2.2 s that turns a tap into
// continuous movement.
static const uint32_t PULSE_MS = 300;

static const uint32_t SERIAL_BAUD = 921600;

// --------------------------------------------------------------- channels ---

static void allChannelsOff() {
  for (uint8_t i = 0; i < 4; i++) digitalWrite(CH_PIN[i], LOW);
}

// Drives one channel for PULSE_MS and reports how long it actually took, so the
// number can be checked rather than trusted.
static void pulse(uint8_t idx) {
  allChannelsOff();  // never two at once, whatever happened before

  uint32_t t0 = millis();
  digitalWrite(CH_PIN[idx], HIGH);
  while ((uint32_t)(millis() - t0) < PULSE_MS) {
  }
  digitalWrite(CH_PIN[idx], LOW);
  uint32_t took = millis() - t0;

  char buf[96];
  snprintf(buf, sizeof(buf), "channel %s (GPIO%u): pulsed %lu ms", CH_NAME[idx],
           (unsigned)CH_PIN[idx], (unsigned long)took);
  Serial.println(buf);
}

static void printLevels() {
  char buf[96];
  Serial.println("current pin levels (all should read 0):");
  for (uint8_t i = 0; i < 4; i++) {
    snprintf(buf, sizeof(buf), "   channel %s  GPIO%-2u = %d", CH_NAME[i],
             (unsigned)CH_PIN[i], digitalRead(CH_PIN[i]));
    Serial.println(buf);
  }
}

static void printHelp() {
  Serial.println();
  Serial.println("  1 2 3 4  pulse that channel for 300 ms");
  Serial.println("  l        print the level of all four pins");
  Serial.println("  h        this help");
  Serial.println();
}

// ------------------------------------------------------------------ main ---

void setup() {
  // FIRST, before anything else can take time: drive every channel low.
  //
  // Between reset and this line the pins are inputs, which cannot source the
  // current the optocoupler LED needs — that is what makes a reset safe. This
  // loop closes the remaining window as early as the program can.
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(CH_PIN[i], OUTPUT);
    digitalWrite(CH_PIN[i], LOW);
  }

  Serial.begin(SERIAL_BAUD);
  delay(300);

  Serial.println();
  Serial.println("=== test_output_channels — actuation safety check ===");
  Serial.println("Output side of every PC817 must be connected to NOTHING.");
  Serial.println("The desk is not involved in this test.");
  char buf[96];
  snprintf(buf, sizeof(buf), "Channels: GPIO%u %u %u %u   pulse = %lu ms",
           (unsigned)CH_PIN[0], (unsigned)CH_PIN[1], (unsigned)CH_PIN[2],
           (unsigned)CH_PIN[3], (unsigned long)PULSE_MS);
  Serial.println(buf);
  printHelp();
  printLevels();
  Serial.println();
  Serial.println("Now check with the multimeter across PC817 pins 3-4:");
  Serial.println("  - holding EN (chip in reset)      -> open, megohms");
  Serial.println("  - releasing EN, during boot       -> no beep in continuity");
  Serial.println("  - idle after boot                 -> open");
  Serial.println("  - while pulsing (press 1)         -> conducts briefly");
  Serial.println();
}

void loop() {
  if (!Serial.available()) return;
  int c = Serial.read();
  if (c >= '1' && c <= '4') {
    pulse((uint8_t)(c - '1'));
  } else if (c == 'l') {
    printLevels();
  } else if (c == 'h') {
    printHelp();
  }
}
