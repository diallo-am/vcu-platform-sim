import argparse
import time

import can
import cantools


def main():
    parser = argparse.ArgumentParser(description="Nœud HMI simulé")
    parser.add_argument("--run-seconds", type=float, default=10.0)
    parser.add_argument("--channel", default="vcan0")
    args = parser.parse_args()

    db = cantools.database.load_file("dbc/vehicle.dbc")
    msg = db.get_message_by_name("HMI_Command")
    bus = can.Bus(interface="socketcan", channel=args.channel)
    counter = 0

    def send(wake, sleep, throttle):
        nonlocal counter
        data = msg.encode({
            "WakeRequest": int(wake),
            "SleepRequest": int(sleep),
            "ThrottlePct": throttle,
            "AliveCounter": counter,
        })
        bus.send(can.Message(arbitration_id=msg.frame_id, data=data, is_extended_id=False))
        counter = (counter + 1) % 16

    try:
        start = time.monotonic()
        while (elapsed := time.monotonic() - start) < args.run_seconds:
            send(True, False, 100.0 * elapsed / args.run_seconds)
            time.sleep(0.1)

        print("Demande de mise en veille")
        end = time.monotonic() + 1.0
        while time.monotonic() < end:
            send(False, True, 0.0)
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        bus.shutdown()


if __name__ == "__main__":
    main()