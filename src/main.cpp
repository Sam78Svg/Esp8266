#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

// ============================================================
// WIFI SETTINGS
// ============================================================

// Keep real Wi-Fi credentials in src/secrets.h (excluded from Git).
#include "secrets.h"
#include "dashboard_ui.h"

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

// Generated automatically by build_blocklist.py and uploaded from data/.
// Keep these separate from CUSTOM_BLOCKLIST_FILE: rebuilding HaGeZi must
// never overwrite domains added from the dashboard.
#define BLOOM_FILE "/bloom.bin"
#define HASH_FILE "/hashes.bin"
#define BLOOM_BITS 1048576UL
#define BLOOM_HASHES 4
#define HASH_RECORD_SIZE 16

bool autoBlocklistReady = false;
uint32_t autoHashCount = 0;

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

// ============================================================
// GENERATED BLOCKLIST LOOKUP
// Must match build_blocklist.py:
// FNV-1a 64 + custom hash2, Bloom positions, sorted little-endian <QQ>.
// ============================================================

uint64_t fnv1a64(const String &text)
{
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < text.length(); i++)
    {
        h ^= (uint8_t)text[i];
        h *= 1099511628211ULL;
    }
    return h;
}

uint64_t hash2Domain(const String &text)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < text.length(); i++)
    {
        uint64_t byteValue = (uint8_t)text[i];
        h ^= (byteValue + 0x9ULL + (uint64_t)i);
        h *= 0x100000001b3ULL;
        h ^= (h >> 29);
    }
    return h;
}

uint64_t readLE64(File &file)
{
    uint64_t value = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        int b = file.read();
        if (b < 0)
            return 0;
        value |= ((uint64_t)(uint8_t)b) << (8 * i);
    }
    return value;
}

bool bloomMayContain(File &bloom, uint64_t h1, uint64_t h2)
{
    for (uint8_t i = 0; i < BLOOM_HASHES; i++)
    {
        uint32_t position = (uint32_t)((h1 + ((uint64_t)i * h2)) % BLOOM_BITS);
        uint32_t byteIndex = position / 8;
        uint8_t bitIndex = position % 8;

        if (!bloom.seek(byteIndex, SeekSet))
            return false;

        int value = bloom.read();
        if (value < 0 || (((uint8_t)value & (1U << bitIndex)) == 0))
            return false;
    }
    return true;
}

bool hashPairExists(File &hashFile, uint32_t count, uint64_t targetH1, uint64_t targetH2)
{
    uint32_t low = 0;
    uint32_t high = count;

    while (low < high)
    {
        uint32_t mid = low + ((high - low) / 2);
        uint32_t offset = mid * HASH_RECORD_SIZE;

        if (!hashFile.seek(offset, SeekSet))
            return false;

        uint64_t h1 = readLE64(hashFile);
        uint64_t h2 = readLE64(hashFile);

        if (h1 < targetH1 || (h1 == targetH1 && h2 < targetH2))
            low = mid + 1;
        else
            high = mid;
    }

    if (low >= count || !hashFile.seek(low * HASH_RECORD_SIZE, SeekSet))
        return false;

    uint64_t h1 = readLE64(hashFile);
    uint64_t h2 = readLE64(hashFile);
    return h1 == targetH1 && h2 == targetH2;
}

String normalizeDomain(String domain)
{
    domain.toLowerCase();
    domain.trim();

    while (domain.endsWith("."))
        domain.remove(domain.length() - 1);

    return domain;
}

bool isAutoBlocked(String domain)
{
    if (!autoBlocklistReady)
        return false;

    File bloom = LittleFS.open(BLOOM_FILE, "r");
    File hashFile = LittleFS.open(HASH_FILE, "r");
    if (!bloom || !hashFile)
    {
        if (bloom)
            bloom.close();
        if (hashFile)
            hashFile.close();
        return false;
    }

    bool blocked = false;
    domain = normalizeDomain(domain);

    // Check the full hostname and each parent domain (e.g. a.b.example.com,
    // then b.example.com, then example.com).
    while (domain.length() > 0)
    {
        uint64_t h1 = fnv1a64(domain);
        uint64_t h2 = hash2Domain(domain);

        if (bloomMayContain(bloom, h1, h2) &&
            hashPairExists(hashFile, autoHashCount, h1, h2))
        {
            blocked = true;
            break;
        }

        int dot = domain.indexOf('.');
        if (dot < 0)
            break;
        domain = domain.substring(dot + 1);
    }

    bloom.close();
    hashFile.close();
    return blocked;
}

// ============================================================
// CHECK IF DOMAIN IS BLOCKED
// Custom dashboard domains are checked first; generated HaGeZi
// domains are checked second. Either source can block a query.
// ============================================================

bool isBlocked(String domain)
{
    domain = normalizeDomain(domain);

    // Manually added domains remain independent and persistent.
    for (int i = 0; i < blockCount; i++)
    {
        String blocked = normalizeDomain(blocklist[i]);

        if (domain == blocked)
            return true;

        String suffix = "." + blocked;
        if (domain.endsWith(suffix))
            return true;
    }

    // Automatically generated HaGeZi blocklist.
    return isAutoBlocked(domain);
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
// WEB: STATS API (JSON for the dashboard)
// ============================================================

void handleStats()
{
    FSInfo fs;
    const bool fsOK = LittleFS.info(fs);

    String j;
    j.reserve(460);
    j += "{\"blocked\":" + String(blockedQueries);
    j += ",\"allowed\":" + String(allowedQueries);
    j += ",\"total\":" + String(totalQueries);
    j += ",\"uptime\":" + String(millis());
    j += ",\"ssid\":\"" + jsonEscape(WiFi.SSID().c_str()) + "\"";
    j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
    j += ",\"gw\":\"" + WiFi.gatewayIP().toString() + "\"";
    j += ",\"rssi\":" + String(WiFi.RSSI());
    j += ",\"flash\":" + String(ESP.getFlashChipRealSize());
    j += ",\"fw\":" + String(ESP.getSketchSize());
    j += ",\"ota\":" + String(ESP.getFreeSketchSpace());
    j += ",\"heap\":" + String(ESP.getFreeHeap());
    j += ",\"maxBlock\":" + String(ESP.getMaxFreeBlockSize());
    j += ",\"fsTotal\":" + String(fsOK ? fs.totalBytes : 0);
    j += ",\"fsUsed\":" + String(fsOK ? fs.usedBytes : 0);
    j += ",\"custom\":" + String(blockCount);
    j += ",\"customMax\":" + String(MAX_BLOCKED_DOMAINS);
    j += ",\"autoReady\":" + String(autoBlocklistReady ? "true" : "false");
    j += ",\"autoCount\":" + String(autoBlocklistReady ? autoHashCount : 0) + "}";

    server.send(200, "application/json", j);
}

// ============================================================
// WEB: CUSTOM BLOCKLIST API (JSON for the dashboard)
// ============================================================

void handleBlocklistApi()
{
    String j = "[";

    for (int i = 0; i < blockCount; i++)
    {
        if (i > 0)
        {
            j += ",";
        }

        j += "\"" + jsonEscape(blocklist[i].c_str()) + "\"";
    }

    j += "]";

    server.send(200, "application/json", j);
}

// ============================================================
// WEB: HOME (static page served from flash, no heap use)
// ============================================================

void handleRoot()
{
    server.send_P(
        200,
        "text/html",
        DASHBOARD_HTML);
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

    if (server.hasArg("ajax"))
    {
        server.send(
            200,
            "application/json",
            "{\"success\":true}");

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
// WEB: EXPORT CUSTOM DOMAINS
// Download this before uploading a new filesystem image, because
// uploadfs can replace the current LittleFS contents.
// ============================================================

void handleExportCustomDomains()
{
    if (!LittleFS.exists(CUSTOM_BLOCKLIST_FILE))
    {
        server.send(200, "text/plain; charset=utf-8", "");
        return;
    }

    File file = LittleFS.open(CUSTOM_BLOCKLIST_FILE, "r");
    if (!file)
    {
        server.send(500, "text/plain; charset=utf-8", "Could not open custom blocklist");
        return;
    }

    server.sendHeader("Content-Disposition", "attachment; filename=custom_blocks.txt");
    server.streamFile(file, "text/plain");
    file.close();
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

    if (server.hasArg("ajax"))
    {
        server.send(
            200,
            "application/json",
            "{\"success\":true}");

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

        // Generated list is optional; custom dashboard blocking still works
        // if the generated files are absent or invalid.
        if (LittleFS.exists(BLOOM_FILE) && LittleFS.exists(HASH_FILE))
        {
            File bloomFile = LittleFS.open(BLOOM_FILE, "r");
            File hashFile = LittleFS.open(HASH_FILE, "r");

            bool bloomSizeOK = bloomFile && ((uint32_t)bloomFile.size() == (BLOOM_BITS / 8));
            bool hashSizeOK = hashFile && (hashFile.size() > 0) &&
                              ((hashFile.size() % HASH_RECORD_SIZE) == 0);

            if (bloomFile)
                bloomFile.close();

            if (hashSizeOK)
                autoHashCount = hashFile.size() / HASH_RECORD_SIZE;

            if (hashFile)
                hashFile.close();

            autoBlocklistReady = bloomSizeOK && hashSizeOK && autoHashCount > 0;

            Serial.print("Generated blocklist: ");
            Serial.println(autoBlocklistReady ? "READY" : "INVALID FILES");
            Serial.print("Generated hash entries: ");
            Serial.println(autoBlocklistReady ? autoHashCount : 0);
        }
        else
        {
            Serial.println("Generated blocklist files missing; custom blocking remains enabled.");
        }
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

    server.on(
        "/custom-blocks.txt",
        HTTP_GET,
        handleExportCustomDomains);

    // Dashboard JSON APIs

    server.on(
        "/api/stats",
        HTTP_GET,
        handleStats);

    server.on(
        "/api/blocklist",
        HTTP_GET,
        handleBlocklistApi);

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