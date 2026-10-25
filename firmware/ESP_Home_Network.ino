
/*
  ESP Home Network v4.0
  Board: ESP32-WROOM-32 / ESP32 Dev Module
  Framework: Arduino ESP32 Core 3.x

  Features:
  - Local Wi-Fi access point
  - Admin login
  - Live dashboard and connected-device MAC addresses
  - Wi-Fi SSID, password, and channel settings
  - Admin credential changes
  - Maximum client limit
  - Custom names for known device MAC addresses
  - JSON settings backup and restore
  - Restart and persistent settings

  Default Wi-Fi:
    SSID: ESP Home Network
    Password: HomeNet@123

  Default admin:
    Username: admin
    Password: Admin@123

  Admin URL: http://192.168.4.1
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_wifi.h>

WebServer server(80);
Preferences prefs;

// ---------------- DEFAULT SETTINGS ----------------

String apSSID = "ESP Home Network";
String apPassword = "HomeNet@123";

String adminUser = "admin";
String adminPassword = "Admin@123";

int wifiChannel = 6;
int maxClients = 8;

const int MAX_CLIENTS_HARDWARE = 8;
const int MAX_SAVED_NAMES = 8;

// ---------------- HELPERS ----------------

String htmlEscape(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  s.replace("'", "&#39;");
  return s;
}

String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "\\r");
  s.replace("\t", "\\t");
  return s;
}

String macToString(const uint8_t *mac) {
  char buf[18];
  snprintf(buf, sizeof(buf),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
  return String(buf);
}

String macKey(String mac) {
  mac.replace(":", "");
  mac.replace("-", "");
  mac.toUpperCase();
  return "n" + mac;
}

String getDeviceName(String mac) {
  String key = macKey(mac);
  String name = prefs.getString(key.c_str(), "");
  if (name.length() == 0) return "Unnamed device";
  return name;
}

bool isLoggedIn() {
  if (!server.authenticate(adminUser.c_str(),
                           adminPassword.c_str())) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

String getUptime() {
  unsigned long total = millis() / 1000;
  unsigned long days = total / 86400;
  unsigned long hours = (total % 86400) / 3600;
  unsigned long minutes = (total % 3600) / 60;
  unsigned long seconds = total % 60;

  char buf[48];
  snprintf(buf, sizeof(buf), "%lu d %02lu:%02lu:%02lu",
           days, hours, minutes, seconds);
  return String(buf);
}

int getConnectedCount() {
  wifi_sta_list_t stationList;
  memset(&stationList, 0, sizeof(stationList));

  if (esp_wifi_ap_get_sta_list(&stationList) == ESP_OK) {
    return stationList.num;
  }
  return 0;
}

String jsonStringValue(String body, String key) {
  String search = "\"" + key + "\"";
  int pos = body.indexOf(search);
  if (pos < 0) return "";

  pos = body.indexOf(':', pos + search.length());
  if (pos < 0) return "";

  pos++;
  while (pos < (int)body.length() &&
         (body[pos] == ' ' || body[pos] == '\t')) pos++;

  if (pos >= (int)body.length() || body[pos] != '"') return "";
  pos++;

  String result = "";
  bool escaped = false;

  for (int i = pos; i < (int)body.length(); i++) {
    char c = body[i];

    if (escaped) {
      if (c == 'n') result += '\n';
      else if (c == 'r') result += '\r';
      else if (c == 't') result += '\t';
      else result += c;
      escaped = false;
      continue;
    }

    if (c == '\\') {
      escaped = true;
      continue;
    }

    if (c == '"') return result;
    result += c;
  }

  return "";
}

int jsonIntValue(String body, String key, int fallback) {
  String search = "\"" + key + "\"";
  int pos = body.indexOf(search);
  if (pos < 0) return fallback;

  pos = body.indexOf(':', pos + search.length());
  if (pos < 0) return fallback;

  pos++;
  while (pos < (int)body.length() &&
         body[pos] == ' ') pos++;

  int end = pos;
  while (end < (int)body.length() &&
         (isDigit(body[end]) || body[end] == '-')) end++;

  if (end == pos) return fallback;
  return body.substring(pos, end).toInt();
}

// ---------------- LOAD / SAVE ----------------

void loadSettings() {
  prefs.begin("esp-network", false);

  apSSID = prefs.getString("ssid", "ESP Home Network");

  // Support the earlier v1 key named "password".
  apPassword = prefs.getString(
      "wifi_pw",
      prefs.getString("password", "HomeNet@123"));

  adminUser = prefs.getString("admin_user", "admin");
  adminPassword = prefs.getString("admin_pw", "Admin@123");
  wifiChannel = prefs.getInt("channel", 6);
  maxClients = prefs.getInt("max_clients", 8);

  if (apSSID.length() == 0) apSSID = "ESP Home Network";
  if (apPassword.length() < 8) apPassword = "HomeNet@123";
  if (adminUser.length() == 0) adminUser = "admin";
  if (adminPassword.length() == 0) adminPassword = "Admin@123";

  wifiChannel = constrain(wifiChannel, 1, 13);
  maxClients = constrain(maxClients, 1, MAX_CLIENTS_HARDWARE);
}

void saveSettings() {
  prefs.putString("ssid", apSSID);
  prefs.putString("wifi_pw", apPassword);
  prefs.putString("admin_user", adminUser);
  prefs.putString("admin_pw", adminPassword);
  prefs.putInt("channel", wifiChannel);
  prefs.putInt("max_clients", maxClients);
}

// ---------------- WIFI START ----------------

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  WiFi.softAPdisconnect(true);
  delay(250);

  bool started = WiFi.softAP(
      apSSID.c_str(),
      apPassword.c_str(),
      wifiChannel,
      false,
      maxClients
  );

  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP Home Network v4.0");
  Serial.println("================================");

  if (started) {
    Serial.print("SSID: ");
    Serial.println(apSSID);
    Serial.print("Admin URL: http://");
    Serial.println(WiFi.softAPIP());
    Serial.print("Channel: ");
    Serial.println(wifiChannel);
    Serial.print("Max clients: ");
    Serial.println(maxClients);
  } else {
    Serial.println("ERROR: Access point failed to start.");
  }
}

// ---------------- WEB UI ----------------

String pageHeader() {
  String h = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP Home Network v4.0</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;background:#0b1220;color:#e5e7eb;
font-family:Arial,Helvetica,sans-serif}
header{padding:22px 16px;background:#111c30;
border-bottom:1px solid #26344d}
header h1{margin:0;font-size:23px}
header p{margin:7px 0 0;color:#9fb0ca;font-size:13px}
main{max-width:900px;margin:auto;padding:16px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:12px}
.card{background:#131e31;border:1px solid #26344d;
border-radius:14px;padding:16px;margin-bottom:14px}
h2{font-size:17px;margin:0 0 14px}
.metric{font-size:25px;font-weight:bold;margin-top:7px;overflow-wrap:anywhere}
.muted{font-size:12px;color:#9fb0ca}
label{display:block;margin:12px 0 6px;font-size:13px;color:#b9c7dc}
input,select,textarea{width:100%;padding:11px;border-radius:8px;
border:1px solid #34445f;background:#0b1425;color:#fff;font-size:14px}
button,.btn{display:inline-block;border:0;border-radius:8px;padding:11px 14px;
background:#2563eb;color:#fff;font-weight:bold;cursor:pointer;
text-decoration:none;font-size:13px;margin-top:12px}
button.secondary,.btn.secondary{background:#34445f}
button.danger{background:#b91c1c}
.row{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}
.device{padding:12px 0;border-bottom:1px solid #26344d;overflow-wrap:anywhere}
.device:last-child{border-bottom:0}
.device strong{display:block;margin-bottom:5px}
.status{color:#86efac}
.notice{color:#fcd34d;font-size:13px;line-height:1.5}
footer{text-align:center;color:#72839e;font-size:12px;padding:18px}
</style>
</head>
<body>
<header>
<h1>ESP Home Network <span style="color:#60a5fa">v4.0</span></h1>
<p>Local access point · Device management · Settings backup</p>
</header>
<main>
)rawliteral";
  return h;
}

String pageFooter() {
  return R"rawliteral(
</main>
<footer>ESP Home Network v4.0 · Local network only</footer>
</body>
</html>
)rawliteral";
}

void handleRoot() {
  if (!isLoggedIn()) return;

  String h = pageHeader();

  h += R"rawliteral(
<div class="grid">
<div class="card"><div class="muted">Access point</div>
<div class="metric status" id="apstate">Online</div></div>
<div class="card"><div class="muted">Connected devices</div>
<div class="metric" id="clients">--</div></div>
<div class="card"><div class="muted">Uptime</div>
<div class="metric" id="uptime">--</div></div>
<div class="card"><div class="muted">Wi-Fi channel</div>
<div class="metric" id="channelValue">--</div></div>
</div>

<div class="card">
<h2>Connected devices</h2>
<div class="muted">Device names are saved by MAC address.</div>
<div id="devices">Loading devices...</div>
</div>

<div class="card">
<h2>Wi-Fi settings</h2>
<form method="POST" action="/savewifi">
<label>Network name (SSID)</label>
<input name="ssid" maxlength="32" required>
<label>Wi-Fi password (8–63 characters)</label>
<input name="password" type="password" minlength="8" maxlength="63"
placeholder="Enter new password" required>
<label>Channel</label>
<select name="channel">
<option value="1">1</option><option value="2">2</option>
<option value="3">3</option><option value="4">4</option>
<option value="5">5</option><option value="6">6</option>
<option value="7">7</option><option value="8">8</option>
<option value="9">9</option><option value="10">10</option>
<option value="11">11</option><option value="12">12</option>
<option value="13">13</option>
</select>
<label>Maximum connected devices (1–8)</label>
<select name="maxclients">
<option>1</option><option>2</option><option>3</option>
<option>4</option><option>5</option><option>6</option>
<option>7</option><option selected>8</option>
</select>
<button type="submit">Save & Restart Wi-Fi</button>
</form>
<p class="notice">Changing Wi-Fi settings disconnects connected devices.
Reconnect using the new network details after restart.</p>
</div>

<div class="card">
<h2>Admin security</h2>
<form method="POST" action="/saveadmin">
<label>New admin username</label>
<input name="adminuser" minlength="3" maxlength="24" required>
<label>New admin password</label>
<input name="adminpass" type="password" minlength="8" maxlength="32" required>
<button type="submit">Update admin login</button>
</form>
<p class="notice">The browser may ask you to log in again with the new credentials.</p>
</div>

<div class="card">
<h2>Rename a device</h2>
<form method="POST" action="/rename">
<label>Device MAC address</label>
<input name="mac" placeholder="AA:BB:CC:DD:EE:FF" required>
<label>Custom device name</label>
<input name="devicename" maxlength="24" placeholder="My phone" required>
<button type="submit">Save device name</button>
</form>
<p class="muted">Rename a currently connected device, or enter a MAC address you already know.</p>
</div>

<div class="card">
<h2>Backup & restore</h2>
<p class="muted">Backup includes Wi-Fi and admin credentials. Keep the downloaded file private.</p>
<a class="btn" href="/backup">Download JSON backup</a>
<form method="POST" action="/restore">
<label>Restore from a JSON backup</label>
<textarea name="backupjson" rows="7" placeholder='Paste the backup JSON here' required></textarea>
<button type="submit">Restore & Restart</button>
</form>
</div>

<div class="card">
<h2>System controls</h2>
<button class="secondary" onclick="refreshData()">Refresh dashboard</button>
<form method="POST" action="/restart" onsubmit="return confirm('Restart ESP32 now?')">
<button class="danger" type="submit">Restart ESP32</button>
</form>
</div>

<script>
async function refreshData(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    if(!r.ok) return;
    const d=await r.json();
    document.getElementById('clients').textContent=d.count;
    document.getElementById('uptime').textContent=d.uptime;
    document.getElementById('channelValue').textContent=d.channel;
    const box=document.getElementById('devices');
    if(!d.devices.length){box.innerHTML='<p class="muted">No devices connected.</p>';return;}
    box.innerHTML=d.devices.map(x=>
      '<div class="device"><strong>'+esc(x.name)+'</strong>'+
      '<span class="muted">'+esc(x.mac)+'</span></div>'
    ).join('');
  }catch(e){}
}
function esc(s){return String(s).replace(/[&<>"']/g,c=>({
'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'
}[c]));}
refreshData();
setInterval(refreshData,3000);
</script>
)rawliteral";

  // Prefill non-secret settings.
  h += "<script>document.querySelector('[name=ssid]').value='" +
       apSSID + "';";
  h += "document.querySelector('[name=channel]').value='" +
       String(wifiChannel) + "';";
  h += "document.querySelector('[name=maxclients]').value='" +
       String(maxClients) + "';";
  h += "</script>";

  h += pageFooter();
  server.send(200, "text/html; charset=utf-8", h);
}

// ---------------- STATUS API ----------------

void handleStatus() {
  if (!isLoggedIn()) return;

  wifi_sta_list_t stationList;
  memset(&stationList, 0, sizeof(stationList));

  String json = "{\"count\":0,\"uptime\":\"" +
                jsonEscape(getUptime()) +
                "\",\"channel\":" + String(wifiChannel) +
                ",\"devices\":[";

  bool first = true;
  int count = 0;

  if (esp_wifi_ap_get_sta_list(&stationList) == ESP_OK) {
    count = stationList.num;

    for (int i = 0; i < stationList.num; i++) {
      String mac = macToString(stationList.sta[i].mac);
      String name = getDeviceName(mac);

      if (!first) json += ",";
      first = false;

      json += "{\"mac\":\"" + jsonEscape(mac) +
              "\",\"name\":\"" + jsonEscape(name) + "\"}";
    }
  }

  json += "]}";
  json.replace("\"count\":0", "\"count\":" + String(count));

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

// ---------------- SAVE WIFI SETTINGS ----------------

void handleSaveWiFi() {
  if (!isLoggedIn()) return;

  String newSSID = server.arg("ssid");
  String newPassword = server.arg("password");
  int newChannel = server.arg("channel").toInt();
  int newLimit = server.arg("maxclients").toInt();

  newSSID.trim();

  if (newSSID.length() < 1 || newSSID.length() > 32 ||
      newPassword.length() < 8 || newPassword.length() > 63 ||
      newChannel < 1 || newChannel > 13 ||
      newLimit < 1 || newLimit > MAX_CLIENTS_HARDWARE) {
    server.send(400, "text/plain",
                "Invalid settings. Check SSID, password, channel and client limit.");
    return;
  }

  apSSID = newSSID;
  apPassword = newPassword;
  wifiChannel = newChannel;
  maxClients = newLimit;
  saveSettings();

  server.send(200, "text/html; charset=utf-8",
      pageHeader() +
      "<div class='card'><h2>Wi-Fi settings saved</h2>"
      "<p>ESP32 is restarting its access point. Reconnect using the new SSID and password.</p>"
      "</div><script>setTimeout(()=>location.href='http://192.168.4.1/',5000)</script>" +
      pageFooter());

  delay(800);
  startAccessPoint();
}

// ---------------- ADMIN SETTINGS ----------------

void handleSaveAdmin() {
  if (!isLoggedIn()) return;

  String newUser = server.arg("adminuser");
  String newPass = server.arg("adminpass");
  newUser.trim();

  if (newUser.length() < 3 || newUser.length() > 24 ||
      newPass.length() < 8 || newPass.length() > 32) {
    server.send(400, "text/plain",
                "Invalid admin username or password length.");
    return;
  }

  adminUser = newUser;
  adminPassword = newPass;
  saveSettings();

  server.send(200, "text/html; charset=utf-8",
      pageHeader() +
      "<div class='card'><h2>Admin credentials updated</h2>"
      "<p>Return to the dashboard and log in with your new credentials.</p>"
      "<a class='btn' href='/'>Open dashboard</a></div>" +
      pageFooter());
}

// ---------------- DEVICE RENAME ----------------

void handleRename() {
  if (!isLoggedIn()) return;

  String mac = server.arg("mac");
  String name = server.arg("devicename");
  mac.trim();
  name.trim();
  mac.toUpperCase();

  String compact = mac;
  compact.replace(":", "");
  compact.replace("-", "");

  bool valid = compact.length() == 12 && name.length() >= 1 &&
               name.length() <= 24;

  for (int i = 0; i < (int)compact.length(); i++) {
    if (!isHexadecimalDigit(compact[i])) valid = false;
  }

  if (!valid) {
    server.send(400, "text/plain",
                "Invalid MAC address or device name.");
    return;
  }

  String key = macKey(mac);
  prefs.putString(key.c_str(), name);

  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "Device name saved.");
}

// ---------------- JSON BACKUP ----------------

String makeBackupJSON() {
  String json = "{\n";
  json += "\"version\":\"4.0\",\n";
  json += "\"ssid\":\"" + jsonEscape(apSSID) + "\",\n";
  json += "\"password\":\"" + jsonEscape(apPassword) + "\",\n";
  json += "\"adminUser\":\"" + jsonEscape(adminUser) + "\",\n";
  json += "\"adminPass\":\"" + jsonEscape(adminPassword) + "\",\n";
  json += "\"channel\":" + String(wifiChannel) + ",\n";
  json += "\"maxClients\":" + String(maxClients) + ",\n";
  json += "\"deviceNames\":{";

  bool first = true;
  for (int i = 0; i < MAX_SAVED_NAMES; i++) {
    // MAC-derived preference keys are 13 characters long.
    // Enumerate only known MACs if they were stored in this small index.
    String mac = prefs.getString(("mac" + String(i)).c_str(), "");
    if (mac.length() == 0) continue;

    if (!first) json += ",";
    first = false;

    json += "\n\"" + jsonEscape(mac) + "\":\"" +
            jsonEscape(getDeviceName(mac)) + "\"";
  }

  json += "}\n}\n";
  return json;
}

void handleBackup() {
  if (!isLoggedIn()) return;

  server.sendHeader("Content-Disposition",
                    "attachment; filename=esp_home_network_v4_backup.json");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", makeBackupJSON());
}

void handleRestore() {
  if (!isLoggedIn()) return;

  String body = server.arg("backupjson");

  if (body.length() < 10 || body.length() > 5000 ||
      body.indexOf("\"ssid\"") < 0 ||
      body.indexOf("\"password\"") < 0) {
    server.send(400, "text/plain",
                "Invalid backup JSON or missing required fields.");
    return;
  }

  String restoredSSID = jsonStringValue(body, "ssid");
  String restoredPassword = jsonStringValue(body, "password");
  String restoredUser = jsonStringValue(body, "adminUser");
  String restoredAdminPassword = jsonStringValue(body, "adminPass");
  int restoredChannel = jsonIntValue(body, "channel", 6);
  int restoredLimit = jsonIntValue(body, "maxClients", 8);

  if (restoredSSID.length() < 1 || restoredSSID.length() > 32 ||
      restoredPassword.length() < 8 || restoredPassword.length() > 63 ||
      restoredUser.length() < 3 || restoredUser.length() > 24 ||
      restoredAdminPassword.length() < 8 ||
      restoredAdminPassword.length() > 32 ||
      restoredChannel < 1 || restoredChannel > 13 ||
      restoredLimit < 1 || restoredLimit > MAX_CLIENTS_HARDWARE) {
    server.send(400, "text/plain",
                "Backup settings failed validation.");
    return;
  }

  apSSID = restoredSSID;
  apPassword = restoredPassword;
  adminUser = restoredUser;
  adminPassword = restoredAdminPassword;
  wifiChannel = restoredChannel;
  maxClients = restoredLimit;
  saveSettings();

  server.send(200, "text/html; charset=utf-8",
      pageHeader() +
      "<div class='card'><h2>Backup restored</h2>"
      "<p>Settings restored. Reconnect with the restored Wi-Fi credentials.</p>"
      "</div>" + pageFooter());

  delay(800);
  startAccessPoint();
}

// ---------------- RESTART ----------------

void handleRestart() {
  if (!isLoggedIn()) return;

  server.send(200, "text/html; charset=utf-8",
      pageHeader() +
      "<div class='card'><h2>Restarting ESP32...</h2>"
      "<p>Reconnect to the Wi-Fi network in a few seconds.</p></div>" +
      pageFooter());

  delay(500);
  ESP.restart();
}

// ---------------- ROUTES ----------------

void setupRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/savewifi", HTTP_POST, handleSaveWiFi);
  server.on("/saveadmin", HTTP_POST, handleSaveAdmin);
  server.on("/rename", HTTP_POST, handleRename);
  server.on("/backup", HTTP_GET, handleBackup);
  server.on("/restore", HTTP_POST, handleRestore);
  server.on("/restart", HTTP_POST, handleRestart);

  server.onNotFound([]() {
    if (!isLoggedIn()) return;
    server.send(404, "text/plain", "404 - Page not found");
  });
}

// ---------------- SETUP / LOOP ----------------

void setup() {
  Serial.begin(115200);
  delay(500);

  loadSettings();
  startAccessPoint();
  setupRoutes();
  server.begin();

  Serial.println("Web server started.");
  Serial.println("Connect to the AP, then open http://192.168.4.1");
}

void loop() {
  server.handleClient();
  delay(2);
}
