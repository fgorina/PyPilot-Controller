# Pypilot controller

This applicatiuons uses am M5Dial to control PyPilot.

You may select any of the navigation modes or directly controlling the rudder.

The encoder is used to select the rhumb.

The touch screen is used to select options and whan navigating a click sets rhumb to actual heading.

Also a click when in rudder mode puts the rudder to 0º, just straight.

The device connects to the pypilot through WiFi.

Two BLE characteristics are used to define ssid and password. Then the device uses mDNS to locate the pypilot.

Characteristics are at service "f85015df-6af5-4ee3-a8cb-a8f7250d4466"

and are SSID : "bea80929-aa42-4641-a0c2-8f08b70e0aaa"
    Password : "22643f77-dcfd-4e01-9b9f-bb63692a215f"

Use any BLE application to write them.
