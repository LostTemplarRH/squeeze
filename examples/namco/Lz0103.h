#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace squeeze {

auto compressLz01(const uint8_t* data, const size_t size) -> std::vector<uint8_t>;
auto decompressLz01(const uint8_t* data, const size_t size) -> std::vector<uint8_t>;
auto compressLz03(const uint8_t* data, const size_t size) -> std::vector<uint8_t>;
auto decompressLz03(const uint8_t* data, const size_t size) -> std::vector<uint8_t>;

} // namespace squeeze