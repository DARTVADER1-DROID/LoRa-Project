# Platform adapter boundary

Implement one adapter set per selected board. Keep Arduino/ESP-IDF headers confined to platform code.

Required adapters: IRadioTransport (RA-02/SX1278), IButtonInput (maximum two buttons, debounce and short/long events), IDisplay (16×2 LCD), IBuzzer, IClock, IPersistence and IDiagnostics.

Do not freeze pin maps before exact ESP32 board selection. Uno-to-RA-02 logic-level translation and a suitable 3.3 V transient supply are hardware requirements. Verify exact radio band, antenna, EIRP and current Indian WPC requirements before transmission.
