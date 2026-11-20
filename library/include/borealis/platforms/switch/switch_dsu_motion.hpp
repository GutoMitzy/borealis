#pragma once

#include <switch.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace brls
{
class SwitchDsuMotionServer
{
public:
    static SwitchDsuMotionServer& instance()
    {
        static SwitchDsuMotionServer server;
        return server;
    }

    bool start()
    {
        if (m_socket >= 0)
            return true;
        m_lastError = 0;
        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket < 0 && !m_socketServiceReady)
        {
            if (R_FAILED(socketInitializeDefault()))
                return false;
            m_socketServiceReady = true;
            m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        }
        if (m_socket < 0)
        {
            m_lastError = errno;
            return false;
        }
        sockaddr_in local = {};
        local.sin_family = AF_INET;
        local.sin_port = htons(26760);
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(m_socket, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0)
        {
            m_lastError = errno;
            close(m_socket);
            m_socket = -1;
            return false;
        }
        const int flags = fcntl(m_socket, F_GETFL, 0);
        if (flags < 0 || fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            m_lastError = errno;
            close(m_socket);
            m_socket = -1;
            return false;
        }
        return true;
    }

    void stop()
    {
        if (m_socket >= 0)
            close(m_socket);
        m_socket = -1;
        for (auto& client : m_clients)
            client.active = false;
        m_present.fill(false);
    }

    bool active() const { return m_socket >= 0; }

    int subscribers() const
    {
        int count = 0;
        for (const auto& client : m_clients)
            if (client.active && Clock::now() - client.lastSeen <= std::chrono::seconds(5))
                count++;
        return count;
    }

    bool hasSubscribers() const { return subscribers() > 0; }

    bool sensorPresent(int slot) const
    {
        return slot >= 0 && slot < 2 && m_present[slot]
            && Clock::now() - m_seen[slot] < std::chrono::seconds(1);
    }

    uint32_t sentPackets() const { return m_sentPackets; }
    uint32_t receivedRequests() const { return m_receivedRequests; }
    int lastError() const { return m_lastError; }

    void poll()
    {
        if (m_socket < 0)
            return;
        for (int i = 0; i < 24; i++)
        {
            uint8_t packet[128] = {};
            sockaddr_in remote = {};
            socklen_t addressSize = sizeof(remote);
            const ssize_t count = recvfrom(m_socket, packet, sizeof(packet), 0,
                reinterpret_cast<sockaddr*>(&remote), &addressSize);
            if (count <= 0)
                break;
            if (count < 20 || std::memcmp(packet, "DSUC", 4) != 0
                || read16(packet + 4) != 1001 || read16(packet + 6) + 16 != count)
                continue;
            const uint32_t expected = read32(packet + 8);
            write32(packet + 8, 0);
            if (crc32(packet, static_cast<size_t>(count)) != expected)
                continue;
            ++m_receivedRequests;

            const uint32_t type = read32(packet + 16);
            if (type == 0x100000)
            {
                uint8_t reply[22] = {};
                write16(reply + 20, 1001);
                sendMessage(remote, reply, sizeof(reply), type);
            }
            else if (type == 0x100001 && count >= 24)
            {
                const uint32_t requested = read32(packet + 20);
                if (requested > 4 || count < 24 + requested)
                    continue;
                for (uint32_t k = 0; k < requested; k++)
                {
                    const uint8_t slot = packet[24 + k];
                    if (slot > 3)
                        continue;
                    uint8_t reply[32] = {};
                    setPadInfo(reply + 20, slot);
                    sendMessage(remote, reply, sizeof(reply), type);
                }
            }
            else if (type == 0x100002 && count >= 28)
            {
                const uint8_t flags = packet[20];
                const uint8_t slot = packet[21];
                if ((flags & ~3U) != 0 || ((flags & 1U) && slot > 3))
                    continue;
                Client* client = nullptr;
                for (auto& entry : m_clients)
                    if (entry.active && sameEndpoint(entry.address, remote))
                    {
                        client = &entry;
                        break;
                    }
                if (!client)
                    for (auto& entry : m_clients)
                        if (!entry.active || Clock::now() - entry.lastSeen > std::chrono::seconds(5))
                        {
                            client = &entry;
                            break;
                        }
                if (!client)
                    continue;
                client->address = remote;
                client->lastSeen = Clock::now();
                client->active = true;
                client->flags = flags;
                client->slot = slot;
                std::memcpy(client->mac.data(), packet + 22, 6);
            }
        }
    }

    void publish(int slot, const float accel[3], const float gyro[3])
    {
        if (m_socket < 0 || slot < 0 || slot > 1)
            return;
        if (!std::isfinite(accel[0]) || !std::isfinite(accel[1]) || !std::isfinite(accel[2])
            || !std::isfinite(gyro[0]) || !std::isfinite(gyro[1]) || !std::isfinite(gyro[2]))
            return;
        m_seen[slot] = Clock::now();
        m_present[slot] = true;
        uint8_t packet[100] = {};
        setPadInfo(packet + 20, slot);
        write32(packet + 32, ++m_sequence[slot]);
        packet[40] = packet[41] = packet[42] = packet[43] = 128;
        const uint64_t timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                Clock::now().time_since_epoch()).count());
        write64(packet + 68, timestamp);
        writeFloat(packet + 76, accel[0]);
        writeFloat(packet + 80, accel[2]);
        writeFloat(packet + 84, -accel[1]);
        writeFloat(packet + 88, gyro[0] * (312.0f / (2.0f * 3.14159265358979323846f)));
        writeFloat(packet + 92, -gyro[2] * (312.0f / (2.0f * 3.14159265358979323846f)));
        writeFloat(packet + 96, gyro[1] * (312.0f / (2.0f * 3.14159265358979323846f)));
        for (auto& client : m_clients)
        {
            if (!client.active)
                continue;
            if (Clock::now() - client.lastSeen > std::chrono::seconds(5))
            {
                client.active = false;
                continue;
            }
            bool subscribed = client.flags == 0
                || ((client.flags & 1U) && client.slot == slot);
            if (client.flags & 2U)
                subscribed = subscribed || std::memcmp(client.mac.data(), macFor(slot).data(), 6) == 0;
            if (subscribed)
                sendMessage(client.address, packet, sizeof(packet), 0x100002);
        }
    }

private:
    using Clock = std::chrono::steady_clock;
    struct Client
    {
        bool active = false;
        sockaddr_in address = {};
        Clock::time_point lastSeen = {};
        uint8_t flags = 0;
        uint8_t slot = 0;
        std::array<uint8_t, 6> mac = {};
    };
    int m_socket = -1;
    bool m_socketServiceReady = false;
    int m_lastError = 0;
    uint32_t m_receivedRequests = 0;
    uint32_t m_sentPackets = 0;
    std::array<Client, 8> m_clients = {};
    std::array<uint32_t, 2> m_sequence = {};
    std::array<bool, 2> m_present = {};
    std::array<Clock::time_point, 2> m_seen = {};

    static std::array<uint8_t, 6> macFor(int slot)
    {
        return { 0x02, 0x4d, 0x4c, 0x53, 0x00, static_cast<uint8_t>(slot + 1) };
    }

    static bool sameEndpoint(const sockaddr_in& a, const sockaddr_in& b)
    {
        return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
    }

    static uint16_t read16(const uint8_t* p)
    {
        return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    }

    static uint32_t read32(const uint8_t* p)
    {
        return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
            | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }

    static void write16(uint8_t* p, uint16_t value)
    {
        p[0] = value & 255;
        p[1] = (value >> 8) & 255;
    }

    static void write32(uint8_t* p, uint32_t value)
    {
        for (int k = 0; k < 4; k++)
            p[k] = (value >> (8 * k)) & 255;
    }

    static void write64(uint8_t* p, uint64_t value)
    {
        for (int k = 0; k < 8; k++)
            p[k] = (value >> (8 * k)) & 255;
    }

    static void writeFloat(uint8_t* p, float value)
    {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        write32(p, bits);
    }

    static uint32_t crc32(const uint8_t* p, size_t count)
    {
        uint32_t crc = 0xffffffffU;
        for (size_t i = 0; i < count; i++)
        {
            crc ^= p[i];
            for (int bit = 0; bit < 8; bit++)
                crc = (crc >> 1) ^ ((crc & 1U) ? 0xedb88320U : 0U);
        }
        return ~crc;
    }

    void setPadInfo(uint8_t* p, int slot) const
    {
        const bool present = slot < 2 && m_present[slot]
            && Clock::now() - m_seen[slot] < std::chrono::seconds(1);
        p[0] = static_cast<uint8_t>(slot);
        if (!present)
            return;
        p[1] = 2;
        p[2] = 2;
        p[3] = 2;
        const auto mac = macFor(slot);
        std::memcpy(p + 4, mac.data(), 6);
        p[10] = 5;
        p[11] = 1;
    }

    void sendMessage(const sockaddr_in& remote, uint8_t* packet, size_t size, uint32_t type)
    {
        std::memcpy(packet, "DSUS", 4);
        write16(packet + 4, 1001);
        write16(packet + 6, static_cast<uint16_t>(size - 16));
        write32(packet + 8, 0);
        write32(packet + 12, 0x4d4c5357U);
        write32(packet + 16, type);
        write32(packet + 8, crc32(packet, size));
        if (sendto(m_socket, packet, size, 0, reinterpret_cast<const sockaddr*>(&remote),
                   sizeof(remote)) == static_cast<ssize_t>(size))
            ++m_sentPackets;
        else
            m_lastError = errno;
    }
};
}
