#pragma once
/*
 * Ashish Automation - 7 segment pixel-LED clock usermod for WLED 0.15.0 (ESP8266 + ESP32)
 *
 * LED chain order (one data line):
 *   HH -> colon -> MM -> colon -> SS -> weekdays SUN..SAT -> AM LEDs -> PM LEDs   (HH:MM:SS)
 *   HH -> colon -> MM -> weekdays SUN..SAT -> AM LEDs -> PM LEDs                   (HH:MM)
 *   (AM/PM LEDs only exist in the chain when the AM/PM option is on)
 *   digit = 7 segments (physical order selectable, default F-A-B-C-D-E-G), 1..10 LEDs per segment
 *   colon = 1..6 LEDs per colon
 *
 * The usermod only switches LEDs OFF (or recolours lit ones if you set a colour).
 * Use the "Solid" effect on the main page for a plain clock.
 */

#include "wled.h"

// works on WLED 0.14.4 (VERSION 2405180) and WLED 0.15.0 (VERSION 2412100)
#if VERSION >= 2410000
  #define C7_PM PinManager::
  #define C7_OAPPEND(x) oappend(F(x))
#else
  #define C7_PM pinManager.
  #define C7_OAPPEND(x) oappend(SET_F(x))
#endif

#ifdef ESP8266
  #define C7_DEF_SDA 4    // D2
  #define C7_DEF_SCL 5    // D1
  #define C7_DEF_LDR 17   // A0 (fixed on ESP8266)
#else
  #define C7_DEF_SDA 21
  #define C7_DEF_SCL 22
  #define C7_DEF_LDR 34
#endif

#define C7_RTC_ADDR 0x68  // DS1307 / DS3231

class UsermodAshishClock7Seg : public Usermod {
  private:
    // ---------- settings ----------
    bool    enabled      = true;
    uint8_t mode         = 0;      // 0 = HH:MM:SS (6 digits), 1 = HH:MM (4 digits)
    uint8_t hourFormat   = 0;      // 0 = 24H, 1 = 12H
    bool    leadingZero  = true;
    bool    colonBlink   = true;
    uint8_t weekdayMode  = 1;      // 0 off, 1 today only, 2 all on (today highlighted)
    bool    startupTest  = false;  // lamp test for 3 s after power on

    uint16_t firstLed    = 0;
    uint8_t segPreset    = 1;      // 0 = custom, 1 = F-A-B-G-E-D-C (default), 2 = A-B-C-D-E-F-G, 3 = F-A-B-C-D-E-G
    char    segCustom[9] = "FABGEDC";
    uint8_t ledsPerSeg   = 1;      // 1..10
    uint8_t colonLeds    = 2;      // 1..6 LEDs PER colon
    uint8_t ledsPerDay   = 1;      // 1..5
    bool    ampmOn       = false;  // AM / PM LEDs after the weekday LEDs
    bool    ampmBlink    = false;  // the lit AM/PM LED blinks
    bool    blankExtra   = true;   // turn off LEDs after the clock chain
    uint8_t ampmLeds     = 1;      // 1..6 LEDs for AM, same for PM

    bool    dateShow     = false;
    uint8_t dateOrder    = 0;      // 0 = DD MM, 1 = MM DD
    uint8_t secTime      = 6;
    uint8_t secDayMonth  = 2;
    uint8_t secYear      = 2;

    bool    tempShow     = false;  // needs DS3231
    uint8_t tempUnit     = 0;      // 0 = C, 1 = F
    uint8_t tempSource   = 0;      // 0 = DS3231 RTC, 1 = BMP280
    uint8_t secTemp      = 2;
    int8_t  tempOffset   = 0;      // degrees C added to reading

    char    wdHex[10]    = "";     // today colour (empty = effect colour; mode 2 default red)
    char    otherHex[10] = "";     // other days colour in mode 2 (empty = effect colour)
    char    weekendHex[10] = "";   // Sat/Sun colour in mode 2 (empty = other days colour)
    char    amHex[10]    = "";
    char    pmHex[10]    = "";
    char    colonHex[10] = "";
    char    dateHex[10]  = "";
    char    tempHex[10]  = "";

    bool    alarmOn      = false;
    uint8_t alarmHour    = 7;
    uint8_t alarmMin     = 0;
    uint8_t alarmDays    = 0;      // 0 every day, 1 Mon-Fri, 2 Sat-Sun
    uint8_t ringSec      = 30;
    uint8_t snoozeMin    = 5;      // 0 = no snooze

    bool    chimeOn      = false;
    uint8_t chimeStart   = 8;
    uint8_t chimeEnd     = 22;

    int8_t  buzzerPin    = -1;
    bool    buzzerInvert = false;
    int8_t  buttonPin    = -1;

    bool    nightOn      = false;
    uint8_t nightStart   = 23;
    uint8_t nightEnd     = 6;
    uint8_t nightAction  = 0;      // 0 display off, 1 dim
    uint8_t nightDim     = 20;     // percent brightness when dimmed

    bool    autoBri      = false;
    int8_t  ldrPin       = C7_DEF_LDR;
    bool    ldrInvert    = false;
    uint8_t briMin       = 20;
    uint8_t briMax       = 255;

    bool    cdOn         = true;   // countdown timer enabled
    bool    swOn         = true;   // stopwatch enabled
    uint8_t cdMin        = 10;     // countdown minutes 1..99
    uint8_t timerRingSec = 10;     // beep time when countdown ends
    char    timerHex[10] = "";
    bool    tempAutoColor = false; // blue cold, green normal, red hot
    bool    warnShow     = true;   // show bAtt when RTC battery/time problem
    bool    battCheck    = true;   // RTC battery / time check on or off
    bool    rtcEnabled   = true;
    uint8_t rtcChip      = 0;      // 0 = DS1307, 1 = DS3231
    int8_t  rtcSda       = C7_DEF_SDA;
    int8_t  rtcScl       = C7_DEF_SCL;
    uint8_t rtcSyncHours = 6;
    bool    rereadOn     = true;   // re-read RTC when there is no internet time
    uint8_t rereadMin    = 5;      // minutes between re-reads

    uint8_t testMode     = 0;      // 0 off, 1 all on, 2 segment walk, 3 digit count

    // ---------- runtime ----------
    char     order[8]    = "FABGEDC";
    bool     orderBad    = false;
    bool     setupDone   = false;
    bool     configRead  = false;
    int8_t   heldSda = -1, heldScl = -1, heldLdr = -1, heldBuz = -1, heldBtn = -1;
    bool     rtcReady    = false;
    bool     rtcPinError = false;
    bool     rtcLoaded   = false;
    bool     rtcWritten  = false;
    float    tempC       = -100.0f;
    float    pressHpa    = 0.0f;
    bool     bmpReady    = false;
    uint8_t  bmpAddr     = 0x76;
    int32_t  tFine       = 0;
    uint16_t dT1 = 0, dP1 = 0;
    int16_t  dT2 = 0, dT3 = 0, dP2 = 0, dP3 = 0, dP4 = 0, dP5 = 0, dP6 = 0, dP7 = 0, dP8 = 0, dP9 = 0;
    bool     offline     = true;
    int32_t  rtcDiff     = 0;
    uint16_t rereadCount = 0;
    unsigned long lastReread = 0, lastRereadAt = 0;
    uint8_t  ampmMask    = 0;
    uint8_t  timerMode = 0;        // 0 clock, 1 countdown, 2 stopwatch
    bool     timerRunning = false, timerDone = false, pendingShort = false;
    unsigned long timerStartMs = 0, timerAccumMs = 0, timerRingEnd = 0, lastBtnAct = 0, pendingAt = 0, lastBattCheck = 0;
    bool     rtcWarn = false, rtcOsf = false, rtcInvalid = false;
    unsigned long lastRtcWrite = 0, lastRtcTry = 0, lastTick = 0, lastBriApply = 0, lastTemp = 0;
    uint32_t lastSig     = 0xFFFFFFFF;
    float    ldrAvg      = -1;
    uint8_t  lastBriSet  = 0;

    // alarm / chime / button runtime
    bool     alarmArmed  = true;   // quick toggle with long button press (not saved)
    bool     ringing     = false;
    unsigned long ringEnd = 0, chimeBeepStart = 0, beepUntil = 0, peekStart = 0;
    uint32_t snoozeAt = 0, alarmKey = 0, chimeKey = 0;
    bool     btnDown = false;
    unsigned long bootTestUntil = 0;
    unsigned long btnSince = 0, btnChange = 0;

    // bit0=A ... bit6=G ; 0-9 digits, 10 blank, 11 dash, 12 degree, 13 C, 14 F, 15 b, 16 A, 17 t
    static constexpr uint8_t PAT[18] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x00,0x40,0x63,0x39,0x71,0x7C,0x77,0x78};

    static const char _name[];

    // ---------- helpers ----------
    uint8_t digitCount() const { return mode == 0 ? 6 : 4; }
    uint8_t colonCount() const { return mode == 0 ? 2 : 1; }
    // chain: HH colon MM colon SS weekdays  -> a colon sits after every pair of digits
    uint16_t digitBase(uint8_t d) const { return firstLed + (uint16_t)d * 7 * ledsPerSeg + (uint16_t)(d / 2) * colonLeds; }
    uint16_t colonBaseAt(uint8_t k) const { return firstLed + (uint16_t)(k + 1) * 2 * 7 * ledsPerSeg + (uint16_t)k * colonLeds; }
    uint16_t dayBase()   const { return firstLed + (uint16_t)digitCount() * 7 * ledsPerSeg + (uint16_t)colonCount() * colonLeds; }
    uint16_t ampmBase()  const { return dayBase() + 7 * ledsPerDay; }
    uint16_t ledsNeeded() const { return (uint16_t)digitCount() * 7 * ledsPerSeg + (uint16_t)colonCount() * colonLeds + 7 * ledsPerDay + (ampmOn ? 2 * ampmLeds : 0); }

    static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
    static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

    static bool parseHex(const char* s, uint32_t& out) {
      if (!s) return false;
      if (*s == '#') s++;
      if (strlen(s) != 6) return false;
      for (int i = 0; i < 6; i++) if (!isxdigit((unsigned char)s[i])) return false;
      out = (uint32_t)strtoul(s, nullptr, 16);
      return true;
    }

    static void copyStr(char* dst, size_t n, const char* src) {
      strncpy(dst, src ? src : "", n - 1); dst[n - 1] = 0;
    }

    void buildOrder() {
      orderBad = false;
      switch (segPreset) {
        case 1: strcpy(order, "FABGEDC"); return;
        case 2: strcpy(order, "ABCDEFG"); return;
        case 3: strcpy(order, "FABCDEG"); return;
        default: break;
      }
      char tmp[8]; uint8_t seen = 0; uint8_t n = 0;
      for (const char* p = segCustom; *p && n < 7; p++) {
        char c = toupper((unsigned char)*p);
        if (c == '-' || c == ' ' || c == ',') continue;
        if (c < 'A' || c > 'G' || (seen & (1 << (c - 'A')))) { orderBad = true; break; }
        seen |= 1 << (c - 'A'); tmp[n++] = c;
      }
      if (orderBad || n != 7) { orderBad = true; strcpy(order, "FABGEDC"); return; }
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
      if (b[0] & 0x80) { rtcInvalid = true; return false; }   // DS1307 clock halted
      tmElements_t tm;
      tm.Second = bcd2dec(b[0] & 0x7F);
      tm.Minute = bcd2dec(b[1] & 0x7F);
      uint8_t h;
      if (b[2] & 0x40) { h = bcd2dec(b[2] & 0x1F); if (h == 12) h = 0; if (b[2] & 0x20) h += 12; }
      else h = bcd2dec(b[2] & 0x3F);
      tm.Hour  = h;
      tm.Day   = bcd2dec(b[4] & 0x3F);
      tm.Month = bcd2dec(b[5] & 0x1F);
      tm.Year  = bcd2dec(b[6]) + 30;
      if (tm.Second > 59 || tm.Minute > 59 || tm.Hour > 23 || tm.Day < 1 || tm.Day > 31 || tm.Month < 1 || tm.Month > 12) { rtcInvalid = true; return false; }
      if (tm.Year < 50) { rtcInvalid = true; return false; }  // before 2020 = never set / battery was empty
      rtcInvalid = false;
      out = makeTime(tm);
      return true;
    }

    bool rtcWrite(time_t t) {
      tmElements_t tm; breakTime(t, tm);
      Wire.beginTransmission(C7_RTC_ADDR);
      Wire.write((uint8_t)0);
      Wire.write(dec2bcd(tm.Second));
      Wire.write(dec2bcd(tm.Minute));
      Wire.write(dec2bcd(tm.Hour));
      Wire.write(dec2bcd(tm.Wday));
      Wire.write(dec2bcd(tm.Day));
      Wire.write(dec2bcd(tm.Month));
      Wire.write(dec2bcd(tm.Year - 30));
      bool ok = Wire.endTransmission() == 0;
      if (ok) { rtcInvalid = false; if (rtcChip == 1) rtcClearOsf(); updateRtcWarn(); }
      return ok;
    }

    // DS3231 status register 0x0F bit 7 = oscillator stopped (battery was empty / power lost)
    void rtcReadOsf() {
      uint8_t st = 0;
      if (i2cReadRegs(C7_RTC_ADDR, 0x0F, &st, 1)) rtcOsf = (st & 0x80) != 0;
    }
    void rtcClearOsf() { if (i2cWriteReg(C7_RTC_ADDR, 0x0F, 0x00)) rtcOsf = false; }
    void updateRtcWarn() { rtcWarn = battCheck && rtcEnabled && rtcReady && (rtcInvalid || (rtcChip == 1 && rtcOsf)); }
    void rtcCheckBattery() {
      if (!battCheck) { rtcWarn = false; return; }
      time_t tmp;
      rtcRead(tmp);                                            // updates rtcInvalid
      if (rtcChip == 1) rtcReadOsf();
      updateRtcWarn();
    }

    bool rtcReadTemp() {                                       // DS3231 only
      Wire.beginTransmission(C7_RTC_ADDR);
      Wire.write((uint8_t)0x11);
      if (Wire.endTransmission() != 0) return false;
      if (Wire.requestFrom((uint8_t)C7_RTC_ADDR, (uint8_t)2) != 2) return false;
      int8_t msb = (int8_t)Wire.read();
      uint8_t lsb = Wire.read();
      float c = msb + (lsb >> 6) * 0.25f;
      if (c < -40.0f || c > 85.0f) return false;
      tempC = c;
      return true;
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
      if (heldSda >= 0) { C7_PM deallocatePin(heldSda, PinOwner::UM_Unspecified); heldSda = -1; }
      if (heldScl >= 0) { C7_PM deallocatePin(heldScl, PinOwner::UM_Unspecified); heldScl = -1; }
      if (heldLdr >= 0) { C7_PM deallocatePin(heldLdr, PinOwner::UM_Unspecified); heldLdr = -1; }
      if (heldBuz >= 0) { digitalWrite(heldBuz, buzzerInvert ? HIGH : LOW); C7_PM deallocatePin(heldBuz, PinOwner::UM_Unspecified); heldBuz = -1; }
      if (heldBtn >= 0) { C7_PM deallocatePin(heldBtn, PinOwner::UM_Unspecified); heldBtn = -1; }
    }

    // ---------- BMP280 / BME280 (temperature + pressure, I2C 0x76 or 0x77) ----------
    bool i2cReadRegs(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n) {
      Wire.beginTransmission(addr);
      Wire.write(reg);
      if (Wire.endTransmission() != 0) return false;
      if ((uint8_t)Wire.requestFrom(addr, n) != n) return false;
      for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
      return true;
    }

    bool i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t v) {
      Wire.beginTransmission(addr);
      Wire.write(reg);
      Wire.write(v);
      return Wire.endTransmission() == 0;
    }

    bool bmpInit() {
      for (uint8_t a = 0x76; a <= 0x77; a++) {
        uint8_t id = 0;
        if (!i2cReadRegs(a, 0xD0, &id, 1)) continue;
        if (id != 0x58 && id != 0x56 && id != 0x57 && id != 0x60) continue;   // BMP280 / BME280
        uint8_t c[24];
        if (!i2cReadRegs(a, 0x88, c, 24)) continue;
        dT1 = (uint16_t)(c[0] | (c[1] << 8));  dT2 = (int16_t)(c[2] | (c[3] << 8));  dT3 = (int16_t)(c[4] | (c[5] << 8));
        dP1 = (uint16_t)(c[6] | (c[7] << 8));  dP2 = (int16_t)(c[8] | (c[9] << 8));  dP3 = (int16_t)(c[10] | (c[11] << 8));
        dP4 = (int16_t)(c[12] | (c[13] << 8)); dP5 = (int16_t)(c[14] | (c[15] << 8)); dP6 = (int16_t)(c[16] | (c[17] << 8));
        dP7 = (int16_t)(c[18] | (c[19] << 8)); dP8 = (int16_t)(c[20] | (c[21] << 8)); dP9 = (int16_t)(c[22] | (c[23] << 8));
        i2cWriteReg(a, 0xF5, 0xA8);      // standby 1 s, filter x4
        i2cWriteReg(a, 0xF4, 0x27);      // temp x1, pressure x1, normal mode
        bmpAddr = a;
        return true;
      }
      return false;
    }

    // Bosch datasheet integer compensation. Sets tempC (deg C) and pressHpa
    void bmpCompensate(int32_t adcT, int32_t adcP) {
      int32_t v1 = ((((adcT >> 3) - ((int32_t)dT1 << 1))) * ((int32_t)dT2)) >> 11;
      int32_t v2 = (((((adcT >> 4) - ((int32_t)dT1)) * ((adcT >> 4) - ((int32_t)dT1))) >> 12) * ((int32_t)dT3)) >> 14;
      tFine = v1 + v2;
      tempC = ((tFine * 5 + 128) >> 8) / 100.0f;
      int64_t a1 = (int64_t)tFine - 128000;
      int64_t a2 = a1 * a1 * (int64_t)dP6;
      a2 = a2 + ((a1 * (int64_t)dP5) << 17);
      a2 = a2 + (((int64_t)dP4) << 35);
      a1 = ((a1 * a1 * (int64_t)dP3) >> 8) + ((a1 * (int64_t)dP2) << 12);
      a1 = (((((int64_t)1) << 47) + a1)) * ((int64_t)dP1) >> 33;
      if (a1 == 0) { pressHpa = 0; return; }
      int64_t p = 1048576 - adcP;
      p = (((p << 31) - a2) * 3125) / a1;
      a1 = (((int64_t)dP9) * (p >> 13) * (p >> 13)) >> 25;
      a2 = (((int64_t)dP8) * p) >> 19;
      p = ((p + a1 + a2) >> 8) + (((int64_t)dP7) << 4);
      pressHpa = (float)p / 256.0f / 100.0f;
    }

    bool bmpRead() {
      uint8_t d[6];
      if (!i2cReadRegs(bmpAddr, 0xF7, d, 6)) return false;
      int32_t adcP = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
      int32_t adcT = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
      if (adcT == 0x80000) return false;                      // no measurement yet
      bmpCompensate(adcT, adcP);
      return tempC > -40.0f && tempC < 85.0f;
    }

    void initHw() {
      releasePins();
      rtcReady = false; rtcPinError = false; rtcLoaded = false; ldrAvg = -1; tempC = -100.0f; bmpReady = false; pressHpa = 0;
      bool needI2c = rtcEnabled || tempSource == 1;
      if (needI2c && rtcSda >= 0 && rtcScl >= 0 && rtcSda != rtcScl) {
        bool sameAsGlobal = (rtcSda == i2c_sda && rtcScl == i2c_scl);
        bool ok = true;
        if (!sameAsGlobal) {
          managed_pin_type p[2] = { {(int8_t)rtcSda, true}, {(int8_t)rtcScl, true} };
          if (C7_PM allocateMultiplePins(p, 2, PinOwner::UM_Unspecified)) { heldSda = rtcSda; heldScl = rtcScl; }
          else { ok = false; rtcPinError = true; }
        }
        if (ok) {
          if (!sameAsGlobal) Wire.begin(rtcSda, rtcScl);
          if (rtcEnabled) {
            rtcReady = rtcProbe();
            if (rtcReady) { rtcLoadToWled(); if (rtcChip == 1 && tempSource == 0) rtcReadTemp(); rtcCheckBattery(); }
          }
          if (tempSource == 1) bmpReady = bmpInit();
        }
      }
      #ifndef ESP8266
      if (autoBri && ldrPin >= 0) {
        if (C7_PM allocatePin(ldrPin, false, PinOwner::UM_Unspecified)) heldLdr = ldrPin;
      }
      #endif
      if (buzzerPin >= 0 && C7_PM allocatePin(buzzerPin, true, PinOwner::UM_Unspecified)) {
        heldBuz = buzzerPin; pinMode(heldBuz, OUTPUT); digitalWrite(heldBuz, buzzerInvert ? HIGH : LOW);
      }
      if (buttonPin >= 0 && C7_PM allocatePin(buttonPin, false, PinOwner::UM_Unspecified)) {
        heldBtn = buttonPin; pinMode(heldBtn, INPUT_PULLUP);
      }
    }

    // ---------- LDR ----------
    int readLdr() {   // returns 0..1023
      #ifdef ESP8266
        int raw = analogRead(A0);
      #else
        if (heldLdr < 0) return -1;
        int raw = analogRead(heldLdr) >> 2;
      #endif
      if (ldrInvert) raw = 1023 - raw;
      return raw;
    }

    void handleLdr(unsigned long now) {
      if (!autoBri) return;
      int raw = readLdr();
      if (raw < 0) return;
      if (ldrAvg < 0) ldrAvg = raw; else ldrAvg += ((float)raw - ldrAvg) / 8.0f;
      uint8_t lo = briMin < briMax ? briMin : briMax;
      uint8_t hi = briMin < briMax ? briMax : briMin;
      int target = lo + (int)((hi - lo) * (ldrAvg / 1023.0f));
      lastBriSet = (uint8_t)target;
      if (now - lastBriApply < 1000 || bri == 0) return;
      lastBriApply = now;
      if (abs(target - (int)bri) >= 4) { bri = (uint8_t)target; stateUpdated(CALL_MODE_DIRECT_CHANGE); }
    }

    // ---------- alarm / chime / button ----------
    bool alarmDayOk(uint8_t wd) const {          // wd: 1 = Sunday
      if (alarmDays == 1) return wd >= 2 && wd <= 6;
      if (alarmDays == 2) return wd == 1 || wd == 7;
      return true;
    }

    void startRing() { ringing = true; ringEnd = millis() + (unsigned long)ringSec * 1000UL; }

    void checkAlarmAndChime() {
      time_t t = localTime;
      if (t < 1600000000L) return;               // time not set yet
      uint8_t h = hour(t), m = minute(t), s = second(t), wd = weekday(t);
      if (alarmOn && alarmArmed && !ringing && s < 3 && h == alarmHour && m == alarmMin && alarmDayOk(wd)) {
        uint32_t key = (uint32_t)(t / 60);
        if (key != alarmKey) { alarmKey = key; startRing(); }
      }
      if (snoozeAt && !ringing && (uint32_t)t >= snoozeAt) { snoozeAt = 0; startRing(); }
      if (chimeOn && m == 0 && s < 3) {
        bool inWin = chimeStart <= chimeEnd ? (h >= chimeStart && h <= chimeEnd) : (h >= chimeStart || h <= chimeEnd);
        uint32_t key = (uint32_t)(t / 3600);
        if (inWin && key != chimeKey) { chimeKey = key; chimeBeepStart = millis(); }
      }
    }

    // ---------- countdown timer / stopwatch ----------
    uint32_t timerElapsedSec() const {
      unsigned long ms = timerAccumMs + (timerRunning ? millis() - timerStartMs : 0);
      uint32_t sec = ms / 1000UL;
      return sec > 359999UL ? 359999UL : sec;
    }
    void exitTimer() { timerMode = 0; timerRunning = false; timerDone = false; timerAccumMs = 0; }
    void enterTimer(uint8_t m) { timerMode = m; timerRunning = false; timerDone = false; timerAccumMs = 0; lastBtnAct = millis(); }
    void timerToggle() {
      unsigned long now = millis();
      if (timerRunning) { timerAccumMs += now - timerStartMs; timerRunning = false; }
      else { timerStartMs = now; timerRunning = true; }
      lastBtnAct = now;
    }
    uint8_t nextTimerMode() const {
      if (timerMode == 0) return cdOn ? 1 : (swOn ? 2 : 0);
      if (timerMode == 1) return swOn ? 2 : 0;
      return 0;
    }
    void checkTimer(unsigned long now) {
      if (timerMode == 1 && timerRunning && !timerDone && timerElapsedSec() >= (uint32_t)cdMin * 60UL) {
        timerRunning = false; timerDone = true;
        timerAccumMs = (unsigned long)cdMin * 60000UL;
        timerRingEnd = now + (unsigned long)timerRingSec * 1000UL;
      }
      if (timerDone && (long)(now - timerRingEnd) >= 0) exitTimer();
      if (timerMode && !timerRunning && !timerDone && now - lastBtnAct > 120000UL) exitTimer();   // idle: back to clock
    }

    // ---------- button ----------
    void onPress(bool longPress) {
      unsigned long now = millis();
      lastBtnAct = now;
      if (ringing) {
        ringing = false;
        if (!longPress && snoozeMin > 0) snoozeAt = (uint32_t)localTime + (uint32_t)snoozeMin * 60UL;
        else snoozeAt = 0;
        beepUntil = now + 80;
      } else if (timerDone) {
        exitTimer(); beepUntil = now + 80;
      } else if (timerMode) {
        if (longPress) { timerRunning = false; timerAccumMs = 0; beepUntil = now + 150; }   // reset
        else { timerToggle(); beepUntil = now + 40; }                                       // start / pause
      } else if (longPress) {
        alarmArmed = !alarmArmed; snoozeAt = 0;
        beepUntil = now + (alarmArmed ? 250 : 80);
      } else {
        peekStart = now;                           // show date for a few seconds
      }
    }

    void onDouble() {                              // double press: clock > countdown > stopwatch > clock
      uint8_t m = nextTimerMode();
      if (m) enterTimer(m); else exitTimer();
      beepUntil = millis() + 60;
    }

    void pollButton(unsigned long now) {
      if (pendingShort && now - pendingAt >= 450) { pendingShort = false; onPress(false); }
      if (heldBtn < 0) return;
      bool down = digitalRead(heldBtn) == LOW;
      if (down && !btnDown && now - btnChange > 40) { btnDown = true; btnChange = now; btnSince = now; }
      else if (!down && btnDown && now - btnChange > 40) {
        btnDown = false; btnChange = now;
        if (now - btnSince >= 1000) { pendingShort = false; onPress(true); }
        else if (ringing || timerDone || !(cdOn || swOn)) onPress(false);
        else if (pendingShort && now - pendingAt < 450) { pendingShort = false; onDouble(); }
        else { pendingShort = true; pendingAt = now; }
      }
    }

    void updateBuzzer(unsigned long now) {
      if (heldBuz < 0) return;
      bool want = false;
      if (ringing || timerDone) want = ((now / 250) % 2) == 0;
      else if (chimeBeepStart) {
        unsigned long t = now - chimeBeepStart;
        if (t >= 600) chimeBeepStart = 0; else want = (t < 200) || (t >= 400);
      }
      if ((long)(beepUntil - now) > 0) want = true;
      digitalWrite(heldBuz, (want != buzzerInvert) ? HIGH : LOW);
    }

    // ---------- display logic ----------
    bool inNight() const {
      if (!nightOn) return false;
      uint8_t h = hour(localTime);
      if (nightStart == nightEnd) return false;
      return nightStart < nightEnd ? (h >= nightStart && h < nightEnd) : (h >= nightStart || h < nightEnd);
    }

    bool internetOk() const { return WLED_CONNECTED && ntpEnabled && ntpLastSyncTime != NTP_NEVER && (millis() - ntpLastSyncTime) < 45000000UL; }

    bool tempUsable() const { return tempShow && tempC > -90.0f && ((tempSource == 0 && rtcReady && rtcChip == 1) || (tempSource == 1 && bmpReady)); }

    uint8_t displayState() {   // 0 time, 1 day+month, 2 year, 3 temperature, 5 battery warning
      if (peekStart) {
        unsigned long e = millis() - peekStart;
        if (e < 3000) return 1;
        if (e < 5000) return 2;
        peekStart = 0;
      }
      bool useDate = dateShow, useTemp = tempUsable(), useWarn = rtcWarn && warnShow;
      if (!useDate && !useTemp && !useWarn) return 0;
      uint16_t cyc = secTime + (useDate ? secDayMonth + secYear : 0) + (useTemp ? secTemp : 0) + (useWarn ? 2 : 0);
      uint16_t p = (uint32_t)localTime % cyc;
      if (p < secTime) return 0;
      p -= secTime;
      if (useDate) {
        if (p < secDayMonth) return 1;
        p -= secDayMonth;
        if (p < secYear) return 2;
        p -= secYear;
      }
      if (useTemp) {
        if (p < secTemp) return 3;
        p -= secTemp;
      }
      return 5;
    }

    int tempShown() const {
      float c = tempC + tempOffset;
      float v = tempUnit ? c * 9.0f / 5.0f + 32.0f : c;
      return (int)lroundf(v);
    }

    // Builds segment bit masks per digit, colon state, weekday mask; returns display state
    uint8_t buildFrame(uint8_t* patt, bool& colonOn, uint8_t& wdMask) {
      uint8_t n = digitCount();
      for (uint8_t i = 0; i < 6; i++) patt[i] = 0;
      colonOn = false; wdMask = 0;
      time_t t = localTime;
      uint8_t wd = weekday(t);                                // 1 = Sunday
      if (wd >= 1 && wd <= 7) wdMask = 1 << (wd - 1);
      ampmMask = (hour(t) >= 12) ? 2 : 1;                     // bit0 = AM, bit1 = PM
      if (ampmBlink && toki.getTime().ms >= 500) ampmMask = 0;  // blink: off in the second half of each second

      uint8_t tm = testMode;
      if (!tm && bootTestUntil) { if ((long)(bootTestUntil - millis()) > 0) tm = 1; else bootTestUntil = 0; }
      if (tm) {
        uint8_t step = (millis() / 800) % 7;
        ampmMask = (tm == 2) ? ((step & 1) ? 2 : 1) : 3;
        if (tm == 1) { for (uint8_t i = 0; i < n; i++) patt[i] = 0x7F; colonOn = true; wdMask = 0x7F; }
        else if (tm == 2) { for (uint8_t i = 0; i < n; i++) patt[i] = 1 << step; wdMask = 1 << step; }
        else { uint8_t d = (millis() / 1000) % 10; for (uint8_t i = 0; i < n; i++) patt[i] = PAT[d]; colonOn = true; }
        return 0;
      }

      uint8_t v[6] = {10,10,10,10,10,10};
      if (timerMode) {                                        // countdown / stopwatch display
        uint32_t el = timerElapsedSec();
        uint32_t val = (timerMode == 1) ? (((uint32_t)cdMin * 60UL > el) ? (uint32_t)cdMin * 60UL - el : 0) : el;
        uint8_t h = val / 3600, m = (val / 60) % 60, sc = val % 60;
        if (mode == 0) { v[0] = h / 10; v[1] = h % 10; v[2] = m / 10; v[3] = m % 10; v[4] = sc / 10; v[5] = sc % 10; }
        else if (val >= 3600) { v[0] = h / 10; v[1] = h % 10; v[2] = m / 10; v[3] = m % 10; }
        else { v[0] = m / 10; v[1] = m % 10; v[2] = sc / 10; v[3] = sc % 10; }
        colonOn = timerRunning || timerDone || (toki.getTime().ms < 500);   // paused: colon blinks
        for (uint8_t i = 0; i < n; i++) patt[i] = PAT[v[i]];
        return 4;
      }
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
        if (mode == 0) { v[0] = a / 10; v[1] = a % 10; v[2] = 11; v[3] = 11; v[4] = b / 10; v[5] = b % 10; }
        else           { v[0] = a / 10; v[1] = a % 10; v[2] = b / 10; v[3] = b % 10; }
      } else if (st == 2) {
        int y = year(t);
        if (mode == 0) { v[0] = 10; v[1] = y / 1000; v[2] = (y / 100) % 10; v[3] = (y / 10) % 10; v[4] = y % 10; v[5] = 10; }
        else           { v[0] = y / 1000; v[1] = (y / 100) % 10; v[2] = (y / 10) % 10; v[3] = y % 10; }
      } else if (st == 5) {                                   // RTC battery / time problem: bAtt
        if (mode == 0) { v[0] = 10; v[1] = 15; v[2] = 16; v[3] = 17; v[4] = 17; v[5] = 10; }
        else           { v[0] = 15; v[1] = 16; v[2] = 17; v[3] = 17; }
      } else {
        int tv = tempShown(); bool neg = tv < 0; int a = abs(tv); if (a > 99) a = 99;
        uint8_t tens = a / 10, ones = a % 10, unitCh = tempUnit ? 14 : 13;
        uint8_t t1 = (a < 10) ? (neg ? 11 : 10) : tens;
        if (mode == 0) { v[0] = 10; v[1] = t1; v[2] = ones; v[3] = 12; v[4] = unitCh; v[5] = 10; }
        else           { v[0] = t1; v[1] = ones; v[2] = 12; v[3] = unitCh; }
      }
      for (uint8_t i = 0; i < n; i++) patt[i] = PAT[v[i]];
      return st;
    }

    uint32_t frameSignature() {
      uint8_t patt[6]; bool colon; uint8_t wdm;
      buildFrame(patt, colon, wdm);
      uint32_t sig = 17;
      for (uint8_t i = 0; i < 6; i++) sig = sig * 131 + patt[i];
      sig = sig * 3 + (colon ? 1 : 0);
      sig = sig * 131 + wdm;
      sig = sig * 5 + ampmMask;
      sig = sig * 3 + (inNight() ? 1 + nightAction : 0);
      sig = sig * 3 + ((ringing || timerDone) ? 1 + ((millis() / 500) % 2) : 0);
      return sig;
    }

    static void fmtDays(uint8_t d, char* out, size_t n) {
      const char* s = d == 1 ? "Mon-Fri" : (d == 2 ? "Sat-Sun" : "every day");
      strncpy(out, s, n - 1); out[n - 1] = 0;
    }

  public:
    void setup() {
      setupDone = true;
      buildOrder();
      if (startupTest) bootTestUntil = millis() + 3000;
      if (configRead) initHw();
    }

    void loop() {
      unsigned long now = millis();
      pollButton(now);
      updateBuzzer(now);
      if (now - lastTick < 100) return;
      lastTick = now;

      // temperature sensor (every 10 s)
      if (now - lastTemp > 10000) {
        lastTemp = now;
        if (tempSource == 1) { if (bmpReady) bmpRead(); }
        else if (rtcEnabled && rtcReady && rtcChip == 1) rtcReadTemp();
      }

      // RTC maintenance
      if (rtcEnabled && rtcReady) {
        if (battCheck && now - lastBattCheck > 60000UL) { lastBattCheck = now; rtcCheckBattery(); }
        if (!rtcLoaded && now - lastRtcTry > 10000) { lastRtcTry = now; rtcLoadToWled(); }

        // internet time good? (WiFi up, NTP on, synced in the last ~12 h)
        bool online = internetOk();
        if (online) { if (offline) { offline = false; rtcWritten = false; } }   // internet is back: update RTC soon
        else offline = true;

        // no internet time: correct the clock from the RTC every N minutes (only if 2 s or more off)
        if (rereadOn && !online && now - lastReread >= (unsigned long)rereadMin * 60000UL) {
          lastReread = now;
          time_t rt;
          if (rtcRead(rt)) {
            rtcDiff = (int32_t)((uint32_t)toki.second() - (uint32_t)rt);
            lastRereadAt = now; if (rereadCount < 65535) rereadCount++;
            if (rtcDiff >= 2 || rtcDiff <= -2) { toki.setTime(rt, TOKI_NO_MS_ACCURACY, TOKI_TS_RTC); updateLocalTime(); }
          }
        }

        uint8_t src = toki.getTimeSource();
        if (src > TOKI_TS_RTC) {                              // time came from NTP / browser
          bool due = !rtcWritten || (rtcSyncHours > 0 && now - lastRtcWrite > (unsigned long)rtcSyncHours * 3600000UL);
          if (due && toki.isTick()) {
            if (rtcWrite(toki.second())) { rtcWritten = true; lastRtcWrite = now; }
            else lastRtcWrite = now - (unsigned long)rtcSyncHours * 3600000UL + 60000UL;
          }
        }
      }

      checkAlarmAndChime();
      checkTimer(now);
      if (ringing && (long)(now - ringEnd) >= 0) ringing = false;

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
      if (blankExtra) { for (uint16_t i = endLed; i < total; i++) strip.setPixelColor(i, 0); }   // LEDs after the clock chain stay off
      if (endLed > total) endLed = total;

      bool night = inNight() && !testMode && timerMode == 0;
      bool ringBlank = (ringing || timerDone) && ((millis() / 500) % 2) == 1;
      if (ringBlank || (night && nightAction == 0)) {
        for (uint16_t i = firstLed; i < endLed; i++) strip.setPixelColor(i, 0);
        return;
      }

      uint8_t patt[6]; bool colon; uint8_t wdm;
      uint8_t st = buildFrame(patt, colon, wdm);

      uint32_t dcol = 0;
      bool hasDigitCol = false;
      if (st == 1 || st == 2) hasDigitCol = parseHex(dateHex, dcol);
      else if (st == 3) {
        hasDigitCol = parseHex(tempHex, dcol);
        if (!hasDigitCol && tempAutoColor) {                  // blue under 18 C, red over 30 C, else green
          float tc = tempC + tempOffset;
          dcol = tc < 18.0f ? 0x0066FF : (tc > 30.0f ? 0xFF0000 : 0x00FF00);
          hasDigitCol = true;
        }
      }
      else if (st == 4) hasDigitCol = parseHex(timerHex, dcol);
      else if (st == 5) { dcol = 0xFF0000; hasDigitCol = true; }

      // digits
      for (uint8_t d = 0; d < n; d++) {
        for (uint8_t s = 0; s < 7; s++) {
          bool lit = (patt[d] >> (order[s] - 'A')) & 1;
          uint16_t base = digitBase(d) + (uint16_t)s * ledsPerSeg;
          for (uint8_t k = 0; k < ledsPerSeg; k++) {
            if (base + k >= total) break;
            if (!lit) strip.setPixelColor(base + k, 0);
            else if (hasDigitCol) strip.setPixelColor(base + k, dcol);
          }
        }
      }

      // colons (one after HH, one after MM)
      uint32_t col;
      bool hasColonCol = parseHex(colonHex, col);
      for (uint8_t c = 0; c < colonCount(); c++) {
        for (uint8_t k = 0; k < colonLeds; k++) {
          uint16_t i = colonBaseAt(c) + k;
          if (i >= total) break;
          if (!colon) strip.setPixelColor(i, 0);
          else if (hasColonCol) strip.setPixelColor(i, col);
        }
      }

      // weekdays: 0 = off, 1 = only today, 2 = all on (today in its own colour)
      uint32_t todayCol = 0xFF0000, otherCol = 0;
      bool hasToday = parseHex(wdHex, todayCol);
      bool hasOther = parseHex(otherHex, otherCol);
      uint32_t weekendCol = 0;
      bool hasWeekend = parseHex(weekendHex, weekendCol);
      for (uint8_t d = 0; d < 7; d++) {
        bool isToday = (wdm >> d) & 1;
        for (uint8_t k = 0; k < ledsPerDay; k++) {
          uint16_t i = dayBase() + d * ledsPerDay + k;
          if (i >= total) break;
          if (weekdayMode == 0) strip.setPixelColor(i, 0);
          else if (weekdayMode == 1) {
            if (!isToday) strip.setPixelColor(i, 0);
            else if (hasToday) strip.setPixelColor(i, todayCol);
          } else {
            if (isToday) strip.setPixelColor(i, todayCol);        // red unless you set Weekday Hex
            else if ((d == 0 || d == 6) && hasWeekend) strip.setPixelColor(i, weekendCol);   // Sun / Sat
            else if (hasOther) strip.setPixelColor(i, otherCol);  // else main page colour
          }
        }
      }

      // AM / PM LEDs (after the weekday LEDs): AM group first, then PM group
      if (ampmOn) {
        uint32_t amCol = 0, pmCol = 0;
        bool hasAm = parseHex(amHex, amCol), hasPm = parseHex(pmHex, pmCol);
        for (uint8_t g = 0; g < 2; g++) {
          bool lit = (ampmMask >> g) & 1;
          for (uint8_t k = 0; k < ampmLeds; k++) {
            uint16_t i = ampmBase() + g * ampmLeds + k;
            if (i >= total) break;
            if (!lit) strip.setPixelColor(i, 0);
            else if (g == 0 && hasAm) strip.setPixelColor(i, amCol);
            else if (g == 1 && hasPm) strip.setPixelColor(i, pmCol);
          }
        }
      }

      // night dim: lower the brightness of everything that is lit
      if (night && nightAction == 1) {
        uint8_t amt = (uint8_t)(((uint16_t)constrain(nightDim, 1, 100) * 255) / 100);
        for (uint16_t i = firstLed; i < endLed; i++) strip.setPixelColor(i, color_fade(strip.getPixelColor(i), amt));
      }
    }

    // ---------- JSON / Info page ----------
    void addToJsonInfo(JsonObject& root) {
      JsonObject user = root[F("u")];
      if (user.isNull()) user = root.createNestedObject(F("u"));

      static const char* const WD[7]  = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
      static const char* const MON[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
      time_t t = localTime;
      bool timeSet = t > 1600000000L;
      char b[48];

      // 1) Time
      if (hourFormat) snprintf(b, sizeof(b), "%d:%02d:%02d %s", hourFormat12(t), minute(t), second(t), hour(t) >= 12 ? "PM" : "AM");
      else            snprintf(b, sizeof(b), "%02d:%02d:%02d", hour(t), minute(t), second(t));
      JsonArray a1 = user.createNestedArray(F("Clock time"));
      a1.add(String(b));
      if (!timeSet) a1.add(F(" (not set)"));

      // 2) Date
      uint8_t mo = month(t); if (mo < 1 || mo > 12) mo = 1;
      snprintf(b, sizeof(b), "%02d %s %d", day(t), MON[mo - 1], year(t));
      JsonArray a2 = user.createNestedArray(F("Clock date"));
      a2.add(String(b));

      // 3) Weekday
      uint8_t wd = weekday(t); if (wd < 1 || wd > 7) wd = 1;
      JsonArray a3 = user.createNestedArray(F("Clock weekday"));
      a3.add(WD[wd - 1]);
      snprintf(b, sizeof(b), " (LED %d of 7)", wd);
      a3.add(String(b));

      // 4) LDR / brightness
      JsonArray a4 = user.createNestedArray(F("Clock LDR"));
      if (!autoBri) a4.add(F("auto brightness OFF"));
      else if (ldrAvg < 0) a4.add(F("no reading (check pin)"));
      else { snprintf(b, sizeof(b), "light %d%%, bri %d", (int)(ldrAvg * 100.0f / 1023.0f), lastBriSet); a4.add(String(b)); }

      // 5) RTC
      JsonArray a5 = user.createNestedArray(F("Clock RTC"));
      if (!rtcEnabled) a5.add(F("disabled"));
      else if (rtcPinError) a5.add(F("pin conflict"));
      else if (rtcReady) { a5.add(rtcLoaded || rtcWritten ? F("OK") : F("found, time not set yet")); a5.add(rtcChip ? F(" (DS3231)") : F(" (DS1307)")); }
      else a5.add(F("NOT found - check SDA/SCL/power"));

      // 5b) RTC re-read (when there is no internet time)
      JsonArray a5b = user.createNestedArray(F("Clock RTC re-read"));
      if (!rtcEnabled || !rtcReady) a5b.add(F("n/a"));
      else if (!rereadOn) a5b.add(F("off"));
      else if (internetOk()) a5b.add(F("not needed (internet time OK)"));
      else if (rereadCount == 0) { snprintf(b, sizeof(b), "every %d min, waiting", rereadMin); a5b.add(String(b)); }
      else { snprintf(b, sizeof(b), "every %d min, last diff %ld s", rereadMin, (long)rtcDiff); a5b.add(String(b)); }

      // 5c) RTC battery / time warning
      JsonArray a5c = user.createNestedArray(F("Clock RTC battery"));
      if (!battCheck) a5c.add(F("check is OFF"));
      else if (!rtcEnabled || !rtcReady) a5c.add(F("n/a"));
      else if (rtcWarn) a5c.add(F("WARNING: time lost - replace CR2032 / set time"));
      else a5c.add(F("OK (time valid)"));

      // 5d) Countdown / stopwatch
      JsonArray a5d = user.createNestedArray(F("Clock timer"));
      if (timerMode == 0) a5d.add(F("off (double press button)"));
      else {
        uint32_t el = timerElapsedSec();
        uint32_t val = (timerMode == 1) ? (((uint32_t)cdMin * 60UL > el) ? (uint32_t)cdMin * 60UL - el : 0) : el;
        snprintf(b, sizeof(b), "%s %02d:%02d:%02d %s", timerMode == 1 ? "countdown" : "stopwatch", (int)(val / 3600), (int)((val / 60) % 60), (int)(val % 60), timerDone ? "DONE" : (timerRunning ? "running" : "paused"));
        a5d.add(String(b));
      }

      // 6) Temperature
      JsonArray a6 = user.createNestedArray(F("Clock temperature"));
      if (tempSource == 1) {
        if (!bmpReady) a6.add(F("BMP280 NOT found"));
        else if (tempC < -90.0f) a6.add(F("reading..."));
        else { snprintf(b, sizeof(b), "%d %s", tempShown(), tempUnit ? "F" : "C"); a6.add(String(b)); a6.add(F(" (BMP280)")); }
      } else if (rtcChip == 1 && tempC > -90.0f) { snprintf(b, sizeof(b), "%d %s", tempShown(), tempUnit ? "F" : "C"); a6.add(String(b)); a6.add(F(" (DS3231)")); }
      else a6.add(F("n/a (needs DS3231 or BMP280)"));
      if (tempSource == 1 && bmpReady && pressHpa > 1.0f) {
        JsonArray a6p = user.createNestedArray(F("Clock pressure"));
        snprintf(b, sizeof(b), "%d hPa", (int)lroundf(pressHpa)); a6p.add(String(b));
      }

      // 7) Alarm
      JsonArray a7 = user.createNestedArray(F("Clock alarm"));
      if (!alarmOn) a7.add(F("off"));
      else {
        char d[12]; fmtDays(alarmDays, d, sizeof(d));
        snprintf(b, sizeof(b), "%02d:%02d %s%s", alarmHour, alarmMin, d, alarmArmed ? "" : " (paused)");
        a7.add(String(b));
        if (ringing) a7.add(F(" RINGING"));
      }

      // 8) LED count
      JsonArray a8 = user.createNestedArray(F("Clock LEDs needed"));
      a8.add(ledsNeeded() + firstLed);
      a8.add(strip.getLengthTotal() >= ledsNeeded() + firstLed ? F(" (strip OK)") : F(" - raise LED count in LED Preferences!"));
      {
        uint16_t maxNeed = firstLed + 6 * 7 * ledsPerSeg + 2 * colonLeds + 7 * ledsPerDay + (ampmOn ? 2 * ampmLeds : 0);
        JsonArray a8b = user.createNestedArray(F("Clock LEDs max"));
        snprintf(b, sizeof(b), "%d", (int)maxNeed);
        a8b.add(String(b));
        a8b.add(F(" (6 digits). Set this as LED count to switch modes freely"));
      }

      if (orderBad) { JsonArray a9 = user.createNestedArray(F("Clock seg order")); a9.add(F("invalid - using FABGEDC")); }
    }

    void addToJsonState(JsonObject& root) {
      JsonObject o = root.createNestedObject(F("clk7"));
      o[F("test")] = testMode;
      o[F("date")] = dateShow;
      o[F("on")]   = enabled;
      o[F("timer")] = timerMode;      // 0 clock, 1 countdown, 2 stopwatch
      o[F("run")]  = timerRunning;
    }

    void readFromJsonState(JsonObject& root) {
      JsonObject o = root[F("clk7")];
      if (o.isNull()) return;
      testMode = o[F("test")] | testMode;
      dateShow = o[F("date")] | dateShow;
      enabled  = o[F("on")]   | enabled;
      if (!o[F("timer")].isNull()) {                 // {"clk7":{"timer":1,"run":true}} starts the countdown
        uint8_t m = o[F("timer")] | 0;
        if (m != timerMode) { if (m == 0) exitTimer(); else if ((m == 1 && cdOn) || (m == 2 && swOn)) enterTimer(m); }
      }
      if (!o[F("run")].isNull() && timerMode && !timerDone) {
        bool r = o[F("run")] | timerRunning;
        if (r != timerRunning) timerToggle();
      }
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
      w[F("leds-per-colon")] = colonLeds;
      w[F("leds-per-weekday")] = ledsPerDay;
      w[F("blank-extra")] = blankExtra;

      JsonObject ap = top.createNestedObject(F("am-pm-leds"));
      ap[F("enabled")] = ampmOn;
      ap[F("leds-each")] = ampmLeds;
      ap[F("am-hex")] = amHex;
      ap[F("pm-hex")] = pmHex;
      ap[F("blink")] = ampmBlink;

      JsonObject d = top.createNestedObject(F("display"));
      d[F("mode")] = mode;
      d[F("hour-format")] = hourFormat;
      d[F("leading-zero")] = leadingZero;
      d[F("colon-blink")] = colonBlink;
      d[F("weekday-mode")] = weekdayMode;
      d[F("startup-test")] = startupTest;

      JsonObject dt = top.createNestedObject(F("date"));
      dt[F("show")] = dateShow;
      dt[F("order")] = dateOrder;
      dt[F("time-sec")] = secTime;
      dt[F("day-month-sec")] = secDayMonth;
      dt[F("year-sec")] = secYear;

      JsonObject tp = top.createNestedObject(F("temperature"));
      tp[F("show")] = tempShow;
      tp[F("source")] = tempSource;
      tp[F("auto-color")] = tempAutoColor;
      tp[F("unit")] = tempUnit;
      tp[F("temp-sec")] = secTemp;
      tp[F("offset")] = tempOffset;

      JsonObject c = top.createNestedObject(F("colors"));
      c[F("weekday-hex")] = wdHex;
      c[F("other-days-hex")] = otherHex;
      c[F("weekend-hex")] = weekendHex;
      c[F("timer-hex")] = timerHex;
      c[F("colon-hex")] = colonHex;
      c[F("date-hex")] = dateHex;
      c[F("temp-hex")] = tempHex;

      JsonObject al = top.createNestedObject(F("alarm"));
      al[F("enabled")] = alarmOn;
      al[F("hour")] = alarmHour;
      al[F("minute")] = alarmMin;
      al[F("days")] = alarmDays;
      al[F("ring-sec")] = ringSec;
      al[F("snooze-min")] = snoozeMin;

      JsonObject ch = top.createNestedObject(F("hourly-chime"));
      ch[F("enabled")] = chimeOn;
      ch[F("start-hour")] = chimeStart;
      ch[F("end-hour")] = chimeEnd;

      JsonObject bz = top.createNestedObject(F("buzzer-button"));
      bz[F("buzzer-pin")] = buzzerPin;
      bz[F("buzzer-invert")] = buzzerInvert;
      bz[F("button-pin")] = buttonPin;

      JsonObject tm = top.createNestedObject(F("timer"));
      tm[F("countdown-on")] = cdOn;
      tm[F("countdown-min")] = cdMin;
      tm[F("stopwatch-on")] = swOn;
      tm[F("end-beep-sec")] = timerRingSec;

      JsonObject n = top.createNestedObject(F("night"));
      n[F("enabled")] = nightOn;
      n[F("start-hour")] = nightStart;
      n[F("end-hour")] = nightEnd;
      n[F("action")] = nightAction;
      n[F("dim-percent")] = nightDim;

      JsonObject b = top.createNestedObject(F("brightness"));
      b[F("auto")] = autoBri;
      b[F("ldr-pin")] = ldrPin;
      b[F("ldr-invert")] = ldrInvert;
      b[F("min")] = briMin;
      b[F("max")] = briMax;

      JsonObject r = top.createNestedObject(F("rtc"));
      r[F("enabled")] = rtcEnabled;
      r[F("chip")] = rtcChip;
      r[F("sda-pin")] = rtcSda;
      r[F("scl-pin")] = rtcScl;
      r[F("sync-hours")] = rtcSyncHours;
      r[F("reread-on")] = rereadOn;
      r[F("reread-min")] = rereadMin;
      r[F("battery-check")] = battCheck;
      r[F("warn-display")] = warnShow;

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
      copyStr(segCustom, sizeof(segCustom), w[F("seg-order")] | "FABGEDC");
      ok &= getJsonValue(w[F("leds-per-segment")], ledsPerSeg, (uint8_t)1);
      ok &= getJsonValue(w[F("leds-per-colon")], colonLeds, (uint8_t)2);
      ok &= getJsonValue(w[F("leds-per-weekday")], ledsPerDay, (uint8_t)1);
      ok &= getJsonValue(w[F("blank-extra")], blankExtra, true);

      JsonObject ap = top[F("am-pm-leds")];
      ok &= getJsonValue(ap[F("enabled")], ampmOn, false);
      ok &= getJsonValue(ap[F("leds-each")], ampmLeds, (uint8_t)1);
      copyStr(amHex, sizeof(amHex), ap[F("am-hex")] | "");
      copyStr(pmHex, sizeof(pmHex), ap[F("pm-hex")] | "");
      ok &= getJsonValue(ap[F("blink")], ampmBlink, false);
      if (ap.isNull()) ok = false;

      JsonObject d = top[F("display")];
      ok &= getJsonValue(d[F("mode")], mode, (uint8_t)0);
      ok &= getJsonValue(d[F("hour-format")], hourFormat, (uint8_t)0);
      ok &= getJsonValue(d[F("leading-zero")], leadingZero, true);
      ok &= getJsonValue(d[F("colon-blink")], colonBlink, true);
      ok &= getJsonValue(d[F("weekday-mode")], weekdayMode, (uint8_t)1);
      ok &= getJsonValue(d[F("startup-test")], startupTest, false);

      JsonObject dt = top[F("date")];
      ok &= getJsonValue(dt[F("show")], dateShow, false);
      ok &= getJsonValue(dt[F("order")], dateOrder, (uint8_t)0);
      ok &= getJsonValue(dt[F("time-sec")], secTime, (uint8_t)6);
      ok &= getJsonValue(dt[F("day-month-sec")], secDayMonth, (uint8_t)2);
      ok &= getJsonValue(dt[F("year-sec")], secYear, (uint8_t)2);

      JsonObject tp = top[F("temperature")];
      ok &= getJsonValue(tp[F("show")], tempShow, false);
      ok &= getJsonValue(tp[F("source")], tempSource, (uint8_t)0);
      ok &= getJsonValue(tp[F("auto-color")], tempAutoColor, false);
      ok &= getJsonValue(tp[F("unit")], tempUnit, (uint8_t)0);
      ok &= getJsonValue(tp[F("temp-sec")], secTemp, (uint8_t)2);
      ok &= getJsonValue(tp[F("offset")], tempOffset, (int8_t)0);

      JsonObject c = top[F("colors")];
      copyStr(wdHex, sizeof(wdHex), c[F("weekday-hex")] | "");
      copyStr(otherHex, sizeof(otherHex), c[F("other-days-hex")] | "");
      copyStr(weekendHex, sizeof(weekendHex), c[F("weekend-hex")] | "");
      copyStr(timerHex, sizeof(timerHex), c[F("timer-hex")] | "");
      copyStr(colonHex, sizeof(colonHex), c[F("colon-hex")] | "");
      copyStr(dateHex, sizeof(dateHex), c[F("date-hex")] | "");
      copyStr(tempHex, sizeof(tempHex), c[F("temp-hex")] | "");
      if (c.isNull()) ok = false;

      JsonObject al = top[F("alarm")];
      ok &= getJsonValue(al[F("enabled")], alarmOn, false);
      ok &= getJsonValue(al[F("hour")], alarmHour, (uint8_t)7);
      ok &= getJsonValue(al[F("minute")], alarmMin, (uint8_t)0);
      ok &= getJsonValue(al[F("days")], alarmDays, (uint8_t)0);
      ok &= getJsonValue(al[F("ring-sec")], ringSec, (uint8_t)30);
      ok &= getJsonValue(al[F("snooze-min")], snoozeMin, (uint8_t)5);

      JsonObject ch = top[F("hourly-chime")];
      ok &= getJsonValue(ch[F("enabled")], chimeOn, false);
      ok &= getJsonValue(ch[F("start-hour")], chimeStart, (uint8_t)8);
      ok &= getJsonValue(ch[F("end-hour")], chimeEnd, (uint8_t)22);

      JsonObject bz = top[F("buzzer-button")];
      ok &= getJsonValue(bz[F("buzzer-pin")], buzzerPin, (int8_t)-1);
      ok &= getJsonValue(bz[F("buzzer-invert")], buzzerInvert, false);
      ok &= getJsonValue(bz[F("button-pin")], buttonPin, (int8_t)-1);

      JsonObject tm = top[F("timer")];
      ok &= getJsonValue(tm[F("countdown-on")], cdOn, true);
      ok &= getJsonValue(tm[F("countdown-min")], cdMin, (uint8_t)10);
      ok &= getJsonValue(tm[F("stopwatch-on")], swOn, true);
      ok &= getJsonValue(tm[F("end-beep-sec")], timerRingSec, (uint8_t)10);

      JsonObject n = top[F("night")];
      ok &= getJsonValue(n[F("enabled")], nightOn, false);
      ok &= getJsonValue(n[F("start-hour")], nightStart, (uint8_t)23);
      ok &= getJsonValue(n[F("end-hour")], nightEnd, (uint8_t)6);
      ok &= getJsonValue(n[F("action")], nightAction, (uint8_t)0);
      ok &= getJsonValue(n[F("dim-percent")], nightDim, (uint8_t)20);

      JsonObject b = top[F("brightness")];
      ok &= getJsonValue(b[F("auto")], autoBri, false);
      ok &= getJsonValue(b[F("ldr-pin")], ldrPin, (int8_t)C7_DEF_LDR);
      ok &= getJsonValue(b[F("ldr-invert")], ldrInvert, false);
      ok &= getJsonValue(b[F("min")], briMin, (uint8_t)20);
      ok &= getJsonValue(b[F("max")], briMax, (uint8_t)255);

      JsonObject r = top[F("rtc")];
      ok &= getJsonValue(r[F("enabled")], rtcEnabled, true);
      ok &= getJsonValue(r[F("chip")], rtcChip, (uint8_t)0);
      ok &= getJsonValue(r[F("sda-pin")], rtcSda, (int8_t)C7_DEF_SDA);
      ok &= getJsonValue(r[F("scl-pin")], rtcScl, (int8_t)C7_DEF_SCL);
      ok &= getJsonValue(r[F("sync-hours")], rtcSyncHours, (uint8_t)6);
      ok &= getJsonValue(r[F("reread-on")], rereadOn, true);
      ok &= getJsonValue(r[F("reread-min")], rereadMin, (uint8_t)5);
      ok &= getJsonValue(r[F("battery-check")], battCheck, true);
      ok &= getJsonValue(r[F("warn-display")], warnShow, true);

      JsonObject t = top[F("test")];
      ok &= getJsonValue(t[F("mode")], testMode, (uint8_t)0);

      // sanity limits
      mode = mode ? 1 : 0;
      hourFormat = hourFormat ? 1 : 0;
      ledsPerSeg = constrain(ledsPerSeg, 1, 10);
      colonLeds  = constrain(colonLeds, 1, 6);
      weekdayMode = weekdayMode > 2 ? 1 : weekdayMode;
      cdMin = constrain(cdMin, 1, 99); timerRingSec = constrain(timerRingSec, 3, 60);
      ampmLeds = constrain(ampmLeds, 1, 6); tempSource = tempSource ? 1 : 0; rereadMin = constrain(rereadMin, 1, 60);
      nightAction = nightAction ? 1 : 0; nightDim = constrain(nightDim, 1, 100);
      ledsPerDay = constrain(ledsPerDay, 1, 5);
      segPreset  = segPreset > 3 ? 1 : segPreset;
      dateOrder  = dateOrder ? 1 : 0;
      secTime = constrain(secTime, 1, 60); secDayMonth = constrain(secDayMonth, 1, 30); secYear = constrain(secYear, 1, 30);
      secTemp = constrain(secTemp, 1, 30);
      tempUnit = tempUnit ? 1 : 0;
      tempOffset = constrain(tempOffset, -9, 9);
      alarmHour %= 24; alarmMin %= 60; alarmDays = alarmDays > 2 ? 0 : alarmDays;
      ringSec = constrain(ringSec, 5, 255); snoozeMin = constrain(snoozeMin, 0, 60);
      chimeStart %= 24; chimeEnd %= 24;
      nightStart %= 24; nightEnd %= 24;
      rtcChip = rtcChip ? 1 : 0;
      testMode = testMode > 3 ? 0 : testMode;

      configRead = true;
      lastSig = 0xFFFFFFFF;
      alarmKey = 0; snoozeAt = 0; ringing = false; alarmArmed = true; lastReread = millis(); exitTimer();
      if (setupDone) { buildOrder(); initHw(); }
      return ok;
    }

    void appendConfigData() {
      // Kept small on purpose: WLED 0.14.4 on ESP8266 has only 2048 bytes for the whole settings script.
      // Order = importance (list boxes first), so if space runs out only the last hints are dropped.
      C7_OAPPEND("var U='Ashish Automation:',dd,c7d=function(g,f,o){try{dd=addDropdown(U+g,f);for(var i=0;i<o.length;i++)addOption(dd,o[i],i)}catch(e){}},c7i=function(g,f,t){try{addInfo(U+g+':'+f,1,'<br><small>'+t+'</small>')}catch(e){}};");
      C7_OAPPEND("try{var h=document.getElementsByTagName('h2')[0],t=document.getElementsByTagName('h3');for(var i=0;i<t.length;i++)if(t[i].textContent.trim()=='Ashish Automation')t[i].style.fontSize=getComputedStyle(h).fontSize}catch(e){}");
      C7_OAPPEND("c7d('display','mode',['6 digits HH:MM:SS','4 digits HH:MM']);");
      C7_OAPPEND("c7d('display','weekday-mode',['Off','Today only','All on, today differs']);");
      C7_OAPPEND("c7d('display','hour-format',['24 hour','12 hour']);");
      C7_OAPPEND("c7d('wiring','seg-preset',['Custom','F-A-B-G-E-D-C (default)','A-B-C-D-E-F-G','F-A-B-C-D-E-G']);");
      C7_OAPPEND("c7d('temperature','source',['DS3231','BMP280']);");
      C7_OAPPEND("c7d('temperature','unit',['C','F']);");
      C7_OAPPEND("c7d('date','order',['Day-Month','Month-Day']);");
      C7_OAPPEND("c7d('alarm','days',['Every day','Mon-Fri','Sat-Sun']);");
      C7_OAPPEND("c7d('night','action',['Off','Dim']);");
      C7_OAPPEND("c7d('rtc','chip',['DS1307','DS3231']);");
      C7_OAPPEND("c7d('test','mode',['Off','All ON','Seg walk','Digit count']);");
      C7_OAPPEND("c7i('wiring','seg-order','Used if preset = Custom');");
      C7_OAPPEND("c7i('wiring','leds-per-colon','LEDs in ONE colon');");
      C7_OAPPEND("c7i('wiring','blank-extra','Extra LEDs stay off');");
      C7_OAPPEND("c7i('am-pm-leds','leds-each','Same for AM and PM');");
      C7_OAPPEND("c7i('colors','weekday-hex','Today. RRGGBB');");
      C7_OAPPEND("c7i('colors','other-days-hex','Other days. RRGGBB');");
      C7_OAPPEND("c7i('colors','weekend-hex','Sat + Sun. RRGGBB');");
      C7_OAPPEND("c7i('buzzer-button','buzzer-pin','-1 = none');");
      C7_OAPPEND("c7i('buzzer-button','button-pin','-1 = none');");
      C7_OAPPEND("c7i('brightness','ldr-pin','ESP8266: ignore');");
      C7_OAPPEND("c7i('rtc','battery-check','Off = no check');");
      C7_OAPPEND("c7i('night','dim-percent','If Action = Dim');");
    }

    uint16_t getId() { return USERMOD_ID_UNSPECIFIED; }
};

const char UsermodAshishClock7Seg::_name[] PROGMEM = "Ashish Automation";
constexpr uint8_t UsermodAshishClock7Seg::PAT[18];
