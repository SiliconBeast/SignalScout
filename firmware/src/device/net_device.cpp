// Wi-Fi station + hotspot, HTTPS (ESP-IDF CA bundle), SNTP, OTA with a
// SHA-256 check against the release manifest, and the hotspot web console.
#include "net_device.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <SD.h>
#include <esp_http_client.h>
// The ESP-IDF certificate bundle is built into the Arduino S3 SDK (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE);
// its header is shadowed by WiFiClientSecure's copy, so declare the IDF function directly.
extern "C" esp_err_t esp_crt_bundle_attach(void* conf);
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <time.h>
#include "board.h"
#include "services.h"

using namespace ss;
namespace dev {

static SemaphoreHandle_t sd_mutex = nullptr;
void sd_lock() { if (!sd_mutex) sd_mutex = xSemaphoreCreateRecursiveMutex(); xSemaphoreTakeRecursive(sd_mutex, portMAX_DELAY); }
void sd_unlock() { xSemaphoreGiveRecursive(sd_mutex); }

static Bus* g_bus = nullptr; static Shared* g_sh = nullptr; static IStorage* g_store = nullptr;
void net_attach(Bus* bus, Shared* shared, IStorage* storage) { g_bus = bus; g_sh = shared; g_store = storage; }

// ------------------------------------------------------------------ HTTPS GET into a buffer
struct Sink { char* buf; int cap, len; };
static esp_err_t on_http(esp_http_client_event_t* e) {
  if (e->event_id == HTTP_EVENT_ON_DATA && e->user_data) {
    Sink* s = (Sink*)e->user_data; int n = e->data_len; if (s->len + n > s->cap) n = s->cap - s->len;
    if (n > 0) { memcpy(s->buf + s->len, e->data, (size_t)n); s->len += n; }
  }
  return ESP_OK;
}

// ------------------------------------------------------------------ OTA task
static volatile int ota_prog = -1; static char ota_url[192], ota_sha[65]; static uint32_t ota_size = 0;
static void ota_task(void*) {
  ota_prog = 0;
  esp_http_client_config_t cfg = {}; cfg.url = ota_url; cfg.crt_bundle_attach = esp_crt_bundle_attach; cfg.timeout_ms = 15000; cfg.buffer_size = 4096;
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
  esp_ota_handle_t h = 0; bool ok = c && part;
  int status = 0;
  for (int r = 0; ok && r < 6; r++) { // GitHub release assets redirect to a CDN
    if (esp_http_client_open(c, 0) != ESP_OK) { ok = false; break; }
    esp_http_client_fetch_headers(c); status = esp_http_client_get_status_code(c);
    if (status >= 301 && status <= 308 && status != 304) { esp_http_client_set_redirection(c); esp_http_client_close(c); continue; }
    break;
  }
  ok = ok && status == 200 && esp_ota_begin(part, ota_size, &h) == ESP_OK;
  mbedtls_sha256_context sha; mbedtls_sha256_init(&sha); mbedtls_sha256_starts(&sha, 0);
  static uint8_t buf[4096]; uint32_t got = 0;
  while (ok) {
    int n = esp_http_client_read(c, (char*)buf, sizeof buf);
    if (n < 0) { ok = false; break; }
    if (n == 0) break;
    mbedtls_sha256_update(&sha, buf, (size_t)n);
    if (esp_ota_write(h, buf, (size_t)n) != ESP_OK) { ok = false; break; }
    got += (uint32_t)n; ota_prog = ota_size ? (int)((uint64_t)got * 99 / ota_size) : 50;
  }
  uint8_t dig[32]; mbedtls_sha256_finish(&sha, dig); mbedtls_sha256_free(&sha);
  char hex[65]; for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", dig[i]);
  if (ok) { ota_prog = 100; ok = got == ota_size && !strcasecmp(hex, ota_sha); }        // the manifest's hash, not just the image's own
  if (h) { if (esp_ota_end(h) != ESP_OK) ok = false; }                                   // image header + appended hash
  if (ok) ok = esp_ota_set_boot_partition(part) == ESP_OK;
  if (c) { esp_http_client_close(c); esp_http_client_cleanup(c); }
  ota_prog = ok ? 101 : -1;
  vTaskDelete(nullptr);
}

// ------------------------------------------------------------------ web console (hotspot only)
static WebServer* web = nullptr;
static const char PAGE_HEAD[] = R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Signal Scout</title><style>
:root{color-scheme:dark}body{margin:0;background:#000;color:#e8eaed;font:15px/1.45 ui-monospace,Menlo,Consolas,monospace;padding:16px;max-width:720px}
h1{font-size:20px;margin:0 0 4px}h2{font-size:13px;color:#39ff88;letter-spacing:.06em;margin:22px 0 8px}
a{color:#39ff88}.card{border:1px solid #1d3a28;border-radius:12px;padding:12px 14px;margin:8px 0;background:#0a0d0b}
input,textarea,button{font:inherit;background:#111;color:#e8eaed;border:1px solid #2a2e36;border-radius:8px;padding:8px;width:100%;box-sizing:border-box;margin:4px 0}
button{background:#39ff88;color:#001a0c;border:0;font-weight:600}.dim{color:#8a919c}td{padding:2px 12px 2px 0}
</style></head><body><h1>Signal Scout</h1>)HTML";

static void send_page(const String& body) { web->send(200, "text/html", String(PAGE_HEAD) + body + "</body></html>"); }
static String esc(const String& s) { String o; for (char c : s) { if (c == '<') o += "&lt;"; else if (c == '&') o += "&amp;"; else if (c == '"') o += "&quot;"; else o += c; } return o; }

static void h_root() {
  String b = "<div class=dim>SignalOS " SS_FW_VERSION "</div>";
  if (g_sh) {
    char t[200]; snprintf(t, sizeof t, "<div class=card><table><tr><td>battery</td><td>%u%%</td></tr><tr><td>position</td><td>%.6f, %.6f</td></tr><tr><td>wi-fi</td><td>%u networks, %u ble</td></tr></table></div>",
      g_sh->batt_pct, g_sh->lat_e7 / 1e7, g_sh->lon_e7 / 1e7, g_sh->n_wifi, g_sh->n_ble);
    b += t;
  }
  b += "<h2>FILES</h2><div class=card>";
  sd_lock();
  File root = SD.open("/");
  if (root) { for (File f = root.openNextFile(); f; f = root.openNextFile()) { if (f.isDirectory()) continue; String n = f.name(); if (!n.startsWith("/")) n = "/" + n;
      if (n == "/wifi.txt") continue; // never served: holds passwords
      b += "<a href='/f?p=" + esc(n) + "'>" + esc(n) + "</a> <span class=dim>" + String((unsigned)f.size()) + " B</span><br>"; } }
  else b += "<span class=dim>no microSD card</span>";
  sd_unlock();
  b += "</div><h2>ADD WI-FI NETWORK</h2><form class=card method=post action=/wifi><input name=ssid placeholder='network name' maxlength=32 required>"
       "<input name=pass type=password placeholder='password (empty if open)' maxlength=63><button>Save</button></form>";
  char st[512] = ""; if (g_sh) settings_to_text(g_sh->settings, st, sizeof st);
  b += "<h2>SETTINGS</h2><form class=card method=post action=/settings><textarea name=t rows=12>" + esc(st) + "</textarea><button>Apply</button></form>";
  b += "<h2>FIRMWARE</h2><form class=card method=post action=/update enctype=multipart/form-data><input type=file name=f accept='.bin' required>"
       "<button>Upload and install</button><div class=dim>Installs to the spare slot; restart from the device.</div></form>";
  send_page(b);
}
static void h_file() {
  String p = web->arg("p");
  if (p.indexOf("..") >= 0 || p == "/wifi.txt" || !p.startsWith("/")) { web->send(403, "text/plain", "no"); return; }
  sd_lock(); File f = SD.open(p, FILE_READ);
  if (!f) { sd_unlock(); web->send(404, "text/plain", "not found"); return; }
  web->streamFile(f, p.endsWith(".csv") ? "text/csv" : "text/plain"); f.close(); sd_unlock();
}
static void h_wifi() {
  String s = web->arg("ssid"), p = web->arg("pass");
  if (!s.length() || s.length() > 32 || p.length() > 63 || s.indexOf('\t') >= 0 || s.indexOf('\n') >= 0 || p.indexOf('\n') >= 0) { web->send(400, "text/plain", "bad network"); return; }
  if (g_bus) { Cmd c(CMD_WIFI_ADD); snprintf(c.text, sizeof c.text, "%s\t%s", s.c_str(), p.c_str()); g_bus->cmds.push(c); }
  send_page("<div class=card>Saved <b>" + esc(s) + "</b>. Turn on Wi-Fi in Settings to join.</div><a href=/>back</a>");
}
static void h_settings() {
  if (!g_sh || !g_bus) { web->send(503, "text/plain", "not ready"); return; }
  Settings s = g_sh->settings; settings_from_text(web->arg("t").c_str(), s);
  Cmd c(CMD_SET_SETTINGS); c.settings = s; g_bus->cmds.push(c);
  send_page("<div class=card>Applied.</div><a href=/>back</a>");
}
static bool upload_ok = false;
static void h_update_data() {
  HTTPUpload& u = web->upload();
  if (u.status == UPLOAD_FILE_START) upload_ok = !(g_sh && g_sh->sos) && Update.begin(UPDATE_SIZE_UNKNOWN);
  else if (u.status == UPLOAD_FILE_WRITE && upload_ok) upload_ok = Update.write(u.buf, u.currentSize) == u.currentSize;
  else if (u.status == UPLOAD_FILE_END && upload_ok) upload_ok = Update.end(true);
  else if (u.status == UPLOAD_FILE_ABORTED) { Update.abort(); upload_ok = false; }
}
static void h_update_done() {
  if (upload_ok) ota_prog = 101;
  if (upload_ok && g_bus) { Event e(EV_OTA, millis()); e.u.ota.state = OTA_READY; e.u.ota.progress = 100; snprintf(e.u.ota.version, sizeof e.u.ota.version, "uploaded"); g_bus->events.push(e); }
  send_page(upload_ok ? "<div class=card>Firmware verified and armed. Restart from the device (Updates).</div>" : "<div class=card>Upload failed (or SOS is active). Nothing was changed.</div>");
}
static void web_start() {
  if (web) return;
  web = new WebServer(80);
  web->on("/", HTTP_GET, h_root); web->on("/f", HTTP_GET, h_file);
  web->on("/wifi", HTTP_POST, h_wifi); web->on("/settings", HTTP_POST, h_settings);
  web->on("/update", HTTP_POST, h_update_done, h_update_data);
  web->onNotFound([] { web->sendHeader("Location", "/"); web->send(302); }); // captive-portal style
  web->begin();
}
static void web_stop() { if (!web) return; web->stop(); delete web; web = nullptr; }
void net_poll() { if (web) web->handleClient(); }

// ------------------------------------------------------------------ INet
struct Net : INet {
  bool sta_ = false, ap_ = false, ntp_started_ = false;
  void mode() { WiFi.mode(ap_ ? WIFI_AP_STA : WIFI_STA); }
  void sta_begin(const char* ssid, const char* pass) override { sta_ = true; mode(); WiFi.begin(ssid, pass); }
  void sta_end() override { if (sta_) WiFi.disconnect(false, false); sta_ = false; ntp_started_ = false; }
  uint8_t sta_state() override {
    if (!sta_) return NET_OFF;
    wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED) return NET_ONLINE;
    if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL) return NET_FAILED;
    return NET_CONNECTING;
  }
  int8_t sta_rssi() override { return (int8_t)WiFi.RSSI(); }
  void ip(char* out, int cap) override { snprintf(out, (size_t)cap, "%s", WiFi.localIP().toString().c_str()); }
  bool ap_begin(const char* ssid, const char* pass) override {
    ap_ = true; mode();
    if (!WiFi.softAP(ssid, pass, 6, 0, 4)) { ap_ = false; mode(); return false; }
    web_start(); return true;
  }
  void ap_end() override { web_stop(); WiFi.softAPdisconnect(true); ap_ = false; mode(); }
  uint8_t ap_clients() override { return ap_ ? (uint8_t)WiFi.softAPgetStationNum() : 0; }
  int http_get(const char* url, char* buf, int cap) override {
    Sink s{buf, cap, 0};
    esp_http_client_config_t cfg = {}; cfg.url = url; cfg.crt_bundle_attach = esp_crt_bundle_attach; cfg.timeout_ms = 10000;
    cfg.event_handler = on_http; cfg.user_data = &s; cfg.buffer_size = 2048;
    esp_http_client_handle_t c = esp_http_client_init(&cfg); if (!c) return -1;
    esp_err_t err = esp_http_client_perform(c); int st = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);
    return (err == ESP_OK && st == 200) ? s.len : -1;
  }
  bool ntp(uint32_t& utc) override {
    if (!ntp_started_) { configTime(0, 0, "pool.ntp.org", "time.google.com"); ntp_started_ = true; }
    time_t t = time(nullptr); if (t < 1700000000) return false;
    utc = (uint32_t)t; return true;
  }
  bool ota_begin(const char* url, uint32_t size, const char* sha256) override {
    if (ota_prog >= 0 && ota_prog <= 100) return false; // already running
    snprintf(ota_url, sizeof ota_url, "%s", url); snprintf(ota_sha, sizeof ota_sha, "%s", sha256); ota_size = size; ota_prog = 0;
    return xTaskCreatePinnedToCore(ota_task, "ota", 8192, nullptr, 1, nullptr, 0) == pdPASS;
  }
  int ota_poll() override { return ota_prog; }
  void restart() override { delay(200); ESP.restart(); }
};
static Net net;
INet* net_device() { return &net; }

// ------------------------------------------------------------------ boot: rollback and safe mode
void ota_boot_check(bool safe_mode_key) {
  const esp_partition_t* run = esp_ota_get_running_partition();
  if (safe_mode_key) { // hold B while powering on: boot the other slot (previous firmware)
    const esp_partition_t* other = esp_ota_get_next_update_partition(run);
    esp_ota_img_states_t st;
    if (other && esp_ota_get_state_partition(other, &st) == ESP_OK && st == ESP_OTA_IMG_VALID) { esp_ota_set_boot_partition(other); ESP.restart(); }
  }
}
void ota_mark_good() {
  esp_ota_img_states_t st; const esp_partition_t* run = esp_ota_get_running_partition();
  if (esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) esp_ota_mark_app_valid_cancel_rollback();
}
}
