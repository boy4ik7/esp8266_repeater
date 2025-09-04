#include <Arduino.h>
#include <LittleFS.h>
#include <GyverDBFile.h>
#include <SettingsESP.h>
#include <ESP8266WiFi.h>
#include <lwip/napt.h>
#include <lwip/dns.h>

#define NAPT 1000
#define NAPT_PORT 10

GyverDBFile db(&LittleFS, "/data.db");
GyverDB db_ram;
SettingsESP sett("WiFi Repeater", &db);

DB_KEYS(
    kk,
    wifi_ssid,
    wifi_pass,
    dynamic_flag,
    ssids,
    ssids_index,
    ap_ssid,
    ap_pass
    );

void connect_wifi() {
  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(db[kk::wifi_ssid], db[kk::wifi_pass]);
  int count = 0;
  bool wifi_ok = true;
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    count++;
    if (count > 30) {
      wifi_ok = false;
      Serial.println("Error");
      break;
    }
  }
  if (wifi_ok) {
    Serial.println("Connect to: \n" + String(db[kk::wifi_ssid]) + "\n"+ String(db[kk::wifi_pass]) + "\n");
    Serial.println(WiFi.localIP());
    auto& server = WiFi.softAPDhcpServer();
    server.setDns(WiFi.dnsIP(0));

    WiFi.softAPConfig(
      IPAddress(172, 217, 28, 254),
      IPAddress(172, 217, 28, 254),
      IPAddress(255, 255, 255, 0)
    );
    WiFi.softAP(db[kk::ap_ssid], db[kk::ap_pass]);
    Serial.println(WiFi.softAPIP());
    err_t ret = ip_napt_init(NAPT, NAPT_PORT);
    if (ret == ERR_OK) {
      ret = ip_napt_enable_no(SOFTAP_IF, 1);
    }
  } else {
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(
      IPAddress(192, 168, 4, 1),
      IPAddress(192, 168, 4, 1),
      IPAddress(255, 255, 255, 0)
    );
    WiFi.softAP(db[kk::ap_ssid], db[kk::ap_pass]);
    delay(500);
    Serial.println(WiFi.softAPIP());
  }
  
}

void scan_wifi() {
  int scans_networks = WiFi.scanNetworks();
  db_ram[kk::ssids] = "";
  db_ram[kk::ssids_index] = 0;
  for (int i = 0; i < scans_networks; i++) {
    db_ram[kk::ssids] += WiFi.SSID(i) + ";";
  }
}

void build(sets::Builder& b) {
  if (b.beginGroup("Wi-Fi")) {
    if (db[kk::dynamic_flag]) b.Select(kk::ssids_index, "Found SSIDS:", db_ram[kk::ssids]);
    else b.Input(kk::wifi_ssid, "Manual input SSID:");
    b.Pass(kk::wifi_pass, "Password:");
    if (db[kk::dynamic_flag]) {
      if (b.Button("Cancel", sets::Colors::Red)) {
        db[kk::dynamic_flag] = false;
        db_ram[kk::wifi_pass] = "";
        sett.attachDB(&db);
        b.reload();
      }
    } else {
      if (b.Button("Find networks", sets::Colors::Blue)) {
        db[kk::dynamic_flag] = true;
        scan_wifi();
        sett.attachDB(&db_ram);
        b.reload();
      }
    }
    if (b.Button("Connect")) {
      if(db[kk::dynamic_flag]) {
        db_ram[kk::wifi_ssid] = WiFi.SSID(db_ram[kk::ssids_index]);
        db[kk::wifi_ssid] = String(db_ram[kk::wifi_ssid]);
        db[kk::wifi_pass] = String(db_ram[kk::wifi_pass]);
        db[kk::dynamic_flag] = false;
        db_ram[kk::wifi_pass] = "";
      }
      Serial.println("Connect to: \n" + String(db[kk::wifi_ssid]) + "\n"+ String(db[kk::wifi_pass]) + "\n");
      db.update();
      connect_wifi();
      sett.attachDB(&db);
      b.reload();
    }
    b.endGroup();
  }
  if (b.beginGroup("Wi-Fi AP")) {
    b.Input(kk::ap_ssid, "SSID:");
    b.Pass(kk::ap_pass, "Password:");
    if (b.Button("Update")) {
      db.update();
    }
    b.endGroup();
  }
  if (b.Button("Reboot", sets::Colors::Red)) {
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  #ifdef ESP32
    LittleFS.begin(true);
  #else
    LittleFS.begin();
  #endif
  db.begin();
  db.init(kk::wifi_ssid, "");
  db.init(kk::wifi_pass, "");
  db.init(kk::ap_ssid, "ESP8266 Repeater");
  db.init(kk::ap_pass, "");
  db_ram.init(kk::wifi_ssid, "");
  db_ram.init(kk::wifi_pass, "");
  db_ram.init(kk::dynamic_flag, false);
  db_ram.init(kk::ssids, "");
  db_ram.init(kk::ssids_index, 0);
  connect_wifi();
  sett.begin();
  sett.onBuild(build);
}

void loop() {
  sett.tick();
}