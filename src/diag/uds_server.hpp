#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include "diag/dtc_manager.hpp"
#include "state/vehicle_state.hpp"

using Bytes = std::vector<std::uint8_t>;

namespace uds {
constexpr std::uint8_t kSidSessionControl = 0x10;
constexpr std::uint8_t kSidClearDtc = 0x14;
constexpr std::uint8_t kSidReadDtc = 0x19;
constexpr std::uint8_t kSidReadDataById = 0x22;
constexpr std::uint8_t kSidTesterPresent = 0x3E;
constexpr std::uint8_t kNegativeResponse = 0x7F;

constexpr std::uint8_t kNrcServiceNotSupported = 0x11;
constexpr std::uint8_t kNrcSubFunctionNotSupported = 0x12;
constexpr std::uint8_t kNrcIncorrectLength = 0x13;
constexpr std::uint8_t kNrcResponseTooLong = 0x14;
constexpr std::uint8_t kNrcRequestOutOfRange = 0x31;
constexpr std::uint8_t kNrcNotSupportedInSession = 0x7F;
}  // namespace uds

class UdsServer {
public:
    static constexpr std::chrono::seconds kS3Timeout{5};

    UdsServer(DtcManager& dtcs, const VehicleStateMachine& vehicle);

    // Traite une requête UDS (sans l'en-tête ISO-TP). Retourne la réponse,
    // ou std::nullopt s'il n'y a rien à répondre.
    std::optional<Bytes> handle(const Bytes& request, TimePoint now);
    // Gère le timer S3 (retour en session par défaut).
    void tick(TimePoint now);

    std::uint8_t session() const { return session_; }

private:
    std::optional<Bytes> session_control(const Bytes& req);
    std::optional<Bytes> tester_present(const Bytes& req);
    std::optional<Bytes> read_data(const Bytes& req) const;
    std::optional<Bytes> read_dtc(const Bytes& req) const;
    std::optional<Bytes> clear_dtc(const Bytes& req);

    DtcManager& dtcs_;
    const VehicleStateMachine& vehicle_;
    std::uint8_t session_{0x01};
    TimePoint last_request_{};
};