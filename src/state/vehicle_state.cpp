#include "state/vehicle_state.hpp"

#include <algorithm>

const char* to_string(VehicleState s) {
    switch (s) {
        case VehicleState::Off:    return "OFF";
        case VehicleState::WakeUp: return "WAKEUP";
        case VehicleState::Run:    return "RUN";
        case VehicleState::Sleep:  return "SLEEP";
    }
    return "?";
}

void VehicleStateMachine::on_hmi(const HmiCommand& cmd, TimePoint now) {
    hmi_ = cmd;
    hmi_rx_ = now;
    hmi_seen_ = true;
}

void VehicleStateMachine::on_bms(const BmsStatus& st, TimePoint now) {
    bms_ = st;
    bms_rx_ = now;
    bms_seen_ = true;
}

void VehicleStateMachine::enter(VehicleState next, TimePoint now) {
    state_ = next;
    entered_ = now;
}

void VehicleStateMachine::tick(TimePoint now) {
    // Une demande de réveil périmée ne doit jamais réveiller le véhicule.
    const bool hmi_fresh = hmi_seen_ && (now - hmi_rx_) < kWakeFreshness;

    switch (state_) {
        case VehicleState::Off:
            if (hmi_fresh && hmi_.wake && !hmi_.sleep) {
                enter(VehicleState::WakeUp, now);
            }
            break;
        case VehicleState::WakeUp:
            if (now - entered_ >= kWakeUpDuration) {
                enter(VehicleState::Run, now);
            }
            break;
        case VehicleState::Run:
            // REQ-002 : sommeil sur demande ou après 5 s sans HMI_Command.
            if (hmi_.sleep || (now - hmi_rx_) >= kHmiTimeout) {
                enter(VehicleState::Sleep, now);
            }
            break;
        case VehicleState::Sleep:
            if (now - entered_ >= kSleepDuration) {
                enter(VehicleState::Off, now);
            }
            break;
    }

    // REQ-003 : supervision du BMS en RUN, avec un délai de grâce à l'entrée.
    if (state_ == VehicleState::Run) {
        const TimePoint ref = bms_seen_ ? std::max(bms_rx_, entered_) : entered_;
        bms_timeout_ = (now - ref) >= kBmsTimeout;
    } else {
        bms_timeout_ = false;
    }
}

std::uint8_t VehicleStateMachine::torque_limit_pct() const {
    if (state_ != VehicleState::Run || bms_timeout_) {
        return 0;
    }
    return 100;
}