#pragma once

#include "Crypto.h"

#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace onepassPSI
{
    template <class T>
    concept Wire = requires(const T &t, u8 *out, const u8 *in) {
        { T::kBytes } -> std::convertible_to<std::size_t>;
        t.writeTo(out);
        { T::readFrom(in) } -> std::same_as<T>;
    };

    // Reliable, ordered byte stream between two parties. Only payload bytes are
    // counted, so the stats match the protocol's communication cost.
    class Channel
    {
    public:
        virtual ~Channel() = default;

        void send(std::span<const u8> data);
        void recv(std::span<u8> data);

        void sendU64(u64 v);
        u64 recvU64();

        template <Wire T>
        void send(const T &v)
        {
            std::array<u8, T::kBytes> buf;
            v.writeTo(buf.data());
            send(std::span<const u8>(buf));
        }

        template <Wire T>
        T recv()
        {
            std::array<u8, T::kBytes> buf;
            recv(std::span<u8>(buf));
            return T::readFrom(buf.data());
        }

        // Length-prefixed vector of fixed-size elements.
        template <Wire T>
        void sendVec(std::span<const T> v)
        {
            sendU64(v.size());
            Bytes buf(v.size() * T::kBytes);
            for (std::size_t i = 0; i < v.size(); ++i)
                v[i].writeTo(buf.data() + i * T::kBytes);
            send(std::span<const u8>(buf));
        }

        template <Wire T>
        std::vector<T> recvVec()
        {
            u64 n = recvU64();
            Bytes buf(n * T::kBytes);
            recv(std::span<u8>(buf));
            std::vector<T> v;
            v.reserve(n);
            for (u64 i = 0; i < n; ++i)
                v.push_back(T::readFrom(buf.data() + i * T::kBytes));
            return v;
        }

        u64 bytesSent() const { return mSent; }
        u64 bytesReceived() const { return mReceived; }
        // Time spent blocked in recv(), i.e. waiting for the peer.
        double recvWaitMs() const { return mRecvWaitMs; }
        void resetStats() { mSent = mReceived = 0; mRecvWaitMs = 0; }

    protected:
        virtual void write(const u8 *data, std::size_t len) = 0;
        virtual void read(u8 *data, std::size_t len) = 0;

    private:
        u64 mSent = 0;
        u64 mReceived = 0;
        double mRecvWaitMs = 0;
    };

    // In-process channel pair; each end is used from its own thread.
    class LocalChannel final : public Channel
    {
    public:
        struct Pipe;

        static std::pair<LocalChannel, LocalChannel> makePair();

    protected:
        void write(const u8 *data, std::size_t len) override;
        void read(u8 *data, std::size_t len) override;

    private:
        LocalChannel(std::shared_ptr<Pipe> out, std::shared_ptr<Pipe> in);

        std::shared_ptr<Pipe> mOut;
        std::shared_ptr<Pipe> mIn;
    };

    class TcpChannel final : public Channel
    {
    public:
        // Blocks until one peer connects.
        static TcpChannel listen(u32 port);
        // Retries until the peer is listening or the timeout expires.
        static TcpChannel connect(const std::string &host, u32 port, u32 timeoutMs = 10000);

        TcpChannel(TcpChannel &&o) noexcept;
        TcpChannel &operator=(TcpChannel &&o) noexcept;
        ~TcpChannel() override;

    protected:
        void write(const u8 *data, std::size_t len) override;
        void read(u8 *data, std::size_t len) override;

    private:
        explicit TcpChannel(int fd);

        int mFd = -1;
    };
}
