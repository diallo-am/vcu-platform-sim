# Requirements

Each requirement is verified by an automated test in `tests/test_vcu.py`.

| ID | Requirement | Verification |
|----|-------------|--------------|
| REQ-001 | The VCU leaves OFF (OFF -> WAKEUP -> RUN) when it receives a fresh `HMI_Command.WakeRequest`. Without HMI traffic it stays OFF. In RUN, `TorqueLimit` is 100 %. | `test_req001_wake_on_hmi_request` |
| REQ-002 | In RUN, the VCU goes to SLEEP (then OFF) when `SleepRequest` is set or when no `HMI_Command` is received for 5 s. | `test_req002_hmi_timeout_goes_to_sleep` |
| REQ-003 | In RUN, if `BMS_Status` is missing for 500 ms, the VCU sets `FaultActive`, forces `TorqueLimit` to 0 and records DTC `C11187` (confirmed after 300 ms). The DTC remains stored after the fault disappears and can be cleared. | `test_req003_bms_timeout_sets_fault_and_dtc` |
| REQ-004 | The VCU serves UDS `ReadDataByIdentifier` (0x22) for DIDs `F186`, `F195`, `0100`, `0101`, `0102` and `ReadDTCInformation` (0x19, sub-functions 0x01 and 0x02). Unknown DIDs return NRC 0x31. | `test_req004_read_data_and_dtc_services` |
| REQ-005 | UDS requests received on CAN ID 0x7E0 are answered on 0x7E8 using ISO-TP Single Frames. Unsupported services return NRC 0x11, malformed requests NRC 0x13. | `test_req005_uds_transport_and_error_codes` |
| REQ-006 | `ClearDiagnosticInformation` (0x14) is accepted only in the extended session (NRC 0x7F otherwise). | `test_req006_clear_dtc_requires_extended_session` |
| REQ-007 | The extended session falls back to the default session after 5 s without a request (S3 timer). | `test_req007_s3_timeout_returns_to_default_session` |