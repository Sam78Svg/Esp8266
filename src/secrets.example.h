#pragma once

// Copy this file to src/secrets.h and replace the example values.
// src/secrets.h is excluded from Git; never commit real Wi-Fi credentials.
#define ROUTER_SSID "YOUR_ROUTER_SSID"
#define ROUTER_PASSWORD "YOUR_ROUTER_PASSWORD"

// Optional fallback network. Leave empty if you do not use one.
#define HOTSPOT_SSID ""
#define HOTSPOT_PASSWORD ""

// Documentation-only addresses (RFC 5737 TEST-NET-1).
// Replace these with an unused address and matching gateway on your LAN.
#define NETWORK_IP 192, 0, 2, 50
#define NETWORK_GATEWAY 192, 0, 2, 1
#define NETWORK_SUBNET 255, 255, 255, 0
