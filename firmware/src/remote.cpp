#include "remote.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_system.h>
#include <freertos/queue.h>
#include <string>
#include "audio.h"
#include "protocol.h"
#include "service_config.h"
#include "web_assets.h"

namespace {
constexpr const char* kSsid = kServiceSsid;
QueueHandle_t requests = nullptr, replies = nullptr;
struct Reply { uint32_t id; bool ok, persisted; const char* message; };
portMUX_TYPE snapshotLock = portMUX_INITIALIZER_UNLOCKED;
ControlSnapshot published;
char token[33] = {};
uint32_t owner = 0, nextId = 0;
bool ready = false;

ControlSnapshot snapshot() {
  ControlSnapshot out;
  portENTER_CRITICAL(&snapshotLock); out = published; portEXIT_CRITICAL(&snapshotLock);
  return out;
}
std::string stateJson() {
  const auto s = snapshot(); const auto a = audioStats();
  return encodeState(s, a.peak, a.loadPct, a.stalls);
}
esp_err_t json(httpd_req_t* r, const char* status, const std::string& body) {
  httpd_resp_set_status(r, status); httpd_resp_set_type(r, "application/json");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
  return httpd_resp_send(r, body.c_str(), body.size());
}
esp_err_t fail(httpd_req_t* r, const char* status, const char* message) {
  // All messages are static, controlled strings with no JSON control characters.
  json(r, status, std::string("{\"ok\":false,\"message\":\"") + message + "\"}");
  return ESP_FAIL; // close the socket, including any unread rejected request body
}
bool sameOrigin(httpd_req_t* r) {
  char client[8] = {}, origin[100] = {}, host[80] = {};
  if (httpd_req_get_hdr_value_str(r, "Host", host, sizeof(host)) != ESP_OK ||
      (strcmp(host, "192.168.4.1") && strcmp(host, "192.168.4.1:80"))) return false;
  if (httpd_req_get_hdr_value_str(r, "X-CP100-Client", client, sizeof(client)) != ESP_OK || strcmp(client, "1")) return false;
  if (httpd_req_get_hdr_value_len(r, "Origin")) {
    if (httpd_req_get_hdr_value_str(r, "Origin", origin, sizeof(origin)) != ESP_OK ||
        httpd_req_get_hdr_value_str(r, "Host", host, sizeof(host)) != ESP_OK) return false;
    if (std::string("http://") + host != origin) return false;
  }
  return true;
}
bool authorized(httpd_req_t* r) {
  char value[40] = {};
  if (!owner || snapshot().owner != owner || httpd_req_get_hdr_value_str(r, "X-CP100-Session", value, sizeof(value)) != ESP_OK) return false;
  return strlen(value) == 32 && !strcmp(value, token);
}
bool submit(Command command, Reply* response) {
  Reply stale;
  while (xQueueReceive(replies, &stale, 0) == pdTRUE) {}
  RemoteRequest request{command, ++nextId, millis() + 250};
  if (xQueueSend(requests, &request, 0) != pdTRUE) return false;
  const uint32_t start = millis();
  while (millis() - start < 500) {
    if (xQueueReceive(replies, response, pdMS_TO_TICKS(20)) == pdTRUE && response->id == request.id) return true;
  }
  return false;
}
esp_err_t commandReply(httpd_req_t* r, const Reply& reply, bool includeToken = false) {
  if (!reply.ok) return fail(r, "409 Conflict", reply.message);
  std::string body = "{\"ok\":true,\"persisted\":";
  body += reply.persisted ? "true" : "false";
  if (includeToken) body += std::string(",\"token\":\"") + token + "\"";
  body += ",\"state\":" + stateJson() + "}";
  return json(r, "200 OK", body);
}
esp_err_t asset(httpd_req_t* r) {
  const uint8_t* data; size_t size; const char* type;
  if (!strcmp(r->uri, "/app.js")) { data = kWeb_app; size = sizeof(kWeb_app); type = "text/javascript"; }
  else if (!strcmp(r->uri, "/style.css")) { data = kWeb_style; size = sizeof(kWeb_style); type = "text/css"; }
  else { data = kWeb_index; size = sizeof(kWeb_index); type = "text/html"; }
  httpd_resp_set_type(r, type);
  httpd_resp_set_hdr(r, "Content-Encoding", "gzip"); httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
  httpd_resp_set_hdr(r, "Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
  return httpd_resp_send(r, reinterpret_cast<const char*>(data), size);
}
esp_err_t stateRoute(httpd_req_t* r) { return json(r, "200 OK", stateJson()); }
esp_err_t claim(httpd_req_t* r) {
  if (!sameOrigin(r)) return fail(r, "403 Forbidden", "Origin tidak diizinkan.");
  if (r->content_len != 0) return fail(r, "400 Bad Request", "Sesi tidak memerlukan body.");
  Command c; c.action = Action::Claim;
  do { c.owner = esp_random(); } while (!c.owner);
  Reply reply;
  if (!submit(c, &reply)) return fail(r, "503 Service Unavailable", "Kontrol sibuk. Coba lagi.");
  if (reply.ok) {
    owner = c.owner;
    for (int i = 0; i < 4; ++i) snprintf(token + i * 8, 9, "%08lx", (unsigned long)esp_random());
  }
  return commandReply(r, reply, reply.ok);
}
esp_err_t action(httpd_req_t* r) {
  if (!sameOrigin(r) || !authorized(r)) return fail(r, "403 Forbidden", "Sesi tidak valid. Hubungkan kembali.");
  char contentType[40] = {};
  if (httpd_req_get_hdr_value_str(r, "Content-Type", contentType, sizeof(contentType)) != ESP_OK ||
      strcmp(contentType, "application/json")) return fail(r, "415 Unsupported Media Type", "Gunakan JSON.");
  if (!r->content_len || r->content_len > 4096) return fail(r, "413 Payload Too Large", "Payload maksimal 4096 byte.");
  std::string body(r->content_len, '\0');
  size_t received = 0;
  while (received < body.size()) {
    const int n = httpd_req_recv(r, &body[received], body.size() - received);
    if (n <= 0) return fail(r, "408 Request Timeout", "Pengiriman terputus.");
    received += n;
  }
  Command c; const char* error;
  if (!decodeCommand(body.data(), body.size(), &c, &error)) return fail(r, "400 Bad Request", error);
  c.owner = owner;
  Reply reply;
  if (!submit(c, &reply)) return fail(r, "503 Service Unavailable", "Kontrol sibuk. Coba lagi.");
  return commandReply(r, reply);
}
}
void remotePublish(const ControlSnapshot& s) {
  portENTER_CRITICAL(&snapshotLock); published = s; portEXIT_CRITICAL(&snapshotLock);
}
bool remoteReceive(RemoteRequest* request) { return requests && xQueueReceive(requests, request, 0) == pdTRUE; }
void remoteReply(uint32_t id, const ControlResult& result, bool persisted) {
  Reply reply{id, result.ok, persisted, result.message};
  xQueueSend(replies, &reply, 0);
}
void remotePrintWifi() {
  if (ready) Serial.printf("Wi-Fi: %s\nPassword: %s\nRemote: http://192.168.4.1\n", kSsid, kServicePassword);
  else Serial.println("Wi-Fi unavailable; pads and physical MUTE still operate.");
}
void remoteBegin(const ControlSnapshot& initial) {
  remotePublish(initial);
  requests = xQueueCreate(2, sizeof(RemoteRequest)); replies = xQueueCreate(2, sizeof(Reply));
  if (!requests || !replies) return;
  // The owner selected a shared operator password; ignore legacy random NVS credentials.
  if (!WiFi.mode(WIFI_AP)) return;
  const IPAddress address(192, 168, 4, 1);
  if (!WiFi.softAPConfig(address, address, IPAddress(255, 255, 255, 0)) || !WiFi.softAP(kSsid, kServicePassword, 1, 0, 2)) return;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.core_id = 0; config.stack_size = 8192; config.max_uri_handlers = 6;
  config.max_open_sockets = 4; config.lru_purge_enable = true;
  config.recv_wait_timeout = 1; config.send_wait_timeout = 1;
  httpd_handle_t server = nullptr;
  if (httpd_start(&server, &config) != ESP_OK) { WiFi.softAPdisconnect(true); return; }
  for (const char* uri : {"/", "/app.js", "/style.css"}) {
    httpd_uri_t route{}; route.uri = uri; route.method = HTTP_GET; route.handler = asset;
    if (httpd_register_uri_handler(server, &route) != ESP_OK) return;
  }
  httpd_uri_t route{}; route.uri = "/api/state"; route.method = HTTP_GET; route.handler = stateRoute;
  if (httpd_register_uri_handler(server, &route) != ESP_OK) return;
  route.uri = "/api/session"; route.method = HTTP_POST; route.handler = claim;
  if (httpd_register_uri_handler(server, &route) != ESP_OK) return;
  route.uri = "/api/action"; route.handler = action;
  if (httpd_register_uri_handler(server, &route) != ESP_OK) return;
  ready = true; remotePrintWifi();
}
