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
// ------------------------------------------------------------------ MQTT ---
//
// Transport to Home Assistant is MQTT, not ESPHome: this sniffer BLOCKS -- up
// to 250 ms arming a burst, 2800 ms during a long pulse -- and ESPHome expects
// components to return promptly. Porting it would mean rewriting the timing
// critical part as a state machine. See ADR-030.
//
// The radio was measured NOT to degrade capture (0.67% -> 0.93% malformed,
// within the documented noise floor) but that was soft-AP with no clients.
// Publishing over a real association still has to be checked against the same
// baseline. See docs/capturas/2026-08-22-wifi-impacto.log.
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include "secrets.h"

// --------------------------------------------------------------------- OTA ---
//
// Firmware updates over the network. The board lives on a wall charger, so
// without this every fix means unplugging the desk, moving the USB to the Mac
// and back -- and that dance is itself the ADR-019 hazard (ESP32 unpowered with
// the probe on a live bus).
//
// DANGER, and it is specific to this project: applying an update REBOOTS the
// chip. Reset puts the GPIOs back to input, which OPENS the channels -- and
// opening a contact does NOT stop continuous travel (measured: 6 cm in 6.6 s
// after release). An update mid-travel would leave the desk running to its end
// stop with nobody supervising. So OTA is REFUSED while the desk moves.
//
// The update itself is safe against interruption: the ESP32 writes to the
// inactive partition and only switches the boot target once the whole image is
// verified. A failed transfer leaves the running firmware untouched.
static bool g_otaBusy = false;

static WiFiClient g_net;
static PubSubClient g_mqtt(g_net);

// Height decoded from the display, and WHEN it was decoded. The pair travels
// together on purpose: a height without its age is not usable for deciding
// anything (ADR-012), because the display sleeps and then the last known value
// looks exactly like a current one.
static int32_t  g_heightCm = -1;
static uint32_t g_heightMs = 0;

static const int PIN_CLK = 18;  // red wire
static const int PIN_DIO = 4;   // green wire
static const uint32_t SERIAL_BAUD = 115200;  // ADR-026: 460800 did not receive either

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
static const uint32_t PULSE_MS = 800;  // ADR-027: 300 ms does not move the desk

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

// Wake pulse: long enough for the chip to register the key (160 ms minimum,
// datasheet) but SHORT ENOUGH NOT TO MOVE THE DESK. Measured 2026-08-21: seven
// 300 ms taps in a row moved the desk zero centimetres, while the display woke
// up. That combination is what makes it usable as a refresh.
//
// It matters because the display sleeps on inactivity, and a sleeping display
// means the four digits read 0x00 -- the height is simply not on the bus. After
// a power cut, or after any idle spell, this is how the ESP32 finds out where
// the desk actually is without moving it.
//
// NEVER on a memory channel: any tap on M1/M2 starts a trip to that preset.
// ------------------------------------------------------- travel & limits ---
//
// SOFTWARE LIMITS. This is the condition ADR-028 set for letting continuous
// travel be triggered without someone watching: the supervision moves from the
// human to the ESP32.
//
// Continuous travel cannot be stopped by releasing a contact -- the desk kept
// going 6 cm in 6.6 s after release. Stopping means CLOSING a contact again.
// So everything below is about deciding WHEN to close it.
//
// Every one of these is a reason to brake. None is optional:
//   - reaching the target, or the configured limit
//   - the height reading going stale (display asleep, bus quiet) -> ADR-012
//   - the height not advancing (top reached, or something jammed)
//   - a hard time cap, whatever else happens
//
// What this does NOT protect against: the ESP32 dying mid-travel. Nothing on
// this board can, because braking requires code to run. The physical end stops
// and the handset remain the last line.
static const int32_t  LIMIT_MIN_CM   = 73;    // physical bottom stop
static const int32_t  LIMIT_MAX_CM   = 118;   // physical top stop
static const uint32_t TRAVEL_MAX_MS  = 90000; // 45 cm at 0.68 cm/s is ~66 s
static const uint32_t STALE_MS       = 4000;  // height older than this = blind
static const uint32_t STALL_MS       = 9000;  // no progress = stop or jam
static const int32_t  BRAKE_LEAD_CM  = 1;     // ~1 cm of coasting after braking

enum Motion : uint8_t { MOTION_IDLE = 0, MOTION_UP, MOTION_DOWN, MOTION_BRAKING };
// Review B5: braking used to be open-loop -- stopTravel declared IDLE before
// tapping and never checked the desk actually stopped. If the tap failed to
// register (the class of miss that made a channel look dead on 2026-08-21),
// supervision was already disarmed over a desk still moving. BRAKING keeps
// watching: height must settle within a window, one retry, then give up loudly.
static uint32_t g_brakeStartMs = 0;
static bool     g_brakeRetried = false;
static Motion   g_motion      = MOTION_IDLE;
static int32_t  g_target      = -1;      // -1 = travel until a limit
static uint32_t g_travelStart = 0;
static int32_t  g_progressH   = -1;
static int32_t  g_startH      = -1;   // height when the travel began (B3)
static uint32_t g_progressMs  = 0;
static char     g_stopReason[40] = "";

// Fine adjustment after braking. Braking anticipates BRAKE_LEAD_CM because the
// desk coasts, but the coasting is not always the same centimetre: on
// 2026-08-22 a trip to 95 braked at 96 and simply stayed there. Taps close the
// gap -- that is what the Python loop did when it hit every target exactly.
static int32_t  g_fineTarget = -1;
static uint32_t g_fineNextMs = 0;
static uint8_t  g_fineTries  = 0;

static const uint32_t WAKE_MS = 300;
static const uint8_t WAKE_CH = 1;  // channel 2, down. Never 3 or 4.

// Which channel was pulsed last, and until when we still care. After a pulse we
// watch the bus for the key byte the control box reports, and print it next to
// the channel that caused it. That is how a freshly soldered channel is checked
// against PROTOCOLO.md without guessing: press it, read which button answered.
// A key pressed on the BUS that we did not cause. The sniffer sees every key
// the control box reads, ours and the human's alike, so "a pressed key nobody
// is claiming" means somebody is using the handset. Set from the decoder,
// consumed by the main loop.
static volatile bool    g_manualKey = false;
static volatile uint8_t g_manualKeyByte = 0;
// 64-bit uptime for long-lived ages: millis() wraps at ~49.7 days, and the
// "seconds since last manual use" sensor is meant to be trusted by automations
// for exactly that kind of timescale (review 2026-08-23).
static inline uint64_t uptimeMs64() { return (uint64_t)(esp_timer_get_time() / 1000ULL); }
static uint64_t         g_lastManualMs = 0;   // when the handset was last used

static int8_t g_lastPulsedCh = -1;
static uint32_t g_watchKeyUntilMs = 0;

static void allChannelsOff() {
  for (uint8_t i = 0; i < 4; i++) digitalWrite(CH_PIN[i], LOW);
}

// REAL watchdog around every pulse (finding B1 of the 2026-08-23 review).
// ADR-024 claimed "the ESP32 watchdog implements the pulse limiter" -- but no
// watchdog was ever configured, so the only cap on a stuck-high pin was the
// same software that could hang. This arms the task WDT for the duration of
// the contact: if the CPU wedges inside a pulse, the WDT resets the chip,
// reset puts every GPIO back to input, and the channel opens. The budget is
// the pulse width plus slack -- generous for a hang, far below the 2.2 s
// (continuous travel) and 3.0 s (preset overwrite) thresholds for the taps,
// and for the long pulse it caps a hang at width+1s instead of forever.
#include "esp_task_wdt.h"

static void wdtArm(uint32_t budgetMs) {
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = budgetMs;
  cfg.idle_core_mask = 0;          // watch only the task we add
  cfg.trigger_panic = true;        // panic -> reset -> GPIOs go back to input
  if (esp_task_wdt_init(&cfg) != ESP_OK) esp_task_wdt_reconfigure(&cfg);
  esp_task_wdt_add(NULL);          // current task (loopTask)
}

static void wdtDisarm() {
  esp_task_wdt_delete(NULL);
  esp_task_wdt_deinit();
}

static void pulseChannelFor(uint8_t idx, uint32_t widthMs) {
  allChannelsOff();  // never two at once
  wdtArm(widthMs + 1000);   // a hang with the pin high ends in a chip reset
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

  // CORRECTED 2026-08-23 (adversarial review, finding B2): the first version
  // decoded AND PRINTED inside the pulse with a 20 ms budget per round. But
  // decodeBurst's Serial.print blocks once the TX buffer fills (~356 ms to
  // drain 4 KB at 115200 baud), so one round could stretch the contact far past
  // widthMs -- and the long pulse runs only 200 ms below the 3.0 s threshold
  // that OVERWRITES a preset. Releasing the pin must not depend on how long
  // printing takes.
  //
  // Now: capture during the contact, decode AFTER the pin is low. Only the
  // last burst is kept; the key byte repeats on every keyboard poll (~40 ms)
  // while held, so the last burst is enough for attribution.
  uint32_t lastN = 0;
  while ((uint32_t)(millis() - t0) < widthMs) {
    if ((uint32_t)(millis() - t0) + 6 < widthMs) {
      uint32_t n = captureBurst(3);   // arms up to 3 ms, records at most 2 ms
      if (n) lastN = n;               // decode deferred until pin is low
    }
  }
  digitalWrite(CH_PIN[idx], LOW);
  wdtDisarm();              // pin is low: a hang from here on is survivable
  uint32_t achievedMs = millis() - t0;

  if (lastN) decodeBurst(lastN);      // printing may block freely now

  // If our own key read was not in that burst, shrink the attribution window:
  // with the full 1.5 s open, a HUMAN press right after our tap would be
  // claimed as "CHANNEL n answered" and the give-way path skipped.
  if (g_lastPulsedCh >= 0) g_watchKeyUntilMs = millis() + 400;

  // The width is a safety bound, not a wish: measure it, and say so loudly if
  // it was exceeded. 2.2 s starts continuous travel; 3.0 s overwrites a preset.
  if (achievedMs > widthMs + 100) {
    Serial.printf("\n[!!] pulse overran: %lu ms wanted, %lu ms achieved\n",
                  (unsigned long)widthMs, (unsigned long)achievedMs);
  }

  char buf[96];
  snprintf(buf, sizeof(buf), "\n[channel %u -> GPIO%u pulsed %lu ms; watching the bus]",
           (unsigned)(idx + 1), (unsigned)CH_PIN[idx],
           (unsigned long)achievedMs);
  Serial.println(buf);
}

static void pulseChannel(uint8_t idx) { pulseChannelFor(idx, PULSE_MS); }

// Wakes the display so the height appears on the bus, without moving the desk.
static void wakeDisplay() {
  Serial.println("\n[wake: 300 ms tap, must NOT move the desk]");
  pulseChannelFor(WAKE_CH, WAKE_MS);
}

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
      // Pull a height out of it, if it is one. Only 73..118 is accepted: the
      // display shows partial frames while it refreshes -- "099" appears while
      // going from 089 to 090 -- and those must not be taken as real readings.
      //
      // THIS RUNS ON EVERY REFRESH, not only when the value changes. It used to
      // live inside the "display changed" branch, and that was wrong: with the
      // desk standing still the display keeps showing the same height forever,
      // so the reading aged into "stale" while it was in fact being received
      // continuously. It made the fine adjustment cancel itself silently, and
      // it made the age sensor measure time-since-last-change instead of
      // time-since-last-read.
      {
        // Review finding A2: range alone is not enough. The display refreshes
        // digit by digit, so composite frames appear -- going 089 -> 090 shows
        // "099" for one cycle, and 99 is inside 73..118. Those transients fed
        // the travel supervisor false positions (brake 6 cm early, fake
        // progress, fake limit hits).
        //
        // Rule: a value near the current height (<=3 cm, the most one refresh
        // cycle of real travel can cover) is accepted at once. A larger jump
        // must repeat on the NEXT cycle to be believed -- transients are
        // one-cycle artifacts and never repeat, while a real relocation (desk
        // moved by hand while the display slept) repeats forever and gets
        // accepted 200 ms later. First-ever reading is accepted as-is.
        int v = atoi(shown);
        if (v >= 73 && v <= 118) {
          static int16_t candidate = -1;
          if (g_heightCm < 0 || abs(v - (int)g_heightCm) <= 3 || v == candidate) {
            g_heightCm = v;
            g_heightMs = millis();
            candidate = -1;
          } else {
            candidate = (int16_t)v;   // ask it to prove itself next cycle
          }
        }
      }

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
          } else if (dat & 0x40) {
            // A pressed key with nobody claiming it: the person is using the
            // handset. Their press has already braked whatever we had running,
            // so insisting would be fighting the human -- and we would only
            // notice 9 s later through the stall brake, adding a tap nobody
            // asked for. Give way instead.
            g_manualKey = true;
            g_manualKeyByte = dat;
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
  Serial.println("  1 2 3 4   tap that channel (300 ms: registers, does NOT move)");
  Serial.println("            and report which key the control box saw");
  Serial.println("  A / B     supervised continuous travel up/down (limits,");
  Serial.println("            stall and stale-height brakes apply)");
  Serial.println("  w         wake the display to refresh the height,");
  Serial.println("            WITHOUT moving the desk");
  Serial.println();
}

// ---------------------------------------------------------- MQTT publish ---
//
// Design rule (docs/INTEGRACION_HA.md): THE ESP32 PUBLISHES FACTS, HOME
// ASSISTANT DERIVES STATISTICS. Time spent standing, daily minimum, movement
// counts -- all of that is HA's job. It already has a database; anything this
// board accumulates dies at the next reset.

// Commands arrive on one topic and are QUEUED, not executed inside the MQTT
// callback: the callback runs from inside g_mqtt.loop(), and firing a pulse
// from there would block the client for 800 ms and re-enter publishing. The
// main loop drains it, between bursts, exactly where serial commands run.
static char     g_pendingCmd[24] = {0};   // must hold "continuo_subir" (14) -- review A1

// Review C4: "parar" is the one command that must never be lost, and the queue
// is a single overwrite-on-arrival slot. Stop gets its own flag, set in the
// callback and honoured before anything else; nothing can overwrite it, and it
// deliberately bypasses the post-connect arming window -- a replayed stop is
// harmless, a lost one is not.
static volatile bool g_stopReq = false;


// Round-2 review: a retarget during travel must SURVIVE the braking phase.
// stopTravel leaves MOTION_BRAKING, so calling startTravel right after it is
// refused -- the first fix silently dropped the new target (C3, again). The
// target now waits here and fires once the brake is confirmed.
static int32_t g_retargetCm = -1;

// Commands are ignored for a moment after connecting. A RETAINED message on the
// command topic is delivered by the broker the instant we subscribe, so a
// reboot would replay whatever was last commanded and move the desk with nobody
// asking for it. Seen on 2026-08-22: an "ir:80" arrived right after
// [MQTT conectado] and started a trip.
static uint32_t g_cmdArmedAt = 0;
static const uint32_t CMD_ARM_DELAY_MS = 4000;
static uint32_t g_mqttNextTry = 0;
static uint32_t g_lastPublish = 0;
static bool     g_discoverySent = false;
static const uint32_t PUBLISH_EVERY_MS = 5000;

static String topic(const char *leaf) {
  return String(DEVICE_ID) + "/" + leaf;
}

// One discovery message per entity. The device block is repeated in each so HA
// groups them under a single device.
static void publishOne(const char *component, const char *object,
                       const char *name, const char *stateLeaf,
                       const char *unit, const char *devClass,
                       const char *stateClass, const char *category) {
  char cfg[640];
  snprintf(cfg, sizeof(cfg),
    "{\"name\":\"%s\",\"unique_id\":\"%s_%s\","
    "\"state_topic\":\"%s/%s\","
    "\"availability_topic\":\"%s/disponible\","
    "%s%s%s%s%s%s%s%s"
    "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\","
    "\"manufacturer\":\"Jiecang\",\"model\":\"JK-CH506 + ESP32\"}}",
    name, DEVICE_ID, object,
    DEVICE_ID, stateLeaf,
    DEVICE_ID,
    unit      ? "\"unit_of_measurement\":\"" : "", unit      ? unit      : "", unit      ? "\"," : "",
    devClass  ? "\"device_class\":\""         : "", devClass  ? devClass  : "", devClass  ? "\"," : "",
    stateClass? "\"state_class\":\"measurement\"," : "",
    category  ? "\"entity_category\":\"diagnostic\"," : "",
    DEVICE_ID, DEVICE_NAME);

  String t = String("homeassistant/") + component + "/" + DEVICE_ID + "_" + object + "/config";
  bool ok = g_mqtt.publish(t.c_str(), cfg, true);
  if (!ok) {
    Serial.printf("[discovery FALLO] %s (%u bytes de payload)\n", object, (unsigned)strlen(cfg));
  }
}

// Buttons. Deliberately only TAPS: no long pulse is exposed to Home Assistant.
// A tap cannot start continuous travel (2.2 s) nor overwrite a preset (3.0 s),
// so nothing reachable from a phone can put the desk in a state that needs the
// ESP32 alive to get out of. See ADR-023, ADR-027 and ADR-028.
static void publishButton(const char *object, const char *name,
                          const char *payload, const char *icon) {
  char cfg[560];
  snprintf(cfg, sizeof(cfg),
    "{\"name\":\"%s\",\"unique_id\":\"%s_%s\","
    "\"command_topic\":\"%s/cmd\",\"payload_press\":\"%s\","
    "\"availability_topic\":\"%s/disponible\",\"icon\":\"%s\","
    "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\","
    "\"manufacturer\":\"Jiecang\",\"model\":\"JK-CH506 + ESP32\"}}",
    name, DEVICE_ID, object, DEVICE_ID, payload, DEVICE_ID, icon,
    DEVICE_ID, DEVICE_NAME);
  String t = String("homeassistant/button/") + DEVICE_ID + "_" + object + "/config";
  bool ok = g_mqtt.publish(t.c_str(), cfg, true);
  Serial.printf("[discovery boton %s] %s (%u bytes)\n", object, ok ? "ok" : "FALLO",
                (unsigned)strlen(cfg));
}

static void publishDiscovery() {
  // "unknown" on a numeric sensor is a value error in HA; the template maps it
  // to a real unknown state. (First deployed broker-side on 2026-08-23 while
  // the board could not be reflashed; kept here as the source of truth.)
  {
    char cfg[640];
    snprintf(cfg, sizeof(cfg),
      "{\"name\":\"Altura\",\"unique_id\":\"%s_altura\","
      "\"state_topic\":\"%s/altura\",\"availability_topic\":\"%s/disponible\","
      "\"unit_of_measurement\":\"cm\",\"device_class\":\"distance\","
      "\"state_class\":\"measurement\","
      "\"value_template\":\"{{ none if value == 'unknown' else value }}\","
      "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\","
      "\"manufacturer\":\"Jiecang\",\"model\":\"JK-CH506 + ESP32\"}}",
      DEVICE_ID, DEVICE_ID, DEVICE_ID, DEVICE_ID, DEVICE_NAME);
    String t = String("homeassistant/sensor/") + DEVICE_ID + "_altura/config";
    g_mqtt.publish(t.c_str(), cfg, true);
  }
  // Age of the height reading. NOT decoration: ADR-012 says no movement starts
  // on a stale height, and this is what makes that checkable from HA.
  publishOne("sensor", "altura_edad", "Antiguedad de la altura", "altura_edad", "s", "duration", "1", "diag");
  publishOne("binary_sensor", "display", "Display despierto", "display", NULL, NULL, NULL, "diag");
  publishOne("sensor", "bus_malformadas", "Bus malformadas", "bus_malformadas", "%", NULL, "1", "diag");
  publishOne("sensor", "bus_transacciones", "Bus transacciones", "bus_transacciones", NULL, NULL, "1", "diag");
  publishOne("sensor", "rssi", "WiFi RSSI", "rssi", "dBm", "signal_strength", "1", "diag");
  publishOne("sensor", "ip", "IP", "ip", NULL, NULL, NULL, "diag");
  // Seconds since somebody last touched the handset. Lets an automation stay
  // out of the way ("do not move if it was used in the last hour").
  publishOne("sensor", "uso_manual", "Uso manual hace", "uso_manual", "s", "duration", "1", NULL);
  publishOne("sensor", "uptime", "Uptime", "uptime", "s", "duration", "1", "diag");

  publishButton("subir",  "Subir",      "subir",  "mdi:arrow-up-bold");
  publishButton("bajar",  "Bajar",      "bajar",  "mdi:arrow-down-bold");
  publishButton("m1",     "Memoria 1",  "m1",     "mdi:numeric-1-box");
  publishButton("m2",     "Memoria 2",  "m2",     "mdi:numeric-2-box");
  // Not optional: with continuous travel running, closing a contact is the ONLY
  // thing that stops the desk. If M1/M2 are reachable from a phone, so is this.
  publishButton("parar",  "Parar",      "parar",  "mdi:stop-circle");
  publishButton("refrescar", "Refrescar altura", "refrescar", "mdi:refresh");
  publishButton("cont_subir", "Subir continuo", "continuo_subir", "mdi:arrow-up-bold-box");
  publishButton("cont_bajar", "Bajar continuo", "continuo_bajar", "mdi:arrow-down-bold-box");

  // Type a height and it goes there. This is what replaces "press and hold":
  // holding depends on your finger and on the link staying up; this does not.
  {
    char cfg[620];
    snprintf(cfg, sizeof(cfg),
      "{\"name\":\"Ir a altura\",\"unique_id\":\"%s_altura_objetivo\","
      "\"command_topic\":\"%s/altura_objetivo/set\","
      "\"state_topic\":\"%s/altura_objetivo\","
      "\"availability_topic\":\"%s/disponible\","
      "\"min\":%ld,\"max\":%ld,\"step\":1,\"unit_of_measurement\":\"cm\","
      "\"mode\":\"box\",\"icon\":\"mdi:ruler\","
      "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\","
      "\"manufacturer\":\"Jiecang\",\"model\":\"JK-CH506 + ESP32\"}}",
      DEVICE_ID, DEVICE_ID, DEVICE_ID, DEVICE_ID,
      (long)LIMIT_MIN_CM, (long)LIMIT_MAX_CM, DEVICE_ID, DEVICE_NAME);
    String t = String("homeassistant/number/") + DEVICE_ID + "_altura_objetivo/config";
    g_mqtt.publish(t.c_str(), cfg, true);
  }

  // Online/offline as a visible entity. The state topic is the MQTT will:
  // if this board dies, THE BROKER publishes "offline" -- no firmware needed.
  // (First published straight to the broker on 2026-08-23 while the board ran
  // on a wall charger and could not be reflashed; kept here so the firmware
  // remains the source of truth for its own discovery.)
  {
    char cfg[560];
    snprintf(cfg, sizeof(cfg),
      "{\"name\":\"Online\",\"unique_id\":\"%s_online\","
      "\"state_topic\":\"%s/disponible\","
      "\"payload_on\":\"online\",\"payload_off\":\"offline\","
      "\"device_class\":\"connectivity\",\"entity_category\":\"diagnostic\","
      "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\","
      "\"manufacturer\":\"Jiecang\",\"model\":\"JK-CH506 + ESP32\"}}",
      DEVICE_ID, DEVICE_ID, DEVICE_ID, DEVICE_NAME);
    String t = String("homeassistant/binary_sensor/") + DEVICE_ID + "_online/config";
    g_mqtt.publish(t.c_str(), cfg, true);
  }

  publishOne("sensor", "movimiento", "Movimiento", "movimiento", NULL, NULL, NULL, NULL);
  // Why the last travel ended. Turns "it stopped" into something diagnosable.
  publishOne("sensor", "ultimo_freno", "Motivo del ultimo freno", "ultimo_freno", NULL, NULL, NULL, "diag");

  g_discoverySent = true;
}

static void publishState() {
  char b[24];

  // A height with no age is not publishable. If it was never read, say unknown
  // rather than invent one.
  // Review C6: a height was published retained forever, so HA consumed an
  // hours-old number presented exactly like a current one -- the confusion
  // ADR-012 exists to prevent. Stale (display asleep) now publishes "unknown";
  // the age sensor keeps the last-read timestamp for diagnosis.
  if (g_heightCm > 0 && heightFresh()) {
    snprintf(b, sizeof(b), "%ld", (long)g_heightCm);
    g_mqtt.publish(topic("altura").c_str(), b, true);
  } else {
    g_mqtt.publish(topic("altura").c_str(), "unknown", true);
  }
  if (g_heightCm > 0) {
    snprintf(b, sizeof(b), "%lu", (unsigned long)((millis() - g_heightMs) / 1000));
    g_mqtt.publish(topic("altura_edad").c_str(), b, true);
  }

  // Display asleep means the four digits are 0x00 and the height is simply not
  // on the bus.
  bool awake = false;
  for (uint8_t i = 0; i < 4; i++) if (g_digit[i] != 0x00) awake = true;
  g_mqtt.publish(topic("display").c_str(), awake ? "ON" : "OFF", true);

  float pct = g_txCount ? (100.0f * g_txMalformed / g_txCount) : 0.0f;
  snprintf(b, sizeof(b), "%.2f", pct);
  g_mqtt.publish(topic("bus_malformadas").c_str(), b, true);

  snprintf(b, sizeof(b), "%lu", (unsigned long)g_txCount);
  g_mqtt.publish(topic("bus_transacciones").c_str(), b, true);

  snprintf(b, sizeof(b), "%d", (int)WiFi.RSSI());
  g_mqtt.publish(topic("rssi").c_str(), b, true);

  g_mqtt.publish(topic("ip").c_str(), WiFi.localIP().toString().c_str(), true);

  if (g_lastManualMs) {
    snprintf(b, sizeof(b), "%llu",
             (unsigned long long)((uptimeMs64() - g_lastManualMs) / 1000ULL));
    g_mqtt.publish(topic("uso_manual").c_str(), b, true);
  }

  snprintf(b, sizeof(b), "%lu", (unsigned long)(millis() / 1000));
  g_mqtt.publish(topic("uptime").c_str(), b, true);

  g_mqtt.publish(topic("movimiento").c_str(),
                 g_motion == MOTION_UP      ? "subiendo" :
                 g_motion == MOTION_DOWN    ? "bajando"  :
                 g_motion == MOTION_BRAKING ? "frenando" : "quieto", true);
  g_mqtt.publish(topic("ultimo_freno").c_str(),
                 g_stopReason[0] ? g_stopReason : "-", true);
  // C6 continued: the target box no longer echoes a possibly-stale height.
  int32_t shownTarget = (g_target >= 0) ? g_target : g_fineTarget;
  if (shownTarget < 0 && g_heightCm > 0 && heightFresh()) shownTarget = g_heightCm;
  if (shownTarget >= 0) {
    snprintf(b, sizeof(b), "%ld", (long)shownTarget);
    g_mqtt.publish(topic("altura_objetivo").c_str(), b, true);
  }
}

// Only queues. See the note on g_pendingCmd for why nothing is fired here.
static void onMqttMessage(char *t, byte *payload, unsigned int len) {
  // Review finding A1: the old guard (len >= 9) silently dropped "refrescar",
  // "continuo_subir" and "continuo_bajar" -- three buttons the discovery itself
  // announces. Guard per topic, and REJECT LOUDLY instead of silently.
  bool isTarget = (strstr(t, "altura_objetivo") != NULL);
  unsigned maxLen = isTarget ? (sizeof(g_pendingCmd) - 4)   // room for "ir:" + NUL
                             : (sizeof(g_pendingCmd) - 1);
  if (len == 0 || len > maxLen) {
    Serial.printf("\n[cmd rechazado: %u bytes]\n", len);
    return;
  }
  char v[sizeof(g_pendingCmd)];
  memcpy(v, payload, len);
  v[len] = 0;
  // The number entity sends a bare value on its own topic; turn it into the
  // same queued command shape as everything else.
  if (!isTarget && !strcmp(v, "parar")) { g_stopReq = true; return; }  // C4

  if ((int32_t)(millis() - g_cmdArmedAt) < 0) {
    Serial.printf("\n[cmd ignorado al conectar: %s]\n", v);   // retained replay
    return;
  }
  if (isTarget) snprintf(g_pendingCmd, sizeof(g_pendingCmd), "ir:%s", v);
  else          strncpy(g_pendingCmd, v, sizeof(g_pendingCmd) - 1);
}

// --------------------------------------------------------------- travel ---

static bool heightFresh() {
  return g_heightCm > 0 && (uint32_t)(millis() - g_heightMs) < STALE_MS;
}

// Braking is CLOSING a contact, never opening one. A tap does it and moves at
// most 1 cm. Uses the channel opposite to the travel direction so the tap does
// not nudge the desk further the way it was already going.
static void stopTravel(const char *reason) {
  if (g_motion == MOTION_IDLE) return;
  if (g_motion == MOTION_BRAKING) {
    // Re-entry while a brake is being verified (panic re-press): one more
    // harmless tap, but do NOT reset the escalation timers -- resetting them
    // was blocking the retry / "FRENO FALLIDO" path exactly when it mattered.
    pulseChannelFor(WAKE_CH, WAKE_MS);
    return;
  }
  bool wasUp = (g_motion == MOTION_UP);
  int32_t target = g_target;
  g_motion = MOTION_BRAKING;        // B5: verified by superviseTravel, not assumed
  g_brakeStartMs = millis();
  g_brakeRetried = false;
  g_target = -1;
  strncpy(g_stopReason, reason, sizeof(g_stopReason) - 1);
  g_stopReason[sizeof(g_stopReason) - 1] = 0;
  // Only chase the target when the target is why we stopped. Braking at a limit
  // or on a stale reading must NOT be followed by taps trying to get somewhere.
  if (!strcmp(reason, "objetivo") && target >= 0) {
    g_fineTarget = target;
    g_fineTries  = 0;
  }
  // Brake with WAKE_MS (300 ms), not PULSE_MS (800): both register as a key
  // press (>=160 ms) and stop the travel, but 800 ms MOVES the desk ~1 cm the
  // opposite way -- so every brake at a limit or stall was adding its own
  // displacement (review, angle C finding). 300 ms is measured not to move.
  (void)wasUp;
  pulseChannelFor(WAKE_CH, WAKE_MS);   // the one channel MEASURED not to move
  // Settle window measured from when the contact actually opened, not from
  // before the blocking pulse (review C7).
  g_fineNextMs = millis() + 2500;
  Serial.printf("\n[FRENO: %s] altura %ld cm, leida hace %lu ms\n",
                reason, (long)g_heightCm,
                (unsigned long)(millis() - g_heightMs));
}

// target < 0 means "travel until a limit stops it".
static bool startTravel(bool up, int32_t target) {
  if (g_motion != MOTION_IDLE) {
    Serial.println("\n[ya hay un movimiento en curso; ignorado]");
    return false;
  }
  // Refuse to move blind. This is ADR-012 made executable: a height that has
  // not been refreshed is indistinguishable from a current one, and the display
  // sleeps on its own.
  if (!heightFresh()) {
    wakeDisplay();
    // The tap wakes the display, but the height only lands on the bus with the
    // next refresh cycle -- about 200 ms later. Checking right away always says
    // "no height" and refuses to move, which is exactly what happened the first
    // time this ran.
    uint32_t deadline = millis() + 2500;
    while (!heightFresh() && (int32_t)(millis() - deadline) < 0) {
      uint32_t n = captureBurst(200);
      if (n) decodeBurst(n);
    }
    if (!heightFresh()) {
      Serial.println("\n[NO ARRANCA: sin altura fresca]");
      strncpy(g_stopReason, "sin altura fresca", sizeof(g_stopReason) - 1);
      return false;
    }
  }
  if (target >= 0) {
    if (target < LIMIT_MIN_CM || target > LIMIT_MAX_CM) {
      Serial.printf("\n[NO ARRANCA: %ld fuera de %ld..%ld]\n",
                    (long)target, (long)LIMIT_MIN_CM, (long)LIMIT_MAX_CM);
      return false;
    }
    if (target == g_heightCm) { Serial.println("\n[ya esta en el objetivo]"); return true; }
    up = (target > g_heightCm);
  }
  // Already against the limit in that direction: nothing to do.
  if ((up && g_heightCm >= LIMIT_MAX_CM) || (!up && g_heightCm <= LIMIT_MIN_CM)) {
    Serial.println("\n[NO ARRANCA: ya esta en el limite]");
    return false;
  }

  // Review A3: a 1-2 cm trip used to commit a full 2800 ms continuous pulse
  // and overshoot ~4 cm before hunting back. Short gaps go straight to the
  // fine-adjust taps, which exist for exactly this.
  if (target >= 0 && abs((int)(target - g_heightCm)) <= 2) {
    g_fineTarget = target;
    g_fineTries  = 0;
    g_fineNextMs = millis();
    g_stopReason[0] = 0;   // do not keep showing the previous trip's brake reason
    Serial.printf("\n[VIAJE corto a %ld cm: por toques]\n", (long)target);
    return true;
  }

  g_motion      = up ? MOTION_UP : MOTION_DOWN;
  g_target      = target;
  g_travelStart = millis();
  g_progressH   = g_heightCm;
  g_startH      = g_heightCm;
  g_progressMs  = millis();
  g_stopReason[0] = 0;
  // A manual key decoded BEFORE this launch predates the travel and therefore
  // did not stop it; consuming it later would disarm supervision over a live
  // travel with no brake. Absorb it here (keeping the usage timestamp).
  if (g_manualKey) { g_manualKey = false; g_lastManualMs = uptimeMs64(); }

  Serial.printf("\n[VIAJE %s desde %ld cm, objetivo %s]\n",
                up ? "subiendo" : "bajando", (long)g_heightCm,
                target >= 0 ? String(target).c_str() : "el limite");
  pulseChannelLong(up ? 0 : 1);
  return true;
}

// Called every pass of the main loop. Every branch here ends in a brake.
static void superviseTravel() {
  if (g_motion == MOTION_IDLE) return;

  // B5: after a brake, stay on watch until the height actually settles.
  if (g_motion == MOTION_BRAKING) {
    if (!heightFresh()) {
      // Blind is exactly the case verification exists for. One wake attempt
      // (it doubles as another brake tap), then give up LOUDLY, not silently.
      if (!g_brakeRetried) {
        g_brakeRetried = true;
        g_brakeStartMs = millis();
        pulseChannelFor(WAKE_CH, WAKE_MS);
        return;
      }
      if ((uint32_t)(millis() - g_brakeStartMs) > 4000) {
        Serial.println("\n[!!] freno SIN CONFIRMAR: no hay lectura de altura");
        snprintf(g_stopReason, sizeof(g_stopReason), "freno sin confirmar");
        g_motion = MOTION_IDLE;
      }
      return;
    }
    if (g_heightCm != g_progressH) {     // still moving
      g_progressH = g_heightCm;
      g_progressMs = millis();
      if ((uint32_t)(millis() - g_brakeStartMs) > 4000) {
        if (!g_brakeRetried) {
          g_brakeRetried = true;
          g_brakeStartMs = millis();
          Serial.println("\n[!!] freno sin efecto: reintento");
          pulseChannelFor(WAKE_CH, WAKE_MS);   // any key stops box travel
        } else {
          Serial.println("\n[!!] FRENO FALLIDO dos veces: el escritorio sigue moviendose");
          snprintf(g_stopReason, sizeof(g_stopReason), "freno fallido");
          g_motion = MOTION_IDLE;        // nothing more this code can do
        }
      }
    } else if ((uint32_t)(millis() - g_progressMs) > 3000) {
      // 3000, not 1500: at travel speed the display crosses a centimetre every
      // ~1470 ms, so a 1.5 s window could declare "settled" between two cm
      // updates of a desk still moving at full speed (review 2026-08-23).
      g_motion = MOTION_IDLE;            // settled: brake confirmed
    }
    return;
  }

  bool up = (g_motion == MOTION_UP);

  if ((uint32_t)(millis() - g_travelStart) > TRAVEL_MAX_MS) { stopTravel("tiempo maximo"); return; }
  if (!heightFresh())                                       { stopTravel("altura obsoleta"); return; }

  if (g_heightCm != g_progressH) { g_progressH = g_heightCm; g_progressMs = millis(); }
  else if ((uint32_t)(millis() - g_progressMs) > STALL_MS)  { stopTravel("no avanza"); return; }

  // Review finding B3: these checks used to be keyed on the COMMANDED
  // direction, so a desk moving opposite to the order (a miswired channel --
  // the exact fault this project has already had) could cross the software
  // floor with nothing able to fire. Limits now watch the OBSERVED height in
  // both directions, and moving the wrong way is itself a reason to brake.
  // "approaching" = beyond the height we started from. Without that, starting
  // a downward trip FROM the top would brake instantly with "limite superior".
  if (g_heightCm >= LIMIT_MAX_CM - BRAKE_LEAD_CM && g_heightCm > g_startH)
                                                            { stopTravel("limite superior"); return; }
  if (g_heightCm <= LIMIT_MIN_CM + BRAKE_LEAD_CM && g_heightCm < g_startH)
                                                            { stopTravel("limite inferior"); return; }
  if (up  && g_heightCm <= g_startH - 2)                    { stopTravel("direccion invertida"); return; }
  if (!up && g_heightCm >= g_startH + 2)                    { stopTravel("direccion invertida"); return; }

  if (g_target >= 0) {
    if (up  && g_heightCm >= g_target - BRAKE_LEAD_CM)      { stopTravel("objetivo"); return; }
    if (!up && g_heightCm <= g_target + BRAKE_LEAD_CM)      { stopTravel("objetivo"); return; }
  }
}

// Taps towards the target after a brake. Non blocking: one tap per pass at
// most, and it gives up rather than tapping forever -- a desk that will not
// reach its target is reported, not chased (no retry loops, SEGURIDAD.md).
static void adjustFine() {
  if (g_fineTarget < 0) return;
  if ((int32_t)(millis() - g_fineNextMs) < 0) return;
  if (g_motion == MOTION_BRAKING) return;               // wait, do not cancel
  if (g_motion != MOTION_IDLE) { g_fineTarget = -1; return; }  // a new travel started

  if (!heightFresh()) { g_fineTarget = -1; return; }
  if (g_heightCm == g_fineTarget) {
    Serial.printf("\n[ajuste: %ld cm alcanzados]\n", (long)g_fineTarget);
    g_fineTarget = -1;
    return;
  }
  if (++g_fineTries > 8) {
    Serial.printf("\n[ajuste abandonado en %ld cm, objetivo %ld]\n",
                  (long)g_heightCm, (long)g_fineTarget);
    snprintf(g_stopReason, sizeof(g_stopReason), "ajuste incompleto");
    g_fineTarget = -1;
    return;
  }
  pulseChannel(g_heightCm > g_fineTarget ? 1 : 0);
  g_fineNextMs = millis() + 1800;
}

// Runs from the main loop, between bursts.
static void runPendingCmd() {
  if (!g_pendingCmd[0]) return;
  char cmd[sizeof(g_pendingCmd)];
  strncpy(cmd, g_pendingCmd, sizeof(cmd));
  g_pendingCmd[0] = 0;

  Serial.print("\n[MQTT cmd: ");
  Serial.print(cmd);
  Serial.println("]");

  // A command while travelling means STOP -- that is what the handset does.
  //
  // "refrescar" is NOT exempt any more (review C2): its wake tap closes a
  // contact, which physically brakes the travel anyway; exempting it left the
  // state saying "travelling" over a desk that had stopped, and 9 s later a
  // bogus "no avanza" brake moved it a further centimetre.
  //
  // "ir:N" retargets instead of being swallowed (review C3): before, it acted
  // as a brake and the requested height was silently discarded while the HA
  // number box made it look accepted.
  if (g_motion != MOTION_IDLE) {
    if (!strncmp(cmd, "ir:", 3)) {
      stopTravel("reobjetivo");
      g_retargetCm = atoi(cmd + 3);   // executed once braking confirms
      return;
    }
    stopTravel("parado a mano");
    return;
  }
  if (strcmp(cmd, "refrescar") != 0) g_fineTarget = -1;   // stop chasing the old target

  if      (!strcmp(cmd, "subir")) pulseChannel(0);
  else if (!strcmp(cmd, "bajar")) pulseChannel(1);
  else if (!strcmp(cmd, "m1"))    pulseChannel(2);
  else if (!strcmp(cmd, "m2"))    pulseChannel(3);
  // "parar" never reaches the queue: it travels on g_stopReq (review C4).
  else if (!strcmp(cmd, "refrescar")) wakeDisplay();
  else if (!strcmp(cmd, "continuo_subir")) startTravel(true,  -1);
  else if (!strcmp(cmd, "continuo_bajar")) startTravel(false, -1);
  else if (!strncmp(cmd, "ir:", 3))        startTravel(true, atoi(cmd + 3));
  else Serial.println("[cmd desconocido, ignorado]");
}

// Never blocks for long: one attempt every 10 s at most, so a dead broker
// cannot stall the capture loop.
static void mqttEnsure() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (g_mqtt.connected()) return;
  if ((int32_t)(millis() - g_mqttNextTry) < 0) return;
  g_mqttNextTry = millis() + 10000;

  String will = topic("disponible");
  if (g_mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD,
                     will.c_str(), 0, true, "offline")) {
    g_mqtt.publish(will.c_str(), "online", true);
    // Review B6: the retained defence used to be only a 4 s wall-clock window,
    // but one loop() pass can block longer than that, so a retained command
    // could land after the window and replay a movement nobody asked for.
    // Erase the retained messages AT THE BROKER before subscribing -- the
    // hazardous class of message stops existing. The window stays as backup.
    g_mqtt.publish(topic("cmd").c_str(), (const uint8_t*)"", 0, true);
    g_mqtt.publish(topic("altura_objetivo/set").c_str(), (const uint8_t*)"", 0, true);
    g_cmdArmedAt = millis() + CMD_ARM_DELAY_MS;
    g_mqtt.subscribe(topic("cmd").c_str());
    g_mqtt.subscribe(topic("altura_objetivo/set").c_str());
    if (!g_discoverySent) publishDiscovery();
    Serial.println("\n[MQTT conectado]");
  }
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

  // WiFi asynchronously: begin() returns at once and the loop checks status.
  // Blocking here would delay the capture for no reason.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // sleep adds latency and buys nothing on USB
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  ArduinoOTA.setHostname(DEVICE_ID);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    // Review 2026-08-23: the first version "refused" mid-travel with
    // ESP.restart() -- which IS the runaway it claimed to prevent: reset opens
    // the channels, opening does not stop continuous travel, and the reboot
    // wakes with no travel state so nothing ever brakes. The safe move is the
    // opposite: BRAKE FIRST (a real tap, which is what stops travel), then let
    // the update proceed over a desk that is stopping.
    if (g_motion != MOTION_IDLE) {
      Serial.println("\n[OTA: frenando el escritorio antes de actualizar]");
      stopTravel("OTA");
    }
    g_retargetCm = -1;
    g_fineTarget = -1;
    g_otaBusy = true;
    allChannelsOff();  // belt and braces: no contact closed during an update
    Serial.println("\n[OTA: recibiendo firmware nuevo]");
  });
  ArduinoOTA.onEnd([]() { Serial.println("\n[OTA: completa, reiniciando]"); });
  ArduinoOTA.onError([](ota_error_t e) {
    g_otaBusy = false;
    Serial.printf("\n[OTA ERROR %u: el firmware anterior sigue intacto]\n", e);
  });
  ArduinoOTA.begin();

  g_mqtt.setServer(MQTT_HOST, MQTT_PORT);
  g_mqtt.setBufferSize(1024);    // discovery payloads do not fit in the default 256
  g_mqtt.setCallback(onMqttMessage);
  Serial.println("[WiFi: conectando en segundo plano]");

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
  ArduinoOTA.handle();
  if (g_otaBusy) return;   // an update is streaming: nothing else matters

  uint32_t n = captureBurst(250);
  if (n) {
    decodeBurst(n);
  } else {
    keepClockAlive();  // bus quiet: keep the timestamp base from wrapping
  }

  // MQTT between bursts, never inside one -- and NEVER in front of the brake.
  // Round-2 review: connect() blocks up to the socket timeout and publishState
  // is a dozen blocking TCP writes; during a travel every blocked second is
  // 0.68 cm of unsupervised movement. While the desk moves, the radio waits.
  if (g_motion == MOTION_IDLE) {
    mqttEnsure();
    if (g_mqtt.connected()) {
      g_mqtt.loop();
      if ((uint32_t)(millis() - g_lastPublish) >= PUBLISH_EVERY_MS) {
        g_lastPublish = millis();
        publishState();
      }
    }
  } else if (g_mqtt.connected()) {
    g_mqtt.loop();   // cheap: keepalive + receive "parar"; no publishes
  }
  // Somebody touched the handset: give way. Their press already stopped any
  // travel of ours physically; here we drop OUR intent so we do not chase,
  // brake or fine-adjust against them.
  if (g_manualKey) {
    g_manualKey = false;
    uint8_t k = g_manualKeyByte;
    if (g_motion != MOTION_IDLE || g_fineTarget >= 0 || g_retargetCm >= 0) {
      Serial.printf("\n[mando manual (0x%02X): cedo el paso]\n", (unsigned)k);
      g_motion     = MOTION_IDLE;   // no brake tap: the person already stopped it
      g_target     = -1;
      g_fineTarget = -1;
      g_retargetCm = -1;
      g_pendingCmd[0] = 0;
      snprintf(g_stopReason, sizeof(g_stopReason), "mando manual");
    }
    g_lastManualMs = uptimeMs64();
  }

  superviseTravel();   // every branch of it ends in a brake

  if (g_stopReq) {     // C4: stop outranks everything and cannot be overwritten
    g_stopReq = false;
    g_fineTarget = -1;
    g_retargetCm = -1;
    g_pendingCmd[0] = 0;   // a queued movement must not fire after a stop
    if (g_motion != MOTION_IDLE) stopTravel("parado a mano");
    else                         pulseChannelFor(WAKE_CH, WAKE_MS);  // brakes box travel, moves nothing
  }

  // Pending retarget: fire once the brake has settled.
  if (g_retargetCm >= 0 && g_motion == MOTION_IDLE) {
    int32_t t = g_retargetCm;
    g_retargetCm = -1;
    startTravel(true, t);
  }

  adjustFine();
  runPendingCmd();

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
      // Round-2 review: serial digits are used to IDENTIFY channels and to
      // brake -- both jobs need a tap that registers without moving, and the
      // tools were written assuming 300 ms. At 800 ms every "verification"
      // moved the desk ~1 cm and pulse_loop's cadence could merge closures
      // into a HELD key. Movement stays on A/B/ir: only.
      case '1': g_fineTarget = -1; pulseChannelFor(0, WAKE_MS); break;
      case '2': g_fineTarget = -1; pulseChannelFor(1, WAKE_MS); break;
      case '3': g_fineTarget = -1; pulseChannelFor(2, WAKE_MS); break;
      case '4': g_fineTarget = -1; pulseChannelFor(3, WAKE_MS); break;
      // Review B4: 'A'/'B' used to fire the raw 2800 ms pulse with NO
      // supervision at all -- no limits, no stale brake, no stall brake. They
      // now go through startTravel like the MQTT path. Upper case on purpose:
      // a typo must not start continuous travel.
      case 'A': g_fineTarget = -1; startTravel(true,  -1); break;
      case 'B': g_fineTarget = -1; startTravel(false, -1); break;
      // 'C'/'D' (2800 ms held on a MEMORY channel) were removed: that is
      // 200 ms short of the 3.0 s that OVERWRITES the preset, and
      // identification only needs a tap ('3'/'4'). Review B4.
      case 'd':
        if (g_mqtt.connected()) { Serial.println("\n[republicando discovery]"); publishDiscovery(); }
        else Serial.println("\n[MQTT no conectado]");
        break;
      case 'w': wakeDisplay(); break;
      case 'l': checkLines(); break;
      case 'h': printHelp(); break;
      default: break;
    }
  }
}
