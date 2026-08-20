// Regression tests for AY-3-8912 register width handling.
//
// Several PSG registers are narrower than the 8-bit data bus (tone period high = 4 bits, noise
// period = 5 bits, amplitude = 5 bits, envelope shape = 4 bits). Real hardware ignores the
// unimplemented high bits, and real cartridges rely on that: Bedlam writes $ff to the noise
// period register (which used to trip `assert(period < 32)` and kill the emulator), and Scramble,
// Spike, Berzerk and Solar Quest all write $ff to a tone period high byte.

#include "emulator/Psg.h"

#include <cstdio>
#include <cstdint>

namespace {
    int g_failures = 0;

    void Check(bool condition, const char* what) {
        if (!condition) {
            printf("FAIL: %s\n", what);
            ++g_failures;
        }
    }

    // The PSG latches an address and then a value, and only acts on a mode transition out of
    // Inactive, so every bus operation has to pass back through Inactive.
    void SetMode(Psg& psg, bool bdir, bool bc1) {
        psg.SetBDIR(false);
        psg.SetBC1(false);
        psg.Update(1);
        psg.SetBDIR(bdir);
        psg.SetBC1(bc1);
        psg.Update(1);
    }

    void WriteRegister(Psg& psg, uint8_t reg, uint8_t value) {
        psg.WriteDA(reg);
        SetMode(psg, true, true); // latch address
        psg.WriteDA(value);
        SetMode(psg, true, false); // write
    }

    uint8_t ReadRegister(Psg& psg, uint8_t reg) {
        psg.WriteDA(reg);
        SetMode(psg, true, true); // latch address
        SetMode(psg, false, true); // read
        return psg.ReadDA();
    }

    struct RegisterSpec {
        uint8_t index;
        uint8_t mask;
        const char* name;
    };

    // Mirrors the widths the chip actually implements. R7 (mixer) is excluded: its top two bits
    // select the I/O port directions, which the emulator explicitly reports as unsupported rather
    // than silently ignoring, so writing $ff there is not a truncation case.
    const RegisterSpec kNarrowRegisters[] = {
        {1, 0x0f, "R1 tone A period high"},  {3, 0x0f, "R3 tone B period high"},
        {5, 0x0f, "R5 tone C period high"},  {6, 0x1f, "R6 noise period"},
        {8, 0x1f, "R8 amplitude A"},         {9, 0x1f, "R9 amplitude B"},
        {10, 0x1f, "R10 amplitude C"},       {13, 0x0f, "R13 envelope shape"},
    };
} // namespace

int main() {
    // Writing $ff to every narrow register must be truncated, not asserted or stored wide. This is
    // the crash Bedlam hit on R6.
    {
        Psg psg;
        psg.Init();
        psg.Reset();
        for (const auto& spec : kNarrowRegisters) {
            WriteRegister(psg, spec.index, 0xff);
            const uint8_t readBack = ReadRegister(psg, spec.index);
            char message[128];
            snprintf(message, sizeof(message), "%s: read back 0x%02x, expected <= 0x%02x",
                     spec.name, readBack, spec.mask);
            Check((readBack & ~spec.mask) == 0, message);
        }
    }

    // A value that already fits must survive untouched, so the mask cannot be silently breaking
    // well-behaved cartridges.
    {
        Psg psg;
        psg.Init();
        psg.Reset();
        for (const auto& spec : kNarrowRegisters) {
            const uint8_t value = spec.mask; // largest legal value
            WriteRegister(psg, spec.index, value);
            const uint8_t readBack = ReadRegister(psg, spec.index);
            char message[128];
            snprintf(message, sizeof(message), "%s: legal value 0x%02x read back as 0x%02x",
                     spec.name, value, readBack);
            Check(readBack == value, message);
        }
    }

    // The full-width registers must not be masked at all.
    {
        Psg psg;
        psg.Init();
        psg.Reset();
        const uint8_t kFullWidth[] = {0, 2, 4, 11, 12};
        for (uint8_t reg : kFullWidth) {
            WriteRegister(psg, reg, 0xff);
            const uint8_t readBack = ReadRegister(psg, reg);
            char message[128];
            snprintf(message, sizeof(message), "R%u: 8-bit register read back as 0x%02x", reg,
                     readBack);
            Check(readBack == 0xff, message);
        }
    }

    if (g_failures == 0) {
        printf("psg_tests: all checks passed\n");
        return 0;
    }
    printf("psg_tests: %d check(s) failed\n", g_failures);
    return 1;
}
