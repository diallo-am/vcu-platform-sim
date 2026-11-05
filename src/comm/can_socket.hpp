#pragma once
#include <linux/can.h>

#include <optional>
#include <string>

class CanSocket {
public:
    explicit CanSocket(const std::string& ifname);
    ~CanSocket();
    CanSocket(const CanSocket&) = delete;
    CanSocket& operator=(const CanSocket&) = delete;

    bool send(const can_frame& frame) const;
    // au plus timeout_ms ; retourne std::nullopt si rien n'arrive.
    std::optional<can_frame> receive(int timeout_ms) const;

private:
    int fd_{-1};
};