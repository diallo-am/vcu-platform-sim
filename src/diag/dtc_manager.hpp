#pragma once
#include <chrono>
#include <cstdint>
#include <map>
#include <vector>

#include "state/vehicle_state.hpp"  // Clock, TimePoint

namespace dtc {
constexpr std::uint32_t kBmsTimeout = 0xC11187;  // U0111-87 : message BMS manquant

constexpr std::uint8_t kTestFailed = 0x01;
constexpr std::uint8_t kPending = 0x04;
constexpr std::uint8_t kConfirmed = 0x08;
constexpr std::uint8_t kFailedSinceClear = 0x20;
}  // namespace dtc

struct DtcEntry {
    std::uint32_t code;
    std::uint8_t status;
};

class DtcManager {
public:
    static constexpr std::chrono::milliseconds kConfirmTime{300};

    // À appeler à chaque cycle avec le résultat courant du test.
    void report(std::uint32_t code, bool failed, TimePoint now);
    // Fin de cycle d'opération : efface le bit pending des tests qui passent.
    void end_operation_cycle();
    // ClearDiagnosticInformation (UDS 0x14).
    void clear_all();

    std::uint8_t status(std::uint32_t code) const;
    // Retourne les DTC dont (statut & masque) != 0 (UDS 0x19 0x02).
    std::vector<DtcEntry> read(std::uint8_t status_mask) const;

private:
    struct Record {
        std::uint8_t status{0};
        bool failing{false};
        TimePoint fail_since{};
    };
    std::map<std::uint32_t, Record> records_;
};