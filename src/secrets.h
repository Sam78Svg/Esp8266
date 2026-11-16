#pragma once

// Copy this file to src/secrets.h and replace the placeholders.
// Do not commit secrets.h to GitHub.

#define ROUTER_SSID "HomeRouter"
#define ROUTER_PASSWORD "Nana@8805"

#define HOTSPOT_SSID "moto g86"
#define HOTSPOT_PASSWORD "Nana@8805"

// Static network defaults for the ESP8266 when it should use a fixed LAN IP.
// Change these to match your actual router subnet if your home network is not
// in the 192.168.31.x range. If you prefer DHCP for the router connection,
// set USE_STATIC_ROUTER_IP to 0 in main.cpp.
#define NETWORK_IP 192, 168, 31, 50
#define NETWORK_GATEWAY 192, 168, 31, 1
#define NETWORK_SUBNET 255, 255, 255, 0