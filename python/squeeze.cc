#include <namco/Lz0103.h>
#include <namco/Lz80.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;
using namespace squeeze;

static auto requestReadOnly(py::buffer& b) -> std::pair<const uint8_t*, size_t>
{
    py::buffer_info info = b.request();
    if (info.ndim != 1)
    {
        throw std::runtime_error{"requires a 1-dimensional buffer"};
    }
    return std::make_pair(reinterpret_cast<const uint8_t*>(info.ptr),
                          static_cast<size_t>(info.size));
}

static auto _decompress_lz80(py::buffer buffer) -> py::bytes
{
    auto const [data, size] = requestReadOnly(buffer);
    auto const decompressed = decompressLz80(data, size);
    return py::bytes{reinterpret_cast<const char*>(decompressed.data()), decompressed.size()};
}

static auto _compress_lz80(py::buffer buffer, const size_t windowSize) -> py::bytes
{
    auto const [data, size] = requestReadOnly(buffer);
    auto const compressed = compressLz80(data, size, windowSize);
    return py::bytes{reinterpret_cast<const char*>(compressed.data()), compressed.size()};
}

static auto _decompress_lz03(py::buffer buffer) -> py::bytes
{
    auto const [data, size] = requestReadOnly(buffer);
    auto const decompressed = decompressLz03(data, size);
    return py::bytes{reinterpret_cast<const char*>(decompressed.data()), decompressed.size()};
}

static auto _compress_lz03(py::buffer buffer) -> py::bytes
{
    auto const [data, size] = requestReadOnly(buffer);
    auto const compressed = compressLz03(data, size);
    return py::bytes{reinterpret_cast<const char*>(compressed.data()), compressed.size()};
}

static auto _decompress_lz01(py::buffer buffer) -> py::bytes
{
    auto const [data, size] = requestReadOnly(buffer);
    auto const decompressed = decompressLz01(data, size);
    return py::bytes{reinterpret_cast<const char*>(decompressed.data()), decompressed.size()};
}

static auto _compress_lz01(py::buffer buffer) -> py::bytes
{
    auto const [data, size] = requestReadOnly(buffer);
    auto const compressed = compressLz01(data, size);
    return py::bytes{reinterpret_cast<const char*>(compressed.data()), compressed.size()};
}

PYBIND11_MODULE(_squeeze, m)
{
    m.doc() = "Internal squeeze module";
    m.def("_decompress_lz80", &_decompress_lz80)
        .def("_compress_lz80", &_compress_lz80)
        .def("_decompress_lz01", &_decompress_lz01)
        .def("_compress_lz01", &_compress_lz01)
        .def("_decompress_lz03", &_decompress_lz03)
        .def("_compress_lz03", &_compress_lz03);
}
