from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MESSAGING = (ROOT / "firmware/messaging/messaging.ino").read_text()
CAMPUS = (ROOT / "firmware/campus_assistance/campus_assistance.ino").read_text()
PROTOCOL = (ROOT / "docs/PROTOCOL.md").read_text()
HARDWARE = (ROOT / "docs/HARDWARE_AND_SAFETY.md").read_text()


class MessagingSourceContracts(unittest.TestCase):
    def test_fixed_message_limit_and_serial_input(self):
        self.assertIn("#define MAX_MESSAGE_CHARS       128", MESSAGING)
        self.assertIn("Serial.available()", MESSAGING)
        self.assertIn("sendText(input)", MESSAGING)

    def test_discovery_and_persistent_device_id(self):
        self.assertIn("PKT_HELLO", MESSAGING)
        self.assertIn("HELLO_INTERVAL_MS", MESSAGING)
        self.assertIn("EEPROM.put(0, EEPROM_MAGIC)", MESSAGING)
        self.assertIn("EEPROM.put(2, deviceId)", MESSAGING)

    def test_automatic_ack_and_retry_configuration(self):
        self.assertIn("sendAck(rxPacket.messageId)", MESSAGING)
        self.assertIn("#define MAX_RETRIES", MESSAGING)
        self.assertIn("#define ACK_TIMEOUT_MS", MESSAGING)
        self.assertIn("activePeerCount()", MESSAGING)

    def test_duplicate_suppression(self):
        self.assertIn("lastDisplayedSender", MESSAGING)
        self.assertIn("lastDisplayedMessage", MESSAGING)


class CampusSourceContracts(unittest.TestCase):
    def test_station_and_cart_roles_are_selectable(self):
        self.assertIn("#define DEVICE_ROLE_STATION 1", CAMPUS)
        self.assertIn("#define DEVICE_ROLE_CART    2", CAMPUS)
        self.assertIn("#ifndef DEVICE_ROLE", CAMPUS)

    def test_queue_and_fifo_controls(self):
        self.assertIn("#define MAX_PENDING_REQUESTS             10", CAMPUS)
        self.assertIn("PendingRequest &item = queueItems[queueCount++]", CAMPUS)
        self.assertIn("claimNextRequest()", CAMPUS)
        self.assertIn("PKT_QUEUE_FULL", CAMPUS)

    def test_assignment_is_confirmed_and_heartbeated(self):
        self.assertIn("PKT_ASSIGN_ACK", CAMPUS)
        self.assertIn("ASSIGN_RETRY_INTERVAL_MS", CAMPUS)
        self.assertIn("CART_HEARTBEAT_INTERVAL_MS", CAMPUS)
        self.assertIn("CART_OFFLINE_TIMEOUT_MS", CAMPUS)

    def test_cancellation_and_completion_controls(self):
        self.assertIn("BUTTON_HOLD_CANCEL_MS", CAMPUS)
        self.assertIn("PKT_STUDENT_CANCEL", CAMPUS)
        self.assertIn("PKT_CART_CANCEL", CAMPUS)
        self.assertIn("PKT_COMPLETE", CAMPUS)

    def test_lcd_and_accessible_station_feedback_hardware(self):
        self.assertIn("LiquidCrystal_I2C", CAMPUS)
        self.assertIn("STATION_LED_R_PIN", CAMPUS)
        self.assertIn("STATION_BUZZER_PIN", CAMPUS)

    def test_rf_and_electrical_cautions_are_documented(self):
        self.assertIn("SX1278", HARDWARE)
        self.assertIn("3.3 V", HARDWARE)
        self.assertIn("WPC", HARDWARE)
        self.assertIn("not authentication", PROTOCOL)


if __name__ == "__main__":
    unittest.main(verbosity=2)
