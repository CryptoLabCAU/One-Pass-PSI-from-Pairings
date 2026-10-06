#include "Network.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <thread>
#include <unistd.h>

namespace onepassPSI
{
    void Channel::send(std::span<const u8> data)
    {
        write(data.data(), data.size());
        mSent += data.size();
    }

    void Channel::recv(std::span<u8> data)
    {
        auto start = std::chrono::steady_clock::now();
        read(data.data(), data.size());
        mRecvWaitMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        mReceived += data.size();
    }

    void Channel::sendU64(u64 v)
    {
        std::array<u8, 8> buf;
        for (int i = 0; i < 8; ++i)
            buf[i] = static_cast<u8>(v >> (8 * i));
        send(std::span<const u8>(buf));
    }

    u64 Channel::recvU64()
    {
        std::array<u8, 8> buf;
        recv(std::span<u8>(buf));
        u64 v = 0;
        for (int i = 0; i < 8; ++i)
            v |= static_cast<u64>(buf[i]) << (8 * i);
        return v;
    }

    // ---- LocalChannel -------------------------------------------------------

    struct LocalChannel::Pipe
    {
        std::mutex mtx;
        std::condition_variable cv;
        std::deque<u8> buf;
    };

    LocalChannel::LocalChannel(std::shared_ptr<Pipe> out, std::shared_ptr<Pipe> in)
        : mOut(std::move(out)), mIn(std::move(in)) {}

    std::pair<LocalChannel, LocalChannel> LocalChannel::makePair()
    {
        auto a = std::make_shared<Pipe>();
        auto b = std::make_shared<Pipe>();
        return {LocalChannel(a, b), LocalChannel(b, a)};
    }

    void LocalChannel::write(const u8 *data, std::size_t len)
    {
        {
            std::lock_guard lock(mOut->mtx);
            mOut->buf.insert(mOut->buf.end(), data, data + len);
        }
        mOut->cv.notify_one();
    }

    void LocalChannel::read(u8 *data, std::size_t len)
    {
        std::unique_lock lock(mIn->mtx);
        mIn->cv.wait(lock, [&] { return mIn->buf.size() >= len; });
        std::copy_n(mIn->buf.begin(), len, data);
        mIn->buf.erase(mIn->buf.begin(), mIn->buf.begin() + len);
    }

    // ---- TcpChannel ---------------------------------------------------------

    namespace
    {
        [[noreturn]] void throwErrno(const char *what)
        {
            throw std::system_error(errno, std::generic_category(), what);
        }

        void setNoDelay(int fd)
        {
            int one = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        }
    }

    TcpChannel::TcpChannel(int fd) : mFd(fd) {}

    TcpChannel::TcpChannel(TcpChannel &&o) noexcept : Channel(std::move(o)), mFd(std::exchange(o.mFd, -1)) {}

    TcpChannel &TcpChannel::operator=(TcpChannel &&o) noexcept
    {
        if (this != &o)
        {
            if (mFd >= 0)
                ::close(mFd);
            Channel::operator=(std::move(o));
            mFd = std::exchange(o.mFd, -1);
        }
        return *this;
    }

    TcpChannel::~TcpChannel()
    {
        if (mFd >= 0)
            ::close(mFd);
    }

    TcpChannel TcpChannel::listen(u32 port)
    {
        int srv = ::socket(AF_INET, SOCK_STREAM, 0);
        if (srv < 0)
            throwErrno("socket");
        int one = 1;
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(static_cast<uint16_t>(port));
        if (::bind(srv, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0 || ::listen(srv, 1) < 0)
        {
            int e = errno;
            ::close(srv);
            errno = e;
            throwErrno("bind/listen");
        }

        int fd = ::accept(srv, nullptr, nullptr);
        int e = errno;
        ::close(srv);
        if (fd < 0)
        {
            errno = e;
            throwErrno("accept");
        }
        setNoDelay(fd);
        return TcpChannel(fd);
    }

    TcpChannel TcpChannel::connect(const std::string &host, u32 port, u32 timeoutMs)
    {
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (int rc = ::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res); rc != 0)
            throw std::runtime_error("getaddrinfo(" + host + "): " + gai_strerror(rc));
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> guard(res, freeaddrinfo);

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (true)
        {
            int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (fd < 0)
                throwErrno("socket");
            if (::connect(fd, res->ai_addr, res->ai_addrlen) == 0)
            {
                setNoDelay(fd);
                return TcpChannel(fd);
            }
            int e = errno;
            ::close(fd);
            if (std::chrono::steady_clock::now() >= deadline)
            {
                errno = e;
                throwErrno("connect");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void TcpChannel::write(const u8 *data, std::size_t len)
    {
        while (len > 0)
        {
            ssize_t n = ::send(mFd, data, len, MSG_NOSIGNAL);
            if (n < 0)
            {
                if (errno == EINTR)
                    continue;
                throwErrno("send");
            }
            data += n;
            len -= static_cast<std::size_t>(n);
        }
    }

    void TcpChannel::read(u8 *data, std::size_t len)
    {
        while (len > 0)
        {
            ssize_t n = ::recv(mFd, data, len, 0);
            if (n < 0)
            {
                if (errno == EINTR)
                    continue;
                throwErrno("recv");
            }
            if (n == 0)
                throw std::runtime_error("recv: connection closed by peer");
            data += n;
            len -= static_cast<std::size_t>(n);
        }
    }
}
