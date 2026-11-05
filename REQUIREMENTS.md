# Exigences

| ID | Exigence | Vérification |
|----|----------|--------------|
| REQ-001 | Le VCU passe de OFF à WAKEUP à réception de HMI_Command.WakeRequest | Test pytest |
| REQ-002 | Le VCU passe en SLEEP après 5 s sans HMI_Command | Test pytest |
| REQ-003 | Perte de BMS_Status pendant 500 ms : DTC « BMS timeout » et TorqueLimit à 0 | Test pytest |
| REQ-004 | Réponse UDS à ReadDataByIdentifier (0x22) et ReadDTCInformation (0x19) | Test pytest |