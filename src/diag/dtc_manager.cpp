#include "diag/dtc_manager.hpp"

void DtcManager::report(std::uint32_t code, bool failed, TimePoint now) {
    Record& r = records_[code];
    if (failed) {
        if (!r.failing) {
            r.failing = true;
            r.fail_since = now;
        }
        r.status |= dtc::kTestFailed | dtc::kPending | dtc::kFailedSinceClear;
        if (now - r.fail_since >= kConfirmTime) {
            r.status |= dtc::kConfirmed;
        }
    } else {
        r.failing = false;
        r.status &= static_cast<std::uint8_t>(~dtc::kTestFailed);
    }
}

void DtcManager::end_operation_cycle() {
    for (auto& [code, r] : records_) {
        if (!r.failing) {
            r.status &= static_cast<std::uint8_t>(~dtc::kPending);
        }
    }
}

void DtcManager::clear_all() {
    records_.clear();
}

std::uint8_t DtcManager::status(std::uint32_t code) const {
    const auto it = records_.find(code);
    return it == records_.end() ? 0 : it->second.status;
}

std::vector<DtcEntry> DtcManager::read(std::uint8_t status_mask) const {
    std::vector<DtcEntry> out;
    for (const auto& [code, r] : records_) {
        if ((r.status & status_mask) != 0) {
            out.push_back({code, r.status});
        }
    }
    return out;
}