#include "LaserOutput.h"

#include "core/ConsoleOutput.h"
#include "core/Line.h"

#include <algorithm>
#include <cstring>

namespace {
    // Little-endian wire format (matches x86/x64, the only platforms this project targets).
    //
    // Header (12 bytes):
    //   uint32_t magic       'V' 'L' 'S' 'R'
    //   uint16_t version     = 1
    //   uint32_t frameNumber incrementing counter, lets a receiver detect dropped/out-of-order
    //                        packets (UDP has no delivery guarantee)
    //   uint16_t lineCount   number of LineRecords that follow
    //
    // Followed by lineCount LineRecords (20 bytes each):
    //   float x0, y0, x1, y1  Line endpoints in Vectrex screen space (roughly -128..128, the
    //                         256x256 "grid" the emulator draws to - see Screen.cpp)
    //   float brightness      0..1
    constexpr uint32_t Magic = 'V' | ('L' << 8) | ('S' << 16) | ('R' << 24);
    constexpr uint16_t Version = 1;

    // Keeps the packet within a single UDP datagram with headroom (65507 max payload).
    constexpr size_t MaxLinesPerPacket = 3000;

#pragma pack(push, 1)
    struct PacketHeader {
        uint32_t magic;
        uint16_t version;
        uint32_t frameNumber;
        uint16_t lineCount;
    };

    struct LineRecord {
        float x0, y0, x1, y1;
        float brightness;
    };
#pragma pack(pop)
} // namespace

LaserOutput::~LaserOutput() {
    Shutdown();
}

void LaserOutput::SetEnabled(bool enabled) {
    if (enabled && !m_enabled) {
        EnsureSocket();
    }
    m_enabled = enabled;
}

void LaserOutput::SetTarget(const std::string& host, int port) {
    if (host != m_host || port != m_port) {
        m_host = host;
        m_port = port;
        m_targetDirty = true;
    }
}

void LaserOutput::EnsureSocket() {
    if (!m_socket) {
        m_socket = SDLNet_UDP_Open(0);
        if (!m_socket) {
            Errorf("LaserOutput: failed to open UDP socket: %s\n", SDLNet_GetError());
            m_enabled = false;
            return;
        }
    }

    if (m_targetDirty) {
        if (SDLNet_ResolveHost(&m_address, m_host.c_str(), static_cast<uint16_t>(m_port)) != 0) {
            Errorf("LaserOutput: failed to resolve host %s:%d: %s\n", m_host.c_str(), m_port,
                   SDLNet_GetError());
            m_enabled = false;
            return;
        }
        m_targetDirty = false;
    }
}

void LaserOutput::SendFrame(const std::vector<Line>& lines) {
    if (!m_enabled)
        return;

    EnsureSocket();
    if (!m_socket || m_targetDirty)
        return;

    const size_t lineCount = std::min(lines.size(), MaxLinesPerPacket);

    std::vector<uint8_t> buffer(sizeof(PacketHeader) + lineCount * sizeof(LineRecord));

    PacketHeader header{};
    header.magic = Magic;
    header.version = Version;
    header.frameNumber = m_frameNumber++;
    header.lineCount = static_cast<uint16_t>(lineCount);
    std::memcpy(buffer.data(), &header, sizeof(header));

    auto* records = reinterpret_cast<LineRecord*>(buffer.data() + sizeof(header));
    for (size_t i = 0; i < lineCount; ++i) {
        const Line& line = lines[i];
        records[i] = LineRecord{line.p0.x, line.p0.y, line.p1.x, line.p1.y, line.brightness};
    }

    UDPpacket packet{};
    packet.channel = -1;
    packet.data = buffer.data();
    packet.len = static_cast<int>(buffer.size());
    packet.maxlen = packet.len;
    packet.address = m_address;

    if (SDLNet_UDP_Send(m_socket, -1, &packet) == 0) {
        Errorf("LaserOutput: SDLNet_UDP_Send failed: %s\n", SDLNet_GetError());
    }
}

void LaserOutput::Shutdown() {
    if (m_socket) {
        SDLNet_UDP_Close(m_socket);
        m_socket = nullptr;
    }
}
