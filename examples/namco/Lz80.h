#pragma once

#include <cstdint>
#include <vector>

namespace squeeze {

auto compressLz80(const uint8_t* data, const size_t size) -> std::vector<uint8_t>;
auto decompressLz80(const uint8_t* data, const size_t size) -> std::vector<uint8_t>;

} // namespace squeeze