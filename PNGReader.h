#pragma once

#include <fstream>
#include <iostream>
#include <zlib.h>

class PNGReader {
public:
    std::ifstream input_file;
    uint64_t signature = 0;

    struct IHDRChunk {
        uint32_t width;       // Image width (big-endian)
        uint32_t height;      // Image height (big-endian)
        uint8_t bit_depth;     // Bit depth
        uint8_t color_type;    // Color type
        uint8_t compression_method;  // Compression method
        uint8_t filter_method;       // Filter method
        uint8_t interlace_method;    // Interlace method
    } ihdr_chunk = {};

    uint8_t* _color_buffer = nullptr;

    explicit PNGReader(const std::string& filename) : input_file(filename, std::ios::binary) {
        if (!input_file) {
            throw std::runtime_error("Error opening file");
        }

        read_ihdr_chunk();
        read_idat_chunk();

        input_file.close();
    }

    ~PNGReader() {
        delete[] _color_buffer;
    }

    uint32_t get_width() const { return ihdr_chunk.width; }
    uint32_t get_height() const { return ihdr_chunk.width; }
    uint8_t get_bit_depth() const { return ihdr_chunk.bit_depth; }
    uint8_t get_color_type() const { return ihdr_chunk.color_type; }
    uint8_t get_compression_method() const { return ihdr_chunk.compression_method; }
    uint8_t get_filter_method() const { return ihdr_chunk.filter_method; }
    uint8_t get_interlace_method() const { return ihdr_chunk.interlace_method; }
    uint8_t* get_color_buffer() const { return _color_buffer; }

    std::vector<int> get_pixel_rgba(int x, int y) const;
private:
    static uint8_t paeth_predictor(uint8_t, uint8_t, uint8_t);
    uint8_t* reconstruct_scanline(const uint8_t*) const;
    void read_ihdr_chunk();
    void read_idat_chunk();
};
