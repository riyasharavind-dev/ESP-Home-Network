
/*
  ESP Home Network v2.0
  Board: ESP32 Dev Module (ESP32-WROOM-32)
  Dashboard: http://192.168.4.1
  Local network only - no internet routing.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_wifi.h>

WebServer server(80);
Preferences preferences;

String apName, apPassword, adminUser, adminPassword;
unsigned long bootMillis;

const char* DEFAULT_SSID = "ESP Home Network";
const char* DEFAULT_WIFI_PASSWORD = "HomeNet@123";
const char* DEFAULT_ADMIN_USER = "admin";
const char* DEFAULT_ADMIN_PASSWORD = "Admin@123";

String esc(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  s.replace("'", "&#39;");
  return s;
}

bool auth() {
  if (server.authenticate(adminUser.c_str(), adminPassword.c_str()))
    return true;

  server.requestAuthentication(BASIC_AUTH, "ESP Home Network", "Login required");
  return false;
}

String uptimeText() {
  unsigned long t = (millis() - bootMillis) / 1000;
  unsigned long d = t / 86400; t %= 86400;
  unsigned long h = t / 3600; t %= 3600;
  unsigned long m = t / 60; t %= 60;

  String s = "";
  if (d) s += String(d) + "d ";
  if (h || d) s += String(h) + "h ";
  s += String(m) + "m " + String(t) + "s";
  return s;
}

String page(String notice = "") {
  String p = R"HTML(
<!doctype html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP Home Network</title>
<style>
*{box-sizing:border-box}
body{margin:0;padding:18px;background:#0f172a;color:#e2e8f0;font-family:Arial}
main{max-width:720px;margin:20px auto}
.card{background:#1e293b;border:1px solid #334155;border-radius:14px;padding:20px;margin:14px 0}
h1{font-size:25px}h2{font-size:19px}
.muted{color:#94a3b8;font-size:13px;line-height:1.6}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px}
.stat{background:#0f172a;border-radius:10px;padding:14px}
.value{font-size:19px;font-weight:bold;overflow-wrap:anywhere;margin-top:8px}
label{display:block;margin:15px 0 7px;font-size:14px}
input{width:100%;padding:12px;background:#0f172a;color:white;border:1px solid #475569;border-radius:8px;font-size:16px}
button{padding:12px 15px;margin-top:15px;border:0;border-radius:8px;background:#38bdf8;color:#082f49;font-weight:bold;cursor:pointer}
.danger{background:#f87171;color:#450a0a}
.device{padding:12px 0;border-bottom:1px solid #334155;overflow-wrap:anywhere}
</style></head><body><main>
<div class="card"><h1>ESP Home Network</h1>
<div class="muted">Local network control panel · v2.0</div></div>
)HTML";

  if (notice.length())
    p += "<div class='card'>" + esc(notice) + "</div>";

  p += "<div class='card'><h2>Network Dashboard</h2><div class='grid'>";
  p += "<div class='stat'><div class='muted'>Wi-Fi Name</div><div class='value'>" + esc(apName) + "</div></div>";
  p += "<div class='stat'><div class='muted'>Access Point IP</div><div class='value'>" + WiFi.softAPIP().toString() + "</div></div>";
  p += "<div class='stat'><div class='muted'>Connected Devices</div><div class='value'>" + String(WiFi.softAPgetStationNum()) + " / 8</div></div>";
  p += "<div class='stat'><div class='muted'>Uptime</div><div class='value'>" + uptimeText() + "</div></div>";
  p += "</div><p class='muted'>Local Wi-Fi only. No internet access.</p></div>";

  wifi_sta_list_t stations = {};
  esp_err_t result = esp_wifi_ap_get_sta_list(&stations);

  p += "<div class='card'><h2>Connected Devices</h2>";
  if (result == ESP_OK && stations.num > 0) {
    for (int i = 0; i < stations.num; i++) {
      char mac[18];
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
        stations.sta[i].mac[0], stations.sta[i].mac[1],
        stations.sta[i].mac[2], stations.sta[i].mac[3],
        stations.sta[i].mac[4], stations.sta[i].mac[5]);

      p += "<div class='device'><b>Device " + String(i + 1) +
           "</b><br><span class='muted'>MAC: " + String(mac) +
           "</span></div>";
    }
  } else {
    p += "<p class='muted'>No devices connected.</p>";
  }
  p += "<p class='muted'>Device names and IP addresses are not displayed in this version.</p></div>";

  p += R"HTML(
<div class="card"><h2>Wi-Fi Settings</h2>
<form action="/save-network" method="POST">
<label>Network Name (SSID)</label>
<input name="ssid" maxlength="32" required value=")HTML";
  p += esc(apName);
  p += R"HTML(">
<label>New Wi-Fi Password</label>
<input type="password" name="password" minlength="8" maxlength="63" required placeholder="8–63 characters">
<button type="submit">Save and Restart Wi-Fi</button>
</form><p class="muted">Connected devices will disconnect when Wi-Fi restarts.</p></div>
<div class="card"><h2>Admin Credentials</h2>
<form action="/save-admin" method="POST">
<label>New Username</label>
<input name="username" minlength="3" maxlength="32" required value=")HTML";
  p += esc(adminUser);
  p += R"HTML(">
<label>New Admin Password</label>
<input type="password" name="password" minlength="8" maxlength="63" required placeholder="At least 8 characters">
<button type="submit">Update Admin Login</button>
</form></div>
<div class="card"><h2>System</h2>
<form action="/restart" method="POST">
<button class="danger" type="submit">Restart ESP32</button>
</form></div>
<div class="muted" style="text-align:center">ESP Home Network v2.0</div>
</main></body></html>
)HTML";

  return p;
}

void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(
    IPAddress(192,168,4,1),
    IPAddress(192,168,4,1),
    IPAddress(255,255,255,0)
  );

  bool ok = WiFi.softAP(apName.c_str(), apPassword.c_str(), 1, false, 8);

  Serial.println();
  if (ok) {
    Serial.println("ESP Home Network v2.0 active");
    Serial.print("SSID: "); Serial.println(apName);
    Serial.print("Admin URL: http://"); Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("ERROR: Access point failed");
  }
}

void handleHome() {
  if (!auth()) return;
  server.send(200, "text/html; charset=utf-8", page());
}

void handleSaveNetwork() {
  if (!auth()) return;

  if (!server.hasArg("ssid") || !server.hasArg("password")) {
    server.send(400, "text/plain", "Missing Wi-Fi settings");
    return;
  }

  String ssid = server.arg("ssid");
  String password = server.arg("password");
  ssid.trim();

  if (ssid.length() < 1 || ssid.length() > 32 ||
      password.length() < 8 || password.length() > 63) {
    server.send(400, "text/html; charset=utf-8",
                page("Invalid SSID or password length."));
    return;
  }

  preferences.putString("ssid", ssid);
  preferences.putString("wifi_pw", password);

  server.send(200, "text/html; charset=utf-8",
    "<h2>Wi-Fi settings saved</h2><p>Network restarting. Reconnect using the new credentials.</p>");

  delay(1000);
  ESP.restart();
}

void handleSaveAdmin() {
  if (!auth()) return;

  if (!server.hasArg("username") || !server.hasArg("password")) {
    server.send(400, "text/plain", "Missing admin settings");
    return;
  }

  String user = server.arg("username");
  String password = server.arg("password");
  user.trim();

  if (user.length() < 3 || user.length() > 32 ||
      password.length() < 8 || password.length() > 63) {
    server.send(400, "text/html; charset=utf-8",
                page("Invalid admin username or password length."));
    return;
  }

  preferences.putString("admin_user", user);
  preferences.putString("admin_pw", password);

  adminUser = user;
  adminPassword = password;

  server.send(200, "text/html; charset=utf-8",
    "<h2>Admin login updated</h2><p>Close the browser and reopen http://192.168.4.1 using your new credentials.</p>");
}

void handleRestart() {
  if (!auth()) return;

  server.send(200, "text/html; charset=utf-8",
              "<h2>Restarting ESP32...</h2>");
  delay(500);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  bootMillis = millis();

  preferences.begin("esp-network", false);

  apName = preferences.getString("ssid", DEFAULT_SSID);
  apPassword = preferences.getString("wifi_pw", DEFAULT_WIFI_PASSWORD);
  adminUser = preferences.getString("admin_user", DEFAULT_ADMIN_USER);
  adminPassword = preferences.getString("admin_pw", DEFAULT_ADMIN_PASSWORD);

  if (apName.length() < 1 || apName.length() > 32 ||
      apPassword.length() < 8 || apPassword.length() > 63) {
    apName = DEFAULT_SSID;
    apPassword = DEFAULT_WIFI_PASSWORD;
  }

  if (adminUser.length() < 3 || adminUser.length() > 32 ||
      adminPassword.length() < 8 || adminPassword.length() > 63) {
    adminUser = DEFAULT_ADMIN_USER;
    adminPassword = DEFAULT_ADMIN_PASSWORD;
  }

  startAP();

  server.on("/", HTTP_GET, handleHome);
  server.on("/save-network", HTTP_POST, handleSaveNetwork);
  server.on("/save-admin", HTTP_POST, handleSaveAdmin);
  server.on("/restart", HTTP_POST, handleRestart);

  server.onNotFound([]() {
    if (!auth()) return;
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302, "text/plain", "Open dashboard");
  });

  server.begin();
  Serial.println("Dashboard started.");
  Serial.println("Default login: admin / Admin@123");
}

void loop() {
  server.handleClient();
}
