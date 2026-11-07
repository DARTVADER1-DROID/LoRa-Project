"""Deterministic fault-injection tests for the LoRa reliability contracts.

These tests are an executable reference model. They do not emulate RF physics or
replace Arduino hardware-in-the-loop testing; source-contract tests tie key
invariants to the deployed sketches.
"""
import unittest


def generation_newer(candidate, reference):
    delta = (candidate - reference) & 0xFF
    return 0 < delta < 128


class Tombstones:
    def __init__(self, capacity=3):
        self.capacity = capacity
        self.entries = {}

    def remember(self, key, generation):
        old = self.entries.get(key)
        if old is None or generation_newer(generation, old):
            self.entries[key] = generation
        if len(self.entries) > self.capacity:
            self.entries.pop(next(iter(self.entries)))

    def blocks(self, key, generation):
        old = self.entries.get(key)
        return old is not None and not generation_newer(generation, old)


class MessageReceiver:
    def __init__(self, cache_size=12):
        self.cache_size = cache_size
        self.seen = []
        self.displayed = []
        self.acks = []

    def receive(self, sender, boot, message_id, payload, output_available=True):
        key = (sender, boot, message_id)
        if not output_available:
            return False
        if key not in self.seen:
            self.displayed.append((key, payload))
            self.seen.append(key)
            self.seen = self.seen[-self.cache_size:]
        self.acks.append(key)
        return True


class MessageSender:
    def __init__(self, sender_id, boot_id, message_id, peers, max_retries=3):
        self.identity = (sender_id, boot_id, message_id)
        self.expected = frozenset(peers)
        self.confirmed = set()
        self.max_attempts = max_retries + 1
        self.attempts = 0
        self.done = False

    def ack(self, receiver_id, original_sender, original_boot, message_id):
        if self.done or (original_sender, original_boot, message_id) != self.identity:
            return False
        if receiver_id not in self.expected:
            return False
        self.confirmed.add(receiver_id)
        if self.expected and self.confirmed == set(self.expected):
            self.done = True
        return True

    def retry(self):
        if self.done:
            return False
        if self.attempts >= self.max_attempts:
            self.done = True
            return False
        self.attempts += 1
        return True

    @property
    def status(self):
        if self.done and self.expected and self.confirmed == set(self.expected):
            return "CONFIRMED"
        if self.done:
            return "PARTIAL_OR_UNCONFIRMED"
        return "PENDING"


class CartQueue:
    def __init__(self, capacity=3):
        self.capacity = capacity
        self.items = {}
        self.active = None
        self.tombstones = Tombstones(capacity=12)

    def request(self, key, generation):
        if self.tombstones.blocks(key, generation):
            return "STALE"
        if self.active and self.active[0] == key:
            if generation_newer(generation, self.active[1]):
                self.active = None
            elif generation == self.active[1]:
                return "ACTIVE_DUPLICATE"
            else:
                return "STALE"
        if key in self.items:
            old = self.items[key]
            if generation_newer(generation, old):
                self.items[key] = generation
                return "UPDATED"
            if generation == old:
                return "DUPLICATE"
            return "STALE"
        if len(self.items) >= self.capacity:
            return "FULL"
        self.items[key] = generation
        return "QUEUED"

    def assign(self, key, generation, cart_id):
        if self.active == (key, generation, cart_id):
            return "ACK_DUPLICATE"
        if self.active is not None:
            return "BUSY"
        if self.items.get(key) != generation:
            return "REJECT"
        del self.items[key]
        self.active = (key, generation, cart_id)
        return "ACCEPTED"

    def terminal(self, key, generation):
        self.tombstones.remember(key, generation)
        if self.active and self.active[0] == key and self.active[1] == generation:
            self.active = None
        if self.items.get(key) == generation:
            del self.items[key]


class Dispatcher:
    def __init__(self):
        self.claims = set()
        self.last_winner = 0
        self.assignment = None
        self.assignment_confirmed = False

    def claim(self, cart_id):
        self.claims.add(cart_id)

    def select(self):
        if not self.claims:
            return None
        after = [x for x in self.claims if x > self.last_winner]
        winner = min(after) if after else min(self.claims)
        self.last_winner = winner
        self.assignment = winner
        self.assignment_confirmed = False
        self.claims.clear()
        return winner

    def assign_ack(self, cart_id):
        if cart_id != self.assignment:
            return False
        self.assignment_confirmed = True
        return True


class ReliabilitySimulationTests(unittest.TestCase):
    def test_generation_wrap_forward(self):
        self.assertTrue(generation_newer(0, 255))

    def test_generation_wrap_reverse(self):
        self.assertFalse(generation_newer(255, 0))

    def test_generation_equal_is_not_newer(self):
        self.assertFalse(generation_newer(42, 42))

    def test_generation_half_range_is_ambiguous_and_rejected(self):
        self.assertFalse(generation_newer(128, 0))
        self.assertFalse(generation_newer(0, 128))

    def test_tombstone_blocks_same_generation(self):
        t = Tombstones()
        t.remember(("s", 7), 4)
        self.assertTrue(t.blocks(("s", 7), 4))

    def test_tombstone_blocks_older_generation(self):
        t = Tombstones()
        t.remember(("s", 7), 4)
        self.assertTrue(t.blocks(("s", 7), 3))

    def test_tombstone_allows_newer_generation(self):
        t = Tombstones()
        t.remember(("s", 7), 4)
        self.assertFalse(t.blocks(("s", 7), 5))

    def test_tombstone_keeps_highest_generation(self):
        t = Tombstones()
        key = ("s", 7)
        t.remember(key, 9)
        t.remember(key, 8)
        self.assertEqual(t.entries[key], 9)

    def test_message_duplicate_is_displayed_once_but_reacked(self):
        r = MessageReceiver()
        r.receive(1, 10, 2, "hello")
        r.receive(1, 10, 2, "hello")
        self.assertEqual(len(r.displayed), 1)
        self.assertEqual(len(r.acks), 2)

    def test_message_id_reuse_after_reboot_is_distinct(self):
        r = MessageReceiver()
        r.receive(1, 10, 2, "old")
        r.receive(1, 11, 2, "new")
        self.assertEqual(len(r.displayed), 2)

    def test_output_rejection_is_not_acknowledged(self):
        r = MessageReceiver()
        self.assertFalse(r.receive(1, 10, 2, "full", output_available=False))
        self.assertEqual(r.acks, [])

    def test_sender_ignores_wrong_original_sender(self):
        s = MessageSender(1, 99, 5, [2])
        self.assertFalse(s.ack(2, 7, 99, 5))

    def test_sender_ignores_wrong_boot_nonce(self):
        s = MessageSender(1, 99, 5, [2])
        self.assertFalse(s.ack(2, 1, 98, 5))

    def test_sender_ignores_wrong_message_id(self):
        s = MessageSender(1, 99, 5, [2])
        self.assertFalse(s.ack(2, 1, 99, 6))

    def test_sender_ignores_unexpected_peer(self):
        s = MessageSender(1, 99, 5, [2])
        self.assertFalse(s.ack(3, 1, 99, 5))

    def test_sender_counts_each_expected_peer_once(self):
        s = MessageSender(1, 99, 5, [2, 3])
        s.ack(2, 1, 99, 5)
        s.ack(2, 1, 99, 5)
        self.assertEqual(s.confirmed, {2})

    def test_sender_completes_only_when_snapshot_acked(self):
        s = MessageSender(1, 99, 5, [2, 3])
        s.ack(2, 1, 99, 5)
        self.assertFalse(s.done)
        s.ack(3, 1, 99, 5)
        self.assertTrue(s.done)
        self.assertEqual(s.status, "CONFIRMED")

    def test_sender_peer_snapshot_does_not_expand(self):
        s = MessageSender(1, 99, 5, [2])
        self.assertFalse(s.ack(3, 1, 99, 5))
        self.assertEqual(s.expected, frozenset({2}))

    def test_retry_budget_is_bounded(self):
        s = MessageSender(1, 99, 5, [2], max_retries=3)
        self.assertEqual([s.retry() for _ in range(5)], [True, True, True, True, False])
        self.assertTrue(s.done)

    def test_lost_ack_recovers_on_duplicate_delivery(self):
        r = MessageReceiver()
        r.receive(2, 44, 9, "payload")  # first ACK is lost in the channel
        r.receive(2, 44, 9, "payload")  # retry causes another ACK
        self.assertEqual(len(r.displayed), 1)
        self.assertEqual(len(r.acks), 2)

    def test_queue_duplicate_does_not_duplicate_entry(self):
        q = CartQueue()
        key = ("station", 12)
        self.assertEqual(q.request(key, 1), "QUEUED")
        self.assertEqual(q.request(key, 1), "DUPLICATE")
        self.assertEqual(len(q.items), 1)

    def test_queue_full_does_not_overwrite(self):
        q = CartQueue(capacity=1)
        q.request(("s", 1), 1)
        self.assertEqual(q.request(("s", 2), 1), "FULL")
        self.assertEqual(set(q.items), {("s", 1)})

    def test_stale_request_after_terminal_is_rejected(self):
        q = CartQueue()
        key = ("s", 1)
        q.request(key, 3)
        q.terminal(key, 3)
        self.assertEqual(q.request(key, 3), "STALE")
        self.assertEqual(q.request(key, 2), "STALE")

    def test_new_generation_can_requeue_after_cart_cancel(self):
        q = CartQueue()
        key = ("s", 1)
        q.request(key, 3)
        q.terminal(key, 3)
        self.assertEqual(q.request(key, 4), "QUEUED")

    def test_assignment_is_explicit_and_idempotent(self):
        q = CartQueue()
        key = ("s", 1)
        q.request(key, 1)
        self.assertEqual(q.assign(key, 1, 7), "ACCEPTED")
        self.assertEqual(q.assign(key, 1, 7), "ACK_DUPLICATE")

    def test_assignment_rejects_wrong_generation(self):
        q = CartQueue()
        key = ("s", 1)
        q.request(key, 2)
        self.assertEqual(q.assign(key, 1, 7), "REJECT")

    def test_assignment_rejects_second_active_job(self):
        q = CartQueue()
        q.request(("s", 1), 1)
        q.request(("s", 2), 1)
        q.assign(("s", 1), 1, 7)
        self.assertEqual(q.assign(("s", 2), 1, 7), "BUSY")

    def test_claim_arbitration_is_deterministic(self):
        d = Dispatcher()
        for cart in [9, 2, 5, 2]:
            d.claim(cart)
        self.assertEqual(d.select(), 2)

    def test_claim_arbitration_rotates_fairly(self):
        d = Dispatcher()
        for cart in [1, 2, 3]:
            d.claim(cart)
        self.assertEqual(d.select(), 1)
        for cart in [1, 2, 3]:
            d.claim(cart)
        self.assertEqual(d.select(), 2)
        for cart in [1, 2, 3]:
            d.claim(cart)
        self.assertEqual(d.select(), 3)

    def test_claim_arbitration_empty_window_has_no_winner(self):
        self.assertIsNone(Dispatcher().select())

    def test_assignment_not_confirmed_by_transmission_alone(self):
        d = Dispatcher()
        d.claim(4)
        d.select()
        self.assertFalse(d.assignment_confirmed)

    def test_wrong_cart_assignment_ack_is_ignored(self):
        d = Dispatcher()
        d.claim(4)
        d.select()
        self.assertFalse(d.assign_ack(5))
        self.assertFalse(d.assignment_confirmed)

    def test_matching_cart_assignment_ack_confirms(self):
        d = Dispatcher()
        d.claim(4)
        d.select()
        self.assertTrue(d.assign_ack(4))
        self.assertTrue(d.assignment_confirmed)


if __name__ == "__main__":
    unittest.main(verbosity=2)
