#include "CLI11.h"
#include "namco/Lz80.h"
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

enum class Compression
{
    NamcoLz80,
};

enum class Action
{
    Compress,
    Decompress,
    Verify,
};

struct Arguments
{
    Compression type;
    Action action{Action::Decompress};
    std::filesystem::path input;
    std::filesystem::path output;
};

auto openInput(const Arguments& arguments) -> std::vector<uint8_t>
{
    std::ifstream input{arguments.input, std::ifstream::binary};
    auto const inputSize = std::filesystem::file_size(arguments.input);
    std::vector<uint8_t> inputBuffer(inputSize);
    input.read(reinterpret_cast<char*>(inputBuffer.data()), inputSize);
    input.close();
    return inputBuffer;
}

void writeOutput(const std::filesystem::path& path, const std::vector<uint8_t>& data)
{
    std::ofstream output{path, std::ofstream::binary};
    output.write(reinterpret_cast<const char*>(data.data()), data.size());
    output.close();
}

void writeOutput(const Arguments& arguments, const std::vector<uint8_t>& data)
{
    writeOutput(arguments.output, data);
}

auto doDecompression(const Compression type, const std::vector<uint8_t>& compressed)
    -> std::pair<std::vector<uint8_t>, std::chrono::high_resolution_clock::duration>
{
    std::vector<uint8_t> decompressed;
    auto const start = std::chrono::high_resolution_clock::now();
    switch (type)
    {
    case Compression::NamcoLz80:
        decompressed = squeeze::decompressLz80(compressed.data(), compressed.size());
        break;
    default: throw std::runtime_error{"decompression type not supported"};
    }
    auto const end = std::chrono::high_resolution_clock::now();
    return std::make_pair(std::move(decompressed), end - start);
}

auto doCompression(const Compression type, const std::vector<uint8_t>& decompressed)
    -> std::pair<std::vector<uint8_t>, std::chrono::high_resolution_clock::duration>
{
    std::vector<uint8_t> compressed;
    auto const start = std::chrono::high_resolution_clock::now();
    switch (type)
    {
    case Compression::NamcoLz80:
        compressed = squeeze::compressLz80(decompressed.data(), decompressed.size());
        break;
    default: throw std::runtime_error{"decompression type not supported"};
    }
    auto const end = std::chrono::high_resolution_clock::now();
    return std::make_pair(std::move(compressed), end - start);
}

template <class Duration> auto formatDuration(const Duration duration) -> std::string
{
    auto const inMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    if (inMs < 2000)
    {
        return std::to_string(inMs) + " ms";
    }
    else if (inMs < 60000)
    {
        const unsigned int seconds = inMs / 1000;
        const unsigned int fraction = inMs % 1000;
        std::stringstream ss;
        ss << seconds << "." << std::setfill('0') << std::setw(3) << fraction << " s";
        return ss.str();
    }
    else
    {
        const unsigned int minutes = inMs / 60000;
        const unsigned int seconds = (inMs % 60000) / 1000;
        const unsigned int fraction = (inMs % 60000) % 1000;
        std::stringstream ss;
        ss << minutes << std::setfill('0') << std::setw(2) << seconds << std::setw(3) << fraction
           << " s";
        return ss.str();
    }
}

void decompress(const Arguments& arguments)
{
    auto const input = openInput(arguments);
    auto const [decompressed, duration] = doDecompression(arguments.type, input);
    writeOutput(arguments, decompressed);
    std::cout << "Decompressing took " << formatDuration(duration) << "\n";
}

void compress(const Arguments& arguments)
{
    auto const input = openInput(arguments);
    auto const [compressed, duration] = doCompression(arguments.type, input);
    writeOutput(arguments, compressed);
    std::cout << "Compressing took " << formatDuration(duration) << "\n";
}

void verify(const Arguments& arguments)
{
    auto const input = openInput(arguments);
    auto const [compressed, compressionDuration] = doCompression(arguments.type, input);
    auto const [decompressed, decompressionDuration] = doDecompression(arguments.type, compressed);

    std::cout << "Compressing took " << formatDuration(compressionDuration) << ";\n";
    std::cout << "Decompressing took " << formatDuration(decompressionDuration) << ".\n";

    if (!arguments.output.empty())
    {
        writeOutput(arguments.output / "compressed.lzss", compressed);
        writeOutput(arguments.output / "decompressed.lzss", decompressed);
    }

    auto const result = std::memcmp(decompressed.data(), input.data(), input.size());
    if (result != 0)
    {
        std::cerr
            << "Compressing and decompressing yielded a different result from the original!\n";
        std::exit(EXIT_FAILURE);
    }

    std::cout << "Verification successful.\n";
}

int main(int argc, char* argv[])
{
    CLI::App app{"squeeze-cli"};

    Arguments arguments;

    auto* compressCmd = app.add_subcommand("compress", "Compresses data");
    auto* decompressCmd = app.add_subcommand("decompress", "Decompresses data");
    auto* verifyCmd = app.add_subcommand("verify", "Checks compression against decompression");
    compressCmd->excludes(decompressCmd);
    compressCmd->excludes(verifyCmd);
    compressCmd->final_callback([&arguments]() { arguments.action = Action::Compress; });
    decompressCmd->excludes(compressCmd);
    decompressCmd->excludes(verifyCmd);
    decompressCmd->final_callback([&arguments]() { arguments.action = Action::Decompress; });
    verifyCmd->excludes(compressCmd);
    verifyCmd->excludes(decompressCmd);
    verifyCmd->final_callback([&arguments]() { arguments.action = Action::Verify; });

    std::map<std::string, Compression> compressions{
        {"lz80", Compression::NamcoLz80},
    };

    compressCmd->add_option("-t,--type", arguments.type, "type of compression")
        ->required()
        ->transform(CLI::CheckedTransformer(compressions, CLI::ignore_case));
    compressCmd->add_option("-o,--output", arguments.output, "PATH to output file")->required();
    compressCmd->add_option("input", arguments.input, "PATH to input file")
        ->required()
        ->check(CLI::ExistingFile);

    decompressCmd->add_option("-t,--type", arguments.type, "type of compression")
        ->required()
        ->transform(CLI::CheckedTransformer(compressions, CLI::ignore_case));
    decompressCmd->add_option("-o,--output", arguments.output, "PATH to output file")->required();
    decompressCmd->add_option("input", arguments.input, "PATH to input file")
        ->required()
        ->check(CLI::ExistingFile);

    verifyCmd->add_option("-t,--type", arguments.type, "type of compression")
        ->required()
        ->transform(CLI::CheckedTransformer(compressions, CLI::ignore_case));
    verifyCmd->add_option("-o,--output", arguments.output, "PATH to output file")->required();
    verifyCmd->add_option("input", arguments.input, "PATH to input file")
        ->required()
        ->check(CLI::ExistingFile);

    app.require_subcommand(1, 1);

    CLI11_PARSE(app, argc, argv);

    switch (arguments.action)
    {
    case Action::Decompress: decompress(arguments); break;
    case Action::Compress: compress(arguments); break;
    case Action::Verify: verify(arguments); break;
    default: throw std::runtime_error{"action not supported"};
    }

    return EXIT_SUCCESS;
}
