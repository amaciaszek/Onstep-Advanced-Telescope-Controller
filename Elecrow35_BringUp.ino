// Elecrow 3.5" bring-up — run this BEFORE touching the app.
//
// It answers, in order, the four questions that would otherwise cost you an
// evening of blind debugging:
//
//   1. does the panel come up at all
//   2. is ELECROW35_SWAP_BYTES set correctly
//   3. is the origin where we think it is
//   4. what is actually on the I2C bus
//
// Board settings in Arduino IDE:
//   Board            ESP32S3 Dev Module
//   USB CDC On Boot  Enabled          <- else Serial prints nothing over USB-C
//   Flash Size       16MB (128Mb)
//   PSRAM            OPI PSRAM        <- REQUIRED, the framebuffers need it
//   Partition        16M Flash (3MB APP/9.9MB FATFS)

#include <Wire.h>
#include "Elecrow35Display.h"
#include "Elecrow35Touch.h"

Elecrow35Display gfx;
Elecrow35Touch touch;

static void i2cScan() {
  Serial.println("\n--- I2C scan on 38/39 ---");
  for (uint8_t a = 1; a < 127; ++a) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X", a);
      switch (a) {
        case 0x55: Serial.print("  touch controller"); break;
        case 0x18: Serial.print("  ES8311 audio codec"); break;
        case 0x50: Serial.print("  seesaw gamepad?"); break;
        case 0x68: Serial.print("  DS3231 RTC"); break;
        case 0x76: case 0x77: Serial.print("  BME280"); break;
        case 0x5A: Serial.print("  DRV2605L haptic"); break;
        case 0x57: Serial.print("  AT24C32 EEPROM (on DS3231 board)"); break;
      }
      Serial.println();
    }
  }
  Serial.println("--- end scan ---\n");
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nOnStep / Elecrow 3.5 bring-up");
  Serial.printf("PSRAM: %u bytes free\n", (unsigned)ESP.getFreePsram());
  if (ESP.getFreePsram() < 1000000) {
    Serial.println("!! PSRAM missing — enable 'OPI PSRAM' in Tools. Framebuffers will fail.");
  }

  if (!gfx.begin()) {
    Serial.println("!! gfx.begin() FAILED");
    while (true) delay(1000);
  }
  Serial.println("panel up");

  // ---- TEST 1: colour order.
  // Three bars, top to bottom: RED, GREEN, BLUE.
  // If you see BLUE, GREEN, RED then flip ELECROW35_SWAP_BYTES in the header.
  gfx.fillRect(0,   0, 320, 160, Color(255, 0, 0));
  gfx.fillRect(0, 160, 320, 160, Color(0, 255, 0));
  gfx.fillRect(0, 320, 320, 160, Color(0, 0, 255));

  // ---- TEST 2: origin and extent.
  // A white 20px square must sit hard in the TOP-LEFT corner, and a 1px frame
  // must trace the full panel edge with no wrap and no missing row/column.
  gfx.fillRect(0, 0, 20, 20, Color(255, 255, 255));
  gfx.drawRect(0, 0, 320, 480, Color(255, 255, 255));
  gfx.present();
  Serial.println("TEST 1: bars should read RED / GREEN / BLUE top to bottom");
  Serial.println("TEST 2: white square top-left, 1px frame on all four edges");
  delay(3000);

  // ---- TEST 3: flush throughput.
  gfx.forceFullRefresh();
  uint32_t t0 = millis();
  for (int i = 0; i < 20; ++i) {
    gfx.clear(Color(i & 1 ? 8 : 16, 8, 16));
    gfx.forceFullRefresh();
    gfx.present();
  }
  uint32_t dt = millis() - t0;
  Serial.printf("full-screen present: %.1f ms/frame (%.1f fps ceiling)\n",
                dt / 20.0f, 20000.0f / dt);

  // ---- TEST 4: backlight ladder.
  for (int p : {50, 35, 25, 20, 15, 10, 5}) {
    gfx.setBrightness(p);
    Serial.printf("brightness %d%%\n", p);
    delay(350);
  }
  gfx.setBrightness(25);

  Serial.printf("battery: %u mV\n", (unsigned)gfx.batteryMillivolts());

  // ---- TEST 5: I2C + touch.
  Wire.begin(Elecrow35Touch::kSda, Elecrow35Touch::kScl, 400000);
  i2cScan();
  if (touch.begin()) Serial.println("touch OK — drag on the panel");
  else Serial.println("!! touch init failed — try swapping kRst/kInt in Elecrow35Touch.h");

  gfx.clear(Color(8, 8, 16));
  gfx.present();
}

void loop() {
  Elecrow35Touch::Point p[5];
  const int n = touch.read(p, 5);
  static int lastN = 0;
  for (int i = 0; i < n; ++i) {
    gfx.fillRect(p[i].x - 3, p[i].y - 3, 6, 6, Color(0x5A, 0xC9, 0xEF));
  }
  if (n != lastN) {
    Serial.printf("touch points: %d", n);
    if (n) Serial.printf("  first at %u,%u", p[0].x, p[0].y);
    Serial.println();
    lastN = n;
  }
  gfx.present();
  delay(16);
}
