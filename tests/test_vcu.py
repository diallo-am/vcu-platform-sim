import time

from harness import OFF, RUN, SLEEP, WAKEUP

DTC_BMS = bytes([0xC1, 0x11, 0x87])


def to_run(bench):
    bench.start_bms()
    bench.start_hmi()
    bench.wait_state(RUN, 3.0)


def test_req001_wake_on_hmi_request(bench):
    bench.start_bms()
    time.sleep(1.0)
    assert bench.fresh_status()["VehicleState"] == OFF  # sans HMI, le VCU reste éteint
    bench.start_hmi()
    bench.wait_state(WAKEUP, 2.0)
    bench.wait_state(RUN, 3.0)
    assert bench.fresh_status()["TorqueLimit"] == 100


def test_req002_hmi_timeout_goes_to_sleep(bench):
    to_run(bench)
    bench.stop_hmi()
    t0 = time.monotonic()
    bench.wait_state(SLEEP, 8.0)
    elapsed = time.monotonic() - t0
    assert 4.5 <= elapsed <= 6.0, f"veille après {elapsed:.2f} s au lieu de ~5 s"
    bench.wait_state(OFF, 3.0)


def test_req003_bms_timeout_sets_fault_and_dtc(bench):
    to_run(bench)
    bench.stop_bms()
    status = bench.wait_status(lambda s: s["FaultActive"] == 1, 1.5, "défaut BMS")
    assert status["TorqueLimit"] == 0

    time.sleep(0.5)  # confirmation du DTC (300 ms)
    assert bench.uds(b"\x19\x02\xFF") == bytes([0x59, 0x02, 0x2D]) + DTC_BMS + bytes([0x2D])

    bench.start_bms()
    bench.wait_status(lambda s: s["FaultActive"] == 0, 1.5, "fin du défaut")
    # le DTC reste confirmé, testFailed est retombé
    assert bench.uds(b"\x19\x02\xFF") == bytes([0x59, 0x02, 0x2D]) + DTC_BMS + bytes([0x2C])

    # effacement en session étendue
    assert bench.uds(b"\x10\x03")[:2] == b"\x50\x03"
    assert bench.uds(b"\x14\xFF\xFF\xFF") == b"\x54"
    assert bench.uds(b"\x19\x01\xFF") == bytes([0x59, 0x01, 0x2D, 0x01, 0x00, 0x00])


def test_req004_read_data_and_dtc_services(bench):
    bench.start_bms()
    time.sleep(0.3)
    assert bench.uds(b"\x22\xF1\x95") == b"\x62\xF1\x95" + b"0.1"
    assert bench.uds(b"\x22\x01\x00") == b"\x62\x01\x00\x12\xD4"  # 48,20 V
    assert bench.uds(b"\x22\x99\x99") == b"\x7F\x22\x31"
    assert bench.uds(b"\x19\x01\x08") == bytes([0x59, 0x01, 0x2D, 0x01, 0x00, 0x00])


def test_req005_uds_transport_and_error_codes(bench):
    assert bench.uds(b"\x3E\x00") == b"\x7E\x00"
    assert bench.uds(b"\x31\x01") == b"\x7F\x31\x11"
    assert bench.uds(b"\x22\xF1") == b"\x7F\x22\x13"


def test_req006_clear_dtc_requires_extended_session(bench):
    assert bench.uds(b"\x14\xFF\xFF\xFF") == b"\x7F\x14\x7F"
    assert bench.uds(b"\x10\x03")[:2] == b"\x50\x03"
    assert bench.uds(b"\x14\xFF\xFF\xFF") == b"\x54"


def test_req007_s3_timeout_returns_to_default_session(bench):
    assert bench.uds(b"\x10\x03")[:2] == b"\x50\x03"
    assert bench.uds(b"\x22\xF1\x86") == b"\x62\xF1\x86\x03"
    time.sleep(5.5)
    assert bench.uds(b"\x22\xF1\x86") == b"\x62\xF1\x86\x01"