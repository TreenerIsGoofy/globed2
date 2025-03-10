#pragma once
#include "socket.hpp"
#include <util/sync.hpp>

class UdpSocket : public Socket {
public:
    using Socket::send;
    UdpSocket();
    ~UdpSocket();

    bool create() override;
    Result<> connect(const std::string_view serverIp, unsigned short port) override;
    int send(const char* data, unsigned int dataSize) override;
    int sendTo(const char* data, unsigned int dataSize, const std::string_view address, unsigned short port);
    RecvResult receive(char* buffer, int bufferSize) override;
    bool close() override;
    virtual void disconnect();
    Result<bool> poll(int msDelay) override;

    util::sync::AtomicBool connected = false;
protected:

#ifdef GLOBED_IS_UNIX
    util::sync::AtomicI32 socket_ = 0;
#else
    util::sync::AtomicU32 socket_ = 0;
#endif

private:
    sockaddr_in destAddr_;
};
