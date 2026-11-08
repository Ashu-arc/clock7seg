#pragma once
/*
 * Clock7Seg Pro - 7 segment pixel-LED clock usermod for WLED (ESP8266 + ESP32)
 *
 * LED chain order (one data line):
 *   [digit 1][digit 2]...[digit N] [colon LEDs] [weekday LEDs SUN..SAT]
 *   each digit  = 7 segments, each segment = 1..10 LEDs, physical order set in settings (default F-A-B-C-D-E-G)
 *   N = 6 (HH:MM:SS) or 4 (HH:MM)
 *
 * The clock only switches LEDs OFF that must be dark. Colour/effect/brightness
 * come from the normal WLED main page (use the "Solid" effect for a plain clock).
 */

#include "wled.h"

#ifdef ESP8266
  #define C7_DEF_SDA 4    // D2
  #define C7_DEF_SCL 5    // D1
  #define C7_DEF_LDR 17   // A0 (fixed on ESP8266)
#else
  #define C7_DEF_SDA 21
  #define C7_DEF_SCL 22
  #define C7_DEF_LDR 34
#endif

#define C7_RTC_ADDR 0x68  // DS1307 / DS3231 / PCF-less DS-series

class UsermodClock7Seg : public Usermod {
  private:
    // ---------- settings ----------
    bool    enabled      = true;
    uint8_t mode         = 0;      // 0 = HH:MM:SS (6 digits), 1 = HH:MM (4 digits)
    uint8_t hourFormat   = 0;      // 0 = 24H, 1 = 12H
    bool    leadingZero  = true;   // show 0 before hour 1-9
    bool    colonBlink   = true;
    bool    weekdaysOn   = true;

    uint16_t firstLed    = 0;
    uint8_t segPreset    = 1;      // 0 = custom text, 1..3 presets
    char    segCustom[9] = "FABCDEG";
    uint8_t ledsPerSeg   = 1;      // 1..10
    uint8_t colonLeds    = 2;      // 1..6
    uint8_t ledsPerDay   = 1;      // 1..5

    bool    dateShow     = false;
    uint8_t dateOrder    = 0;      // 0 = DD MM, 1 = MM DD
    uint8_t secTime      = 6;
    uint8_t secDayMonth  = 2;
    uint8_t secYear      = 2;

    char    wdHex[10]    = "";     // empty = use effect colour
    char    colonHex[10] = "";

    bool    nightOn      = false;
    uint8_t nightStart   = 23;
    uint8_t nightEnd     = 6;

    bool    autoBri      = false;
    int8_t  ldrPin       = C7_DEF_LDR;
    bool    ldrInvert    = false;
    uint8_t briMin       = 20;
    uint8_t briMax       = 255;

    bool    rtcEnabled   = true;
    int8_t  rtcSda       = C7_DEF_SDA;
    int8_t  rtcScl       = C7_DEF_SCL;
    uint8_t rtcSyncHours = 6;      // 0 = only first sync after boot

    uint8_t testMode     = 0;      // 0 off, 1 all on, 2 segment walk, 3 digit count

    // ---------- runtime ----------
    char     order[8]    = "FABCDEG";
    bool     orderBad    = false;
    bool     setupDone   = false;
    bool     configRead  = false;
    bool     pinsHeld    = false;
    int8_t   heldSda = -1, heldScl = -1, heldLdr = -1;
    bool     rtcReady    = false;
    bool     rtcPinError = false;
    bool     rtcLoaded   = false;
    bool     rtcWritten  = false;
    unsigned long lastRtcWrite = 0, lastRtcTry = 0, lastTick = 0, lastBriApply = 0;
    uint32_t lastSig     = 0xFFFFFFFF;
    float    ldrAvg      = -1;

    // bit0=A ... bit6=G ; index 0-9 digits, 10 blank, 11 dash
    static constexpr uint8_t PAT[12] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x00,0x40};

    static const char _name[];

    // ---------- helpers ----------
    uint8_t digitCount() const { return mode == 0 ? 6 : 4; }
    uint16_t colonBase() const { return firstLed + (uint16_t)digitCount() * 7 * ledsPerSeg; }
    uint16_t dayBase()   const { return colonBase() + colonLeds; }
    uint16_t ledsNeeded() const { return (uint16_t)digitCount() * 7 * ledsPerSeg + colonLeds + 7 * ledsPerDay; }

    static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
    static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

    bool parseHex(const char* s, uint32_t& out) const {
      if (!s) return false;
      if (*s == '#') s++;
      if (strlen(s) != 6) return false;
      for (int i = 0; i < 6; i++) if (!isxdigit((unsigned char)s[i])) return false;
      out = (uint32_t)strtoul(s, nullptr, 16);
      return true;
    }

    void buildOrder() {
      orderBad = false;
      switch (segPreset) {
        case 1: strcpy(order, "FABCDEG"); return;
        case 2: strcpy(order, "ABCDEFG"); return;
        case 3: strcpy(order, "GFEDCBA"); return;
        default: break;
      }
      char tmp[8]; uint8_t seen = 0; uint8_t n = 0;
      for (const char* p = segCustom; *p && n < 7; p++) {
        char c = toupper((unsigned char)*p);
        if (c == '-' || c == ' ' || c == ',') continue;       // allow "F-A-B-C-D-E-G"
        if (c < 'A' || c > 'G' || (seen & (1 << (c - 'A')))) { orderBad = true; break; }
        seen |= 1 << (c - 'A'); tmp[n++] = c;
      }
      if (orderBad || n != 7) { orderBad = true; strcpy(order, "FABCDEG"); return; }
      tmp[7] = 0; strcpy(order, tmp);
    }

    // ---------- RTC (DS1307 / DS3231, time stored as UTC like WLED core) ----------
    bool rtcProbe() {
      Wire.beginTransmission(C7_RTC_ADDR);
      return Wire.endTransmission() == 0;
    }

    bool rtcRead(time_t &out) {
      Wire.beginTransmission(C7_RTC_ADDR);
      Wire.write((uint8_t)0);
      if (Wire.endTransmission() != 0) return false;
      if (Wire.requestFrom((uint8_t)C7_RTC_ADDR, (uint8_t)7) != 7) return false;
      uint8_t b[7];
      for (int i = 0; i < 7; i++) b[i] = Wire.read();
      if (b[0] & 0x80) return false;                          // DS1307 clock halted
      tmElements_t tm;
      tm.Second = bcd2dec(b[0] & 0x7F);
      tm.Minute = bcd2dec(b[1] & 0x7F);
      uint8_t h;
      if (b[2] & 0x40) { h = bcd2dec(b[2] & 0x1F); if (h == 12) h = 0; if (b[2] & 0x20) h += 12; }
      else h = bcd2dec(b[2] & 0x3F);
      tm.Hour  = h;
      tm.Day   = bcd2dec(b[4] & 0x3F);
      tm.Month = bcd2dec(b[5] & 0x1F);
      tm.Year  = bcd2dec(b[6]) + 30;                          // 2000 -> offset from 1970
      if (tm.Second > 59 || tm.Minute > 59 || tm.Hour > 23 || tm.Day < 1 || tm.Day > 31 || tm.Month < 1 || tm.Month > 12) return false;
      if (tm.Year < 50) return false;                         // before 2020 = RTC never set
      out = makeTime(tm);
      return true;
    }

    bool rtcWrite(time_t t) {
      tmElements_t tm; breakTime(t, tm);
      Wire.beginTransmission(C7_RTC_ADDR);
      Wire.write((uint8_t)0);
      Wire.write(dec2bcd(tm.Second));                         // CH bit cleared -> oscillator runs
      Wire.write(dec2bcd(tm.Minute));
      Wire.write(dec2bcd(tm.Hour));                           // 24H
      Wire.write(dec2bcd(tm.Wday));
      Wire.write(dec2bcd(tm.Day));
      Wire.write(dec2bcd(tm.Month));
      Wire.write(dec2bcd(tm.Year - 30));
      return Wire.endTransmission() == 0;
    }

    void rtcLoadToWled() {
      time_t t;
      if (!rtcRead(t)) return;
      if (toki.getTimeSource() < TOKI_TS_RTC) {
        toki.setTime(t, TOKI_NO_MS_ACCURACY, TOKI_TS_RTC);
        updateLocalTime();
      }
      rtcLoaded = true;
    }

    // ---------- pins / hardware ----------
    void releasePins() {
      if (heldSda >= 0) { pinManager.deallocatePin(heldSda, PinOwner::UM_Unspecified); heldSda = -1; }
      if (heldScl >= 0) { pinManager.deallocatePin(heldScl, PinOwner::UM_Unspecified); heldScl = -1; }
      if (heldLdr >= 0) { pinManager.deallocatePin(heldLdr, PinOwner::UM_Unspecified); heldLdr = -1; }
    }

    void initHw() {
      releasePins();
      rtcReady = false; rtcPinError = false; rtcLoaded = false; ldrAvg = -1;
      if (rtcEnabled && rtcSda >= 0 && rtcScl >= 0 && rtcSda != rtcScl) {
        bool sameAsGlobal = (rtcSda == i2c_sda && rtcScl == i2c_scl);
        bool ok = true;
        if (!sameAsGlobal) {
          managed_pin_type p[2] = { {(int8_t)rtcSda, true}, {(int8_t)rtcScl, true} };
          if (pinManager.allocateMultiplePins(p, 2, PinOwner::UM_Unspecified)) { heldSda = rtcSda; heldScl = rtcScl; }
          else { ok = false; rtcPinError = true; }
        }
        if (ok) {
          if (!sameAsGlobal) {
            Wire.begin(rtcSda, rtcScl);   // (old ESP32 core 1.0.x has no Wire.end())
          }
          rtcReady = rtcProbe();
          if (rtcReady) rtcLoadToWled();
        }
      }
      #ifndef ESP8266
      if (autoBri && ldrPin >= 0) {
        if (pinManager.allocatePin(ldrPin, false, PinOwner::UM_Unspecified)) heldLdr = ldrPin;
      }
      #endif
    }

    // ---------- LDR ----------
    int readLdr() {   // returns 0..1023
      #ifdef ESP8266
        int raw = analogRead(A0);
      #else
        if (heldLdr < 0) return -1;
        int raw = analogRead(heldLdr) >> 2;                   // 12bit -> 10bit
      #endif
      if (ldrInvert) raw = 1023 - raw;
      return raw;
    }

    void handleLdr(unsigned long now) {
      if (!autoBri) return;
      int raw = readLdr();
      if (raw < 0) return;
      if (ldrAvg < 0) ldrAvg = raw; else ldrAvg += ((float)raw - ldrAvg) / 8.0f;
      if (now - lastBriApply < 1000 || bri == 0) return;
      lastBriApply = now;
      uint8_t lo = briMin < briMax ? briMin : briMax;
      uint8_t hi = briMin < briMax ? briMax : briMin;
      int target = lo + (int)((hi - lo) * (ldrAvg / 1023.0f));
      if (abs(target - (int)bri) >= 4) { bri = (uint8_t)target; stateUpdated(CALL_MODE_DIRECT_CHANGE); }
    }

    // ---------- display logic ----------
    bool inNight() const {
      if (!nightOn) return false;
      uint8_t h = hour(localTime);
      if (nightStart == nightEnd) return false;
      return nightStart < nightEnd ? (h >= nightStart && h < nightEnd) : (h >= nightStart || h < nightEnd);
    }

    uint8_t displayState() const {   // 0 time, 1 day+month, 2 year
      if (!dateShow) return 0;
      uint16_t cyc = (uint16_t)secTime + secDayMonth + secYear;
      if (cyc == 0) return 0;
      uint16_t p = (uint32_t)localTime % cyc;
      if (p < secTime) return 0;
      if (p < (uint16_t)secTime + secDayMonth) return 1;
      return 2;
    }

    // Builds segment bit masks per digit, colon state, weekday mask
    void buildFrame(uint8_t* patt, bool& colonOn, uint8_t& wdMask) {
      uint8_t n = digitCount();
      for (uint8_t i = 0; i < 6; i++) patt[i] = 0;
      colonOn = false; wdMask = 0;
      time_t t = localTime;
      uint8_t wd = weekday(t);                                // 1 = Sunday
      if (wd >= 1 && wd <= 7) wdMask = 1 << (wd - 1);

      if (testMode) {
        uint8_t step = (millis() / 800) % 7;
        if (testMode == 1) { for (uint8_t i = 0; i < n; i++) patt[i] = 0x7F; colonOn = true; wdMask = 0x7F; }
        else if (testMode == 2) { for (uint8_t i = 0; i < n; i++) patt[i] = 1 << step; wdMask = 1 << step; }
        else { uint8_t d = (millis() / 1000) % 10; for (uint8_t i = 0; i < n; i++) patt[i] = PAT[d]; colonOn = true; }
        return;
      }

      uint8_t v[6] = {10,10,10,10,10,10};
      uint8_t st = displayState();
      if (st == 0) {
        int h = hourFormat ? hourFormat12(t) : hour(t);
        int m = minute(t), s = second(t);
        v[0] = (h < 10 && !leadingZero) ? 10 : h / 10; v[1] = h % 10;
        v[2] = m / 10; v[3] = m % 10;
        if (mode == 0) { v[4] = s / 10; v[5] = s % 10; }
        colonOn = !colonBlink || (toki.getTime().ms < 500);
      } else if (st == 1) {
        int a = dateOrder == 0 ? day(t) : month(t);
        int b = dateOrder == 0 ? month(t) : day(t);
        if (mode == 0) { v[0] = a / 10; v[1] = a % 10; v[2] = 11; v[3] = 11; v[4] = b / 10; v[5] = b % 10; }   // DD -- MM
        else           { v[0] = a / 10; v[1] = a % 10; v[2] = b / 10; v[3] = b % 10; }                         // DD MM
      } else {
        int y = year(t);
        if (mode == 0) { v[0] = 10; v[1] = y / 1000; v[2] = (y / 100) % 10; v[3] = (y / 10) % 10; v[4] = y % 10; v[5] = 10; }
        else           { v[0] = y / 1000; v[1] = (y / 100) % 10; v[2] = (y / 10) % 10; v[3] = y % 10; }
      }
      for (uint8_t i = 0; i < n; i++) patt[i] = PAT[v[i]];
    }

    uint32_t frameSignature() {
      uint8_t patt[6]; bool colon; uint8_t wdm;
      buildFrame(patt, colon, wdm);
      uint32_t sig = 17;
      for (uint8_t i = 0; i < 6; i++) sig = sig * 131 + patt[i];
      sig = sig * 3 + (colon ? 1 : 0);
      sig = sig * 131 + wdm;
      sig = sig * 3 + (inNight() ? 1 : 0);
      return sig;
    }

  public:
    void setup() {
      setupDone = true;
      buildOrder();
      if (configRead) initHw();
    }

    void loop() {
      unsigned long now = millis();
      if (now - lastTick < 100) return;
      lastTick = now;

      // RTC maintenance (once per second is plenty)
      if (rtcEnabled && rtcReady) {
        if (!rtcLoaded && now - lastRtcTry > 10000) { lastRtcTry = now; rtcLoadToWled(); }
        uint8_t src = toki.getTimeSource();
        if (src > TOKI_TS_RTC) {                              // time came from NTP / browser
          bool due = !rtcWritten || (rtcSyncHours > 0 && now - lastRtcWrite > (unsigned long)rtcSyncHours * 3600000UL);
          if (due && toki.isTick()) {
            if (rtcWrite(toki.second())) { rtcWritten = true; lastRtcWrite = now; }
            else lastRtcWrite = now - (unsigned long)rtcSyncHours * 3600000UL + 60000UL;   // retry in 1 min
          }
        }
      }

      if (!enabled) return;
      handleLdr(now);

      uint32_t sig = frameSignature();
      if (sig != lastSig) { lastSig = sig; strip.trigger(); }
    }

    void handleOverlayDraw() {
      if (!enabled) return;
      uint16_t total = strip.getLengthTotal();
      uint8_t n = digitCount();
      uint16_t endLed = firstLed + ledsNeeded();
      if (endLed > total) endLed = total;

      if (inNight() && !testMode) {
        for (uint16_t i = firstLed; i < endLed; i++) strip.setPixelColor(i, 0);
        return;
      }

      uint8_t patt[6]; bool colon; uint8_t wdm;
      buildFrame(patt, colon, wdm);

      // digits
      for (uint8_t d = 0; d < n; d++) {
        for (uint8_t s = 0; s < 7; s++) {
          bool lit = (patt[d] >> (order[s] - 'A')) & 1;
          if (lit) continue;
          uint16_t base = firstLed + ((uint16_t)d * 7 + s) * ledsPerSeg;
          for (uint8_t k = 0; k < ledsPerSeg; k++) if (base + k < total) strip.setPixelColor(base + k, 0);
        }
      }

      // colon
      uint32_t col;
      bool hasColonCol = parseHex(colonHex, col);
      for (uint8_t k = 0; k < colonLeds; k++) {
        uint16_t i = colonBase() + k;
        if (i >= total) break;
        if (!colon) strip.setPixelColor(i, 0);
        else if (hasColonCol) strip.setPixelColor(i, col);
      }

      // weekdays
      uint32_t wcol;
      bool hasWdCol = parseHex(wdHex, wcol);
      for (uint8_t d = 0; d < 7; d++) {
        bool lit = weekdaysOn && ((wdm >> d) & 1);
        for (uint8_t k = 0; k < ledsPerDay; k++) {
          uint16_t i = dayBase() + d * ledsPerDay + k;
          if (i >= total) break;
          if (!lit) strip.setPixelColor(i, 0);
          else if (hasWdCol) strip.setPixelColor(i, wcol);
        }
      }
    }

    // ---------- JSON ----------
    void addToJsonInfo(JsonObject& root) {
      JsonObject user = root[F("u")];
      if (user.isNull()) user = root.createNestedObject(F("u"));

      JsonArray need = user.createNestedArray(F("Clock LEDs needed"));
      need.add(ledsNeeded() + firstLed);
      uint16_t total = strip.getLengthTotal();
      need.add(total >= ledsNeeded() + firstLed ? F(" (strip OK)") : F(" - raise LED count in LED Preferences!"));

      JsonArray r = user.createNestedArray(F("Clock RTC"));
      if (!rtcEnabled) r.add(F("disabled"));
      else if (rtcPinError) r.add(F("pin conflict"));
      else if (rtcReady) r.add(rtcLoaded || rtcWritten ? F("OK") : F("found, time not set yet"));
      else r.add(F("NOT found - check SDA/SCL/power"));

      if (orderBad) { JsonArray o = user.createNestedArray(F("Clock seg order")); o.add(F("invalid - using FABCDEG")); }
    }

    void addToJsonState(JsonObject& root) {
      JsonObject o = root.createNestedObject(F("clk7"));
      o[F("test")] = testMode;
      o[F("date")] = dateShow;
      o[F("on")]   = enabled;
    }

    void readFromJsonState(JsonObject& root) {
      JsonObject o = root[F("clk7")];
      if (o.isNull()) return;
      testMode = o[F("test")] | testMode;
      dateShow = o[F("date")] | dateShow;
      enabled  = o[F("on")]   | enabled;
    }

    // ---------- settings page ----------
    void addToConfig(JsonObject& root) {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[F("enabled")] = enabled;

      JsonObject w = top.createNestedObject(F("wiring"));
      w[F("first-led")] = firstLed;
      w[F("seg-preset")] = segPreset;
      w[F("seg-order")] = segCustom;
      w[F("leds-per-segment")] = ledsPerSeg;
      w[F("colon-leds")] = colonLeds;
      w[F("leds-per-weekday")] = ledsPerDay;

      JsonObject d = top.createNestedObject(F("display"));
      d[F("mode")] = mode;
      d[F("hour-format")] = hourFormat;
      d[F("leading-zero")] = leadingZero;
      d[F("colon-blink")] = colonBlink;
      d[F("weekdays-on")] = weekdaysOn;

      JsonObject dt = top.createNestedObject(F("date"));
      dt[F("show")] = dateShow;
      dt[F("order")] = dateOrder;
      dt[F("time-sec")] = secTime;
      dt[F("day-month-sec")] = secDayMonth;
      dt[F("year-sec")] = secYear;

      JsonObject c = top.createNestedObject(F("colors"));
      c[F("weekday-hex")] = wdHex;
      c[F("colon-hex")] = colonHex;

      JsonObject n = top.createNestedObject(F("night"));
      n[F("enabled")] = nightOn;
      n[F("start-hour")] = nightStart;
      n[F("end-hour")] = nightEnd;

      JsonObject b = top.createNestedObject(F("brightness"));
      b[F("auto")] = autoBri;
      b[F("ldr-pin")] = ldrPin;
      b[F("ldr-invert")] = ldrInvert;
      b[F("min")] = briMin;
      b[F("max")] = briMax;

      JsonObject r = top.createNestedObject(F("rtc"));
      r[F("enabled")] = rtcEnabled;
      r[F("sda-pin")] = rtcSda;
      r[F("scl-pin")] = rtcScl;
      r[F("sync-hours")] = rtcSyncHours;

      JsonObject t = top.createNestedObject(F("test"));
      t[F("mode")] = testMode;
    }

    bool readFromConfig(JsonObject& root) {
      JsonObject top = root[FPSTR(_name)];
      bool ok = !top.isNull();
      if (!ok) { configRead = true; if (setupDone) { buildOrder(); initHw(); } return false; }

      ok &= getJsonValue(top[F("enabled")], enabled, true);

      JsonObject w = top[F("wiring")];
      ok &= getJsonValue(w[F("first-led")], firstLed, (uint16_t)0);
      ok &= getJsonValue(w[F("seg-preset")], segPreset, (uint8_t)1);
      const char* so = w[F("seg-order")] | "";
      if (*so) { strncpy(segCustom, so, sizeof(segCustom) - 1); segCustom[sizeof(segCustom) - 1] = 0; } else ok = false;
      ok &= getJsonValue(w[F("leds-per-segment")], ledsPerSeg, (uint8_t)1);
      ok &= getJsonValue(w[F("colon-leds")], colonLeds, (uint8_t)2);
      ok &= getJsonValue(w[F("leds-per-weekday")], ledsPerDay, (uint8_t)1);

      JsonObject d = top[F("display")];
      ok &= getJsonValue(d[F("mode")], mode, (uint8_t)0);
      ok &= getJsonValue(d[F("hour-format")], hourFormat, (uint8_t)0);
      ok &= getJsonValue(d[F("leading-zero")], leadingZero, true);
      ok &= getJsonValue(d[F("colon-blink")], colonBlink, true);
      ok &= getJsonValue(d[F("weekdays-on")], weekdaysOn, true);

      JsonObject dt = top[F("date")];
      ok &= getJsonValue(dt[F("show")], dateShow, false);
      ok &= getJsonValue(dt[F("order")], dateOrder, (uint8_t)0);
      ok &= getJsonValue(dt[F("time-sec")], secTime, (uint8_t)6);
      ok &= getJsonValue(dt[F("day-month-sec")], secDayMonth, (uint8_t)2);
      ok &= getJsonValue(dt[F("year-sec")], secYear, (uint8_t)2);

      JsonObject c = top[F("colors")];
      const char* s1 = c[F("weekday-hex")] | "";
      strncpy(wdHex, s1, sizeof(wdHex) - 1); wdHex[sizeof(wdHex) - 1] = 0;
      const char* s2 = c[F("colon-hex")] | "";
      strncpy(colonHex, s2, sizeof(colonHex) - 1); colonHex[sizeof(colonHex) - 1] = 0;
      if (c.isNull()) ok = false;

      JsonObject n = top[F("night")];
      ok &= getJsonValue(n[F("enabled")], nightOn, false);
      ok &= getJsonValue(n[F("start-hour")], nightStart, (uint8_t)23);
      ok &= getJsonValue(n[F("end-hour")], nightEnd, (uint8_t)6);

      JsonObject b = top[F("brightness")];
      ok &= getJsonValue(b[F("auto")], autoBri, false);
      ok &= getJsonValue(b[F("ldr-pin")], ldrPin, (int8_t)C7_DEF_LDR);
      ok &= getJsonValue(b[F("ldr-invert")], ldrInvert, false);
      ok &= getJsonValue(b[F("min")], briMin, (uint8_t)20);
      ok &= getJsonValue(b[F("max")], briMax, (uint8_t)255);

      JsonObject r = top[F("rtc")];
      ok &= getJsonValue(r[F("enabled")], rtcEnabled, true);
      ok &= getJsonValue(r[F("sda-pin")], rtcSda, (int8_t)C7_DEF_SDA);
      ok &= getJsonValue(r[F("scl-pin")], rtcScl, (int8_t)C7_DEF_SCL);
      ok &= getJsonValue(r[F("sync-hours")], rtcSyncHours, (uint8_t)6);

      JsonObject t = top[F("test")];
      ok &= getJsonValue(t[F("mode")], testMode, (uint8_t)0);

      // sanity limits
      mode = mode ? 1 : 0;
      hourFormat = hourFormat ? 1 : 0;
      ledsPerSeg = constrain(ledsPerSeg, 1, 10);
      colonLeds  = constrain(colonLeds, 1, 6);
      ledsPerDay = constrain(ledsPerDay, 1, 5);
      segPreset  = segPreset > 3 ? 1 : segPreset;
      dateOrder  = dateOrder ? 1 : 0;
      secTime = constrain(secTime, 1, 60); secDayMonth = constrain(secDayMonth, 1, 30); secYear = constrain(secYear, 1, 30);
      nightStart %= 24; nightEnd %= 24;
      testMode = testMode > 3 ? 0 : testMode;

      configRead = true;
      lastSig = 0xFFFFFFFF;
      if (setupDone) { buildOrder(); initHw(); }
      return ok;
    }

    void appendConfigData() {
      oappend(SET_F("dd=addDropdown('Clock7Seg:display','mode');"));
      oappend(SET_F("addOption(dd,'HH:MM:SS (6 digits)',0);addOption(dd,'HH:MM (4 digits)',1);"));
      oappend(SET_F("dd=addDropdown('Clock7Seg:display','hour-format');"));
      oappend(SET_F("addOption(dd,'24 hour',0);addOption(dd,'12 hour',1);"));
      oappend(SET_F("dd=addDropdown('Clock7Seg:wiring','seg-preset');"));
      oappend(SET_F("addOption(dd,'Custom (use text below)',0);addOption(dd,'F-A-B-C-D-E-G (default)',1);"));
      oappend(SET_F("addOption(dd,'A-B-C-D-E-F-G',2);addOption(dd,'G-F-E-D-C-B-A',3);"));
      oappend(SET_F("dd=addDropdown('Clock7Seg:date','order');"));
      oappend(SET_F("addOption(dd,'Day then Month',0);addOption(dd,'Month then Day',1);"));
      oappend(SET_F("dd=addDropdown('Clock7Seg:test','mode');"));
      oappend(SET_F("addOption(dd,'Off (normal clock)',0);addOption(dd,'All segments ON',1);"));
      oappend(SET_F("addOption(dd,'Segment walk A to G',2);addOption(dd,'Digits 0-9 count',3);"));

      oappend(SET_F("addInfo('Clock7Seg:wiring:first-led',1,'LED number where digit 1 starts (0 = first)');"));
      oappend(SET_F("addInfo('Clock7Seg:wiring:seg-order',1,'only used if preset = Custom. 7 letters A-G, e.g. FABCDEG');"));
      oappend(SET_F("addInfo('Clock7Seg:wiring:leds-per-segment',1,'1 to 10');"));
      oappend(SET_F("addInfo('Clock7Seg:wiring:colon-leds',1,'1 to 6 (all colon dots together)');"));
      oappend(SET_F("addInfo('Clock7Seg:wiring:leds-per-weekday',1,'1 to 5');"));
      oappend(SET_F("addInfo('Clock7Seg:colors:weekday-hex',1,'RRGGBB, empty = effect colour');"));
      oappend(SET_F("addInfo('Clock7Seg:colors:colon-hex',1,'RRGGBB, empty = effect colour');"));
      oappend(SET_F("addInfo('Clock7Seg:night:start-hour',1,'0-23, display OFF from');"));
      oappend(SET_F("addInfo('Clock7Seg:night:end-hour',1,'0-23, display back ON at');"));
      oappend(SET_F("addInfo('Clock7Seg:brightness:ldr-pin',1,'ESP32 only (ADC1: 32-39). ESP8266 uses A0');"));
      oappend(SET_F("addInfo('Clock7Seg:rtc:sync-hours',1,'write NTP time to RTC every N hours (0 = once after boot)');"));
    }

    uint16_t getId() { return USERMOD_ID_UNSPECIFIED; }
};

const char UsermodClock7Seg::_name[] PROGMEM = "Clock7Seg";
constexpr uint8_t UsermodClock7Seg::PAT[12];
