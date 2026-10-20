#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

// ============================================================
// WIFI SETTINGS
// ============================================================

const char *ROUTER_SSID = "HomeRouter";
const char *ROUTER_PASSWORD = "Nana@8805";

// Replace these two values with your mobile hotspot details.
const char *HOTSPOT_SSID = "moto g86";
const char *HOTSPOT_PASSWORD = "Nana@8805";

// The NodeMCU built-in LED is active-low.
const uint8_t WIFI_LED_PIN = LED_BUILTIN;

// ============================================================
// IN MY CASEE
// NETWORK SETTINGS
// Router: 192.168.31.1
// ESP8266: 192.168.31.50
// ============================================================

IPAddress localIP(192, 168, 31, 50);
IPAddress gateway(192, 168, 31, 1);
IPAddress subnet(255, 255, 255, 0);

// DNS servers used by ESP8266 itself
IPAddress dns1(1, 1, 1, 1);
IPAddress dns2(8, 8, 8, 8);

// ============================================================
// UPSTREAM DNS
// ============================================================

IPAddress upstreamDNS(1, 1, 1, 1);

// ============================================================
// DNS SERVERS
// ============================================================

WiFiUDP clientDNS;
WiFiUDP upstreamDNSUDP;

const uint16_t DNS_PORT = 53;
const uint16_t UPSTREAM_LOCAL_PORT = 5353;

// ============================================================
// WEB SERVER
// ============================================================

ESP8266WebServer server(80);

// ============================================================
// BLOCKLIST
// ============================================================

#define MAX_BLOCKED_DOMAINS 150
#define CUSTOM_BLOCKLIST_FILE "/custom_blocks.txt"

String blocklist[MAX_BLOCKED_DOMAINS];

int blockCount = 0;
bool loadingCustomBlocklist = false;

bool addBlockedDomain(String domain);

bool saveCustomBlocklist()
{
    if (loadingCustomBlocklist)
    {
        return true;
    }

    File file = LittleFS.open(
        CUSTOM_BLOCKLIST_FILE,
        "w");

    if (!file)
    {
        Serial.println(
            "ERROR: Could not save custom blocklist");
        return false;
    }

    for (int i = 0; i < blockCount; i++)
    {
        if (file.println(blocklist[i]) == 0)
        {
            file.close();
            Serial.println(
                "ERROR: Could not write custom blocklist");
            return false;
        }
    }

    file.close();
    return true;
}

void loadCustomBlocklist()
{
    if (!LittleFS.exists(CUSTOM_BLOCKLIST_FILE))
    {
        return;
    }

    File file = LittleFS.open(
        CUSTOM_BLOCKLIST_FILE,
        "r");

    if (!file)
    {
        Serial.println(
            "ERROR: Could not load custom blocklist");
        return;
    }

    loadingCustomBlocklist = true;

    while (file.available())
    {
        String domain = file.readStringUntil('\n');
        domain.trim();

        if (domain.length() == 0)
        {
            continue;
        }

        if (blockCount >= MAX_BLOCKED_DOMAINS ||
            addBlockedDomain(domain) == false)
        {
            Serial.println(
                "WARNING: Skipping invalid custom blocklist entry");
        }
    }

    file.close();
    loadingCustomBlocklist = false;
}

// ============================================================
// STATISTICS
// ============================================================

unsigned long blockedQueries = 0;
unsigned long allowedQueries = 0;
unsigned long totalQueries = 0;

// ============================================================
// DNS LOG TERMINAL
// ============================================================
//
// Keeps the latest 100 DNS requests.
//
// Fixed-size buffers are used instead of String objects
// to reduce heap fragmentation on ESP8266.
//

#define MAX_LOGS 100
#define MAX_LOG_DOMAIN_LENGTH 128

struct DNSLog
{
    unsigned long time;
    bool blocked;
    char domain[MAX_LOG_DOMAIN_LENGTH];
};

DNSLog dnsLogs[MAX_LOGS];

int logCount = 0;

// ============================================================
// ADD DNS LOG
// ============================================================

void addDNSLog(
    const String &domain,
    bool blocked)
{
    // If already full, shift everything left.
    // This removes the oldest log.

    if (logCount >= MAX_LOGS)
    {
        for (int i = 1; i < MAX_LOGS; i++)
        {
            dnsLogs[i - 1] = dnsLogs[i];
        }

        logCount = MAX_LOGS - 1;
    }

    dnsLogs[logCount].time = millis();
    dnsLogs[logCount].blocked = blocked;

    String cleanDomain = domain;

    if (cleanDomain.length() >= MAX_LOG_DOMAIN_LENGTH)
    {
        cleanDomain =
            cleanDomain.substring(
                0,
                MAX_LOG_DOMAIN_LENGTH - 1);
    }

    cleanDomain.toCharArray(
        dnsLogs[logCount].domain,
        MAX_LOG_DOMAIN_LENGTH);

    logCount++;
}

// ============================================================
// CLEAR DNS LOGS
// ============================================================

void clearDNSLogs()
{
    logCount = 0;

    memset(
        dnsLogs,
        0,
        sizeof(dnsLogs));

    Serial.println(
        "DNS terminal logs cleared");
}

// ============================================================
// WIFI CONNECTION
// ============================================================

void connectToNetwork(const char *ssid, const char *password, bool useStaticIP)
{
    Serial.println();
    Serial.print("Trying Wi-Fi: ");
    Serial.println(ssid);

    WiFi.disconnect();
    delay(250);

    if (useStaticIP)
    {
        if (!WiFi.config(localIP, gateway, subnet, dns1, dns2))
        {
            Serial.println("WARNING: Static IP configuration failed; continuing");
        }
    }
    else
    {
        // Use DHCP for the mobile hotspot, whose subnet is usually different.
        WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0),
                    IPAddress(0, 0, 0, 0));
    }

    WiFi.begin(ssid, password);
    unsigned long attemptStarted = millis();
    bool ledState = HIGH;

    while (WiFi.status() != WL_CONNECTED && millis() - attemptStarted < 12000)
    {
        digitalWrite(WIFI_LED_PIN, ledState);
        ledState = !ledState;
        delay(300);
        yield();
    }
}

void connectWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    // Keep trying both networks until one connects.
    while (WiFi.status() != WL_CONNECTED)
    {
        connectToNetwork(ROUTER_SSID, ROUTER_PASSWORD, true);

        if (WiFi.status() != WL_CONNECTED)
        {
            connectToNetwork(HOTSPOT_SSID, HOTSPOT_PASSWORD, false);
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("Neither Wi-Fi network is available; retrying both...");
            unsigned long waitStarted = millis();
            bool ledState = HIGH;
            while (millis() - waitStarted < 1500)
            {
                digitalWrite(WIFI_LED_PIN, ledState);
                ledState = !ledState;
                delay(250);
                yield();
            }
        }
    }

    digitalWrite(WIFI_LED_PIN, LOW);
    Serial.println("==============================");
    Serial.println("Wi-Fi connected!");
    Serial.print("Connected network: ");
    Serial.println(WiFi.SSID());
    Serial.print("ESP8266 IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println("==============================");
}

// ============================================================
// CHECK IF DOMAIN IS BLOCKED
// ============================================================

bool isBlocked(String domain)
{
    domain.toLowerCase();
    domain.trim();

    // Remove trailing dot

    if (domain.endsWith("."))
    {
        domain.remove(
            domain.length() - 1);
    }

    for (int i = 0; i < blockCount; i++)
    {
        String blocked =
            blocklist[i];

        blocked.toLowerCase();
        blocked.trim();

        if (blocked.endsWith("."))
        {
            blocked.remove(
                blocked.length() - 1);
        }

        // Exact match

        if (domain == blocked)
        {
            return true;
        }

        // Subdomain match

        String suffix =
            "." + blocked;

        if (domain.endsWith(suffix))
        {
            return true;
        }
    }

    return false;
}

// ============================================================
// ADD DOMAIN
// ============================================================

bool addBlockedDomain(String domain)
{
    domain.toLowerCase();
    domain.trim();

    // Remove protocol

    domain.replace(
        "http://",
        "");

    domain.replace(
        "https://",
        "");

    // Remove path

    int slashIndex =
        domain.indexOf('/');

    if (slashIndex >= 0)
    {
        domain =
            domain.substring(
                0,
                slashIndex);
    }

    // Remove trailing dot

    if (domain.endsWith("."))
    {
        domain.remove(
            domain.length() - 1);
    }

    if (domain.length() == 0)
    {
        return false;
    }

    if (blockCount >= MAX_BLOCKED_DOMAINS)
    {
        return false;
    }

    // Check duplicate

    for (int i = 0; i < blockCount; i++)
    {
        if (blocklist[i] == domain)
        {
            return true;
        }
    }

    blocklist[blockCount] =
        domain;

    blockCount++;

    if (!saveCustomBlocklist())
    {
        blockCount--;
        blocklist[blockCount] = "";
        return false;
    }

    Serial.print(
        "Added to blocklist: ");

    Serial.println(domain);

    return true;
}

// ============================================================
// REMOVE DOMAIN
// ============================================================

bool removeBlockedDomain(int index)
{
    if (
        index < 0 ||
        index >= blockCount)
    {
        return false;
    }

    String removedDomain =
        blocklist[index];

    Serial.print(
        "Removed from blocklist: ");

    Serial.println(
        blocklist[index]);

    for (
        int i = index;
        i < blockCount - 1;
        i++)
    {
        blocklist[i] =
            blocklist[i + 1];
    }

    blockCount--;

    if (!saveCustomBlocklist())
    {
        for (
            int i = blockCount;
            i > index;
            i--)
        {
            blocklist[i] =
                blocklist[i - 1];
        }

        blocklist[index] =
            removedDomain;
        blockCount++;
        return false;
    }

    return true;
}

// ============================================================
// READ DNS DOMAIN NAME
// ============================================================

String readDomainName(
    uint8_t *packet,
    int &position,
    int packetSize)
{
    String domain = "";

    while (
        position < packetSize)
    {
        uint8_t length =
            packet[position++];

        // End of domain

        if (length == 0)
        {
            break;
        }

        // DNS compression pointer

        if ((length & 0xC0) == 0xC0)
        {
            if (
                position >= packetSize)
            {
                return "";
            }

            position++;

            break;
        }

        // Invalid packet

        if (
            position + length >
            packetSize)
        {
            return "";
        }

        if (domain.length() > 0)
        {
            domain += ".";
        }

        for (
            int i = 0;
            i < length;
            i++)
        {
            domain +=
                (char)packet[position++];
        }
    }

    domain.toLowerCase();

    return domain;
}

// ============================================================
// SEND BLOCKED DNS RESPONSE
// ============================================================

void sendBlockedResponse(
    uint8_t *request,
    int requestLength,
    IPAddress clientIP,
    uint16_t clientPort)
{
    uint8_t response[512];

    if (requestLength > 512)
    {
        requestLength = 512;
    }

    memcpy(
        response,
        request,
        requestLength);

    // Response flag

    response[2] = 0x81;

    // NXDOMAIN

    response[3] = 0x83;

    // Questions = 1

    response[4] = 0x00;
    response[5] = 0x01;

    // Answers = 0

    response[6] = 0x00;
    response[7] = 0x00;

    // Authority = 0

    response[8] = 0x00;
    response[9] = 0x00;

    // Additional = 0

    response[10] = 0x00;
    response[11] = 0x00;

    clientDNS.beginPacket(
        clientIP,
        clientPort);

    clientDNS.write(
        response,
        requestLength);

    clientDNS.endPacket();
}

// ============================================================
// FORWARD DNS REQUEST
// ============================================================

void forwardDNSRequest(
    uint8_t *request,
    int requestLength,
    IPAddress clientIP,
    uint16_t clientPort)
{
    // Save original transaction ID

    uint8_t originalID0 =
        request[0];

    uint8_t originalID1 =
        request[1];

    // Create new transaction ID

    uint16_t newID =
        (uint16_t)(micros() & 0xFFFF);

    request[0] =
        (newID >> 8) & 0xFF;

    request[1] =
        newID & 0xFF;

    // Send to upstream DNS

    upstreamDNSUDP.beginPacket(
        upstreamDNS,
        53);

    upstreamDNSUDP.write(
        request,
        requestLength);

    upstreamDNSUDP.endPacket();

    // Wait for response

    unsigned long start =
        millis();

    while (
        millis() - start < 2000)
    {
        int responseSize =
            upstreamDNSUDP.parsePacket();

        if (responseSize > 0)
        {
            uint8_t response[512];

            int responseLength =
                upstreamDNSUDP.read(
                    response,
                    sizeof(response));

            if (
                responseLength <= 0)
            {
                return;
            }

            // Restore client's transaction ID

            response[0] =
                originalID0;

            response[1] =
                originalID1;

            // Send response to client

            clientDNS.beginPacket(
                clientIP,
                clientPort);

            clientDNS.write(
                response,
                responseLength);

            clientDNS.endPacket();

            return;
        }

        yield();
    }

    Serial.println(
        "ERROR: Upstream DNS timeout");
}

// ============================================================
// PROCESS DNS REQUEST
// ============================================================

void processDNS()
{
    int packetSize =
        clientDNS.parsePacket();

    if (packetSize <= 0)
    {
        return;
    }

    uint8_t packet[512];

    int length =
        clientDNS.read(
            packet,
            sizeof(packet));

    if (length < 12)
    {
        return;
    }

    // Save client information

    IPAddress clientIP =
        clientDNS.remoteIP();

    uint16_t clientPort =
        clientDNS.remotePort();

    // Read domain

    int position = 12;

    String domain =
        readDomainName(
            packet,
            position,
            length);

    if (domain.length() == 0)
    {
        return;
    }

    totalQueries++;

    Serial.print(
        "DNS request: ");

    Serial.println(domain);

    // Check blocklist

    if (isBlocked(domain))
    {
        blockedQueries++;

        Serial.print(
            ">>> BLOCKED: ");

        Serial.println(domain);

        // Add terminal log

        addDNSLog(
            domain,
            true);

        sendBlockedResponse(
            packet,
            length,
            clientIP,
            clientPort);
    }
    else
    {
        allowedQueries++;

        Serial.print(
            ">>> ALLOWED: ");

        Serial.println(domain);

        // Add terminal log

        addDNSLog(
            domain,
            false);

        forwardDNSRequest(
            packet,
            length,
            clientIP,
            clientPort);
    }
}

// ============================================================
// JSON ESCAPE
// ============================================================

String jsonEscape(
    const char *text)
{
    String result = "";

    while (*text)
    {
        char c = *text++;

        if (c == '\\')
        {
            result += "\\\\";
        }
        else if (c == '"')
        {
            result += "\\\"";
        }
        else if (c == '\n')
        {
            result += "\\n";
        }
        else if (c == '\r')
        {
            result += "\\r";
        }
        else
        {
            result += c;
        }
    }

    return result;
}

// ============================================================
// WEB: DNS LOGS API
// ============================================================

void handleLogs()
{
    String json = "[";

    for (int i = 0; i < logCount; i++)
    {
        if (i > 0)
        {
            json += ",";
        }

        json += "{";

        json += "\"time\":";
        json += String(dnsLogs[i].time);

        json += ",";

        json += "\"blocked\":";
        json +=
            dnsLogs[i].blocked
                ? "true"
                : "false";

        json += ",";

        json += "\"domain\":\"";
        json +=
            jsonEscape(
                dnsLogs[i].domain);
        json += "\"";

        json += "}";
    }

    json += "]";

    server.send(
        200,
        "application/json",
        json);
}

// ============================================================
// WEB: CLEAR DNS LOGS
// ============================================================

void handleClearLogs()
{
    clearDNSLogs();

    server.send(
        200,
        "application/json",
        "{\"success\":true}");
}

// ============================================================
// WEB DASHBOARD HTML
// ============================================================

String createWebPage()
{
    String html = "";

    html += "<!DOCTYPE html>";
    html += "<html>";

    html += "<head>";

    html +=
        "<meta name='viewport' "
        "content='width=device-width,initial-scale=1'>";

    html +=
        "<title>ESP8266 DNS Ad Blocker</title>";

    html += "<style>";

    // --------------------------------------------------------
    // GENERAL
    // --------------------------------------------------------

    html +=
        "body{"
        "font-family:Arial,sans-serif;"
        "background:#f1f3f4;"
        "margin:0;"
        "padding:20px;"
        "}";

    html +=
        ".container{"
        "max-width:700px;"
        "margin:auto;"
        "}";

    html +=
        ".card{"
        "background:white;"
        "padding:20px;"
        "margin-bottom:16px;"
        "border-radius:12px;"
        "box-shadow:0 2px 8px rgba(0,0,0,.08);"
        "}";

    // --------------------------------------------------------
    // INPUT
    // --------------------------------------------------------

    html +=
        "input{"
        "padding:12px;"
        "width:65%;"
        "font-size:16px;"
        "border:1px solid #ccc;"
        "border-radius:6px;"
        "}";

    // --------------------------------------------------------
    // BUTTON
    // --------------------------------------------------------

    html +=
        "button{"
        "padding:12px 16px;"
        "font-size:15px;"
        "border:0;"
        "border-radius:6px;"
        "cursor:pointer;"
        "}";

    // --------------------------------------------------------
    // COLORS
    // --------------------------------------------------------

    html +=
        ".block{"
        "color:#d93025;"
        "}";

    html +=
        ".allow{"
        "color:#188038;"
        "}";

    html +=
        ".domain{"
        "padding:8px 0;"
        "border-bottom:1px solid #eee;"
        "}";

    // --------------------------------------------------------
    // TERMINAL
    // --------------------------------------------------------

    html +=
        ".terminal-header{"
        "display:flex;"
        "justify-content:space-between;"
        "align-items:center;"
        "margin-bottom:12px;"
        "}";

    html +=
        ".terminal-header h2{"
        "margin:0;"
        "}";

    html +=
        ".clear-btn{"
        "background:#d93025;"
        "color:white;"
        "padding:8px 14px;"
        "}";

    html +=
        ".terminal{"
        "background:#101214;"
        "color:#e8eaed;"
        "height:320px;"
        "overflow-y:auto;"
        "padding:12px;"
        "border-radius:8px;"
        "font-family:monospace;"
        "font-size:13px;"
        "line-height:1.7;"
        "box-sizing:border-box;"
        "}";

    html +=
        ".log-line{"
        "white-space:nowrap;"
        "overflow:hidden;"
        "text-overflow:ellipsis;"
        "}";

    html +=
        ".log-time{"
        "color:#9aa0a6;"
        "}";

    html +=
        ".log-blocked{"
        "color:#ff6b6b;"
        "}";

    html +=
        ".log-allowed{"
        "color:#69db7c;"
        "}";

    html +=
        ".empty-log{"
        "color:#777;"
        "}";

    html += "</style>";

    html += "</head>";

    html += "<body>";

    html += "<div class='container'>";

    // ========================================================
    // HEADER
    // ========================================================

    html += "<div class='card'>";

    html +=
        "<h1>ESP8266 DNS Ad Blocker</h1>";

    html +=
        "<p><b>Status:</b> Connected</p>";

    html +=
        "<p><b>ESP IP:</b> ";

    html +=
        WiFi.localIP().toString();

    html += "</p>";

    html +=
        "<p><b>Router:</b> ";

    html +=
        WiFi.gatewayIP().toString();

    html += "</p>";

    html += "</div>";

    // ========================================================
    // STATISTICS
    // ========================================================

    html += "<div class='card'>";

    html += "<h2>Statistics</h2>";

    html +=
        "<p class='block'><b>Blocked requests:</b> ";

    html +=
        String(blockedQueries);

    html += "</p>";

    html +=
        "<p class='allow'><b>Allowed requests:</b> ";

    html +=
        String(allowedQueries);

    html += "</p>";

    html +=
        "<p><b>Total requests:</b> ";

    html +=
        String(totalQueries);

    html += "</p>";

    html += "</div>";

    // ========================================================
    // DNS TERMINAL
    // ========================================================

    html += "<div class='card'>";

    html +=
        "<div class='terminal-header'>";

    html +=
        "<h2>DNS Terminal</h2>";

    html +=
        "<button "
        "class='clear-btn' "
        "onclick='clearLogs()'>"
        "Clear Logs"
        "</button>";

    html += "</div>";

    html +=
        "<div id='terminal' "
        "class='terminal'>";

    html +=
        "<div class='empty-log'>"
        "Waiting for DNS requests..."
        "</div>";

    html += "</div>";

    html += "</div>";

    // ========================================================
    // ADD DOMAIN
    // ========================================================

    html += "<div class='card'>";

    html += "<h2>Add domain</h2>";

    html +=
        "<form action='/add' method='GET'>";

    html +=
        "<input "
        "name='domain' "
        "placeholder='example.com' "
        "required>";

    html +=
        "<button type='submit'>Block</button>";

    html += "</form>";

    html +=
        "<p>Example: ads.example.com</p>";

    html += "</div>";

    // ========================================================
    // BLOCKLIST
    // ========================================================

    html += "<div class='card'>";

    html += "<h2>Blocked domains</h2>";

    if (blockCount == 0)
    {
        html +=
            "<p>No domains are currently blocked.</p>";
    }
    else
    {
        for (
            int i = 0;
            i < blockCount;
            i++)
        {
            html +=
                "<div class='domain'>";

            html +=
                blocklist[i];

            html +=
                " &nbsp; ";

            html +=
                "<a href='/remove?index=";

            html +=
                String(i);

            html +=
                "'>";

            html +=
                "<button>Remove</button>";

            html +=
                "</a>";

            html +=
                "</div>";
        }
    }

    html += "</div>";

    // ========================================================
    // FOOTER
    // ========================================================

    html += "<div class='card'>";

    html +=
        "<p><b>Upstream DNS:</b> 1.1.1.1</p>";

    html +=
        "<p>DNS server: port 53</p>";

    html += "</div>";

    html += "</div>";

    // ========================================================
    // JAVASCRIPT
    // ========================================================

    html += "<script>";

    // --------------------------------------------------------
    // LOAD LOGS
    // --------------------------------------------------------

    html +=
        "async function loadLogs(){";

    html +=
        "try{";

    html +=
        "const response="
        "await fetch('/logs');";

    html +=
        "const logs="
        "await response.json();";

    html +=
        "const terminal="
        "document.getElementById('terminal');";

    html +=
        "if(logs.length===0){";

    html +=
        "terminal.innerHTML="
        "'<div class=\"empty-log\">"
        "Waiting for DNS requests..."
        "</div>';";

    html +=
        "return;"
        "}";

    html +=
        "let html='';";

    html +=
        "logs.forEach(log=>{";

    html +=
        "const seconds="
        "Math.floor(log.time/1000);";

    html +=
        "const minutes="
        "Math.floor(seconds/60);";

    html +=
        "const secs="
        "seconds%60;";

    html +=
        "const time="
        "String(minutes).padStart(2,'0')"
        "+':'"
        "+String(secs).padStart(2,'0');";

    html +=
        "const status="
        "log.blocked"
        "?'<span class=\"log-blocked\">"
        "[BLOCKED]</span>'"
        ":"
        "'<span class=\"log-allowed\">"
        "[ALLOWED]</span>';";

    html +=
        "html+="
        "'<div class=\"log-line\">'"
        "+'<span class=\"log-time\">'"
        "+time"
        "+'</span> '"
        "+status"
        "+' '"
        "+escapeHtml(log.domain)"
        "+'</div>';";

    html +=
        "});";

    html +=
        "terminal.innerHTML=html;";

    html +=
        "terminal.scrollTop="
        "terminal.scrollHeight;";

    html +=
        "}catch(e){"
        "console.log(e);"
        "}";

    html += "}";

    // --------------------------------------------------------
    // ESCAPE HTML
    // --------------------------------------------------------

    html +=
        "function escapeHtml(text){";

    html +=
        "const div=document.createElement('div');";

    html +=
        "div.textContent=text;";

    html +=
        "return div.innerHTML;";

    html += "}";

    // --------------------------------------------------------
    // CLEAR LOGS
    // --------------------------------------------------------

    html +=
        "async function clearLogs(){";

    html +=
        "try{";

    html +=
        "await fetch('/clear-logs',{"
        "method:'POST'"
        "});";

    html +=
        "loadLogs();";

    html +=
        "}catch(e){"
        "console.log(e);"
        "}";

    html += "}";

    // --------------------------------------------------------
    // AUTO REFRESH
    // --------------------------------------------------------

    html +=
        "loadLogs();";

    html +=
        "setInterval(loadLogs,1000);";

    html += "</script>";

    html += "</body>";

    html += "</html>";

    return html;
}

// ============================================================
// WEB: HOME
// ============================================================

void handleRoot()
{
    server.send(
        200,
        "text/html",
        createWebPage());
}

// ============================================================
// WEB: ADD DOMAIN
// ============================================================

void handleAdd()
{
    if (
        !server.hasArg("domain"))
    {
        server.send(
            400,
            "text/plain",
            "Domain missing");

        return;
    }

    String domain =
        server.arg("domain");

    bool success =
        addBlockedDomain(domain);

    if (!success)
    {
        server.send(
            400,
            "text/plain",
            "Could not add domain");

        return;
    }

    server.sendHeader(
        "Location",
        "/");

    server.send(
        303,
        "text/plain",
        "");
}

// ============================================================
// WEB: REMOVE DOMAIN
// ============================================================

void handleRemove()
{
    if (
        !server.hasArg("index"))
    {
        server.send(
            400,
            "text/plain",
            "Index missing");

        return;
    }

    int index =
        server.arg("index").toInt();

    if (
        !removeBlockedDomain(index))
    {
        server.send(
            400,
            "text/plain",
            "Invalid index");

        return;
    }

    server.sendHeader(
        "Location",
        "/");

    server.send(
        303,
        "text/plain",
        "");
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    pinMode(
        WIFI_LED_PIN,
        OUTPUT);

    digitalWrite(
        WIFI_LED_PIN,
        HIGH);

    Serial.println();
    Serial.println(
        "================================");

    Serial.println(
        "ESP8266 DNS AD BLOCKER");

    Serial.println(
        "================================");

    if (!LittleFS.begin())
    {
        Serial.println(
            "ERROR: Could not mount LittleFS");
    }
    else
    {
        loadCustomBlocklist();
        Serial.print(
            "Loaded custom blocked domains: ");
        Serial.println(blockCount);
    }

    // --------------------------------------------------------
    // Wi-Fi
    // --------------------------------------------------------

    connectWiFi();

    if (
        WiFi.status() != WL_CONNECTED)
    {
        Serial.println(
            "Wi-Fi connection failed.");

        return;
    }

    // --------------------------------------------------------
    // Start client DNS socket
    // --------------------------------------------------------

    if (
        clientDNS.begin(DNS_PORT))
    {
        Serial.println(
            "Client DNS server started");

        Serial.println(
            "Listening on UDP port 53");
    }
    else
    {
        Serial.println(
            "ERROR: Could not start DNS server");
    }

    // --------------------------------------------------------
    // Start upstream DNS socket
    // --------------------------------------------------------

    if (
        upstreamDNSUDP.begin(
            UPSTREAM_LOCAL_PORT))
    {
        Serial.println(
            "Upstream DNS socket started");
    }
    else
    {
        Serial.println(
            "ERROR: Could not start upstream DNS socket");
    }

    // --------------------------------------------------------
    // Web routes
    // --------------------------------------------------------

    server.on(
        "/",
        HTTP_GET,
        handleRoot);

    server.on(
        "/add",
        HTTP_GET,
        handleAdd);

    server.on(
        "/remove",
        HTTP_GET,
        handleRemove);

    // DNS terminal API

    server.on(
        "/logs",
        HTTP_GET,
        handleLogs);

    server.on(
        "/clear-logs",
        HTTP_POST,
        handleClearLogs);

    // --------------------------------------------------------
    // Start web server
    // --------------------------------------------------------

    server.begin();

    Serial.println(
        "Web dashboard started");

    Serial.println();

    Serial.println(
        "================================");

    Serial.print(
        "Dashboard: http://");

    Serial.println(
        WiFi.localIP());

    Serial.print(
        "DNS Server: ");

    Serial.println(
        WiFi.localIP());

    Serial.println(
        "DNS Terminal: ENABLED");

    Serial.println(
        "Maximum logs: 100");

    Serial.println(
        "================================");

    Serial.println();

    Serial.println(
        "Ready!");
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // Reconnect Wi-Fi if disconnected
    // --------------------------------------------------------

    if (
        WiFi.status() != WL_CONNECTED)
    {
        digitalWrite(
            WIFI_LED_PIN,
            HIGH);

        Serial.println(
            "Wi-Fi disconnected!");

        connectWiFi();

        delay(1000);

        return;
    }

    // --------------------------------------------------------
    // Process DNS
    // --------------------------------------------------------

    processDNS();

    // --------------------------------------------------------
    // Process web dashboard
    // --------------------------------------------------------

    server.handleClient();

    yield();
}