/*
  ESP Home Network - Firmware v1.0
  Board: ESP32 Dev Module (ESP32-WROOM-32)
  Wi-Fi AP: ESP Home Network
  Admin UI: http://192.168.4.1
  Note: Local Wi-Fi only; no internet routing.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

WebServer server(80);
Preferences preferences;

String apName;
String apPassword;

const char* DEFAULT_SSID = "ESP Home Network";
const char* DEFAULT_PASSWORD = "HomeNet@123";

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  return value;
}

String makePage(String message = "") {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta charset="UTF-8">
<title>ESP Home Network</title>
<style>
*{box-sizing:border-box}
body{margin:0;background:#101827;color:#f1f5f9;font-family:Arial,sans-serif;padding:20px}
.container{max-width:520px;margin:25px auto}
.card{background:#1e293b;padding:24px;border-radius:16px;border:1px solid #334155}
h1{font-size:25px;margin-top:0}
.sub{color:#94a3b8;font-size:14px;line-height:1.6}
.status{background:#064e3b;padding:12px;border-radius:9px;color:#a7f3d0;margin:18px 0;font-size:14px}
label{display:block;margin:18px 0 8px;font-size:14px}
input{width:100%;padding:13px;border-radius:8px;border:1px solid #475569;background:#0f172a;color:white;font-size:16px}
button{width:100%;padding:14px;border:0;border-radius:9px;background:#38bdf8;color:#082f49;font-size:16px;font-weight:bold;margin-top:20px;cursor:pointer}
.note{font-size:12px;color:#94a3b8;line-height:1.6;margin-top:20px}
</style>
</head>
<body><div class="container"><div class="card">
<h1>ESP Home Network</h1>
<div class="sub">Local Network Control Panel</div>
<div class="status">● Access Point Active</div>
)rawliteral";

  page += "<p>Network IP: 192.168.4.1</p>";
  if (message.length() > 0) {
    page += "<p style='color:#7dd3fc'>" + htmlEscape(message) + "</p>";
  }

  page += R"rawliteral(
<form action="/save" method="POST">
<label>Wi-Fi Network Name (SSID)</label>
<input type="text" name="ssid" maxlength="32" required value=")rawliteral";
  page += htmlEscape(apName);
  page += R"rawliteral(">
<label>New Wi-Fi Password</label>
<input type="password" name="password" minlength="8" maxlength="63" required placeholder="Minimum 8 characters">
<button type="submit">Save & Restart Network</button>
</form>
<div class="note">Changing these settings restarts Wi-Fi. Reconnect with the new credentials. Only trusted users should access this control panel.</div>
</div></div></body></html>
)rawliteral";
  return page;
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  bool started = WiFi.softAP(apName.c_str(), apPassword.c_str(), 1, false, 8);
  if (started) {
    Serial.println();
    Serial.println("ESP Home Network is active!");
    Serial.print("Wi-Fi Name: "); Serial.println(apName);
    Serial.print("Admin URL: http://"); Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Failed to start Wi-Fi access point!");
  }
}

void handleHome() {
  server.send(200, "text/html; charset=utf-8", makePage());
}

void handleSave() {
  if (!server.hasArg("ssid") || !server.hasArg("password")) {
    server.send(400, "text/plain", "Missing Wi-Fi settings.");
    return;
  }
  String newSSID = server.arg("ssid");
  String newPassword = server.arg("password");
  newSSID.trim();

  if (newSSID.length() < 1 || newSSID.length() > 32 ||
      newPassword.length() < 8 || newPassword.length() > 63) {
    server.send(400, "text/html; charset=utf-8",
                makePage("Invalid settings. Check SSID and password length."));
    return;
  }

  server.send(200, "text/html; charset=utf-8",
    "<html><body style='font-family:Arial;padding:25px'><h2>Settings saved</h2>"
    "<p>The network is restarting. Reconnect using your new Wi-Fi name and password.</p></body></html>");

  preferences.putString("ssid", newSSID);
  preferences.putString("password", newPassword);
  delay(1200);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  preferences.begin("esp-network", false);
  apName = preferences.getString("ssid", DEFAULT_SSID);
  apPassword = preferences.getString("password", DEFAULT_PASSWORD);

  if (apName.length() < 1 || apName.length() > 32 ||
      apPassword.length() < 8 || apPassword.length() > 63) {
    apName = DEFAULT_SSID;
    apPassword = DEFAULT_PASSWORD;
  }

  startAccessPoint();
  server.on("/", HTTP_GET, handleHome);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302, "text/plain", "Open ESP Home Network settings");
  });
  server.begin();
  Serial.println("Local admin website started.");
}

void loop() {
  server.handleClient();
}
