#ifndef MINI_HUFFMAN_CODEC_HUFFMAN_H
#define MINI_HUFFMAN_CODEC_HUFFMAN_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace huffman {

// Maximum number of bits in one canonical Huffman code.
inline constexpr std::size_t kMaxCodeLength = 15;

// The encoded stream starts with one byte per possible input byte.  A slot is
// zero when the byte does not occur, otherwise it stores that byte's code
// length.  The canonical bit stream follows immediately.
inline constexpr std::size_t kHeaderSize = 256;

std::vector<std::uint8_t> encode(const std::vector<std::uint8_t>& data);

std::vector<std::uint8_t> decode(const std::vector<std::uint8_t>& packed,
                                 std::size_t original_size);

// Returns 256 entries indexed by byte value.
std::vector<std::uint8_t> code_lengths(const std::vector<std::uint8_t>& data);

}  // namespace huffman

#endif  // MINI_HUFFMAN_CODEC_HUFFMAN_H
