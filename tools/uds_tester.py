import argparse
import sys
import time

import can

REQUEST_ID = 0x7E0
RESPONSE_ID = 0x7E8

NRC_NAMES = {
    0x11: "serviceNotSupported",
    0x12: "subFunctionNotSupported",
    0x13: "incorrectMessageLengthOrInvalidFormat",
    0x14: "responseTooLong",
    0x31: "requestOutOfRange",
    0x7F: "serviceNotSupportedInActiveSession",
}


def uds_request(bus, payload, timeout=1.0):
    """Envoie une requête en Single Frame ISO-TP et attend la réponse."""
    if not 1 <= len(payload) <= 7:
        raise ValueError("Single Frame : 1 à 7 octets")
    data = bytes([len(payload)]) + payload
    data += b"\xAA" * (8 - len(data))
    bus.send(can.Message(arbitration_id=REQUEST_ID, data=data, is_extended_id=False))

    deadline = time.monotonic() + timeout
    while (remaining := deadline - time.monotonic()) > 0:
        msg = bus.recv(remaining)
        if msg is None:
            break
        if msg.arbitration_id == RESPONSE_ID and (msg.data[0] & 0xF0) == 0x00:
            length = msg.data[0] & 0x0F
            return bytes(msg.data[1:1 + length])
    return None


def main():
    parser = argparse.ArgumentParser(description="Testeur UDS minimal (ISO-TP Single Frame)")
    parser.add_argument("payload", nargs="+", help="octets en hexadécimal, ex. : 22 F1 86")
    parser.add_argument("--channel", default="vcan0")
    args = parser.parse_args()

    try:
        payload = bytes(int(b, 16) for b in args.payload)
    except ValueError:
        sys.exit("Octets hexadécimaux invalides")

    bus = can.Bus(interface="socketcan", channel=args.channel)
    try:
        response = uds_request(bus, payload)
    finally:
        bus.shutdown()

    if response is None:
        print("Pas de réponse")
        sys.exit(1)

    print("Réponse :", response.hex(" ").upper())
    if response[0] == 0x7F and len(response) == 3:
        name = NRC_NAMES.get(response[2], "inconnu")
        print(f"Réponse négative : service 0x{response[1]:02X}, NRC 0x{response[2]:02X} ({name})")


if __name__ == "__main__":
    main()