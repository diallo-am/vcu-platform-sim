# Exigences

| ID | Exigence | Vérification |
|----|----------|--------------|
| REQ-001 | Le VCU passe de OFF à WAKEUP à réception de HMI_Command.WakeRequest | Test pytest |
| REQ-002 | Le VCU passe en SLEEP après 5 s sans HMI_Command | Test pytest |
| REQ-003 | Perte de BMS_Status pendant 500 ms : DTC « BMS timeout » et TorqueLimit à 0 | Test pytest |
| REQ-004 | Réponse UDS à ReadDataByIdentifier (0x22) et ReadDTCInformation (0x19) | Test pytest |
| REQ-005 | Le VCU répond aux requêtes UDS sur 0x7E0 par 0x7E8 (ISO-TP, Single Frame) | Test pytest |
| REQ-006 | ClearDiagnosticInformation (0x14) n'est accepté qu'en session étendue (NRC 0x7F sinon) | Test pytest |
| REQ-007 | La session étendue revient en session par défaut après 5 s sans requête (S3) | Test pytest |