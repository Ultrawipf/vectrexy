#pragma once

// Streams the vector line list drawn to the virtual Vectrex screen out over UDP each frame, so an
// external program (e.g. a laser projector bridge) can render the same vectors. This is a
// lightweight, engine-specific wire format (see LaserOutput.cpp for the packet layout) - it is NOT
// an ILDA/lumaxnet format. Consumers are expected to convert to whatever their output device needs.

#include <SDL_net.h>
#include <cstdint>
#include <string>
#include <vector>

struct Line;

class LaserOutput {
public:
    ~LaserOutput();

    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_enabled; }

    // Applied on the next SendFrame call if changed
    void SetTarget(const std::string& host, int port);

    void SendFrame(const std::vector<Line>& lines);

    void Shutdown();

private:
    void EnsureSocket();

    bool m_enabled = false;
    std::string m_host = "127.0.0.1";
    int m_port = 12000;
    bool m_targetDirty = true;

    UDPsocket m_socket = nullptr;
    IPaddress m_address{};
    uint32_t m_frameNumber = 0;
};
