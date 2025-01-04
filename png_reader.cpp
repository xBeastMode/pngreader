#include "png_reader.h"

void PNGReader::verify_signature() {
    input_file.read(reinterpret_cast<char*>(&signature), sizeof(signature));

    if (signature != 0x0A1A0A0D474E5089) {
        throw std::runtime_error("File is not a PNG file");
    }
}

uint8_t PNGReader::get_bytes_per_pixel() const {
    int channels = 0;
    switch (ihdr_chunk.color_type) {
        case 0: channels = 1; break; // Grayscale
        case 2: channels = 3; break; // RGB
        case 3: channels = 1; break; // Palette-based
        case 4: channels = 2; break; // Grayscale + Alpha
        case 6: channels = 4; break; // RGBA
        default:
            throw std::runtime_error("Unsupported PNG color type.");
    }
    return ihdr_chunk.bit_depth * channels / 8;
}

uint64_t PNGReader::get_scanline_length() const {
    return ihdr_chunk.width * get_bytes_per_pixel();
}

uint64_t PNGReader::get_reconstructed_scanline_length() const {
    return (1 + get_scanline_length()) * ihdr_chunk.height;
}

std::vector<int> PNGReader::get_pixel_rgba(const int x, const int y) const {
    const size_t index = (y * ihdr_chunk.width + x) * get_bytes_per_pixel();

    if (index >= get_reconstructed_scanline_length()) {
        return {};
    }

    return {
        _color_buffer[index],
        _color_buffer[index + 1],
        _color_buffer[index + 2],
        _color_buffer[index + 3]
    };
}

uint8_t PNGReader::paeth_predictor(const uint8_t a, const uint8_t b, const uint8_t c) {
    const int p = a + b - c;
    const int pa = abs(p - a);
    const int pb = abs(p - b);
    const int pc = abs(p - c);
    if (pa <= pb && pa <= pc) {
        return a;
    }
    if(pb <= pc) {
        return b;
    }
    return c;
}

uint8_t* PNGReader::reconstruct_scanline(const uint8_t* color_buffer) const {
    const uint8_t bytes_per_pixel = get_bytes_per_pixel();
    const uint32_t scanline_len = get_scanline_length();
    const auto reconstructed_color_buffer = new uint8_t[get_reconstructed_scanline_length()];

    auto recon_a = [&](const int i, const int j) {
        return j >= bytes_per_pixel ? reconstructed_color_buffer[i * scanline_len + j - bytes_per_pixel] : 0;
    };

    auto recon_b = [&](const int i, const int j) {
        return i > 0 ? reconstructed_color_buffer[(i - 1) * scanline_len + j] : 0;
    };

    auto recon_c = [&](const int i, const int j) {
        return i > 0 && j >= bytes_per_pixel ? reconstructed_color_buffer[(i - 1) * scanline_len + j - bytes_per_pixel] : 0;
    };

    for (int i = 0, f = 0, g = 0; i < ihdr_chunk.height; i++) {
        const uint8_t filter_type = color_buffer[f++];
        for (int j = 0; j < scanline_len; j++) {
            uint8_t reconstructed_x = 0;
            const uint8_t x = color_buffer[f++];
            switch (filter_type) {
                case 0: reconstructed_x = x; break; // None
                case 1: reconstructed_x = x + recon_a(i, j); break; // Sub
                case 2: reconstructed_x = x + recon_b(i, j); break; // Up
                case 3: reconstructed_x = x + (recon_a(i, j) + recon_b(i, j)) / 2; break; // Average
                case 4: reconstructed_x = x + paeth_predictor(recon_a(i, j), recon_b(i, j), recon_c(i, j)); break; // Paeth
                default:;
            }
            reconstructed_color_buffer[g++] = reconstructed_x & 0xff;
        }
    }

    return reconstructed_color_buffer;
}

void PNGReader::read_ihdr_chunk() {
    uint32_t chunkLength;
    input_file.read(reinterpret_cast<char*>(&chunkLength), sizeof(chunkLength));

    char chunkType[4] = {};
    input_file.read(chunkType, 4);

    if (std::string(chunkType) != "IHDR") {
        throw std::runtime_error("First chunk is not IHDR");
    }

    input_file.read(reinterpret_cast<char*>(&ihdr_chunk), sizeof(IHDRChunk));

    ihdr_chunk.width = __builtin_bswap32(ihdr_chunk.width);
    ihdr_chunk.height = __builtin_bswap32(ihdr_chunk.height);

    input_file.seekg(1, std::ios::cur);
}

void PNGReader::read_idat_chunk() {
    std::vector<uint8_t> compressed_data;
    size_t compressed_data_len = 0;

    uint32_t chunk_length;
    char chunk_type[5] = {};

    while (input_file.read(reinterpret_cast<char*>(&chunk_length), sizeof(chunk_length))) {
        chunk_length = __builtin_bswap32(chunk_length);
        input_file.read(chunk_type, 4);

        if (std::string(chunk_type) == "IDAT") {
            compressed_data.resize(compressed_data_len + chunk_length);

            input_file.read(reinterpret_cast<char*>(&compressed_data[compressed_data_len]), chunk_length);
            compressed_data_len += chunk_length;

            input_file.seekg(4, std::ios::cur);
        } else {
            input_file.seekg(chunk_length + 4, std::ios::cur);
        }
    }

    if (compressed_data_len == 0) {
        throw std::runtime_error("IDAT chunk not found");
    }

    const size_t decompressed_data_len = get_reconstructed_scanline_length();
    std::vector<uint8_t> decompressed_data(decompressed_data_len);

    z_stream zs;
    zs.avail_in = compressed_data_len;
    zs.next_in = compressed_data.data();
    zs.avail_out = decompressed_data_len;
    zs.next_out = decompressed_data.data();

    inflateInit(&zs);
    if (!inflate(&zs, 0)) {
        throw std::runtime_error("zlib decompression error");
    }
    inflateEnd(&zs);

    _color_buffer = reconstruct_scanline(decompressed_data.data());
}