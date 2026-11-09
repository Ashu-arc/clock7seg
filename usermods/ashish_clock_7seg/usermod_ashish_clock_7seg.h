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
    uint8_t segPreset    = 1;      // 0 = custom text, 1..4 presets
    char    segCustom[9] = "FABCDEG";
    uint8_t ledsPerSeg   = 1;      // 1..10
    uint8_t colonLeds    = 2;      // 1..6 LEDs PER colon
    uint8_t ledsPerDay   = 1;      // 1..5
    bool    ampmOn       = false;  // AM / PM LEDs after the weekday LEDs
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

    bool    rtcEnabled   = true;
    uint8_t rtcChip      = 0;      // 0 = DS1307, 1 = DS3231
    int8_t  rtcSda       = C7_DEF_SDA;
    int8_t  rtcScl       = C7_DEF_SCL;
    uint8_t rtcSyncHours = 6;
    bool    rereadOn     = true;   // re-read RTC when there is no internet time
    uint8_t rereadMin    = 5;      // minutes between re-reads

    uint8_t testMode     = 0;      // 0 off, 1 all on, 2 segment walk, 3 digit count

    // ---------- runtime ----------
    char     order[8]    = "FABCDEG";
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

    // bit0=A ... bit6=G ; 0-9 digits, 10 blank, 11 dash, 12 degree, 13 C, 14 F
    static constexpr uint8_t PAT[15] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x00,0x40,0x63,0x39,0x71};

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
        case 1: strcpy(order, "FABCDEG"); return;
        case 2: strcpy(order, "ABCDEFG"); return;
        case 3: strcpy(order, "GFEDCBA"); return;
        case 4: strcpy(order, "GABCDEF"); return;
        default: break;
      }
      char tmp[8]; uint8_t seen = 0; uint8_t n = 0;
      for (const char* p = segCustom; *p && n < 7; p++) {
        char c = toupper((unsigned char)*p);
        if (c == '-' || c == ' ' || c == ',') continue;
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
      tm.Year  = bcd2dec(b[6]) + 30;
      if (tm.Second > 59 || tm.Minute > 59 || tm.Hour > 23 || tm.Day < 1 || tm.Day > 31 || tm.Month < 1 || tm.Month > 12) return false;
      if (tm.Year < 50) return false;                         // before 2020 = never set
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
      return Wire.endTransmission() == 0;
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
      if (heldSda >= 0) { PinManager::deallocatePin(heldSda, PinOwner::UM_Unspecified); heldSda = -1; }
      if (heldScl >= 0) { PinManager::deallocatePin(heldScl, PinOwner::UM_Unspecified); heldScl = -1; }
      if (heldLdr >= 0) { PinManager::deallocatePin(heldLdr, PinOwner::UM_Unspecified); heldLdr = -1; }
      if (heldBuz >= 0) { digitalWrite(heldBuz, buzzerInvert ? HIGH : LOW); PinManager::deallocatePin(heldBuz, PinOwner::UM_Unspecified); heldBuz = -1; }
      if (heldBtn >= 0) { PinManager::deallocatePin(heldBtn, PinOwner::UM_Unspecified); heldBtn = -1; }
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
          if (PinManager::allocateMultiplePins(p, 2, PinOwner::UM_Unspecified)) { heldSda = rtcSda; heldScl = rtcScl; }
          else { ok = false; rtcPinError = true; }
        }
        if (ok) {
          if (!sameAsGlobal) Wire.begin(rtcSda, rtcScl);
          if (rtcEnabled) {
            rtcReady = rtcProbe();
            if (rtcReady) { rtcLoadToWled(); if (rtcChip == 1 && tempSource == 0) rtcReadTemp(); }
          }
          if (tempSource == 1) bmpReady = bmpInit();
        }
      }
      #ifndef ESP8266
      if (autoBri && ldrPin >= 0) {
        if (PinManager::allocatePin(ldrPin, false, PinOwner::UM_Unspecified)) heldLdr = ldrPin;
      }
      #endif
      if (buzzerPin >= 0 && PinManager::allocatePin(buzzerPin, true, PinOwner::UM_Unspecified)) {
        heldBuz = buzzerPin; pinMode(heldBuz, OUTPUT); digitalWrite(heldBuz, buzzerInvert ? HIGH : LOW);
      }
      if (buttonPin >= 0 && PinManager::allocatePin(buttonPin, false, PinOwner::UM_Unspecified)) {
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

    void onPress(bool longPress) {
      unsigned long now = millis();
      if (ringing) {
        ringing = false;
        if (!longPress && snoozeMin > 0) snoozeAt = (uint32_t)localTime + (uint32_t)snoozeMin * 60UL;
        else snoozeAt = 0;
        beepUntil = now + 80;
      } else if (longPress) {
        alarmArmed = !alarmArmed; snoozeAt = 0;
        beepUntil = now + (alarmArmed ? 250 : 80);
      } else {
        peekStart = now;                           // show date for a few seconds
      }
    }

    void pollButton(unsigned long now) {
      if (heldBtn < 0) return;
      bool down = digitalRead(heldBtn) == LOW;
      if (down && !btnDown && now - btnChange > 40) { btnDown = true; btnChange = now; btnSince = now; }
      else if (!down && btnDown && now - btnChange > 40) { btnDown = false; btnChange = now; onPress(now - btnSince >= 1000); }
    }

    void updateBuzzer(unsigned long now) {
      if (heldBuz < 0) return;
      bool want = false;
      if (ringing) want = ((now / 250) % 2) == 0;
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

    uint8_t displayState() {   // 0 time, 1 day+month, 2 year, 3 temperature
      if (peekStart) {
        unsigned long e = millis() - peekStart;
        if (e < 3000) return 1;
        if (e < 5000) return 2;
        peekStart = 0;
      }
      bool useDate = dateShow, useTemp = tempUsable();
      if (!useDate && !useTemp) return 0;
      uint16_t cyc = secTime + (useDate ? secDayMonth + secYear : 0) + (useTemp ? secTemp : 0);
      uint16_t p = (uint32_t)localTime % cyc;
      if (p < secTime) return 0;
      p -= secTime;
      if (useDate) {
        if (p < secDayMonth) return 1;
        p -= secDayMonth;
        if (p < secYear) return 2;
        p -= secYear;
      }
      return 3;
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
      sig = sig * 3 + (ringing ? 1 + ((millis() / 500) % 2) : 0);
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
      if (endLed > total) endLed = total;

      bool night = inNight() && !testMode;
      bool ringBlank = ringing && ((millis() / 500) % 2) == 1;
      if (ringBlank || (night && nightAction == 0)) {
        for (uint16_t i = firstLed; i < endLed; i++) strip.setPixelColor(i, 0);
        return;
      }

      uint8_t patt[6]; bool colon; uint8_t wdm;
      uint8_t st = buildFrame(patt, colon, wdm);

      uint32_t dcol = 0;
      bool hasDigitCol = false;
      if (st == 1 || st == 2) hasDigitCol = parseHex(dateHex, dcol);
      else if (st == 3)       hasDigitCol = parseHex(tempHex, dcol);

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

      if (orderBad) { JsonArray a9 = user.createNestedArray(F("Clock seg order")); a9.add(F("invalid - using FABCDEG")); }
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
      w[F("leds-per-colon")] = colonLeds;
      w[F("leds-per-weekday")] = ledsPerDay;

      JsonObject ap = top.createNestedObject(F("am-pm-leds"));
      ap[F("enabled")] = ampmOn;
      ap[F("leds-each")] = ampmLeds;
      ap[F("am-hex")] = amHex;
      ap[F("pm-hex")] = pmHex;

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
      tp[F("unit")] = tempUnit;
      tp[F("temp-sec")] = secTemp;
      tp[F("offset")] = tempOffset;

      JsonObject c = top.createNestedObject(F("colors"));
      c[F("weekday-hex")] = wdHex;
      c[F("other-days-hex")] = otherHex;
      c[F("weekend-hex")] = weekendHex;
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
      copyStr(segCustom, sizeof(segCustom), w[F("seg-order")] | "FABCDEG");
      ok &= getJsonValue(w[F("leds-per-segment")], ledsPerSeg, (uint8_t)1);
      ok &= getJsonValue(w[F("leds-per-colon")], colonLeds, (uint8_t)2);
      ok &= getJsonValue(w[F("leds-per-weekday")], ledsPerDay, (uint8_t)1);

      JsonObject ap = top[F("am-pm-leds")];
      ok &= getJsonValue(ap[F("enabled")], ampmOn, false);
      ok &= getJsonValue(ap[F("leds-each")], ampmLeds, (uint8_t)1);
      copyStr(amHex, sizeof(amHex), ap[F("am-hex")] | "");
      copyStr(pmHex, sizeof(pmHex), ap[F("pm-hex")] | "");
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
      ok &= getJsonValue(tp[F("unit")], tempUnit, (uint8_t)0);
      ok &= getJsonValue(tp[F("temp-sec")], secTemp, (uint8_t)2);
      ok &= getJsonValue(tp[F("offset")], tempOffset, (int8_t)0);

      JsonObject c = top[F("colors")];
      copyStr(wdHex, sizeof(wdHex), c[F("weekday-hex")] | "");
      copyStr(otherHex, sizeof(otherHex), c[F("other-days-hex")] | "");
      copyStr(weekendHex, sizeof(weekendHex), c[F("weekend-hex")] | "");
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

      JsonObject t = top[F("test")];
      ok &= getJsonValue(t[F("mode")], testMode, (uint8_t)0);

      // sanity limits
      mode = mode ? 1 : 0;
      hourFormat = hourFormat ? 1 : 0;
      ledsPerSeg = constrain(ledsPerSeg, 1, 10);
      colonLeds  = constrain(colonLeds, 1, 6);
      weekdayMode = weekdayMode > 2 ? 1 : weekdayMode;
      ampmLeds = constrain(ampmLeds, 1, 6); tempSource = tempSource ? 1 : 0; rereadMin = constrain(rereadMin, 1, 60);
      nightAction = nightAction ? 1 : 0; nightDim = constrain(nightDim, 1, 100);
      ledsPerDay = constrain(ledsPerDay, 1, 5);
      segPreset  = segPreset > 4 ? 1 : segPreset;
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
      alarmKey = 0; snoozeAt = 0; ringing = false; alarmArmed = true; lastReread = millis();
      if (setupDone) { buildOrder(); initHw(); }
      return ok;
    }

    void appendConfigData() {
      oappend(F("dd=addDropdown('Ashish Automation:display','mode');"));
      oappend(F("addOption(dd,'HH:MM:SS (6 digits)',0);addOption(dd,'HH:MM (4 digits)',1);"));
      oappend(F("dd=addDropdown('Ashish Automation:display','hour-format');"));
      oappend(F("addOption(dd,'24 hour',0);addOption(dd,'12 hour',1);"));
      oappend(F("dd=addDropdown('Ashish Automation:display','weekday-mode');"));
      oappend(F("addOption(dd,'Off',0);addOption(dd,'Today only',1);addOption(dd,'All on, today different',2);"));
      oappend(F("dd=addDropdown('Ashish Automation:wiring','seg-preset');"));
      oappend(F("addOption(dd,'Custom (use Seg Order)',0);addOption(dd,'F-A-B-C-D-E-G (default)',1);addOption(dd,'A-B-C-D-E-F-G',2);addOption(dd,'G-F-E-D-C-B-A',3);addOption(dd,'G-A-B-C-D-E-F',4);"));
      oappend(F("dd=addDropdown('Ashish Automation:date','order');"));
      oappend(F("addOption(dd,'Day then Month',0);addOption(dd,'Month then Day',1);"));
      oappend(F("dd=addDropdown('Ashish Automation:temperature','source');"));
      oappend(F("addOption(dd,'DS3231 RTC sensor',0);addOption(dd,'BMP280 sensor',1);"));
      oappend(F("dd=addDropdown('Ashish Automation:temperature','unit');"));
      oappend(F("addOption(dd,'Celsius (C)',0);addOption(dd,'Fahrenheit (F)',1);"));
      oappend(F("dd=addDropdown('Ashish Automation:alarm','days');"));
      oappend(F("addOption(dd,'Every day',0);addOption(dd,'Mon to Fri',1);addOption(dd,'Sat and Sun',2);"));
      oappend(F("dd=addDropdown('Ashish Automation:night','action');"));
      oappend(F("addOption(dd,'Display off',0);addOption(dd,'Dim',1);"));
      oappend(F("dd=addDropdown('Ashish Automation:rtc','chip');"));
      oappend(F("addOption(dd,'DS1307',0);addOption(dd,'DS3231 (has temp)',1);"));
      oappend(F("dd=addDropdown('Ashish Automation:test','mode');"));
      oappend(F("addOption(dd,'Off (normal clock)',0);addOption(dd,'All segments ON',1);addOption(dd,'Segment walk A to G',2);addOption(dd,'Digits 0-9 count',3);"));

      oappend(F("addInfo('Ashish Automation:wiring:first-led',1,'<br><small>LED number of digit 1 (0 = first)</small>');"));
      oappend(F("addInfo('Ashish Automation:wiring:seg-order',1,'<br><small>Only if preset = Custom. Letters A-G</small>');"));
      oappend(F("addInfo('Ashish Automation:wiring:leds-per-segment',1,'<br><small>1 to 10</small>');"));
      oappend(F("addInfo('Ashish Automation:wiring:leds-per-colon',1,'<br><small>1 to 6 LEDs in ONE colon</small>');"));
      oappend(F("addInfo('Ashish Automation:wiring:leds-per-weekday',1,'<br><small>1 to 5</small>');"));
      oappend(F("addInfo('Ashish Automation:display:weekday-mode',1,'<br><small>All on: today = Weekday Hex (red)</small>');"));
      oappend(F("addInfo('Ashish Automation:display:startup-test',1,'<br><small>All LEDs ON for 3 s at power on</small>');"));
      oappend(F("addInfo('Ashish Automation:temperature:show',1,'<br><small>Needs DS3231 RTC</small>');"));
      oappend(F("addInfo('Ashish Automation:temperature:offset',1,'<br><small>Degrees C added (-9 to 9)</small>');"));
      oappend(F("addInfo('Ashish Automation:colors:weekday-hex',1,'<br><small>Today colour. RRGGBB, empty = main</small>');"));
      oappend(F("addInfo('Ashish Automation:colors:other-days-hex',1,'<br><small>Other 6 days. RRGGBB, empty = main</small>');"));
      oappend(F("addInfo('Ashish Automation:colors:colon-hex',1,'<br><small>RRGGBB, empty = main colour</small>');"));
      oappend(F("addInfo('Ashish Automation:colors:date-hex',1,'<br><small>Date and year. RRGGBB</small>');"));
      oappend(F("addInfo('Ashish Automation:colors:temp-hex',1,'<br><small>Temperature. RRGGBB</small>');"));
      oappend(F("addInfo('Ashish Automation:alarm:ring-sec',1,'<br><small>5 to 255 seconds</small>');"));
      oappend(F("addInfo('Ashish Automation:alarm:snooze-min',1,'<br><small>0 = no snooze</small>');"));
      oappend(F("addInfo('Ashish Automation:hourly-chime:start-hour',1,'<br><small>0-23, first chime</small>');"));
      oappend(F("addInfo('Ashish Automation:hourly-chime:end-hour',1,'<br><small>0-23, last chime</small>');"));
      oappend(F("addInfo('Ashish Automation:buzzer-button:buzzer-pin',1,'<br><small>Active buzzer</small>');"));
      oappend(F("addInfo('Ashish Automation:buzzer-button:button-pin',1,'<br><small>Button to GND</small>');"));
      oappend(F("addInfo('Ashish Automation:night:start-hour',1,'<br><small>0-23, night starts</small>');"));
      oappend(F("addInfo('Ashish Automation:night:end-hour',1,'<br><small>0-23, night ends</small>');"));
      oappend(F("addInfo('Ashish Automation:night:dim-percent',1,'<br><small>1-100, used when Action = Dim</small>');"));
      oappend(F("addInfo('Ashish Automation:brightness:ldr-pin',1,'<br><small>ESP32: 32-39. ESP8266: A0</small>');"));
      oappend(F("addInfo('Ashish Automation:rtc:sync-hours',1,'<br><small>Copy net time to RTC (0 = once)</small>');"));
          oappend(F("addInfo('Ashish Automation:temperature:source',1,'<br><small>BMP280 shares SDA/SCL with RTC</small>');"));
      oappend(F("addInfo('Ashish Automation:colors:weekend-hex',1,'<br><small>Sun and Sat (mode All on)</small>');"));
      oappend(F("addInfo('Ashish Automation:am-pm-leds:enabled',1,'<br><small>AM/PM LEDs go after weekdays</small>');"));
      oappend(F("addInfo('Ashish Automation:am-pm-leds:leds-each',1,'<br><small>1 to 6 LEDs for AM, same for PM</small>');"));
      oappend(F("addInfo('Ashish Automation:am-pm-leds:am-hex',1,'<br><small>RRGGBB, empty = main colour</small>');"));
      oappend(F("addInfo('Ashish Automation:am-pm-leds:pm-hex',1,'<br><small>RRGGBB, empty = main colour</small>');"));
      oappend(F("addInfo('Ashish Automation:rtc:reread-on',1,'<br><small>Fix drift when no internet time</small>');"));
      oappend(F("addInfo('Ashish Automation:rtc:reread-min',1,'<br><small>1 to 60 minutes</small>');"));
    }

    uint16_t getId() { return USERMOD_ID_UNSPECIFIED; }
};

const char UsermodAshishClock7Seg::_name[] PROGMEM = "Ashish Automation";
constexpr uint8_t UsermodAshishClock7Seg::PAT[15];
