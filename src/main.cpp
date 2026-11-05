#include <algorithm>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "comm/can_socket.hpp"
#include "diag/dtc_manager.hpp"
#include "diag/uds_server.hpp"
#include "state/vehicle_state.hpp"

namespace {

constexpr canid_t kIdVcuStatus = 0x100;
constexpr canid_t kIdBmsStatus = 0x200;
constexpr canid_t kIdHmiCommand = 0x210;
constexpr canid_t kIdUdsRequest = 0x7E0;
constexpr canid_t kIdUdsResponse = 0x7E8;
constexpr std::size_t kMaxSingleFrame = 7;
constexpr std::chrono::milliseconds kTxPeriod{100};

volatile std::sig_atomic_t g_running = 1;
void on_signal(int) { g_running = 0; }

// Décodage conforme à dbc/vehicle.dbc (Intel, little-endian).
BmsStatus decode_bms(const can_frame& f) {
    BmsStatus b;
    const auto raw_v = static_cast<std::uint16_t>(f.data[0] | (f.data[1] << 8));
    const auto raw_i = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(f.data[2] | (f.data[3] << 8)));
    b.voltage_v = raw_v * 0.01;
    b.current_a = raw_i * 0.1;
    b.soc = f.data[4];
    return b;
}

HmiCommand decode_hmi(const can_frame& f) {
    HmiCommand h;
    h.wake = (f.data[0] & 0x01) != 0;
    h.sleep = (f.data[0] & 0x02) != 0;
    h.throttle_pct = f.data[1] * 0.5;
    return h;
}

can_frame encode_vcu_status(const VehicleStateMachine& sm, std::uint8_t counter) {
    can_frame f{};
    f.can_id = kIdVcuStatus;
    f.can_dlc = 8;
    f.data[0] = static_cast<std::uint8_t>(
        (static_cast<std::uint8_t>(sm.state()) & 0x0F) | (sm.fault_active() ? 0x10 : 0x00));
    f.data[1] = counter & 0x0F;
    f.data[2] = sm.torque_limit_pct();
    return f;
}

// ISO-TP : Single Frame uniquement (PCI 0x0N, N = longueur de 1 à 7).
bool decode_isotp_sf(const can_frame& f, Bytes& out) {
    if (f.can_dlc < 2 || (f.data[0] & 0xF0) != 0x00) {
        return false;
    }
    const std::size_t len = f.data[0] & 0x0F;
    if (len == 0 || len > kMaxSingleFrame || len > static_cast<std::size_t>(f.can_dlc) - 1) {
        return false;
    }
    out.assign(f.data + 1, f.data + 1 + len);
    return true;
}

can_frame encode_isotp_sf(const Bytes& payload) {
    can_frame f{};
    f.can_id = kIdUdsResponse;
    f.can_dlc = 8;
    std::memset(f.data, 0xAA, sizeof(f.data));  // remplissage
    f.data[0] = static_cast<std::uint8_t>(payload.size());
    std::copy(payload.begin(), payload.end(), f.data + 1);
    return f;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string ifname = (argc > 1) ? argv[1] : "vcan0";
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    try {
        CanSocket can(ifname);
        VehicleStateMachine sm;
        DtcManager dtcs;
        UdsServer uds(dtcs, sm);
        std::uint8_t counter = 0;
        VehicleState last_state = sm.state();
        bool last_fault = false;
        std::uint8_t last_dtc_status = 0;
        std::uint8_t last_session = uds.session();
        TimePoint next_tx = Clock::now();

        std::cout << "VCU démarré sur " << ifname << " (état " << to_string(last_state) << ")\n";

        while (g_running) {
            if (auto f = can.receive(10)) {
                const TimePoint rx = Clock::now();
                const canid_t id = f->can_id & CAN_SFF_MASK;
                if (id == kIdBmsStatus && f->can_dlc >= 6) {
                    sm.on_bms(decode_bms(*f), rx);
                } else if (id == kIdHmiCommand && f->can_dlc >= 3) {
                    sm.on_hmi(decode_hmi(*f), rx);
                } else if (id == kIdUdsRequest) {
                    Bytes request;
                    if (decode_isotp_sf(*f, request)) {
                        const auto response = uds.handle(request, rx);
                        if (response && response->size() <= kMaxSingleFrame) {
                            can.send(encode_isotp_sf(*response));
                        }
                    }
                }
            }

            const TimePoint now = Clock::now();
            sm.tick(now);
            uds.tick(now);
            dtcs.report(dtc::kBmsTimeout, sm.fault_active(), now);

            if (sm.state() != last_state) {
                std::cout << to_string(last_state) << " -> " << to_string(sm.state()) << "\n";
                if (sm.state() == VehicleState::Off) {
                    dtcs.end_operation_cycle();  // fin du cycle d'opération
                }
                last_state = sm.state();
            }
            if (sm.fault_active() != last_fault) {
                last_fault = sm.fault_active();
                std::cout << (last_fault ? "DEFAUT : BMS_Status absent\n" : "Défaut BMS disparu\n");
            }

            const std::uint8_t dtc_status = dtcs.status(dtc::kBmsTimeout);
            if (dtc_status != last_dtc_status) {
                last_dtc_status = dtc_status;
                std::printf("DTC %06X : statut 0x%02X\n", dtc::kBmsTimeout, dtc_status);
            }
            if (uds.session() != last_session) {
                last_session = uds.session();
                std::printf("Session UDS : 0x%02X\n", last_session);
            }

            if (now >= next_tx) {
                can.send(encode_vcu_status(sm, counter));
                counter = (counter + 1) & 0x0F;
                next_tx = now + kTxPeriod;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Erreur : " << e.what() << "\n";
        return 1;
    }
    return 0;
}