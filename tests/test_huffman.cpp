#include "huffman.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const std::string& label) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        if (g_failures <= 30) {
            std::cerr << "FAIL: " << label << '\n';
        }
    }
}

std::vector<std::uint8_t> make_fibonacci_data() {
    std::vector<std::uint8_t> data;
    std::uint64_t a = 1;
    std::uint64_t b = 1;
    for (int symbol = 0; symbol < 20; ++symbol) {
        data.insert(data.end(), static_cast<std::size_t>(a),
                    static_cast<std::uint8_t>(symbol));
        const std::uint64_t next = a + b;
        a = b;
        b = next;
    }
    return data;
}

void check_entropy_bound(const std::vector<std::uint8_t>& data,
                         const std::string& label) {
    if (data.empty()) {
        return;
    }
    const std::vector<std::uint8_t> lengths = huffman::code_lengths(data);
    std::array<std::uint64_t, 256> counts{};
    std::uint64_t total_length = 0;
    for (std::uint8_t value : data) {
        ++counts[value];
    }
    for (int symbol = 0; symbol < 256; ++symbol) {
        total_length += counts[symbol] * lengths[symbol];
    }

    const double count = static_cast<double>(data.size());
    double entropy = 0.0;
    for (std::uint64_t frequency : counts) {
        if (frequency != 0) {
            const double probability =
                static_cast<double>(frequency) / count;
            entropy -= probability * std::log2(probability);
        }
    }
    const double average = static_cast<double>(total_length) / count;
    check(average <= entropy + 1.0 + 1e-9,
          "average code length within entropy + 1: " + label);
}

}  // namespace

int main() {
    using huffman::decode;
    using huffman::encode;

    // Empty input.
    check(encode({}).empty(), "empty input encodes to an empty stream");
    check(decode({}, 0).empty(), "empty stream decodes to empty output");
    {
        const std::vector<std::uint8_t> lengths = huffman::code_lengths({});
        const bool all_zero =
            std::all_of(lengths.begin(), lengths.end(),
                        [](std::uint8_t value) { return value == 0; });
        check(lengths.size() == 256 && all_zero,
              "empty input has 256 zero code lengths");
    }

    // One byte value repeated 1000 times.
    {
        const std::vector<std::uint8_t> data(1000, 0x5A);
        const std::vector<std::uint8_t> lengths = huffman::code_lengths(data);
        check(lengths[0x5A] == 1, "single symbol has a one-bit code");
        std::size_t used = 0;
        for (std::uint8_t length : lengths) {
            used += (length != 0) ? 1U : 0U;
        }
        check(used == 1, "single-symbol histogram has one used code");

        const std::vector<std::uint8_t> packed = encode(data);
        check(packed.size() == huffman::kHeaderSize + 125,
              "single-symbol payload is 125 bytes plus header");
        check(decode(packed, data.size()) == data,
              "single-symbol round trip");
    }

    // All 256 byte values appear exactly once.
    {
        std::vector<std::uint8_t> data(256);
        std::iota(data.begin(), data.end(), std::uint8_t{0});
        const std::vector<std::uint8_t> lengths = huffman::code_lengths(data);
        bool all_used = true;
        std::uint8_t maximum = 0;
        for (std::uint8_t length : lengths) {
            all_used = all_used && length != 0;
            maximum = std::max(maximum, length);
        }
        check(all_used, "all 256 symbols receive a code");
        check(maximum <= huffman::kMaxCodeLength,
              "all 256 codes fit within 15 bits");

        const std::vector<std::uint8_t> packed = encode(data);
        check(packed.size() == huffman::kHeaderSize + 256,
              "256 uniform symbols use 256 payload bytes");
        check(decode(packed, data.size()) == data,
              "all-256-symbols round trip");
    }

    // Skewed distribution: 90 percent 'a'.
    {
        std::vector<std::uint8_t> data;
        data.reserve(10000);
        for (int index = 0; index < 10000; ++index) {
            if (index % 10 == 0) {
                data.push_back(static_cast<std::uint8_t>('b' + (index / 10) % 8));
            } else {
                data.push_back(static_cast<std::uint8_t>('a'));
            }
        }
        const std::vector<std::uint8_t> lengths = huffman::code_lengths(data);
        check(lengths[static_cast<std::uint8_t>('a')] <= 2,
              "dominant symbol has code length at most two");
        const std::vector<std::uint8_t> packed = encode(data);
        check(decode(packed, data.size()) == data,
              "skewed distribution round trip");
    }

    // 1000 deterministic random data sets.
    {
        std::mt19937 generator(0xC0FFEEU);
        for (int case_index = 0; case_index < 1000; ++case_index) {
            const std::size_t size = 1 + generator() % 512;
            std::vector<std::uint8_t> data(size);
            const int mode = case_index % 5;
            if (mode == 0) {
                for (std::uint8_t& value : data) {
                    value = static_cast<std::uint8_t>(generator() & 0xFFU);
                }
            } else if (mode == 1) {
                const int alphabet = 2 + static_cast<int>(generator() % 15);
                for (std::uint8_t& value : data) {
                    value = static_cast<std::uint8_t>(generator() % alphabet);
                }
            } else if (mode == 2) {
                const std::uint8_t dominant =
                    static_cast<std::uint8_t>(generator() & 0xFFU);
                for (std::uint8_t& value : data) {
                    value = (generator() % 100 < 85)
                                ? dominant
                                : static_cast<std::uint8_t>(generator() & 0xFFU);
                }
            } else if (mode == 3) {
                for (std::size_t index = 0; index < size; ++index) {
                    data[index] = static_cast<std::uint8_t>(
                        (index * 73 + case_index) & 0xFFU);
                }
            } else {
                const std::uint8_t first =
                    static_cast<std::uint8_t>(generator() & 0xFFU);
                const std::uint8_t second =
                    static_cast<std::uint8_t>(generator() & 0xFFU);
                for (std::uint8_t& value : data) {
                    value = (generator() & 1U) ? first : second;
                }
            }

            const std::vector<std::uint8_t> packed = encode(data);
            const std::vector<std::uint8_t> restored =
                decode(packed, data.size());
            check(restored == data,
                  "random round trip case " + std::to_string(case_index));
        }
    }

    // Average code length bound.
    {
        std::vector<std::uint8_t> skewed;
        for (int index = 0; index < 10000; ++index) {
            skewed.push_back(index % 10 == 0
                                 ? static_cast<std::uint8_t>('b' + (index / 10) % 4)
                                 : static_cast<std::uint8_t>('a'));
        }
        check_entropy_bound(skewed, "90% 'a'");

        std::vector<std::uint8_t> binary;
        for (int index = 0; index < 8000; ++index) {
            binary.push_back((index % 10 < 7) ? 0x11 : 0xEE);
        }
        check_entropy_bound(binary, "70/30 binary");

        std::vector<std::uint8_t> geometric;
        for (int symbol = 0; symbol < 48; ++symbol) {
            const int repetitions = 1 << ((47 - symbol) / 6);
            geometric.insert(geometric.end(),
                             static_cast<std::size_t>(repetitions),
                             static_cast<std::uint8_t>(symbol));
        }
        check_entropy_bound(geometric, "geometric");

        std::vector<std::uint8_t> uniform;
        for (int repeat = 0; repeat < 64; ++repeat) {
            for (int symbol = 0; symbol < 256; ++symbol) {
                uniform.push_back(static_cast<std::uint8_t>(symbol));
            }
        }
        check_entropy_bound(uniform, "uniform 256");
    }

    // Force the unbounded tree past 15 levels.
    {
        const std::vector<std::uint8_t> data = make_fibonacci_data();
        const std::vector<std::uint8_t> lengths = huffman::code_lengths(data);
        std::uint8_t maximum = 0;
        double kraft = 0.0;
        for (std::uint8_t length : lengths) {
            maximum = std::max(maximum, length);
            if (length != 0) {
                kraft += std::ldexp(1.0, -static_cast<int>(length));
            }
        }
        check(maximum <= huffman::kMaxCodeLength,
              "Fibonacci histogram is limited to 15 bits");
        check(kraft <= 1.0 + 1e-9,
              "limited Fibonacci code satisfies Kraft's inequality");
        const std::vector<std::uint8_t> packed = encode(data);
        check(decode(packed, data.size()) == data,
              "length-limited Fibonacci round trip");
    }

    std::cout << "checks=" << g_checks << " failures=" << g_failures << '\n';
    return g_failures == 0 ? 0 : 1;
}
