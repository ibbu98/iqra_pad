// ============================================================
//  Standalone SD card test — bit-bang + hardware SPI
//  No display, no buttons needed.
//
//  Wiring:
//    SD CS   -> GPIO 5
//    SD SCK  -> GPIO 12
//    SD MOSI -> GPIO 11
//    SD MISO -> GPIO 13
//    SD VCC  -> 5V   <-- MUST be 5V, NOT 3.3V
//    SD GND  -> GND
// ============================================================

#ifdef SD_TEST

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>

#define T_CS   5
#define T_SCK  12
#define T_MOSI 11
#define T_MISO 13

// ---- Bit-bang helpers ----

static void bbPinsInit()
{
  pinMode(T_CS,   OUTPUT); digitalWrite(T_CS,   HIGH);
  pinMode(T_SCK,  OUTPUT); digitalWrite(T_SCK,  LOW);
  pinMode(T_MOSI, OUTPUT); digitalWrite(T_MOSI, HIGH);
  pinMode(T_MISO, INPUT_PULLUP);
}

static uint8_t bbTransfer(uint8_t out)
{
  uint8_t in = 0;
  for (int b = 7; b >= 0; b--) {
    digitalWrite(T_MOSI, (out >> b) & 1);
    delayMicroseconds(10);
    digitalWrite(T_SCK, HIGH);
    delayMicroseconds(10);
    if (digitalRead(T_MISO)) in |= (1 << b);
    digitalWrite(T_SCK, LOW);
    delayMicroseconds(10);
  }
  return in;
}

// ---- Phase 1: raw MISO before any clocking ----
static void phase1_rawMiso()
{
  Serial.println("\n--- Phase 1: MISO state before clocking (CS HIGH) ---");
  pinMode(T_MISO, INPUT_PULLUP);
  delay(100);
  // sample 10 times
  Serial.print("  samples: ");
  int lows = 0;
  for (int i = 0; i < 10; i++) {
    int v = digitalRead(T_MISO);
    Serial.print(v);
    if (v == LOW) lows++;
    delay(5);
  }
  if (lows == 0)
    Serial.println("  -> ALL HIGH  (pin free, nothing driving it)");
  else if (lows == 10)
    Serial.println("  -> ALL LOW   (something ACTIVELY driving MISO to GND!)");
  else
    Serial.printf("  -> MIXED (%d low, %d high)  (floating or noisy)\n", lows, 10-lows);
}

// ---- Phase 2: clock pulses with CS HIGH, watch MISO ----
static void phase2_powerUpClocks()
{
  Serial.println("\n--- Phase 2: 320 power-up clocks (CS HIGH), MISO trace ---");
  bbPinsInit();
  Serial.print("  first 40 MISO bits: ");
  for (int i = 0; i < 320; i++) {
    digitalWrite(T_SCK, HIGH); delayMicroseconds(10);
    int m = digitalRead(T_MISO);
    if (i < 40) Serial.print(m);
    digitalWrite(T_SCK, LOW);  delayMicroseconds(10);
  }
  Serial.printf("\n  MISO after 320 clocks: %d\n", digitalRead(T_MISO));
}

// ---- Phase 2b: MISO when CS is LOW (tests if MISO buffer is CS-gated) ----
static void phase2b_csLowMiso()
{
  Serial.println("\n--- Phase 2b: MISO immediately after CS goes LOW (no clocking) ---");
  digitalWrite(T_CS, LOW);
  delayMicroseconds(50);
  Serial.print("  MISO samples (CS LOW, no data): ");
  for (int i = 0; i < 10; i++) { Serial.print(digitalRead(T_MISO)); delay(1); }
  Serial.println();
  Serial.println("  (if changed from Phase1 -> CS is reaching module's MISO gate)");
  digitalWrite(T_CS, HIGH);
  delayMicroseconds(20);
}

// ---- Phase 3: bit-bang CMD0, retry up to 10 times ----
static uint8_t phase3_cmd0()
{
  Serial.println("\n--- Phase 3: CMD0 (bit-bang, up to 10 retries) ---");

  uint8_t r1 = 0xFF;
  for (int attempt = 0; attempt < 10 && r1 == 0xFF; attempt++) {
    // 80 extra clocks before each attempt
    digitalWrite(T_CS, HIGH);
    for (int i = 0; i < 10; i++) bbTransfer(0xFF);

    digitalWrite(T_CS, LOW);
    delayMicroseconds(20);

    uint8_t cmd[] = { 0x40, 0x00, 0x00, 0x00, 0x00, 0x95 };
    for (int i = 0; i < 6; i++) bbTransfer(cmd[i]);

    // read up to 32 bytes for R1
    Serial.printf("  attempt %d bytes: ", attempt);
    for (int i = 0; i < 32; i++) {
      uint8_t b = bbTransfer(0xFF);
      if (i < 12) Serial.printf("0x%02X ", b);
      if (b != 0xFF && r1 == 0xFF) r1 = b;
    }
    Serial.println();

    digitalWrite(T_CS, HIGH);
    bbTransfer(0xFF);
    delay(100);
  }

  Serial.printf("  Final R1 = 0x%02X -> ", r1);
  if      (r1 == 0x01) Serial.println("IDLE — card in SPI mode");
  else if (r1 == 0xFF) {
    Serial.println("NO RESPONSE after 10 attempts");
    Serial.println();
    Serial.println("  *** DIAGNOSIS: CMD0 is NOT reaching the SD card ***");
    Serial.println("  The 74LVC125A module blocks MOSI/CS/SCK when underpowered.");
    Serial.println();
    Serial.println("  FIX 1 (most likely): Module VCC must be 5V, NOT 3.3V.");
    Serial.println("    Measure module VCC-GND with multimeter -> must read ~5.0V");
    Serial.println("    Measure module 3V3 output (or AMS1117 out) -> must read ~3.3V");
    Serial.println();
    Serial.println("  FIX 2: Bypass the 74LVC125A module entirely.");
    Serial.println("    Use a bare microSD socket and connect direct:");
    Serial.println("    SD VCC -> ESP32 3.3V  (no AMS1117 needed for 3.3V system)");
    Serial.println("    SD GND -> GND");
    Serial.println("    SD CLK -> GPIO 12");
    Serial.println("    SD CMD/DI -> GPIO 11");
    Serial.println("    SD DAT0/DO -> GPIO 13");
    Serial.println("    SD CS/DAT3 -> GPIO 5");
  }
  else if (r1 == 0x00) Serial.println("0x00 — unexpected; card might need CMD8 next");
  else                 Serial.printf("unexpected 0x%02X\n", r1);

  return r1;
}

// ---- Phase 4: CMD8 + ACMD41 with 10s timeout ----
static bool phase4_init()
{
  Serial.println("\n--- Phase 4: CMD8 + ACMD41 (10s timeout) ---");

  // CMD8 — voltage range check (required for SDHC/SDXC)
  digitalWrite(T_CS, HIGH); bbTransfer(0xFF);
  digitalWrite(T_CS, LOW); delayMicroseconds(20);
  uint8_t cmd8[] = { 0x48, 0x00, 0x00, 0x01, 0xAA, 0x87 };
  for (int i = 0; i < 6; i++) bbTransfer(cmd8[i]);
  uint8_t r8 = 0xFF;
  for (int i = 0; i < 8 && r8 == 0xFF; i++) r8 = bbTransfer(0xFF);
  uint8_t trail[4]; for (int i = 0; i < 4; i++) trail[i] = bbTransfer(0xFF);
  digitalWrite(T_CS, HIGH); bbTransfer(0xFF);
  Serial.printf("  CMD8  R1=0x%02X  trail=0x%02X 0x%02X 0x%02X 0x%02X\n",
                r8, trail[0], trail[1], trail[2], trail[3]);
  if (r8 == 0x05) Serial.println("  (0x05 = illegal cmd, card is SD v1 — OK, continue)");

  // ACMD41 loop — 10 seconds
  Serial.println("  ACMD41 loop (10s): R1 per attempt...");
  uint32_t t0 = millis();
  uint8_t acmd = 0xFF;
  int iter = 0;
  while (millis() - t0 < 10000) {
    // CMD55 (prefix for APP command)
    digitalWrite(T_CS, HIGH); bbTransfer(0xFF);
    digitalWrite(T_CS, LOW); delayMicroseconds(20);
    uint8_t cmd55[] = { 0x77, 0x00, 0x00, 0x00, 0x00, 0x01 };
    for (int i = 0; i < 6; i++) bbTransfer(cmd55[i]);
    uint8_t r55 = 0xFF;
    for (int i = 0; i < 8 && r55 == 0xFF; i++) r55 = bbTransfer(0xFF);
    digitalWrite(T_CS, HIGH); bbTransfer(0xFF);

    // ACMD41 (HCS=1 for SDHC)
    digitalWrite(T_CS, LOW); delayMicroseconds(20);
    uint8_t a41[] = { 0x69, 0x40, 0x00, 0x00, 0x00, 0x01 };
    for (int i = 0; i < 6; i++) bbTransfer(a41[i]);
    acmd = 0xFF;
    for (int i = 0; i < 8 && acmd == 0xFF; i++) acmd = bbTransfer(0xFF);
    digitalWrite(T_CS, HIGH); bbTransfer(0xFF);

    iter++;
    if (iter % 5 == 0)  // print every 5th iteration to avoid flooding
      Serial.printf("  t=%lums CMD55=0x%02X ACMD41=0x%02X\n",
                    millis() - t0, r55, acmd);
    if (acmd == 0x00) break;
    delay(50);
  }

  if (acmd == 0x00) {
    Serial.printf("  CARD READY after %lums  (%d attempts)\n", millis()-t0, iter);
    return true;
  }
  Serial.printf("\n  ACMD41 TIMEOUT — last R1=0x%02X after 10s\n", acmd);
  if (acmd == 0x01)
    Serial.println("  R1=0x01 means card is stuck in IDLE — VCC too low to finish init.");
  else if (acmd == 0xFF)
    Serial.println("  R1=0xFF means no response — MOSI/CS not reaching card.");
  return false;
}

// ---- Phase 5: speed sweep with integrity check at every step ----
static SPIClass spi2(HSPI);
static SdFat32  sd;

static bool rwTest()
{
  const char* msg = "iqra_pad_speed_test_OK";
  File32 f;
  if (!f.open("spd.txt", O_WRITE | O_CREAT | O_TRUNC)) return false;
  f.print(msg); f.close();
  if (!f.open("spd.txt", O_READ)) return false;
  char buf[32] = {}; f.read(buf, 31); f.close();
  sd.remove("spd.txt");
  return strcmp(buf, msg) == 0;
}

static void phase5_hardwareSPI()
{
  Serial.println("\n--- Phase 5: SdFat speed sweep ---");

  const uint32_t speeds[] = {
    250000, 500000, 1000000, 2000000, 4000000,
    8000000, 10000000, 16000000, 20000000, 25000000
  };
  const char* labels[] = {
    "250kHz","500kHz","1MHz","2MHz","4MHz",
    "8MHz","10MHz","16MHz","20MHz","25MHz"
  };

  uint32_t bestSpeed = 0;

  for (int i = 0; i < 10; i++) {
    // sd.end() internally calls spi2.end() which loses the pin mapping.
    // Re-establish explicit pins before every attempt.
    spi2.begin(T_SCK, T_MISO, T_MOSI, -1);
    delay(20);

    SdSpiConfig cfg(T_CS, DEDICATED_SPI, speeds[i], &spi2);
    Serial.printf("  %-7s  mount:", labels[i]);
    if (!sd.begin(cfg)) {
      Serial.print(" FAIL  ");
      // print error code only, not the "do not reformat" line
      Serial.printf("(SdError 0x%02X)\n", sd.sdErrorCode());
      sd.end();
      continue;   // try next speed anyway
    }
    Serial.print(" OK   rw:");
    if (rwTest()) {
      Serial.println(" PASS");
      bestSpeed = speeds[i];
    } else {
      Serial.println(" FAIL (data error)");
    }
    sd.end();
    delay(20);
  }

  if (bestSpeed == 0) {
    Serial.println("\n  No speed worked.");
    return;
  }

  // Final mount at best speed + full info
  Serial.printf("\n  Best stable speed: ");
  for (int i = 0; i < 10; i++)
    if (speeds[i] == bestSpeed) { Serial.println(labels[i]); break; }

  spi2.begin(T_SCK, T_MISO, T_MOSI, -1);
  delay(20);
  SdSpiConfig best(T_CS, DEDICATED_SPI, bestSpeed, &spi2);
  sd.begin(best);

  uint32_t mb = (uint32_t)(sd.card()->sectorCount() / 2048UL);
  Serial.printf("  Card: %lu MB  type: %s\n", mb,
                sd.card()->type() == 3 ? "SDHC/SDXC" :
                sd.card()->type() == 2 ? "SD2" : "SD1");

  Serial.println("  Files:");
  File32 root; root.open("/");
  File32 e;
  while (e.openNext(&root, O_RDONLY)) {
    char name[64]; e.getName(name, sizeof(name));
    Serial.printf("    %-32s %lu B\n", name, (uint32_t)e.fileSize());
    e.close();
  }
  root.close();
  Serial.println("\n===== TEST COMPLETE =====");
}

void setup()
{
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n============================");
  Serial.println("  SD CARD BIT-BANG TEST");
  Serial.println("============================");
  Serial.printf("Pins: CS=%d SCK=%d MOSI=%d MISO=%d\n",
                T_CS, T_SCK, T_MOSI, T_MISO);
  Serial.println();
  Serial.println("ACTION: Unplug MISO wire from SD module, see if Phase 1 goes HIGH.");
  Serial.println("If it does -> SD module is actively pulling MISO LOW (power issue).");
  Serial.println("If it stays LOW -> GPIO13 itself has a short somewhere.");
  Serial.println();

  phase1_rawMiso();
  phase2_powerUpClocks();
  // Note: phase2b removed — asserting CS LOW without clocks confuses cards on marginal power

  uint8_t r1 = phase3_cmd0();

  // 0x01 = idle (expected)
  // 0x00 = card saying "ready" (already inited, or marginal power partial response)
  // Both mean the card is electrically present — hand off to SdFat which
  // does its own CMD0 loop with fresh timing and proper retries.
  if (r1 == 0x01 || r1 == 0x00) {
    Serial.printf("\n  Got R1=0x%02X — card is alive.\n", r1);
    // CMD0 warm-up is enough. Give the supply 1.5s to fully stabilise,
    // then hand off to SdFat for CMD8 / ACMD41 / mount.
    // Do NOT call spi2.end() — that glitches the bus and confuses the card.
    Serial.println("  Stabilising 1.5s ...");
    delay(1500);
    phase5_hardwareSPI();
  } else {
    Serial.println("\n============================");
    Serial.println("  STOPPED: CMD0 got no response (0xFF).");
    Serial.println("  Card is not receiving our commands.");
    Serial.println("  Check: VCC->5V, MOSI->GPIO11, CS->GPIO5");
    Serial.println("============================");
  }
}

void loop()
{
  delay(5000);
  Serial.println("[idle] reset to re-run test");
}

#endif // SD_TEST
