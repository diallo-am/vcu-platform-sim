import pathlib
import time

import can
import cantools

ROOT = pathlib.Path(__file__).resolve().parent.parent
VCU_BIN = ROOT / "build" / "vcu"
DBC_PATH = ROOT / "dbc" / "vehicle.dbc"
CHANNEL = "vcan0"

OFF, WAKEUP, RUN, SLEEP = 0, 1, 2, 3
ID_VCU_STATUS = 0x100
ID_UDS_REQUEST = 0x7E0
ID_UDS_RESPONSE = 0x7E8


def channel_is_quiet(channel=CHANNEL, duration=0.4):
    """Vrai si aucun trafic CAN n'est présent (ni VCU ni nœud lancé à la main)."""
    with can.Bus(interface="socketcan", channel=channel) as probe:
        return probe.recv(duration) is None


class Harness:
    """Banc de test : simule BMS et HMI, lit VCU_Status, envoie des requêtes UDS."""

    def __init__(self, channel=CHANNEL):
        self.db = cantools.database.load_file(str(DBC_PATH))
        self._bms = self.db.get_message_by_name("BMS_Status")
        self._hmi = self.db.get_message_by_name("HMI_Command")
        self._vcu = self.db.get_message_by_name("VCU_Status")
        self.bus = can.Bus(
            interface="socketcan",
            channel=channel,
            can_filters=[
                {"can_id": ID_VCU_STATUS, "can_mask": 0x7FF, "extended": False},
                {"can_id": ID_UDS_RESPONSE, "can_mask": 0x7FF, "extended": False},
            ],
        )
        self._tasks = {}
        self.status = None

    # --- émission périodique (100 ms) ---------------------------------
    def _start_periodic(self, name, message, data):
        self._stop_periodic(name)
        frame = can.Message(arbitration_id=message.frame_id, data=data, is_extended_id=False)
        self._tasks[name] = self.bus.send_periodic(frame, 0.1)

    def _stop_periodic(self, name):
        task = self._tasks.pop(name, None)
        if task is not None:
            task.stop()

    def start_bms(self, voltage=48.2, current=5.0, soc=80):
        data = self._bms.encode({
            "PackVoltage": voltage, "PackCurrent": current, "SOC": soc, "AliveCounter": 0,
        })
        self._start_periodic("bms", self._bms, data)

    def stop_bms(self):
        self._stop_periodic("bms")

    def start_hmi(self, wake=True, sleep=False, throttle=0.0):
        data = self._hmi.encode({
            "WakeRequest": int(wake), "SleepRequest": int(sleep),
            "ThrottlePct": throttle, "AliveCounter": 0,
        })
        self._start_periodic("hmi", self._hmi, data)

    def stop_hmi(self):
        self._stop_periodic("hmi")

    # --- réception -----------------------------------------------------
    def drain(self):
        while self.bus.recv(0) is not None:
            pass

    def _recv_until(self, predicate, timeout):
        deadline = time.monotonic() + timeout
        while (remaining := deadline - time.monotonic()) > 0:
            msg = self.bus.recv(remaining)
            if msg is None:
                break
            if msg.arbitration_id == ID_VCU_STATUS:
                self.status = self._vcu.decode(msg.data, decode_choices=False)
            if predicate(msg):
                return msg
        return None

    def wait_status(self, condition, timeout, what):
        """Attend qu'un VCU_Status récent vérifie la condition."""
        self.drain()
        deadline = time.monotonic() + timeout
        while (remaining := deadline - time.monotonic()) > 0:
            msg = self._recv_until(lambda m: m.arbitration_id == ID_VCU_STATUS, remaining)
            if msg is not None and condition(self.status):
                return self.status
        raise AssertionError(f"Délai dépassé : {what} (dernier statut : {self.status})")

    def wait_state(self, state, timeout):
        return self.wait_status(lambda s: s["VehicleState"] == state, timeout, f"état {state}")

    def fresh_status(self, timeout=1.0):
        self.drain()
        msg = self._recv_until(lambda m: m.arbitration_id == ID_VCU_STATUS, timeout)
        assert msg is not None, "VCU_Status absent"
        return self.status

    def sees_status(self, timeout):
        return self._recv_until(lambda m: m.arbitration_id == ID_VCU_STATUS, timeout) is not None

    # --- UDS (ISO-TP Single Frame) ---------------------------------------
    def uds(self, payload, timeout=1.0):
        assert 1 <= len(payload) <= 7
        data = bytes([len(payload)]) + payload
        data += b"\xAA" * (8 - len(data))
        self.drain()
        self.bus.send(can.Message(arbitration_id=ID_UDS_REQUEST, data=data, is_extended_id=False))
        msg = self._recv_until(lambda m: m.arbitration_id == ID_UDS_RESPONSE, timeout)
        assert msg is not None, f"Pas de réponse UDS à {payload.hex(' ')}"
        length = msg.data[0] & 0x0F
        return bytes(msg.data[1:1 + length])

    def close(self):
        for name in list(self._tasks):
            self._stop_periodic(name)
        self.bus.shutdown()