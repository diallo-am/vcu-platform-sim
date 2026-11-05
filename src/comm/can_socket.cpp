#include "comm/can_socket.hpp"

#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

CanSocket::CanSocket(const std::string& ifname) {
    fd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd_ < 0) {
        throw std::runtime_error("socket(PF_CAN) a échoué");
    }

    ifreq ifr{};
    std::strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ - 1);
    if (::ioctl(fd_, SIOCGIFINDEX, &ifr) < 0) {
        ::close(fd_);
        throw std::runtime_error("interface introuvable : " + ifname);
    }

    sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd_);
        throw std::runtime_error("bind a échoué sur " + ifname);
    }
}

CanSocket::~CanSocket() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

bool CanSocket::send(const can_frame& frame) const {
    return ::write(fd_, &frame, sizeof(frame)) == static_cast<ssize_t>(sizeof(frame));
}

std::optional<can_frame> CanSocket::receive(int timeout_ms) const {
    pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLIN;
    if (::poll(&pfd, 1, timeout_ms) <= 0) {
        return std::nullopt;
    }
    can_frame frame{};
    if (::read(fd_, &frame, sizeof(frame)) != static_cast<ssize_t>(sizeof(frame))) {
        return std::nullopt;
    }
    return frame;
}