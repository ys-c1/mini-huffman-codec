#include "huffman.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

namespace huffman {
namespace {

using Lengths = std::array<std::uint8_t, 256>;
using Frequencies = std::array<std::uint64_t, 256>;

struct Code {
    std::uint32_t bits = 0;
    std::uint8_t length = 0;
};

struct Node {
    std::uint64_t frequency = 0;
    int symbol = -1;
    int left = -1;
    int right = -1;
};

Lengths calculate_lengths(const std::vector<std::uint8_t>& data) {
    Lengths lengths{};
    Frequencies frequencies{};
    for (std::uint8_t value : data) {
        ++frequencies[value];
    }

    std::vector<int> active;
    active.reserve(256);
    for (int symbol = 0; symbol < 256; ++symbol) {
        if (frequencies[symbol] != 0) {
            active.push_back(symbol);
        }
    }
    if (active.empty()) {
        return lengths;
    }
    if (active.size() == 1) {
        lengths[active[0]] = 1;
        return lengths;
    }

    std::vector<Node> nodes;
    nodes.reserve(active.size() * 2 - 1);
    auto compare = [&nodes](int lhs, int rhs) {
        if (nodes[lhs].frequency != nodes[rhs].frequency) {
            return nodes[lhs].frequency > nodes[rhs].frequency;
        }
        return lhs > rhs;
    };
    std::priority_queue<int, std::vector<int>, decltype(compare)> queue(compare);

    for (int symbol : active) {
        nodes.push_back(Node{frequencies[symbol], symbol, -1, -1});
        queue.push(static_cast<int>(nodes.size()) - 1);
    }

    while (queue.size() > 1) {
        const int lhs = queue.top();
        queue.pop();
        const int rhs = queue.top();
        queue.pop();
        Node parent;
        parent.frequency = nodes[lhs].frequency + nodes[rhs].frequency;
        parent.left = lhs;
        parent.right = rhs;
        nodes.push_back(parent);
        queue.push(static_cast<int>(nodes.size()) - 1);
    }

    const int root = queue.top();
    std::vector<std::pair<int, int>> pending;
    pending.emplace_back(root, 0);
    while (!pending.empty()) {
        const auto [index, depth] = pending.back();
        pending.pop_back();
        const Node& node = nodes[index];
        if (node.left < 0 && node.right < 0) {
            lengths[node.symbol] = static_cast<std::uint8_t>(std::min(depth, 255));
        } else {
            pending.emplace_back(node.left, depth + 1);
            pending.emplace_back(node.right, depth + 1);
        }
    }

    constexpr int max_bits = static_cast<int>(kMaxCodeLength);
    bool needs_limit = false;
    for (int symbol : active) {
        if (lengths[symbol] > kMaxCodeLength) {
            needs_limit = true;
            break;
        }
    }

    if (needs_limit) {
        for (int symbol : active) {
            if (lengths[symbol] > max_bits) {
                lengths[symbol] = static_cast<std::uint8_t>(max_bits);
            }
        }
    }

    return lengths;
}

std::array<Code, 256> make_encode_codes(const Lengths& lengths) {
    std::array<std::uint32_t, 16> count{};
    for (std::uint8_t length : lengths) {
        if (length != 0) {
            ++count[length];
        }
    }

    std::array<std::uint32_t, 16> next_code{};
    std::uint32_t code = 0;
    for (int length = 1; length <= 15; ++length) {
        code = (code + count[length - 1]) << 1;
        next_code[length] = code;
    }

    std::vector<int> order;
    order.reserve(256);
    for (int symbol = 255; symbol >= 0; --symbol) {
        if (lengths[symbol] != 0) {
            order.push_back(symbol);
        }
    }
    std::stable_sort(order.begin(), order.end(), [&lengths](int lhs, int rhs) {
        return lengths[lhs] < lengths[rhs];
    });

    std::array<Code, 256> codes{};
    for (int symbol : order) {
        const std::uint8_t length = lengths[symbol];
        codes[symbol].bits = next_code[length]++;
        codes[symbol].length = length;
    }
    return codes;
}

std::unordered_map<std::uint64_t, std::uint8_t> make_decode_table(
    const Lengths& lengths) {
    std::array<std::uint32_t, 16> count{};
    for (std::uint8_t length : lengths) {
        if (length != 0) {
            ++count[length];
        }
    }

    std::array<std::uint32_t, 16> next_code{};
    std::uint32_t code = 0;
    for (int length = 1; length <= 15; ++length) {
        code = (code + count[length - 1]) << 1;
        next_code[length] = code;
    }

    std::unordered_map<std::uint64_t, std::uint8_t> table;
    table.reserve(256);
    for (int symbol = 0; symbol < 256; ++symbol) {
        const std::uint8_t length = lengths[symbol];
        if (length == 0) {
            continue;
        }
        const std::uint32_t symbol_code = next_code[length]++;
        const std::uint64_t key =
            (static_cast<std::uint64_t>(length) << 32) | symbol_code;
        table.emplace(key, static_cast<std::uint8_t>(symbol));
    }
    return table;
}

}  // namespace

std::vector<std::uint8_t> encode(const std::vector<std::uint8_t>& data) {
    if (data.empty()) {
        return {};
    }

    const Lengths lengths = calculate_lengths(data);
    const auto codes = make_encode_codes(lengths);

    std::vector<std::uint8_t> packed;
    packed.reserve(kHeaderSize + (data.size() * kMaxCodeLength + 7) / 8);
    packed.insert(packed.end(), lengths.begin(), lengths.end());

    std::uint64_t bit_buffer = 0;
    int bit_count = 0;
    for (std::uint8_t value : data) {
        const Code& code = codes[value];
        for (int bit_index = 0; bit_index < code.length; ++bit_index) {
            const std::uint64_t bit = (code.bits >> bit_index) & 1U;
            bit_buffer |= bit << bit_count;
            ++bit_count;
            if (bit_count == 8) {
                packed.push_back(static_cast<std::uint8_t>(bit_buffer & 0xFFU));
                bit_buffer = 0;
                bit_count = 0;
            }
        }
    }
    if (bit_count != 0) {
        packed.push_back(static_cast<std::uint8_t>(bit_buffer & 0xFFU));
    }
    return packed;
}

std::vector<std::uint8_t> decode(const std::vector<std::uint8_t>& packed,
                                 std::size_t original_size) {
    if (original_size == 0) {
        return {};
    }
    if (packed.size() < kHeaderSize) {
        return {};
    }

    Lengths lengths{};
    std::copy_n(packed.begin(), kHeaderSize, lengths.begin());
    for (std::uint8_t length : lengths) {
        if (length > kMaxCodeLength) {
            return {};
        }
    }

    const auto table = make_decode_table(lengths);
    std::vector<std::uint8_t> result;
    result.reserve(std::min<std::size_t>(original_size, packed.size() * 8));

    const std::size_t total_bits = packed.size() * 8;
    std::size_t bit_position = kHeaderSize * 8;
    while (result.size() < original_size) {
        std::uint32_t code = 0;
        bool found = false;
        for (int length = 1; length <= 15; ++length) {
            if (bit_position >= total_bits) {
                return {};
            }
            const std::uint8_t bit =
                static_cast<std::uint8_t>((packed[bit_position >> 3] >>
                                           (bit_position & 7U)) &
                                          1U);
            ++bit_position;
            code = (code << 1) | static_cast<std::uint32_t>(bit);

            const std::uint64_t key =
                (static_cast<std::uint64_t>(length) << 32) | code;
            const auto it = table.find(key);
            if (it != table.end()) {
                result.push_back(it->second);
                found = true;
                break;
            }
        }
        if (!found) {
            return {};
        }
    }
    return result;
}

std::vector<std::uint8_t> code_lengths(const std::vector<std::uint8_t>& data) {
    const Lengths lengths = calculate_lengths(data);
    return std::vector<std::uint8_t>(lengths.begin(), lengths.end());
}

}  // namespace huffman
