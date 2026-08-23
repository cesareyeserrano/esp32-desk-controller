// test_wifi_impact -- desk_sniffer with the WiFi radio turned on.
//
// WHY THIS EXISTS (2026-08-22, opening phase 4): the WiFi radio has NEVER been
// switched on on this board. The sniffer samples at 4 MHz with interrupts
// disabled for 2 ms per burst, and the WiFi stack needs regular CPU attention.
// Either can starve the other, and this ESP32 already locked up twice on
// 2026-08-03 from interrupt saturation.
//
// The whole phase-4 architecture depends on the answer, so it gets measured
// before anything is designed on top of it.
//
// Method: identical to desk_sniffer, same 's' statistics, only difference is
// the radio. Run 60 s, print stats, compare against the no-WiFi baseline in
// docs/capturas/2026-08-22-wifi-impacto.log.
//
// Soft AP on purpose: it needs no credentials, and it still brings up the radio
// and the full network stack (beacons every 100 ms).

#include <WiFi.h>

// desk_sniffer — reads the AiP650E bus of a Jiecang handset, and presses its
// buttons.
//
// Listens to the two-wire bus between the desk control box (master) and the
// AiP650E in the handset (slave). The BUS is read-only and always will be:
// those two pins are inputs from setup() to power-off. See docs/ADR-011 —
// injecting key presses on the bus is electrically impossible.
//
// Pressing buttons happens somewhere else entirely: four optocouplers wired in
// parallel with the physical buttons, on their own pins. That is exactly the
// route ADR-011 prescribes once bus injection turned out to be impossible.
//
// The two halves live in one sketch on purpose. Splitting them would mean two
// copies of the protocol decoder drifting apart — and it would make the one
// check that matters impossible: pulse a channel, and read from the bus which
// button the control box actually saw. A channel soldered to the wrong button
// announces itself immediately instead of days later.
//
// Wiring (verified 2026-08-02, see docs/HARDWARE.md):
//   RED    CLK  -> divider -> GPIO18
//   GREEN  DIO  -> divider -> GPIO4
//   BLUE   GND  -> ESP32 GND
//   YELLOW 5V   -> NOT CONNECTED
//
// Protocol reference: AiP650E Product Specification, Wuxi I-CORE,
// AiP650E-AX-XS-B037EN, 2024-01-B1. Archived in docs/hardware/datasheets/.

#include <Arduino.h>

#include "soc/gpio_struct.h"

// Cycle counter read straight from the Xtensa special register: no headers, no
// function call, safe to use from an ISR regardless of core version.
static inline uint32_t IRAM_ATTR cycleCount() {
#if defined(__XTENSA__)
  uint32_t c;
  __asm__ __volatile__("rsr %0, ccount" : "=a"(c));
  return c;
#else
  return (uint32_t)esp_timer_get_time() * 240;  // fallback, coarser
#endif
}

// ---------------------------------------------------------------- config ---

// GPIO18 and GPIO4 are free on BOTH WROOM-32 and WROVER modules, are not
// strapping pins (0, 2, 5, 12, 15) and are not wired to the flash (6..11).
// CLK deliberately avoids GPIO16/17: WROVER modules use those for the PSRAM,
// and which module this board carries was never verified. See docs/ADR-020.
static const int PIN_CLK = 18;  // red wire
static const int PIN_DIO = 4;   // green wire
static const uint32_t SERIAL_BAUD = 115200;  // ADR-026: 460800 tampoco recibio

// Both pins must be below 32 so a single GPIO.in read covers them.
static_assert(PIN_CLK < 32 && PIN_DIO < 32, "pins must be in GPIO0..31");


// -------------------------------------------------------------- actuation ---
//
// Four dry-contact channels in parallel with the handset buttons, driven
// through PC817 optocouplers. This does NOT break the read-only rule of
// ADR-011: that rule is about the BUS, and the bus pins stay inputs forever.
// The buttons are a separate, galvanically isolated path — which is exactly
// what ADR-011 prescribes once injecting on the bus turned out to be
// impossible.
//
// Verified on the bench 2026-08-20 with test_output_channels: all four read 0
// at boot, open at rest, ~150 ohm while pulsing, 300 ms every time.

static const uint8_t CH_PIN[4] = {27, 26, 25, 33};
static const uint32_t PULSE_MS = 800;  // ADR-027: 300 ms no mueve el escritorio

// Long pulse: deliberately CROSSES the 2.2-2.6 s threshold to start continuous
// travel. Measured 2026-08-06: 2.2 s still behaved as a tap, 2.6 s did not.
//
// DANGER, and it is a different kind from everything else in this sketch: once
// continuous travel starts, RELEASING THE CONTACT DOES NOT STOP IT. The desk
// kept going 6 cm in 6.6 s going down. Stopping it requires CLOSING a contact
// again. So a crash or a reset while travelling leaves the desk moving with
// nothing able to stop it -- the watchdog of ADR-024 opens the channel, which
// is precisely the action that does NOT stop it.
//
// Capped below the 3.0 s that overwrites a preset. See ADR-028.
static const uint32_t LONG_PULSE_MS = 2800;

// Which channel was pulsed last, and until when we still care. After a pulse we
// watch the bus for the key byte the control box reports, and print it next to
// the channel that caused it. That is how a freshly soldered channel is checked
// against PROTOCOLO.md without guessing: press it, read which button answered.
static int8_t g_lastPulsedCh = -1;
static uint32_t g_watchKeyUntilMs = 0;

static void allChannelsOff() {
  for (uint8_t i = 0; i < 4; i++) digitalWrite(CH_PIN[i], LOW);
}

static void pulseChannelFor(uint8_t idx, uint32_t widthMs) {
  allChannelsOff();  // never two at once
  uint32_t t0 = millis();
  digitalWrite(CH_PIN[idx], HIGH);

  // Keep sniffing WHILE the contact is closed. The first version sat in an
  // empty loop here, and was therefore blind during the only moment the key is
  // actually pressed: by the time it looked at the bus again the key was
  // released. That is why no channel could ever be attributed, and why a
  // working channel looked dead on 2026-08-21 — the desk moved and the capture
  // said nothing had happened.
  //
  // A burst is only started if it still fits inside the pulse, so decoding can
  // never stretch the contact past PULSE_MS. That bound is what ADR-023 rests
  // on: 2.2 s starts continuous travel and 3.0 s overwrites a preset.
  // Arm the attribution BEFORE the pulse: the key byte we are after appears
  // DURING the contact, not after it.
  g_lastPulsedCh = (int8_t)idx;
  g_watchKeyUntilMs = millis() + widthMs + 1500;

  const uint32_t TAIL_MS = 20;  // headroom for one capture-and-decode round
  while ((uint32_t)(millis() - t0) < widthMs) {
    if ((uint32_t)(millis() - t0) + TAIL_MS < widthMs) {
      uint32_t n = captureBurst(5);
      if (n) decodeBurst(n);
    }
  }
  digitalWrite(CH_PIN[idx], LOW);

  char buf[96];
  snprintf(buf, sizeof(buf), "\n[channel %u -> GPIO%u pulsed %lu ms; watching the bus]",
           (unsigned)(idx + 1), (unsigned)CH_PIN[idx],
           (unsigned long)(millis() - t0));
  Serial.println(buf);
}

static void pulseChannel(uint8_t idx) { pulseChannelFor(idx, PULSE_MS); }

// Starts continuous travel on purpose. Read the warning on LONG_PULSE_MS.
static void pulseChannelLong(uint8_t idx) {
  Serial.println("\n[LONG PULSE - this starts continuous travel]");
  pulseChannelFor(idx, LONG_PULSE_MS);
}

// --------------------------------------------------------------- capture ---
//
// BURST SAMPLING, not interrupts. This is the second capture front-end; the
// first one used attachInterrupt() on both lines and could not keep up.
//
// Measured on this desk 2026-08-06: the bus clock runs at ~202 kHz, and that
// figure is a lower bound. Measured on this same ESP32 2026-08-03 with
// test_capture_ceiling: interrupt capture is exact up to 125 kHz and starts
// losing edges SILENTLY at 150 kHz — two edges arriving closer together than
// the ISR takes to run collapse into a single interrupt, and nothing counts
// them. The symptom was 40 of 41 transactions malformed with `dropped` at zero.
//
// An edge-interval histogram ruled out the alternative explanation, that the
// resistive probe was rounding edges into double crossings: zero intervals
// under 0.5 us. The signal is clean, the bus is simply fast.
//
// So we stop reacting to edges and start filming the lines. A tight paced loop
// samples both pins at a fixed rate into RAM, and the decoding happens
// afterwards from the recording. Nothing can arrive "too fast" because nothing
// has to be serviced on time: at 4 MHz against a 200 kHz bus every state of
// the line is captured about twenty times over.
//
// The cost is that capture is a burst, not continuous. That is fine for
// reverse-engineering the protocol, which is what phase 2 needs. Continuous
// monitoring for phase 4 is a separate problem, to be solved once we know what
// the traffic means.

static const uint32_t SAMPLE_MHZ = 4;   // 20x oversampling of a 200 kHz bus
static const uint32_t BURST_N = 8000;   // 2 ms at 4 MHz
static uint8_t g_burst[BURST_N];

// Why 2 ms and not longer: interrupts are off while recording, and the longest
// transaction seen so far — eight bytes at ~200 kHz — lasts about 360 us, so
// 2 ms covers it five times over. Recording for much longer would keep
// interrupts off for most of the time and start starving the serial port,
// which is how commands get missed and output stutters.

static uint32_t g_edges = 0;    // transitions found in the recordings
static uint32_t g_dropped = 0;  // samples where the paced loop ran late
static uint32_t g_bursts = 0;
static uint32_t g_burstsEmpty = 0;  // armed but the bus never woke up

// bit 0 = CLK, bit 1 = DIO. Both idle high, so 0x03 is a quiet bus.
static inline uint8_t sampleLines(uint32_t in) {
  return (uint8_t)(((in >> PIN_CLK) & 1) | (((in >> PIN_DIO) & 1) << 1));
}

// ------------------------------------------------------------ timestamps ---
//
// The cycle counter is 32 bits: at 240 MHz it wraps every ~17.9 s. Deltas
// between consecutive events are computed with unsigned math, which absorbs a
// single wrap correctly — but only if events never sit more than one wrap
// apart. The bus CAN go quiet for longer than that: the control box is allowed
// to put the chip to sleep (see ADR-012), which is exactly the case we care
// about. keepClockAlive() folds elapsed cycles in while the bus is silent so a
// wrap is never missed.

static uint64_t g_cycTotal = 0;
static uint32_t g_lastCyc = 0;
static bool g_haveLastCyc = false;
static uint32_t g_cyclesPerUs = 240;

static uint64_t stampMicros(uint32_t cyc) {
  if (!g_haveLastCyc) {
    g_lastCyc = cyc;
    g_haveLastCyc = true;
  }
  // Unsigned math absorbs one wrap — but it also turns a BACKWARDS step into a
  // ~2^32 jump. That can happen: the ISR may queue an event just after loop()
  // decided the queue was empty, and keepClockAlive() then advances the base
  // past that event's timestamp. The log would show a 17.9 s hole and never
  // recover. keepClockAlive() folds every 5 s, so a genuine delta never gets
  // near 2^31 cycles (8.9 s) and anything that big can only be that race.
  uint32_t d = cyc - g_lastCyc;
  if (d > 0x80000000u) d = 0;
  g_cycTotal += d;
  g_lastCyc = cyc;
  return g_cycTotal / g_cyclesPerUs;
}

// -------------------------------------------------------------- protocol ---

static const uint8_t CMD_SYSTEM = 0x48;  // followed by the display control byte
static const uint8_t CMD_GETKEY_MASK = 0xF9;  // command is 0100_1XX1
static const uint8_t CMD_GETKEY = 0x49;
static const uint8_t KEY_NONE = 0x2E;

static bool isDigitAddress(uint8_t b) {
  return b == 0x68 || b == 0x6A || b == 0x6C || b == 0x6E;
}

static bool isGetKey(uint8_t b) { return (b & CMD_GETKEY_MASK) == CMD_GETKEY; }

// Segment bit order: A=b0 B=b1 C=b2 D=b3 E=b4 F=b5 G=b6 DP=b7
static const uint8_t SEG_DIGIT[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                      0x6D, 0x7D, 0x07, 0x7F, 0x6F};

static char segToChar(uint8_t seg) {
  uint8_t s = seg & 0x7F;  // ignore decimal point
  if (s == 0x00) return ' ';
  for (uint8_t i = 0; i < 10; i++) {
    if (s == SEG_DIGIT[i]) return '0' + i;
  }
  return '?';  // letter, symbol, or a pattern we have not seen yet
}

// ---------------------------------------------------------- display state ---
//
// Digits are shown in DIG1..DIG4 order. Whether that matches left-to-right on
// the physical panel is an open question (see docs/PROTOCOLO.md); if the
// captured string comes out reversed, that is the answer, not a bug.

static uint8_t g_digit[4] = {0, 0, 0, 0};
static bool g_digitSeen[4] = {false, false, false, false};
static char g_lastShown[16] = {0};
static uint8_t g_lastKeyByte = 0xFF;  // 0xFF = nothing seen yet

static void renderDisplay(char *out, size_t n) {
  size_t k = 0;
  for (uint8_t i = 0; i < 4 && k + 2 < n; i++) {
    if (!g_digitSeen[i]) continue;
    out[k++] = segToChar(g_digit[i]);
    if (g_digit[i] & 0x80) out[k++] = '.';
  }
  out[k] = 0;
}

// ------------------------------------------------- edge interval histogram ---
//
// Tells a genuinely fast bus apart from one real edge being counted twice.
//
// The resistive probe rounds the edges. A slow enough rise, with any noise on
// it, can cross the GPIO threshold more than once, and every crossing is an
// interrupt. The symptom is indistinguishable from a fast bus if you only look
// at the shortest clock period: both report a small number. The difference is
// in the DISTRIBUTION. A real bus cannot produce two edges 200 ns apart; a
// bouncing threshold does it constantly.
//
// Computed in loop() from the queued timestamps, never in the ISR — adding work
// to the ISR is what makes edges coalesce in the first place.

static const uint8_t HIST_N = 7;
static const uint32_t HIST_NS[HIST_N] = {250, 500, 1000, 2000, 5000, 10000, 0};
static const char *HIST_LABEL[HIST_N] = {"< 0.25", "< 0.5 ", "< 1   ",
                                         "< 2   ", "< 5   ", "< 10  ", ">= 10 "};
static uint32_t g_histLimitCyc[HIST_N];  // filled in setup(), once CPU MHz known
static uint32_t g_hist[HIST_N];
static uint32_t g_prevEdgeCyc = 0;
static bool g_havePrevEdge = false;

static void histEdge(uint32_t cyc) {
  if (g_havePrevEdge) {
    uint32_t d = cyc - g_prevEdgeCyc;
    uint8_t i = 0;
    while (i < HIST_N - 1 && d >= g_histLimitCyc[i]) i++;
    g_hist[i]++;
  }
  g_prevEdgeCyc = cyc;
  g_havePrevEdge = true;
}

// ---------------------------------------------------------------- output ---

static bool g_raw = true;  // dump every transaction; toggle with 'r'

static void printStamp(uint64_t us) {
  char buf[24];
  snprintf(buf, sizeof(buf), "[%6llu.%06llu] ", (unsigned long long)(us / 1000000),
           (unsigned long long)(us % 1000000));
  Serial.print(buf);
}

static void describeDisplayControl(uint8_t b) {
  char buf[80];
  snprintf(buf, sizeof(buf), "CTRL display=%s sleep=%s seg=%d brightness=%u",
           (b & 0x01) ? "ON" : "OFF", (b & 0x04) ? "YES" : "no",
           (b & 0x08) ? 7 : 8, (unsigned)((b >> 4) & 0x07));
  Serial.print(buf);
}

static void describeKey(uint8_t b) {
  if (b == KEY_NONE) {
    Serial.print("KEY none");
    return;
  }
  if (!(b & 0x40)) {
    Serial.print("KEY released/idle");
    return;
  }
  uint8_t ki = (b >> 3) & 0x07;
  uint8_t dig = (b & 0x03) + 1;
  char buf[64];
  if (ki == 0x07) {
    snprintf(buf, sizeof(buf), "KEY PRESSED KI1+KI2 / DIG%u", dig);
  } else {
    snprintf(buf, sizeof(buf), "KEY PRESSED KI%u / DIG%u", ki + 1, dig);
  }
  Serial.print(buf);
}

// ------------------------------------------------------------- statistics ---

static uint32_t g_txCount = 0;
static uint32_t g_txMalformed = 0;
static uint32_t g_txRepeatedStart = 0;
// Transactions cut in half by the end of a recording. Not protocol errors —
// ours. Counted separately so `malformed` keeps meaning "the bus said something
// we could not read".
static uint32_t g_txTruncated = 0;
static uint32_t g_keyReads = 0;
static uint32_t g_digitWrites = 0;
static uint32_t g_ctrlWrites = 0;
static uint32_t g_minClkPeriodCyc = 0xFFFFFFFF;  // in cycles, not microseconds

// ---------------------------------------------------------------- decoder ---

struct Decoder {
  uint8_t prevClk = 1;
  uint8_t prevDio = 1;
  bool inFrame = false;
  uint8_t bitCount = 0;
  uint8_t shift = 0;
  uint8_t bytes[8] = {0};
  uint8_t acks[8] = {0};
  uint8_t nBytes = 0;
  uint64_t startUs = 0;
  uint32_t prevClkRiseCyc = 0;
  bool haveClkRise = false;
};

static Decoder g_dec;

static void emitTransaction(bool repeatedStart) {
  g_txCount++;
  if (repeatedStart) g_txRepeatedStart++;

  // Every AiP650E transaction is 16 bits: command/address + data, each with
  // its ACK. Anything else means we lost an edge.
  bool malformed = (g_dec.nBytes != 2);
  if (malformed) g_txMalformed++;

  if (g_raw || malformed || repeatedStart) {
    printStamp(g_dec.startUs);
    Serial.print("S ");
    for (uint8_t i = 0; i < g_dec.nBytes; i++) {
      char buf[12];
      snprintf(buf, sizeof(buf), "%02X%s ", g_dec.bytes[i],
               g_dec.acks[i] ? "-" : "a");  // 'a' = ACK low (acknowledged)
      Serial.print(buf);
    }
    Serial.print(repeatedStart ? "Sr | " : "P  | ");

    if (malformed) {
      char buf[56];
      snprintf(buf, sizeof(buf), "MALFORMED: %u byte(s), expected 2",
               g_dec.nBytes);
      Serial.print(buf);
    } else {
      uint8_t cmd = g_dec.bytes[0];
      uint8_t dat = g_dec.bytes[1];
      if (cmd == CMD_SYSTEM) {
        describeDisplayControl(dat);
      } else if (isDigitAddress(cmd)) {
        char buf[40];
        snprintf(buf, sizeof(buf), "DIG%u seg=0x%02X '%c'%s",
                 (unsigned)((cmd - 0x68) / 2 + 1), dat, segToChar(dat),
                 (dat & 0x80) ? " +DP" : "");
        Serial.print(buf);
      } else if (isGetKey(cmd)) {
        describeKey(dat);
      } else {
        Serial.print("unknown command");
      }
    }
    if (repeatedStart) Serial.print("   (ended by repeated START, no STOP)");
    Serial.println();
  }

  if (!malformed) {
    uint8_t cmd = g_dec.bytes[0];
    uint8_t dat = g_dec.bytes[1];
    if (isDigitAddress(cmd)) {
      uint8_t idx = (cmd - 0x68) / 2;
      g_digit[idx] = dat;
      g_digitSeen[idx] = true;
      g_digitWrites++;

      char shown[16];
      renderDisplay(shown, sizeof(shown));
      if (strcmp(shown, g_lastShown) != 0) {
        strncpy(g_lastShown, shown, sizeof(g_lastShown) - 1);
        g_lastShown[sizeof(g_lastShown) - 1] = 0;
        printStamp(g_dec.startUs);
        Serial.print(">>> DISPLAY: \"");
        Serial.print(shown);
        Serial.println("\"");
      }
    } else if (isGetKey(cmd)) {
      g_keyReads++;
      // Only report changes. The box polls the keyboard every ~40 ms, so
      // printing every poll would bury everything else and make holding a
      // button unreadable.
      if (dat != g_lastKeyByte) {
        bool firstEver = (g_lastKeyByte == 0xFF);
        g_lastKeyByte = dat;
        if (!firstEver || dat != KEY_NONE) {
          printStamp(g_dec.startUs);
          Serial.print(">>> ");
          describeKey(dat);
          // If we caused this, say so. A channel just soldered to the wrong
          // button shows up here immediately instead of much later.
          // Bit 0x40 is what marks a key as actually pressed; without it the
          // byte is idle. Checked on 2026-08-21 before widening this test to
          // "anything but idle" — that would have blamed a channel for every
          // resting poll. 0x17 and 0x27 are both idle, not unknown keys.
          if (g_lastPulsedCh >= 0 && (dat & 0x40) &&
              (int32_t)(millis() - g_watchKeyUntilMs) < 0) {
            char b2[72];
            snprintf(b2, sizeof(b2), "   <== CHANNEL %u answered with 0x%02X",
                     (unsigned)(g_lastPulsedCh + 1), dat);
            Serial.print(b2);
            g_lastPulsedCh = -1;
          }
          Serial.println();
        }
      }
    } else if (cmd == CMD_SYSTEM) {
      g_ctrlWrites++;
    }
  }

  g_dec.nBytes = 0;
  g_dec.bitCount = 0;
  g_dec.shift = 0;
  g_dec.inFrame = false;
}

static void feed(uint8_t clk, uint8_t dio, uint64_t us, uint32_t cyc) {
  // START / STOP: DIO moves while CLK stays high.
  if (clk == 1 && g_dec.prevClk == 1 && dio != g_dec.prevDio) {
    if (dio == 0) {  // high -> low = START
      // A repeated START ends the previous transaction without a STOP. Emit it
      // instead of discarding it silently, or a whole transaction disappears
      // from the capture with nothing to show it ever existed.
      if (g_dec.inFrame && g_dec.nBytes > 0) emitTransaction(true);
      g_dec.inFrame = true;
      g_dec.nBytes = 0;
      g_dec.bitCount = 0;
      g_dec.shift = 0;
      g_dec.startUs = us;
      // Clock-period measurement restarts with every transaction. Without this,
      // the first rising edge of a frame measures back to the last edge of the
      // PREVIOUS frame, across the idle gap. If that gap outlasts a 32-bit
      // cycle wrap (17.9 s — exactly what happens when the control box sleeps
      // the chip, ADR-012) the subtraction wraps and poisons the running
      // minimum with a bogus tiny value, which is the one number that decides
      // whether the divider is good enough.
      g_dec.haveClkRise = false;
    } else {  // low -> high = STOP
      if (g_dec.inFrame) emitTransaction(false);
    }
  }

  // Data is latched on the rising edge of CLK.
  if (clk == 1 && g_dec.prevClk == 0) {
    // Measure the clock period only inside a frame: the gap between
    // transactions is idle time, not clock speed, and folding it in would also
    // let a counter wrap produce a bogus minimum.
    if (g_dec.inFrame && g_dec.haveClkRise) {
      uint32_t period = cyc - g_dec.prevClkRiseCyc;
      if (period > 0 && period < g_minClkPeriodCyc) g_minClkPeriodCyc = period;
    }
    g_dec.prevClkRiseCyc = cyc;
    g_dec.haveClkRise = true;

    if (g_dec.inFrame) {
      if (g_dec.bitCount < 8) {
        g_dec.shift = (uint8_t)((g_dec.shift << 1) | dio);  // MSB first
        g_dec.bitCount++;
      } else {  // ninth clock is the ACK
        if (g_dec.nBytes < sizeof(g_dec.bytes)) {
          g_dec.bytes[g_dec.nBytes] = g_dec.shift;
          g_dec.acks[g_dec.nBytes] = dio;
          g_dec.nBytes++;
        }
        g_dec.bitCount = 0;
        g_dec.shift = 0;
      }
    }
  }

  g_dec.prevClk = clk;
  g_dec.prevDio = dio;
}

// ------------------------------------------------------- burst capture ---
//
// Waits for the bus to wake up, then records both lines at a fixed rate.
// Returns the number of samples recorded, or 0 if the bus stayed quiet.
//
// Interrupts are off during the recording so the pacing cannot slip. Ten
// milliseconds is short enough that no watchdog notices and long enough to hold
// dozens of transactions on a bus that is busy — and the trigger below
// guarantees we only ever record when it is.

static uint32_t g_burstStartCyc = 0;
static uint32_t g_samplePeriodCyc = 60;  // set in setup() from the real CPU clock

static uint32_t captureBurst(uint32_t waitMs) {
  // Arm: sit on the idle state until something pulls a line low. Without this
  // we would mostly record silence — measured traffic is about 23 transactions
  // a second, so a blind 10 ms window would miss four times out of five.
  uint32_t deadline = cycleCount() + g_cyclesPerUs * 1000UL * waitMs;
  while (sampleLines(GPIO.in) == 0x03) {
    if ((int32_t)(cycleCount() - deadline) >= 0) {
      g_burstsEmpty++;
      return 0;
    }
  }

  const uint32_t period = g_samplePeriodCyc;
  noInterrupts();
  uint32_t t = cycleCount();
  g_burstStartCyc = t;
  for (uint32_t i = 0; i < BURST_N; i++) {
    while ((int32_t)(cycleCount() - t) < 0) {
    }
    g_burst[i] = sampleLines(GPIO.in);
    // If we are already past the next slot, the loop ran late and the timeline
    // is no longer uniform. Count it rather than pretend otherwise.
    if ((int32_t)(cycleCount() - (t + period)) >= 0) g_dropped++;
    t += period;
  }
  interrupts();

  g_bursts++;
  return BURST_N;
}

// Replays a recording through the same decoder the interrupt version fed, so
// the protocol logic is unchanged and stays comparable across both front-ends.
static void decodeBurst(uint32_t n) {
  if (n < 2) return;
  uint8_t prev = g_burst[0];

  // Reseed the decoder from the first sample of THIS recording, and throw away
  // anything it thought was in flight.
  //
  // Between one burst and the next the bus keeps moving and we are not
  // watching. The decoder state is global, so without this it compares the
  // first transition it sees against a level from milliseconds ago and invents
  // a START or a STOP at the seam. That produced a steady 0.7-1% of "0 byte"
  // transactions in the captures of 2026-08-06 — 171 of them, every single one
  // with zero bytes, all of them ours and none of them real traffic.
  //
  // It matters beyond tidiness: `malformed` is the counter that tells us
  // whether capture is healthy. A counter polluted by our own instrument
  // cannot do that job.
  if (g_dec.inFrame && g_dec.nBytes > 0) g_txTruncated++;
  g_dec.inFrame = false;
  g_dec.nBytes = 0;
  g_dec.bitCount = 0;
  g_dec.shift = 0;
  g_dec.haveClkRise = false;
  g_dec.prevClk = (uint8_t)(prev & 1);
  g_dec.prevDio = (uint8_t)((prev >> 1) & 1);
  for (uint32_t i = 1; i < n; i++) {
    uint8_t v = g_burst[i];
    if (v == prev) continue;
    uint32_t cyc = g_burstStartCyc + i * g_samplePeriodCyc;
    g_edges++;
    histEdge(cyc);
    feed((uint8_t)(v & 1), (uint8_t)((v >> 1) & 1), stampMicros(cyc), cyc);
    prev = v;
  }
}

// ------------------------------------------------------------ line check ---
//
// The wiring is known (red=CLK on GPIO18, green=DIO on GPIO4), but a swapped
// pair, or one wire not making contact, are the two most likely mistakes and
// both produce garbage that looks like a protocol problem. Counting transitions
// tells them apart: CLK carries a burst of pulses per transaction, DIO changes
// at most once per bit, and a dead wire shows zero.

static void checkLines() {
  Serial.println("Identifying lines (recording up to 2 s of traffic)...");
  uint32_t nClk = 0, nDio = 0;

  uint32_t n = captureBurst(2000);
  if (n >= 2) {
    uint8_t prev = g_burst[0];
    for (uint32_t i = 1; i < n; i++) {
      uint8_t v = g_burst[i];
      if ((v & 1) != (prev & 1)) nClk++;
      if ((v & 2) != (prev & 2)) nDio++;
      prev = v;
    }
  }

  char buf[96];
  snprintf(buf, sizeof(buf), "  GPIO%d (expected CLK, red)  : %lu edges", PIN_CLK,
           (unsigned long)nClk);
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  GPIO%d (expected DIO, green): %lu edges", PIN_DIO,
           (unsigned long)nDio);
  Serial.println(buf);

  if (nClk == 0 && nDio == 0) {
    Serial.println("  !! No activity on either line.");
    Serial.println("     Check GND, check the dividers, check the desk is on.");
  } else if (nClk == 0) {
    Serial.println("  !! CLK is dead while DIO moves.");
    Serial.println("     Broken joint on the red wire, or the divider is not");
    Serial.println("     connected. The pin itself is fine on any module.");
  } else if (nDio == 0) {
    Serial.println("  !! DIO is dead while CLK moves. Every byte will read as");
    Serial.println("     0x00 or 0xFF. Broken joint on the green wire.");
  } else if (nClk < nDio) {
    Serial.println("  !! DIO shows more edges than CLK — the lines look SWAPPED.");
    Serial.println("     Swap the two wires, or swap PIN_CLK/PIN_DIO and reflash.");
  } else {
    Serial.println("  OK: both lines active, edge counts match the expected wiring.");
  }
  Serial.println();
}

// ----------------------------------------------------------------- stats ---

static void printStats() {
  char buf[140];
  Serial.println();
  Serial.println("--- stats ---");
  snprintf(buf, sizeof(buf), "  bursts         : %lu recorded, %lu armed on a quiet bus",
           (unsigned long)g_bursts, (unsigned long)g_burstsEmpty);
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  edges captured : %lu", (unsigned long)g_edges);
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  late samples   : %lu%s", (unsigned long)g_dropped,
           g_dropped ? "   <-- pacing slipped: the timeline is not uniform" : "");
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  transactions   : %lu (%lu malformed, %lu ended by repeated START)",
           (unsigned long)g_txCount, (unsigned long)g_txMalformed,
           (unsigned long)g_txRepeatedStart);
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  cut by burst   : %lu   (ours, not the bus)",
           (unsigned long)g_txTruncated);
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  digit writes   : %lu", (unsigned long)g_digitWrites);
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  key reads      : %lu", (unsigned long)g_keyReads);
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  ctrl writes    : %lu", (unsigned long)g_ctrlWrites);
  Serial.println(buf);
  if (g_minClkPeriodCyc != 0xFFFFFFFF && g_cyclesPerUs > 0) {
    unsigned long ns = (unsigned long)((uint64_t)g_minClkPeriodCyc * 1000 / g_cyclesPerUs);
    unsigned long khz = ns ? (unsigned long)(1000000UL / ns) : 0;
    snprintf(buf, sizeof(buf), "  fastest clock  : %lu ns period (~%lu kHz)", ns, khz);
    Serial.println(buf);
  }
  snprintf(buf, sizeof(buf), "  display now    : \"%s\"", g_lastShown);
  Serial.println(buf);

  // Interval between consecutive edges, whichever line they came from.
  uint32_t total = 0, impossible = 0;
  for (uint8_t i = 0; i < HIST_N; i++) total += g_hist[i];
  impossible = g_hist[0] + g_hist[1];  // under 0.5 us

  Serial.println("  edge intervals (us):");
  for (uint8_t i = 0; i < HIST_N; i++) {
    uint32_t pct = total ? (uint32_t)((uint64_t)g_hist[i] * 100 / total) : 0;
    snprintf(buf, sizeof(buf), "    %s : %8lu  (%2lu%%)", HIST_LABEL[i],
             (unsigned long)g_hist[i], (unsigned long)pct);
    Serial.println(buf);
  }

  if (total) {
    uint32_t pct = (uint32_t)((uint64_t)impossible * 100 / total);
    if (pct >= 10) {
      snprintf(buf, sizeof(buf),
               "  >> %lu%% of edges arrive under 0.5 us apart. No real bus does", (unsigned long)pct);
      Serial.println(buf);
      Serial.println("     that: these are single edges counted twice, and the");
      Serial.println("     divider is rounding them. The clock rate above is");
      Serial.println("     inflated by roughly the same factor.");
    } else {
      Serial.println("  >> Interval distribution is clean: no double-counted edges.");
      Serial.println("     The clock rate above can be trusted.");
    }
  }
  Serial.println("-------------");
  Serial.println();
}

static void printHelp() {
  Serial.println();
  Serial.println("  r  toggle raw transaction dump");
  Serial.println("  s  print statistics");
  Serial.println("  c  reset statistics");
  Serial.println("  l  re-run the line identification check");
  Serial.println("  h  this help");
  Serial.println();
  Serial.println("  1 2 3 4   pulse that actuation channel for 300 ms");
  Serial.println("            and report which key the control box saw");
  Serial.println();
}

// ------------------------------------------------------------------ main ---

// Folds elapsed cycles into the timestamp base while the bus is silent, so the
// 32-bit cycle counter never wraps unnoticed between two events.
static void keepClockAlive() {
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if ((uint32_t)(now - lastMs) >= 5000) {
    lastMs = now;
    if (g_haveLastCyc) stampMicros(cycleCount());
  }
}

void setup() {
  // FIRST of all, before anything can take time: every actuation channel low.
  // Between reset and this line the pins are inputs and cannot source the
  // current the optocoupler LED needs, which is what makes a reset safe
  // (ADR-024). This closes the remaining window as early as the program can.
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(CH_PIN[i], OUTPUT);
    digitalWrite(CH_PIN[i], LOW);
  }

#if defined(ARDUINO_ARCH_ESP32)
  Serial.setTxBufferSize(4096);  // must come before begin(); absorbs bursts
#endif
  Serial.begin(SERIAL_BAUD);
  delay(300);

  // ---- the only functional difference from desk_sniffer ----
  WiFi.mode(WIFI_AP);
  WiFi.softAP("desk-sniffer-test", "midesk1234");
  Serial.print("\n[WiFi AP up at ");
  Serial.print(WiFi.softAPIP());
  Serial.println("]");

  g_cyclesPerUs = getCpuFrequencyMhz();
  if (g_cyclesPerUs == 0) g_cyclesPerUs = 240;  // never divide by zero

  // Histogram bucket edges, in cycles. The last one is the catch-all.
  for (uint8_t i = 0; i < HIST_N; i++) {
    g_histLimitCyc[i] = HIST_NS[i] ? HIST_NS[i] * g_cyclesPerUs / 1000 : 0xFFFFFFFF;
  }

  g_samplePeriodCyc = g_cyclesPerUs / SAMPLE_MHZ;
  if (g_samplePeriodCyc == 0) g_samplePeriodCyc = 1;

  pinMode(PIN_CLK, INPUT);  // no pull-up: the bus has its own, inside the chip
  pinMode(PIN_DIO, INPUT);

  Serial.println();
  Serial.println("=== desk_sniffer — AiP650E passive bus sniffer ===");
  char buf[120];
  snprintf(buf, sizeof(buf), "CLK=GPIO%d (red)  DIO=GPIO%d (green)  CPU=%u MHz",
           PIN_CLK, PIN_DIO, (unsigned)g_cyclesPerUs);
  Serial.println(buf);
  snprintf(buf, sizeof(buf),
           "Burst sampling at %u MHz, %u samples per burst (%u ms).",
           (unsigned)SAMPLE_MHZ, (unsigned)BURST_N,
           (unsigned)(BURST_N / (SAMPLE_MHZ * 1000)));
  Serial.println(buf);
  Serial.println("Read-only. This firmware never drives the bus.");
  printHelp();

  checkLines();

  Serial.println("Listening. Format:");
  Serial.println("  [time] S <byte><ack> <byte><ack> P | interpretation");
  Serial.println("  ack: 'a' = acknowledged (low), '-' = line high");
  Serial.println("  On key reads a '-' on the second byte is normal, not a fault.");
  Serial.println();
}

void loop() {
  // Record, then decode. Serial is only touched between bursts, so printing can
  // never steal time from the sampling — which was the first thing suspected of
  // breaking the interrupt version, and turned out not to be, but is designed
  // out here anyway.
  uint32_t n = captureBurst(250);
  if (n) {
    decodeBurst(n);
  } else {
    keepClockAlive();  // bus quiet: keep the timestamp base from wrapping
  }

  if (Serial.available()) {
    // Echo every byte received, always. This distinguishes "the command never
    // arrived" from "it arrived and did nothing" — the two were confused twice
    // on 2026-08-21, once blaming a channel for what was a serial fault.
    // Costs 64 bytes of program. See ADR-026.
    int rx = Serial.read();
    char rxb[48];
    snprintf(rxb, sizeof(rxb), "\n[RX 0x%02X '%c']", rx,
             (rx >= 32 && rx < 127) ? rx : '.');
    Serial.println(rxb);

    switch (rx) {
      case 'r':
        g_raw = !g_raw;
        Serial.println(g_raw ? "\n[raw dump ON]\n" : "\n[raw dump OFF]\n");
        break;
      case 's': printStats(); break;
      case 'c':
        g_edges = 0;
        g_dropped = 0;
        g_txCount = 0;
        g_txMalformed = 0;
        g_txRepeatedStart = 0;
        g_txTruncated = 0;
        g_keyReads = 0;
        g_digitWrites = 0;
        g_ctrlWrites = 0;
        g_minClkPeriodCyc = 0xFFFFFFFF;
        for (uint8_t i = 0; i < HIST_N; i++) g_hist[i] = 0;
        g_havePrevEdge = false;
        g_bursts = 0;
        g_burstsEmpty = 0;
        Serial.println("\n[stats reset]\n");
        break;
      case '1': pulseChannel(0); break;
      case '2': pulseChannel(1); break;
      case '3': pulseChannel(2); break;
      case '4': pulseChannel(3); break;
      // Long pulses, upper case. Separate keys on purpose: a typo must not be
      // able to start continuous travel.
      case 'A': pulseChannelLong(0); break;
      case 'B': pulseChannelLong(1); break;
      case 'C': pulseChannelLong(2); break;
      case 'D': pulseChannelLong(3); break;
      case 'l': checkLines(); break;
      case 'h': printHelp(); break;
      default: break;
    }
  }
}
