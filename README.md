# mini-huffman-codec

A small, dependency-free canonical Huffman codec written in C++17.

## Files

- `src/huffman.h` / `src/huffman.cpp` — codec implementation.
- `tests/test_huffman.cpp` — deterministic self-checking test program.
- `Makefile` — build and test entry points.

## API

```cpp
std::vector<std::uint8_t> encode(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> decode(const std::vector<std::uint8_t>& packed,
                                 std::size_t original_size);
std::vector<std::uint8_t> code_lengths(const std::vector<std::uint8_t>& data);
```

`code_lengths` always returns 256 entries indexed by input byte value. A value of
zero means that byte does not occur in the input.

## Stream format

An encoded non-empty stream is:

1. A 256-byte header. Header byte `i` is the canonical code length of input byte
   value `i` (zero for unused bytes).
2. A packed bit stream. Code bits are written most-significant bit first within
   each code, and bytes are filled from bit 0 upward.

The empty input is represented by an empty byte vector. `decode` receives the
original output size separately, so the header does not repeat a length field.

## Algorithm

1. Count byte frequencies and build a Huffman tree with a min-heap.
2. Extract leaf depths as code lengths.
3. If any length exceeds 15 bits, clamp the deep leaves and repair the Kraft
   sum by lengthening low-frequency codes; then spend remaining Kraft slack on
   the most frequent codes.
4. Sort symbols by `(code length, byte value)` and assign canonical codes.
5. Pack the canonical bits. Decoding rebuilds the same table from the header and
   consumes bits in the matching order.

## Build and test

```sh
make test
```

The test target builds with `g++ -std=c++17` and runs all assertions. A run that
prints `failures=0` exits with status zero.

Clean generated binaries with:

```sh
make clean
```
