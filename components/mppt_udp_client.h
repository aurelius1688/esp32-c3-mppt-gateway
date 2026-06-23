#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define MPPT_LOG_SIZE 50
#define MPPT_LOG_LEN 200
#define MPPT_MAX_FIELDS 30

class MPPTUDPClient {
private:
  WiFiUDP udp_;
  String host_;
  uint16_t port_;
  bool connected_ = false;
  bool initialized_ = false;
  bool udp_started_ = false;
  unsigned long last_send_ = 0;
  unsigned long last_recv_ = 0;
  float values_[MPPT_MAX_FIELDS] = {0};
  String field_names_[MPPT_MAX_FIELDS];
  int field_count_ = 0;
  char log_buf_[MPPT_LOG_SIZE][MPPT_LOG_LEN];
  int log_head_ = 0;
  int log_count_ = 0;

  int idx(const char* n) {
    for (int i = 0; i < field_count_; i++) {
      if (field_names_[i] == n) return i;
    }
    if (field_count_ < MPPT_MAX_FIELDS) {
      field_names_[field_count_] = String(n);
      ESP_LOGD("mppt", "NEW FIELD[%d]: %s", field_count_, n);
      return field_count_++;
    }
    return -1;
  }

  void parse(const char* data) {
    char raw[256]; strncpy(raw,data,200); raw[200]=0;
    char buf[256]; strncpy(buf,data,sizeof(buf)-1); buf[sizeof(buf)-1]=0;
    char* t = strtok(buf,",\n\r");
    int cnt = 0;
    String line;
    while (t) {
      char name[32]={0}, val[32]={0};
      int ni=0, vi=0; bool dig=false;
      for (int i=0; t[i]; i++) {
        char c = t[i];
        if (!dig && ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_')) { if (ni<31) name[ni++]=c; }
        else { dig=true; if (vi<31) val[vi++]=c; }
      }
      if (strlen(name) && strlen(val)) {
        int i = idx(name);
        float fv = atof(val);
        if (i >= 0) { values_[i] = fv; cnt++; }
        if (line.length()) line += ", ";
        line += String(name) + "=" + String(fv,2);
      }
      t = strtok(nullptr,",\n\r");
    }
    add_log("%s", line.c_str());
  }

public:
  MPPTUDPClient() {}

  void begin(const char* host, uint16_t port) {
    host_ = String(host);
    port_ = port;
    connected_ = false;
    initialized_ = true;
    udp_started_ = false;
    memset(values_, 0, sizeof(values_));
    add_log("Set target: %s:%d", host_.c_str(), port_);
  }

  void loop() {
    if (!initialized_) { add_log("not init"); return; }
    unsigned long now = millis();
    if (!udp_started_) {
      udp_.begin(2333);
      udp_started_ = true;
      String ip = WiFi.localIP().toString();
      add_log("UDP port 2333, IP: %s", ip.isEmpty() ? "waiting..." : ip.c_str());
    }
    if (now - last_send_ > 3000) {
      IPAddress ip;
      if (ip.fromString(host_)) {
        udp_.beginPacket(ip, port_);
        udp_.write((const uint8_t*)"START1", 6);
        udp_.endPacket();
        if (!connected_) add_log("Sent START1 -> %s:%d", host_.c_str(), port_);
        last_send_ = now;
      }
    }
    int n = udp_.parsePacket();
    if (n > 0) {
      char buf[512];
      int len = udp_.read(buf, sizeof(buf)-1);
      if (len > 0) {
        buf[len] = 0;
        if (!connected_) add_log("Response! %d bytes from %s:%d", len,
          udp_.remoteIP().toString().c_str(), udp_.remotePort());
        parse(buf);
        connected_ = true;
        last_recv_ = now;
      }
    }
    if (connected_ && now - last_recv_ > 10000) {
      connected_ = false;
      add_log("Timeout");
    }
  }

  bool is_connected() { return connected_; }
  bool is_initialized() { return initialized_; }

  float get_value(const char* name) {
    for (int i = 0; i < field_count_; i++) {
      if (field_names_[i] == name) return values_[i];
    }
    return 0;
  }
  float get_voltage_in()   { return get_value("VI"); }
  float get_current_in()   { return get_value("CI"); }
  float get_power_in()     { return get_value("PI"); }
  float get_voltage_out()  { return get_value("VO"); }
  float get_current_out()  { return get_value("CO"); }
  float get_power_out()    { return get_value("PO"); }
  float get_temperature()  { return get_value("LT"); }
  float get_battery_pct()  { return get_value("Bat"); }
  float get_wh()           { return get_value("Wh"); }
  float get_pwm()          { return get_value("P"); }
  float get_ppwm()         { return get_value("PP"); }
  float get_pwmc()         { return get_value("PC"); }
  float get_error_code()   { return get_value("ERR"); }
  float get_buck_status()  { return get_value("Buck"); }
  float get_fan_status()   { return get_value("Fan"); }

  void send_cmd(const char* cmd) {
    if (!initialized_) return;
    IPAddress ip;
    if (ip.fromString(host_)) {
      udp_.beginPacket(ip, port_);
      udp_.write((const uint8_t*)cmd, strlen(cmd));
      udp_.endPacket();
      add_log("Sent: %s", cmd);
    }
  }

  void add_log(const char* fmt, ...) {
    char buf[MPPT_LOG_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, MPPT_LOG_LEN, fmt, args);
    va_end(args);
    strncpy(log_buf_[log_head_], buf, MPPT_LOG_LEN-1);
    log_buf_[log_head_][MPPT_LOG_LEN-1] = 0;
    ESP_LOGD("mppt", "%s", buf);
    log_head_ = (log_head_ + 1) % MPPT_LOG_SIZE;
    if (log_count_ < MPPT_LOG_SIZE) log_count_++;
  }

  String get_debug_page() {
    String s = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>MPPT Debug</title></head>";
    s += "<body style='background:#111;color:#0f0;font:13px monospace;padding:20px'>";
    s += "<h2 style='color:#0f0'>MPPT Gateway Debug</h2>";
    s += "<p><a href='/'>Home</a> | <a href='/debug'>Debug</a></p>";
    s += "<p>Uptime: " + String(millis()/1000) + "s | IP: " + WiFi.localIP().toString();
    s += " | Target: " + host_ + ":" + String(port_);
    s += " | " + String(connected_ ? "<b style='color:#0f0'>CONNECTED</b>" : "<b style='color:#f00'>DISCONNECTED</b>");
    s += "</p><hr><pre>";
    int start = (log_count_ < MPPT_LOG_SIZE) ? 0 : log_head_;
    for (int i = 0; i < log_count_; i++) {
      int idx = (start + i) % MPPT_LOG_SIZE;
      s += "[" + String(i+1) + "] " + String(log_buf_[idx]) + "\n";
    }
    s += "</pre></body></html>";
    return s;
  }
};

MPPTUDPClient mppt_udp;
