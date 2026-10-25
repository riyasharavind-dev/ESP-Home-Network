
/*
  ESP Home Network v3.0
  Board: ESP32 Dev Module (ESP32-WROOM-32)
  Dashboard: http://192.168.4.1
  Features: Admin authentication, live device count,
  connected MAC addresses, channel settings, persistent config.
  Local Wi-Fi only. No internet routing.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_wifi.h>

WebServer server(80);
Preferences preferences;

String apName;
String apPassword;
String adminUser;
String adminPassword;

uint8_t wifiChannel = 1;
unsigned long bootMillis = 0;

const char* DEFAULT_SSID = "ESP Home Network";
const char* DEFAULT_WIFI_PASSWORD = "HomeNet@123";
const char* DEFAULT_ADMIN_USER = "admin";
const char* DEFAULT_ADMIN_PASSWORD = "Admin@123";

// ---------- Utility functions ----------

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
  return s;
}

bool requireAuth() {
  if (server.authenticate(adminUser.c_str(),
                          adminPassword.c_str())) {
    return true;
  }

  server.requestAuthentication(
    BASIC_AUTH,
    "ESP Home Network",
    "Administrator login required"
  );
  return false;
}

String uptimeText() {
  unsigned long t = (millis() - bootMillis) / 1000;
  unsigned long days = t / 86400;
  t %= 86400;
  unsigned long hours = t / 3600;
  t %= 3600;
  unsigned long minutes = t / 60;
  unsigned long seconds = t % 60;

  String result = "";
  if (days) result += String(days) + "d ";
  if (hours || days) result += String(hours) + "h ";
  result += String(minutes) + "m " +
            String(seconds) + "s";
  return result;
}

String macAddressString(const uint8_t* mac) {
  char buf[18];
  snprintf(buf, sizeof(buf),
    "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2],
    mac[3], mac[4], mac[5]);
  return String(buf);
}

// ---------- Dashboard HTML ----------

String makeDashboard() {
  String p = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP Home Network v3.0</title>
<style>
*{box-sizing:border-box}
body{margin:0;padding:15px;background:#f1f5f9;color:#172033;
font-family:Arial,sans-serif}
main{max-width:680px;margin:15px auto}
header,.card{background:#fff;border:1px solid #dbe3ed;
border-radius:10px;padding:17px;margin-bottom:12px}
h1{font-size:23px;margin:0 0 6px}
h2{font-size:17px;margin:0 0 14px}
.small{font-size:12px;color:#64748b;line-height:1.6}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:9px}
.stat{padding:12px;background:#f8fafc;border:1px solid #e2e8f0;border-radius:8px;overflow-wrap:anywhere}
.label{font-size:12px;color:#64748b}
.value{font-size:18px;font-weight:bold;margin-top:6px;overflow-wrap:anywhere}
label{display:block;font-size:13px;margin:13px 0 6px}
input,select{width:100%;padding:11px;border:1px solid #cbd5e1;
border-radius:7px;background:white;color:#172033;font-size:15px}
button{border:0;border-radius:7px;background:#2563eb;color:white;
padding:11px 14px;font-weight:bold;cursor:pointer;margin-top:13px}
.danger{background:#dc2626}
.device{border-bottom:1px solid #e2e8f0;padding:10px 0;font-size:14px;overflow-wrap:anywhere}
.device:last-child{border-bottom:0}
form{margin:0}
.note{margin-top:12px}
@media(max-width:380px){.grid{grid-template-columns:1fr}body{padding:9px}}
</style>
</head>
<body>
<main>
<header>
<h1>ESP Home Network</h1>
<div class="small">Local network dashboard · Firmware v3.0</div>
</header>

<section class="card">
<h2>Live Network Status</h2>
<div class="grid">
<div class="stat"><div class="label">Wi-Fi Status</div>
<div class="value" id="status">Checking...</div></div>
<div class="stat"><div class="label">Connected Devices</div>
<div class="value" id="count">--</div></div>
<div class="stat"><div class="label">Network Name</div>
<div class="value" id="ssid">--</div></div>
<div class="stat"><div class="label">Access Point IP</div>
<div class="value" id="ip">192.168.4.1</div></div>
<div class="stat"><div class="label">Wi-Fi Channel</div>
<div class="value" id="channel">--</div></div>
<div class="stat"><div class="label">Uptime</div>
<div class="value" id="uptime">--</div></div>
</div>
<p class="small">Refreshes automatically every 3 seconds.</p>
</section>

<section class="card">
<h2>Connected Devices</h2>
<div id="devices" class="small">Loading devices...</div>
<p class="small">MAC addresses are shown when available. Device names and IP addresses are not provided by this dashboard.</p>
</section>

<section class="card">
<h2>Wi-Fi Settings</h2>
<form action="/save-network" method="POST">
<label>Network name (SSID)</label>
<input name="ssid" maxlength="32" required value=")HTML";

  p += htmlEscape(apName);

  p += R"HTML(">
<label>New Wi-Fi password</label>
<input type="password" name="password" minlength="8"
maxlength="63" required placeholder="8–63 characters">

<label>Wi-Fi channel</label>
<select name="channel">
)HTML";

  for (int c = 1; c <= 13; c++) {
    p += "<option value='" + String(c) + "'";
    if (wifiChannel == c) p += " selected";
    p += ">Channel " + String(c) + "</option>";
  }

  p += R"HTML(
</select>
<p class="small">Choose a channel supported by your local Wi-Fi regulations and devices.</p>
<button type="submit">Save & Restart Wi-Fi</button>
</form>
</section>

<section class="card">
<h2>Administrator Account</h2>
<form action="/save-admin" method="POST">
<label>New username</label>
<input name="username" minlength="3" maxlength="32"
required value=")HTML";

  p += htmlEscape(adminUser);

  p += R"HTML(">
<label>New admin password</label>
<input type="password" name="password" minlength="8"
maxlength="63" required placeholder="At least 8 characters">
<button type="submit">Update Login</button>
</form>
</section>

<section class="card">
<h2>System</h2>
<form action="/restart" method="POST"
onsubmit="return confirm('Restart ESP32 now?')">
<button class="danger" type="submit">Restart ESP32</button>
</form>
<p class="small note">Local Wi-Fi only. Internet access is not provided.</p>
</section>
</main>

<script>
async function updateStatus(){
  try{
    const response = await fetch('/api/status', {
      cache:'no-store',
      credentials:'same-origin'
    });
    if(!response.ok) throw new Error('Authentication required');

    const d = await response.json();
    document.getElementById('status').textContent = d.status;
    document.getElementById('count').textContent =
      d.count + ' / 8';
    document.getElementById('ssid').textContent = d.ssid;
    document.getElementById('ip').textContent = d.ip;
    document.getElementById('channel').textContent = d.channel;
    document.getElementById('uptime').textContent = d.uptime;

    const box = document.getElementById('devices');
    box.replaceChildren();

    if(d.devices.length === 0){
      box.textContent = 'No devices connected.';
    }else{
      d.devices.forEach((mac,index)=>{
        const row = document.createElement('div');
        row.className = 'device';
        row.textContent = 'Device ' + (index+1) + ' — ' + mac;
        box.appendChild(row);
      });
    }
  }catch(e){
    document.getElementById('status').textContent = 'Retrying...';
  }
}
updateStatus();
setInterval(updateStatus,3000);
</script>
</body>
</html>
)HTML";

  return p;
}

// ---------- Live status API ----------

void handleStatus() {
  if (!requireAuth()) return;

  wifi_sta_list_t stations = {};
  esp_err_t result = esp_wifi_ap_get_sta_list(&stations);
  int count = (result == ESP_OK) ? stations.num : 0;

  String json = "{";
  json += "\"status\":\"Active\",";
  json += "\"ssid\":\"" + jsonEscape(apName) + "\",";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"channel\":" + String(wifiChannel) + ",";
  json += "\"count\":" + String(count) + ",";
  json += "\"uptime\":\"" + uptimeText() + "\",";
  json += "\"devices\":[";

  if (result == ESP_OK) {
    for (int i = 0; i < stations.num; i++) {
      if (i > 0) json += ",";
      json += "\"" + macAddressString(stations.sta[i].mac) + "\"";
    }
  }

  json += "]}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

// ---------- Wi-Fi access point ----------

void startAccessPoint() {
  WiFi.mode(WIFI_AP);

  WiFi.softAPConfig(
    IPAddress(192, 168, 4, 1),
    IPAddress(192, 168, 4, 1),
    IPAddress(255, 255, 255, 0)
  );

  bool ok = WiFi.softAP(
    apName.c_str(),
    apPassword.c_str(),
    wifiChannel,
    false,
    8
  );

  Serial.println();

  if (ok) {
    Serial.println("ESP Home Network v3.0 active");
    Serial.print("SSID: ");
    Serial.println(apName);
    Serial.print("Channel: ");
    Serial.println(wifiChannel);
    Serial.print("Dashboard: http://");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("ERROR: Failed to start access point");
  }
}

// ---------- Request handlers ----------

void handleHome() {
  if (!requireAuth()) return;
  server.send(200, "text/html; charset=utf-8", makeDashboard());
}

void handleSaveNetwork() {
  if (!requireAuth()) return;

  if (!server.hasArg("ssid") ||
      !server.hasArg("password") ||
      !server.hasArg("channel")) {
    server.send(400, "text/plain", "Missing settings");
    return;
  }

  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String channelValue = server.arg("channel");
  ssid.trim();

  int channel = channelValue.toInt();

  if (ssid.length() < 1 || ssid.length() > 32 ||
      password.length() < 8 || password.length() > 63 ||
      channel < 1 || channel > 13) {
    server.send(400, "text/plain",
                "Invalid SSID, password or channel");
    return;
  }

  preferences.putString("ssid", ssid);
  preferences.putString("wifi_pw", password);
  preferences.putUChar("channel", (uint8_t)channel);

  server.send(200, "text/html; charset=utf-8",
    "<!doctype html><html><body style='font-family:Arial;padding:24px'>"
    "<h2>Settings saved</h2>"
    "<p>Wi-Fi is restarting. Reconnect using your new credentials.</p>"
    "</body></html>");

  delay(1000);
  ESP.restart();
}

void handleSaveAdmin() {
  if (!requireAuth()) return;

  if (!server.hasArg("username") ||
      !server.hasArg("password")) {
    server.send(400, "text/plain", "Missing admin settings");
    return;
  }

  String username = server.arg("username");
  String password = server.arg("password");
  username.trim();

  if (username.length() < 3 || username.length() > 32 ||
      password.length() < 8 || password.length() > 63) {
    server.send(400, "text/plain",
                "Invalid admin username or password length");
    return;
  }

  preferences.putString("admin_user", username);
  preferences.putString("admin_pw", password);

  adminUser = username;
  adminPassword = password;

  server.send(200, "text/html; charset=utf-8",
    "<!doctype html><html><body style='font-family:Arial;padding:24px'>"
    "<h2>Admin login updated</h2>"
    "<p>Close this browser tab, then reopen the dashboard and sign in with your new credentials.</p>"
    "</body></html>");
}

void handleRestart() {
  if (!requireAuth()) return;

  server.send(200, "text/html; charset=utf-8",
    "<!doctype html><html><body style='font-family:Arial;padding:24px'>"
    "<h2>Restarting ESP32...</h2></body></html>");

  delay(500);
  ESP.restart();
}

// ---------- Setup ----------

void setup() {
  Serial.begin(115200);
  delay(300);
  bootMillis = millis();

  preferences.begin("esp-network", false);

  apName = preferences.getString("ssid", DEFAULT_SSID);
  apPassword = preferences.getString("wifi_pw",
                                    DEFAULT_WIFI_PASSWORD);
  adminUser = preferences.getString("admin_user",
                                    DEFAULT_ADMIN_USER);
  adminPassword = preferences.getString("admin_pw",
                                        DEFAULT_ADMIN_PASSWORD);
  wifiChannel = preferences.getUChar("channel", 1);

  if (apName.length() < 1 || apName.length() > 32 ||
      apPassword.length() < 8 || apPassword.length() > 63) {
    apName = DEFAULT_SSID;
    apPassword = DEFAULT_WIFI_PASSWORD;
  }

  if (adminUser.length() < 3 || adminUser.length() > 32 ||
      adminPassword.length() < 8 ||
      adminPassword.length() > 63) {
    adminUser = DEFAULT_ADMIN_USER;
    adminPassword = DEFAULT_ADMIN_PASSWORD;
  }

  if (wifiChannel < 1 || wifiChannel > 13) {
    wifiChannel = 1;
  }

  startAccessPoint();

  server.on("/", HTTP_GET, handleHome);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/save-network", HTTP_POST, handleSaveNetwork);
  server.on("/save-admin", HTTP_POST, handleSaveAdmin);
  server.on("/restart", HTTP_POST, handleRestart);

  server.onNotFound([]() {
    if (!requireAuth()) return;
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302, "text/plain", "Open dashboard");
  });

  server.begin();

  Serial.println("Dashboard started.");
  Serial.println("Default admin username: admin");
  Serial.println("Default admin password: Admin@123");
}

void loop() {
  server.handleClient();
}
