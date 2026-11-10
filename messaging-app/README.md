# LoRa Messaging Desktop App

This directory contains the desktop application for communicating with the LoRa radio gateway over USB serial.

Planned responsibilities:
- Discover and connect to the Arduino serial port.
- Verify the USB connection and radio-gateway status.
- Send commands to the radio gateway.
- Receive and display incoming messages.
- Track peers, acknowledgements, and delivery status.

The desktop application will remain separate from the Arduino firmware and LoRa radio protocol implementation.
