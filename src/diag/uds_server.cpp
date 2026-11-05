#include "diag/uds_server.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr std::uint8_t kSessionDefault = 0x01;
constexpr std::uint8_t kSessionExtended = 0x03;
constexpr std::uint8_t kDtcAvailabilityMask =
    dtc::kTestFailed | dtc::kPending | dtc::kConfirmed | dtc::kFailedSinceClear;
constexpr std::size_t kMaxSingleFrame = 7;
constexpr char kSwVersion[] = "0.1";

Bytes negative(std::uint8_t sid, std::uint8_t nrc) {
    return {uds::kNegativeResponse, sid, nrc};
}

}  // namespace

UdsServer::UdsServer(DtcManager& dtcs, const VehicleStateMachine& vehicle)
    : dtcs_(dtcs), vehicle_(vehicle) {}

void UdsServer::tick(TimePoint now) {
    if (session_ != kSessionDefault && (now - last_request_) >= kS3Timeout) {
        session_ = kSessionDefault;
    }
}

std::optional<Bytes> UdsServer::handle(const Bytes& req, TimePoint now) {
    if (req.empty()) {
        return std::nullopt;
    }
    tick(now);
    last_request_ = now;

    switch (req[0]) {
        case uds::kSidSessionControl: return session_control(req);
        case uds::kSidClearDtc:       return clear_dtc(req);
        case uds::kSidReadDtc:        return read_dtc(req);
        case uds::kSidReadDataById:   return read_data(req);
        case uds::kSidTesterPresent:  return tester_present(req);
        default:                      return negative(req[0], uds::kNrcServiceNotSupported);
    }
}

// 0x10 : DiagnosticSessionControl
std::optional<Bytes> UdsServer::session_control(const Bytes& req) {
    if (req.size() != 2) {
        return negative(req[0], uds::kNrcIncorrectLength);
    }
    if (req[1] != kSessionDefault && req[1] != kSessionExtended) {
        return negative(req[0], uds::kNrcSubFunctionNotSupported);
    }
    session_ = req[1];
    // P2 = 50 ms (0x0032), P2* = 5000 ms (0x01F4, unité 10 ms)
    return Bytes{0x50, session_, 0x00, 0x32, 0x01, 0xF4};
}

// 0x3E : TesterPresent
std::optional<Bytes> UdsServer::tester_present(const Bytes& req) {
    if (req.size() != 2) {
        return negative(req[0], uds::kNrcIncorrectLength);
    }
    if (req[1] == 0x80) {
        return std::nullopt;  // suppressPosRspMsgIndicationBit
    }
    if (req[1] != 0x00) {
        return negative(req[0], uds::kNrcSubFunctionNotSupported);
    }
    return Bytes{0x7E, 0x00};
}

// 0x22 : ReadDataByIdentifier (un seul DID par requête, valeurs en big-endian)
std::optional<Bytes> UdsServer::read_data(const Bytes& req) const {
    if (req.size() != 3) {
        return negative(req[0], uds::kNrcIncorrectLength);
    }
    const auto did = static_cast<std::uint16_t>((req[1] << 8) | req[2]);
    Bytes resp{0x62, req[1], req[2]};

    switch (did) {
        case 0xF186:  // session de diagnostic active
            resp.push_back(session_);
            break;
        case 0xF195:  // version logicielle
            for (std::size_t i = 0; i + 1 < sizeof(kSwVersion); ++i) {
                resp.push_back(static_cast<std::uint8_t>(kSwVersion[i]));
            }
            break;
        case 0x0100: {  // tension du pack, 0,01 V par bit
            const long raw = std::lround(vehicle_.bms().voltage_v / 0.01);
            const auto v = static_cast<std::uint16_t>(std::clamp(raw, 0L, 65535L));
            resp.push_back(static_cast<std::uint8_t>(v >> 8));
            resp.push_back(static_cast<std::uint8_t>(v & 0xFF));
            break;
        }
        case 0x0101:  // état de charge, %
            resp.push_back(vehicle_.bms().soc);
            break;
        case 0x0102:  // état véhicule
            resp.push_back(static_cast<std::uint8_t>(vehicle_.state()));
            break;
        default:
            return negative(req[0], uds::kNrcRequestOutOfRange);
    }
    return resp;
}

// 0x19 : ReadDTCInformation (0x01 : nombre de DTC, 0x02 : liste par masque)
std::optional<Bytes> UdsServer::read_dtc(const Bytes& req) const {
    if (req.size() < 2) {
        return negative(req[0], uds::kNrcIncorrectLength);
    }
    const std::uint8_t sub = req[1];
    if (sub != 0x01 && sub != 0x02) {
        return negative(req[0], uds::kNrcSubFunctionNotSupported);
    }
    if (req.size() != 3) {
        return negative(req[0], uds::kNrcIncorrectLength);
    }
    const auto entries = dtcs_.read(req[2]);

    if (sub == 0x01) {
        const auto count = static_cast<std::uint16_t>(entries.size());
        // 0x01 = format DTC ISO 14229-1
        return Bytes{0x59, 0x01, kDtcAvailabilityMask, 0x01,
                     static_cast<std::uint8_t>(count >> 8),
                     static_cast<std::uint8_t>(count & 0xFF)};
    }

    Bytes resp{0x59, 0x02, kDtcAvailabilityMask};
    for (const auto& e : entries) {
        resp.push_back(static_cast<std::uint8_t>((e.code >> 16) & 0xFF));
        resp.push_back(static_cast<std::uint8_t>((e.code >> 8) & 0xFF));
        resp.push_back(static_cast<std::uint8_t>(e.code & 0xFF));
        resp.push_back(e.status);
    }
    if (resp.size() > kMaxSingleFrame) {
        return negative(req[0], uds::kNrcResponseTooLong);  // pas de segmentation ISO-TP
    }
    return resp;
}

// 0x14 : ClearDiagnosticInformation (session étendue requise)
std::optional<Bytes> UdsServer::clear_dtc(const Bytes& req) {
    if (req.size() != 4) {
        return negative(req[0], uds::kNrcIncorrectLength);
    }
    if (session_ != kSessionExtended) {
        return negative(req[0], uds::kNrcNotSupportedInSession);
    }
    if (req[1] != 0xFF || req[2] != 0xFF || req[3] != 0xFF) {
        return negative(req[0], uds::kNrcRequestOutOfRange);  // seul « tous les DTC » est géré
    }
    dtcs_.clear_all();
    return Bytes{0x54};
}