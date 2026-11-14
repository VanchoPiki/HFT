#include "eth/keccak.hpp"

#include <bit>
#include <cstring>

namespace hft::eth {
namespace {

static_assert(std::endian::native == std::endian::little, "Keccak lanes are loaded as little-endian");

constexpr std::size_t kRate = 136;

constexpr std::uint64_t kRoundConstants[24] = {
    0x0000000000000001, 0x0000000000008082, 0x800000000000808a, 0x8000000080008000,
    0x000000000000808b, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
    0x000000000000008a, 0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
    0x000000008000808b, 0x800000000000008b, 0x8000000000008089, 0x8000000000008003,
    0x8000000000008002, 0x8000000000000080, 0x000000000000800a, 0x800000008000000a,
    0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
};

constexpr int kRotations[24] = {1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
                                27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44};
constexpr int kPiLanes[24] = {10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
                              15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1};

void keccak_f1600(std::uint64_t st[25]) {
    std::uint64_t bc[5];
    for (int round = 0; round < 24; ++round) {
        for (int i = 0; i < 5; ++i) {
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
        }
        for (int i = 0; i < 5; ++i) {
            const std::uint64_t t = bc[(i + 4) % 5] ^ std::rotl(bc[(i + 1) % 5], 1);
            for (int j = 0; j < 25; j += 5) {
                st[j + i] ^= t;
            }
        }
        std::uint64_t t = st[1];
        for (int i = 0; i < 24; ++i) {
            const int j = kPiLanes[i];
            const std::uint64_t next = st[j];
            st[j] = std::rotl(t, kRotations[i]);
            t = next;
        }
        for (int j = 0; j < 25; j += 5) {
            for (int i = 0; i < 5; ++i) {
                bc[i] = st[j + i];
            }
            for (int i = 0; i < 5; ++i) {
                st[j + i] ^= ~bc[(i + 1) % 5] & bc[(i + 2) % 5];
            }
        }
        st[0] ^= kRoundConstants[round];
    }
}

void absorb_block(std::uint64_t st[25], const std::uint8_t* block) {
    for (std::size_t i = 0; i < kRate / 8; ++i) {
        std::uint64_t lane;
        std::memcpy(&lane, block + 8 * i, 8);
        st[i] ^= lane;
    }
    keccak_f1600(st);
}

}

Hash keccak256(std::span<const std::uint8_t> data) {
    std::uint64_t st[25] = {};
    const std::uint8_t* p = data.data();
    std::size_t len = data.size();
    while (len >= kRate) {
        absorb_block(st, p);
        p += kRate;
        len -= kRate;
    }
    std::uint8_t last[kRate] = {};
    if (len > 0) {
        std::memcpy(last, p, len);
    }
    last[len] ^= 0x01;
    last[kRate - 1] ^= 0x80;
    absorb_block(st, last);

    Hash out;
    std::memcpy(out.data(), st, out.size());
    return out;
}

Hash keccak256(std::string_view text) {
    return keccak256(std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

}
