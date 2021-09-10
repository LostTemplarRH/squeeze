#include "CLI11.h"
#include "namco/Lz80.h"
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vector>

enum class Compression
{
    NamcoLz80,
};

enum class Action
{
    Compress,
    Decompress,
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

void writeOutput(const Arguments& arguments, const std::vector<uint8_t>& data)
{
    std::ofstream output{arguments.output, std::ofstream::binary};
    output.write(reinterpret_cast<const char*>(data.data()), data.size());
    output.close();
}

void decompress(const Arguments& arguments)
{
    std::vector<uint8_t> decompressed;
    auto const input = openInput(arguments);
    switch (arguments.type)
    {
    case Compression::NamcoLz80:
        decompressed = squeeze::decompressLz80(input.data(), input.size());
        break;
    default: throw std::runtime_error{"decompression type not supported"};
    }
    writeOutput(arguments, decompressed);
}

void compress(const Arguments& arguments)
{
    std::vector<uint8_t> compressed;
    auto const input = openInput(arguments);
    switch (arguments.type)
    {
    case Compression::NamcoLz80:
        compressed = squeeze::compressLz80(input.data(), input.size());
        break;
    default: throw std::runtime_error{"compression type not supported"};
    }
    writeOutput(arguments, compressed);
}

int main(int argc, char* argv[])
{
    CLI::App app{"squeeze-cli"};

    Arguments arguments;

    auto* compressCmd = app.add_subcommand("compress", "Compresses data");
    auto* decompressCmd = app.add_subcommand("decompress", "Decompresses data");
    compressCmd->excludes(decompressCmd);
    compressCmd->final_callback([&arguments]() { arguments.action = Action::Compress; });
    decompressCmd->excludes(compressCmd);
    decompressCmd->final_callback([&arguments]() { arguments.action = Action::Decompress; });

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

    app.require_subcommand(1, 1);

    CLI11_PARSE(app, argc, argv);

    switch (arguments.action)
    {
    case Action::Decompress: decompress(arguments); break;
    case Action::Compress: compress(arguments); break;
    default: throw std::runtime_error{"action not supported"};
    }

    return EXIT_SUCCESS;
}
