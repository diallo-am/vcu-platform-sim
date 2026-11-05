import time
import can
import cantools

db = cantools.database.load_file("dbc/vehicle.dbc")
msg = db.get_message_by_name("BMS_Status")
bus = can.Bus(interface="socketcan", channel="vcan0")

counter = 0
try:
    while True:
        data = msg.encode({
            "PackVoltage": 48.2,
            "PackCurrent": 5.0,
            "SOC": 80,
            "AliveCounter": counter,
        })
        bus.send(can.Message(arbitration_id=msg.frame_id, data=data, is_extended_id=False))
        counter = (counter + 1) % 16
        time.sleep(0.1)
except KeyboardInterrupt:
    pass
finally:
    bus.shutdown()