#include "PNGReader.h"

int main() {
    const PNGReader png("test.png");

    const auto pixel_rgba = png.get_pixel_rgba(0, 0);

    printf("RGBA(%u, %u, %u, %u)", pixel_rgba[0], pixel_rgba[1], pixel_rgba[2], pixel_rgba[3]);
    return 0;
}
