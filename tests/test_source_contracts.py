from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MESSAGING = (ROOT / "firmware/messaging/messaging.ino").read_text()
CAMPUS = (ROOT / "firmware/campus_assistance/campus_assistance.ino").read_text()
PROTOCOL = (ROOT / "docs/PROTOCOL.md").read_text()
MESSAGING_PROTOCOL = (ROOT / "docs/MESSAGING_RELIABILITY_PROTOCOL.md").read_text()
HARDWARE = (ROOT / "docs/HARDWARE_AND_SAFETY.md").read_text()


class MessagingProtocolDocumentContracts(unittest.TestCase):
    def test_normative_messaging_protocol_covers_delivery_and_ack_rules(self):
        self.assertIn("CONFIRMED_TO_N", MESSAGING_PROTOCOL)
        self.assertIn("PARTIALLY_CONFIRMED", MESSAGING_PROTOCOL)
        self.assertIn("senderBootId", MESSAGING_PROTOCOL)
        self.assertIn("duplicate cache", MESSAGING_PROTOCOL)
        self.assertIn("Retry, timing, and congestion control", MESSAGING_PROTOCOL)
        self.assertIn("Failure matrix", MESSAGING_PROTOCOL)
        self.assertIn("Verification and release gates", MESSAGING_PROTOCOL)

    def test_shared_protocol_points_to_messaging_specification(self):
        self.assertIn("MESSAGING_RELIABILITY_PROTOCOL.md", PROTOCOL)


class MessagingSourceContracts(unittest.TestCase):
    def test_fixed_message_limit_and_serial_input(self):
        self.assertIn("#define MAX_MESSAGE_CHARS             128", MESSAGING)
        self.assertIn("Serial.available()", MESSAGING)
        self.assertIn("startText(input)", MESSAGING)

    def test_discovery_and_persistent_device_id(self):
        self.assertIn("PKT_HELLO", MESSAGING)
        self.assertIn("HELLO_INTERVAL_MS", MESSAGING)
        self.assertIn("EEPROM.put(0, EEPROM_MAGIC)", MESSAGING)
        self.assertIn("EEPROM.put(2, deviceId)", MESSAGING)

    def test_automatic_ack_and_retry_configuration(self):
        self.assertIn("scheduleAck(rxPacket)", MESSAGING)
        self.assertIn("#define MAX_RETRIES", MESSAGING)
        self.assertIn("#define ACK_TIMEOUT_MS", MESSAGING)
        self.assertIn("activePeerCount()", MESSAGING)

    def test_duplicate_suppression(self):
        self.assertIn("SeenMessage seenMessages", MESSAGING)
        self.assertIn("wasMessageSeen(rxPacket.senderId, rxPacket.senderBootId, rxPacket.messageId)", MESSAGING)
        self.assertIn("ackForSenderId == deviceId", MESSAGING)


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

    def test_expected_peer_snapshot_increments_only_for_live_slots(self):
        self.assertIn("if(peers[i].used && millis()-peers[i].lastSeen<=PEER_TIMEOUT_MS) {", MESSAGING)
        self.assertIn("pendingTx.expectedCount++;\n    }\n  }", MESSAGING)
        self.assertIn("pendingTx.expectedBootIds[pendingTx.expectedCount]=peers[i].bootId;", MESSAGING)

    def test_received_packet_strings_are_null_terminated(self):
        self.assertIn("p.userName[sizeof(p.userName) - 1] = '\\0';", MESSAGING)
        self.assertIn("p.text[MAX_MESSAGE_CHARS] = '\\0';", MESSAGING)

    def test_radio_transmit_failures_are_not_silently_consumed(self):
        self.assertIn("bool transmitPacket(const RadioPacket &p)", MESSAGING)
        self.assertIn("if (!LoRa.beginPacket()) return false;", MESSAGING)
        self.assertIn("written != sizeof(p)", MESSAGING)
        self.assertIn("LoRa.endPacket() == 1", MESSAGING)
        self.assertIn("[ACK TX ERROR] Retaining ACK for retry", MESSAGING)
        self.assertIn("[TX RADIO ERROR] Attempt will be retried", MESSAGING)
        self.assertIn("static_assert(sizeof(RadioPacket) <= 255", MESSAGING)

    def test_packet_identity_and_type_are_validated(self):
        self.assertIn("p.senderId == 0", MESSAGING)
        self.assertIn("p.type != PKT_HELLO && p.type != PKT_TEXT && p.type != PKT_ACK", MESSAGING)
        self.assertIn("p.version != PROTOCOL_VERSION", MESSAGING)
        self.assertIn("p.payloadLength > MAX_MESSAGE_CHARS", MESSAGING)

    def test_embedded_nul_is_rejected_without_sending_prefix(self):
        self.assertIn("c == '\\0'", MESSAGING)
        self.assertIn("NUL byte in input", MESSAGING)
        self.assertIn("REJECTED_LOCAL", MESSAGING)

    def test_oversized_serial_line_is_discarded_until_newline(self):
        self.assertIn("discardUntilNewline", MESSAGING)
        self.assertIn("inputLength < MAX_MESSAGE_CHARS", MESSAGING)
        self.assertIn("[ERROR] Input too long; entire line discarded.", MESSAGING)

    def test_rmp_v1_wire_envelope_and_persistent_transaction_identity(self):
        self.assertIn("#define PROTOCOL_VERSION                 1", MESSAGING)
        self.assertIn("uint32_t senderBootId", MESSAGING)
        self.assertIn("uint32_t messageId", MESSAGING)
        self.assertIn("uint32_t ackForBootId", MESSAGING)
        self.assertIn("uint32_t ackForMessageId", MESSAGING)
        self.assertIn("uint8_t payloadLength", MESSAGING)
        self.assertIn("EEPROM.get(4, bootId)", MESSAGING)
        self.assertIn("EEPROM.put(4, bootId)", MESSAGING)
        self.assertIn("Message ID space exhausted", MESSAGING)

    def test_rmp_bounded_backoff_lifetime_and_terminal_outcomes(self):
        self.assertIn("#define RETRY_BACKOFF_BASE_MS", MESSAGING)
        self.assertIn("#define RETRY_BACKOFF_MAX_MS", MESSAGING)
        self.assertIn("#define MAX_TRANSACTION_LIFETIME_MS", MESSAGING)
        self.assertIn("PARTIALLY_CONFIRMED", MESSAGING)
        self.assertIn("UNCONFIRMED", MESSAGING)
        self.assertIn("NO_KNOWN_RECIPIENTS", MESSAGING)
        self.assertIn("Coalesce duplicate DATA", MESSAGING)
        self.assertIn("ACK_SLOT_MS", MESSAGING)

    def test_rmp_queue_and_peer_overflow_are_diagnostic(self):
        self.assertIn("[ACK QUEUE FULL]", MESSAGING)
        self.assertIn("[PEER TABLE FULL]", MESSAGING)
        self.assertIn("[ACK IGNORED]", MESSAGING)
        self.assertIn("ignoredAckCount", MESSAGING)
        self.assertIn("New peer not recorded", MESSAGING)

    def test_acknowledgement_is_bound_to_original_sender(self):
        self.assertIn("ack.ackForSenderId = item.targetId", MESSAGING)
        self.assertIn("ack.ackForBootId = item.targetBootId", MESSAGING)
        self.assertIn("rxPacket.ackForBootId == bootId", MESSAGING)
        self.assertIn("expectedBootIds[MAX_PEERS]", MESSAGING)
        self.assertIn("pendingTx.expectedBootIds[i] == rxPacket.senderBootId", MESSAGING)
        self.assertIn("servicePendingTx()", MESSAGING)
        self.assertNotIn("collectAcks(", MESSAGING)
        self.assertNotIn("delay((unsigned long)random", MESSAGING)

    def test_cancellation_retry_and_tombstone_contracts(self):
        self.assertIn("PKT_CANCEL_ACK = 12", CAMPUS)
        self.assertIn("CANCEL_MAX_RETRIES", CAMPUS)
        self.assertIn("CANCEL_RETRY_INTERVAL_MS", CAMPUS)
        self.assertIn("isTombstoned(p.stationId,p.requestId,p.generation)", CAMPUS)
        self.assertIn("generationIsNewer(uint8_t candidate, uint8_t reference)", CAMPUS)
        self.assertIn("(int8_t)(candidate-reference) > 0", CAMPUS)
        self.assertIn("generationIsNewer(p.generation, activeRequest.generation)", CAMPUS)
        self.assertIn("generationIsNewer(p.generation, queueItems[existing].generation)", CAMPUS)
        self.assertIn("rememberTombstone(p.stationId,p.requestId,p.generation)", CAMPUS)
        self.assertIn("removeQueuedGeneration(p.stationId, p.requestId, p.generation)", CAMPUS)
        self.assertIn("activeRequest.generation == p.generation", CAMPUS)
        self.assertIn("[CANCEL UNCONFIRMED]", CAMPUS)
        self.assertIn("PKT_CART_CANCEL_ACK = 13", CAMPUS)
        self.assertIn("PKT_COMPLETE_ACK = 14", CAMPUS)
        self.assertIn("TERMINAL_MAX_RETRIES", CAMPUS)
        self.assertIn("terminalPending", CAMPUS)
        self.assertIn("TERMINAL_CACHE_CAPACITY 4", CAMPUS)
        self.assertIn("resendTerminalAck(p)", CAMPUS)
        self.assertIn("rememberTerminal(p)", CAMPUS)
        self.assertIn("claimantIds[MAX_CLAIMANTS]", CAMPUS)
        self.assertIn("lastWinningCartId", CAMPUS)
        self.assertIn("OFFLINE_RECOVERY_CONFIRM_MS", CAMPUS)
        self.assertIn("offlineRecoveryBlocked", CAMPUS)
        self.assertIn("[OFFLINE UNCERTAIN]", CAMPUS)
        self.assertIn("[HEARTBEAT RETURNED] Safety lock remains", CAMPUS)
        self.assertIn("[OPERATOR CONFIRMED]", CAMPUS)
        self.assertIn("claimRetryDelayMs", CAMPUS)
        self.assertIn("scheduleCartReply(ack)", CAMPUS)
        self.assertIn("serviceCartReplies()", CAMPUS)
        self.assertIn("REPLY_QUEUE_SIZE 4", CAMPUS)
        self.assertNotIn("delay((unsigned long)random", CAMPUS)
        self.assertIn("p.senderId == assignedCartId", CAMPUS)
        self.assertIn("p.generation != queueItems[existing].generation) return", CAMPUS)


if __name__ == "__main__":
    unittest.main(verbosity=2)
