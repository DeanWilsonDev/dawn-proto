#include "UuidGenerator.h"

#include <array>
#include <cstdint>
#include <random>

namespace Dawn {

namespace {

// One Mersenne Twister per thread, seeded once from the OS entropy source. A PoC
// editor is single-threaded, but thread_local keeps this trivially safe regardless.
std::mt19937& Engine() {
    thread_local std::mt19937 Generator{std::random_device{}()};
    return Generator;
}

char HexDigit(unsigned Value) {
    return static_cast<char>(Value < 10 ? '0' + Value : 'a' + (Value - 10));
}

} // namespace

std::string GenerateUuidV4() {
    std::array<std::uint8_t, 16> Bytes{};
    std::uniform_int_distribution<int> ByteDist(0, 255);
    for (auto& Byte : Bytes) {
        Byte = static_cast<std::uint8_t>(ByteDist(Engine()));
    }

    // Version 4 (random): high nibble of byte 6 is 0100.
    Bytes[6] = static_cast<std::uint8_t>((Bytes[6] & 0x0F) | 0x40);
    // Variant 1 (RFC 4122): top two bits of byte 8 are 10.
    Bytes[8] = static_cast<std::uint8_t>((Bytes[8] & 0x3F) | 0x80);

    // Format as 8-4-4-4-12 lowercase hex with dashes.
    std::string Result;
    Result.reserve(36);
    for (std::size_t Index = 0; Index < Bytes.size(); ++Index) {
        if (Index == 4 || Index == 6 || Index == 8 || Index == 10) {
            Result.push_back('-');
        }
        Result.push_back(HexDigit(Bytes[Index] >> 4));
        Result.push_back(HexDigit(Bytes[Index] & 0x0F));
    }
    return Result;
}

} // namespace Dawn
