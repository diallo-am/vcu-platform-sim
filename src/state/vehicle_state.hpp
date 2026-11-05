#pragma once
#include <chrono>
#include <cstdint>

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

enum class VehicleState : std::uint8_t { Off = 0, WakeUp = 1, Run = 2, Sleep = 3 };

struct HmiCommand {
    bool wake{false};
    bool sleep{false};
    double throttle_pct{0.0};
};

struct BmsStatus {
    double voltage_v{0.0};
    double current_a{0.0};
    std::uint8_t soc{0};
};

const char* to_string(VehicleState s);

class VehicleStateMachine {
public:
    // Paramètres temporels (REQ-001 à REQ-003).
    static constexpr std::chrono::milliseconds kWakeUpDuration{500};
    static constexpr std::chrono::milliseconds kWakeFreshness{500};
    static constexpr std::chrono::milliseconds kBmsTimeout{500};
    static constexpr std::chrono::milliseconds kSleepDuration{1000};
    static constexpr std::chrono::seconds kHmiTimeout{5};

    void on_hmi(const HmiCommand& cmd, TimePoint now);
    void on_bms(const BmsStatus& st, TimePoint now);
    void tick(TimePoint now);

    VehicleState state() const { return state_; }
    bool fault_active() const { return bms_timeout_; }
    const BmsStatus& bms() const { return bms_; }
    std::uint8_t torque_limit_pct() const;

private:
    void enter(VehicleState next, TimePoint now);

    VehicleState state_{VehicleState::Off};
    TimePoint entered_{};
    HmiCommand hmi_{};
    TimePoint hmi_rx_{};
    bool hmi_seen_{false};
    BmsStatus bms_{};
    TimePoint bms_rx_{};
    bool bms_seen_{false};
    bool bms_timeout_{false};
};