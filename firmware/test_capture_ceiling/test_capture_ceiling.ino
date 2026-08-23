// test_capture_ceiling — how fast can the sniffer capture before it loses edges?
//
// The project has no logic analyser, so the ESP32 has to measure itself. This
// sketch drives a square wave of known frequency out of one GPIO, feeds it back
// into the pin the sniffer uses for CLK, and reports where the interrupt-driven
// capture path starts dropping events.
//
// WIRING: a single jumper from GPIO19 to GPIO18. Both are screw terminals on
// the expansion board, adjacent in the same column. Nothing else is connected —
// the desk must NOT be plugged into anything here.
//
// The ISR, the Event struct and the ring buffer below are copied verbatim from
// desk_sniffer.ino. If that file changes, this one has to change with it or the
// number it reports stops meaning anything.
//
// WHAT THIS DOES NOT MEASURE: loop() here only drains the queue, it does not
// run the protocol decoder. Real capture does slightly more work per edge, so
// treat the result as an optimistic ceiling, not a guarantee.

#include <Arduino.h>
#include "soc/gpio_struct.h"

static const int PIN_GEN = 19;  // square wave out — jumper this...
static const int PIN_CAP = 18;  // ...to here. Same pin desk_sniffer uses for CLK
static const uint32_t SERIAL_BAUD = 921600;

static_assert(PIN_CAP < 32, "capture pin must be in GPIO0..31");

// LEDC: 4-bit resolution keeps freq * 2^bits under the 80 MHz peripheral clock
// all the way up to 5 MHz. Duty 8 of 16 is a symmetric square wave.
static const uint8_t LEDC_BITS = 4;
static const uint32_t LEDC_DUTY = 8;
#if ESP_ARDUINO_VERSION_MAJOR < 3
static const uint8_t LEDC_CHANNEL = 0;  // only the 2.x API needs a channel
#endif

// Frequencies to try, in Hz. Each produces 2 edges per cycle. The steps bunch
// up above 100 kHz because that is where the ceiling turned out to be on the
// first run, and the sweep stops at the first frequency that drops anything —
// past the ceiling there is nothing left to learn.
//
// The sweep STOPS AT 150 kHz on purpose. At 175 kHz (350 k edges/s) this board
// stops responding entirely — measured twice, on 2026-08-03, and not because of
// the millis() bug fixed above: it hung again with the cycle-counter window.
// The exact mechanism was never pinned down (interrupt watchdog panic, or plain
// CPU starvation) because it changes no decision here. Going past this line
// buys nothing and costs a reset every run.
static const uint32_t FREQS[] = {1000,  2000,   5000,   10000,
                                 20000, 50000,  100000, 125000,
                                 150000};
static const size_t N_FREQS = sizeof(FREQS) / sizeof(FREQS[0]);

static const uint32_t STEP_MS = 1000;  // measurement window per frequency

// ------------------------------------------------- copied from the sniffer ---

static inline uint32_t cycleCount() {
#if defined(__XTENSA__)
  uint32_t c;
  __asm__ __volatile__("rsr %0, ccount" : "=a"(c));
  return c;
#else
  return (uint32_t)(esp_timer_get_time() * 240);
#endif
}

struct Event {
  uint32_t cyc;
  uint8_t clk;
  uint8_t dio;
};

static const uint32_t EVT_N = 4096;  // must match desk_sniffer.ino
static volatile Event g_evt[EVT_N];
static volatile uint32_t g_head = 0;
static volatile uint32_t g_tail = 0;
static volatile uint32_t g_dropped = 0;
static volatile uint32_t g_edges = 0;

static void IRAM_ATTR onEdge() {
  uint32_t in = GPIO.in;
  uint32_t head = g_head;
  uint32_t next = (head + 1) & (EVT_N - 1);
  if (next == g_tail) {
    g_dropped++;
    return;
  }
  g_evt[head].cyc = cycleCount();
  g_evt[head].clk = (in >> PIN_CAP) & 1;
  g_evt[head].dio = 0;
  g_head = next;
  g_edges++;
}

// ------------------------------------------------------------- generator ---

static void genStart(uint32_t hz) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_GEN, hz, LEDC_BITS);
  ledcWrite(PIN_GEN, LEDC_DUTY);
#else
  ledcSetup(LEDC_CHANNEL, hz, LEDC_BITS);
  ledcAttachPin(PIN_GEN, LEDC_CHANNEL);
  ledcWrite(LEDC_CHANNEL, LEDC_DUTY);
#endif
}

static void genStop() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_GEN, 0);
  ledcDetach(PIN_GEN);
#else
  ledcWrite(LEDC_CHANNEL, 0);
  ledcDetachPin(PIN_GEN);
#endif
  pinMode(PIN_GEN, INPUT);
}

// ------------------------------------------------------------------ test ---

// Drains the queue the way loop() does in the sniffer, minus the decoder.
static inline void drain() {
  while (g_tail != g_head) {
    Event e = *(Event *)&g_evt[g_tail];
    g_tail = (g_tail + 1) & (EVT_N - 1);
    (void)e;
  }
}

static uint32_t g_bestClean = 0;  // highest frequency with zero drops

// Returns true if this step was clean, false if it dropped anything.
//
// The GPIO interrupt is attached ONLY for the measurement window. Past ~100 kHz
// the edge rate saturates the CPU completely, and leaving the interrupt live
// while changing frequency or printing would starve the serial port. Worse, the
// first version of this sketch timed the window with millis(): under a full
// interrupt storm the timer tick itself gets starved, millis() stops advancing
// and the loop never exits. It hung the board at 200 kHz. The window is now
// timed with the CPU cycle counter, which is a register read and cannot be
// starved by anything.
static bool runStep(uint32_t hz) {
  // Settle with the generator already running, so the window is not polluted
  // by the frequency change itself.
  genStart(hz);
  delay(50);

  g_edges = 0;
  g_dropped = 0;
  g_tail = g_head;

  const uint32_t cyclesPerUs = getCpuFrequencyMhz() ? getCpuFrequencyMhz() : 240;
  const uint32_t window = cyclesPerUs * 1000u * STEP_MS;

  attachInterrupt(digitalPinToInterrupt(PIN_CAP), onEdge, CHANGE);
  const uint32_t startCyc = cycleCount();
  while ((uint32_t)(cycleCount() - startCyc) < window) {
    drain();
  }
  detachInterrupt(digitalPinToInterrupt(PIN_CAP));

  uint32_t edges = g_edges;
  uint32_t dropped = g_dropped;
  genStop();
  drain();

  uint64_t expected = (uint64_t)hz * 2 * STEP_MS / 1000;
  uint32_t pct = expected ? (uint32_t)((uint64_t)edges * 100 / expected) : 0;

  // A step passes only if the edges ACTUALLY ARRIVED. Judging by dropped == 0
  // alone is worthless: that counter stays at zero when nothing reaches the ISR
  // at all, so an unplugged jumper scores a perfect run at every frequency.
  // This sketch shipped with that bug and reported "clean up to 400 kHz" with
  // no wire attached — the same false-pass that the 2026-08-02 review already
  // found once in desk_sniffer's own line check. Count what arrived.
  //
  // Slack: whichever is larger of 4 edges or 0.1%. That absorbs the one or two
  // edges that fall either side of the window without hiding real loss — the
  // 150 kHz step lost 0.13%, and it must still fail.
  uint64_t slack = expected / 1000;
  if (slack < 4) slack = 4;
  bool arrived = edges > 0 && (uint64_t)edges + slack >= expected;
  bool noSignal = expected > 0 && (uint64_t)edges * 2 < expected;
  bool clean = arrived && dropped == 0;

  const char *verdict = clean            ? "OK"
                        : noSignal       ? "<-- NO SIGNAL: is the jumper on?"
                        : dropped        ? "<-- LOSING EDGES (queue full)"
                                         : "<-- LOSING EDGES SILENTLY";

  char buf[180];
  snprintf(buf, sizeof(buf),
           "%8lu Hz | expected %9llu | captured %9lu (%3lu%%) | dropped %9lu %s",
           (unsigned long)hz, (unsigned long long)expected,
           (unsigned long)edges, (unsigned long)pct, (unsigned long)dropped,
           verdict);
  Serial.println(buf);

  if (clean) g_bestClean = hz;

  delay(200);  // let the watchdog breathe between steps
  return clean;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(400);

  Serial.println();
  Serial.println("=== test_capture_ceiling ===");
  char buf[120];
  snprintf(buf, sizeof(buf), "Jumper GPIO%d (generator) -> GPIO%d (capture).",
           PIN_GEN, PIN_CAP);
  Serial.println(buf);
  Serial.println("Nothing else connected. The desk is not involved.");
  Serial.println();

  pinMode(PIN_CAP, INPUT);

  // A square wave gives two edges per cycle, so 'expected' is 2 x freq.
  Serial.println("A square wave produces 2 edges per cycle.");
  Serial.println();

  for (size_t i = 0; i < N_FREQS; i++) {
    if (!runStep(FREQS[i])) {
      Serial.println("  (stopping: nothing above this point can be trusted)");
      break;
    }
  }

  Serial.println();
  if (g_bestClean == 0) {
    Serial.println("RESULT: FAILED at the very first step — no usable measurement.");
    Serial.println("  Almost certainly the jumper: GPIO19 must be wired to GPIO18.");
    Serial.println("  A run with no wire produces zero edges everywhere, which is");
    Serial.println("  a failure, not a pass. Check the wire and press EN/RST.");
  } else {
    snprintf(buf, sizeof(buf),
             "RESULT: clean up to %lu Hz  (%lu edges/s) with zero drops.",
             (unsigned long)g_bestClean, (unsigned long)g_bestClean * 2);
    Serial.println(buf);
    Serial.println("  A bus transaction is ~54 edges. Divide to compare.");
  }
  Serial.println("Press EN/RST to run again.");
}

void loop() {}
