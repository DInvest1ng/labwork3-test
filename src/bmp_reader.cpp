#include "bmp_reader.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

uint32_t U16(const std::vector<uint8_t>& b, size_t at) {
  return b[at] | (b[at + 1] << 8);
}

uint32_t U32(const std::vector<uint8_t>& b, size_t at) {
  return U16(b, at) | (U16(b, at + 2) << 16);
}

int32_t I32(const std::vector<uint8_t>& b, size_t at) {
  return static_cast<int32_t>(U32(b, at));
}

uint32_t Rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}

}  // namespace

uint32_t Image::At(int x, int y) const {
  return rgb.at(static_cast<size_t>(y) * width + x);
}

SandColor Image::ColorAt(int x, int y) const {
  return Classify(At(x, y));
}

std::string ReadBmp(const std::filesystem::path& path, Image& image) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return "cannot open " + path.string();
  }
  const std::vector<uint8_t> b((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
  if (b.size() < 54 || b[0] != 'B' || b[1] != 'M') {
    return "not a BMP file (no 'BM' signature or too short)";
  }
  const uint32_t data_offset = U32(b, 10);
  const uint32_t dib_size = U32(b, 14);
  if (dib_size < 40) {
    return "DIB header is smaller than BITMAPINFOHEADER (40 bytes): " + std::to_string(dib_size);
  }
  const int32_t width = I32(b, 18);
  const int32_t raw_height = I32(b, 22);
  const int bit_count = U16(b, 28);
  const uint32_t compression = U32(b, 30);
  uint32_t colors_used = U32(b, 46);

  if (width <= 0 || raw_height == 0) {
    return "bad image size " + std::to_string(width) + "x" + std::to_string(raw_height);
  }
  const bool top_down = raw_height < 0;
  const int height = top_down ? -raw_height : raw_height;

  const bool rgb_ok = compression == 0 || (compression == 3 && bit_count == 32);
  if (!rgb_ok) {
    return "unsupported compression " + std::to_string(compression);
  }
  if (bit_count != 1 && bit_count != 2 && bit_count != 4 && bit_count != 8 &&
      bit_count != 24 && bit_count != 32) {
    return "unsupported bit count " + std::to_string(bit_count);
  }

  std::vector<uint32_t> palette;
  if (bit_count <= 8) {
    if (colors_used == 0) {
      colors_used = 1u << bit_count;
    }
    const size_t palette_at = 14 + dib_size;
    if (palette_at + colors_used * 4 > b.size()) {
      return "palette goes past end of file";
    }
    for (uint32_t i = 0; i < colors_used; ++i) {
      const size_t at = palette_at + i * 4;
      palette.push_back(Rgb(b[at + 2], b[at + 1], b[at]));
    }
  }

  const size_t row_size = (static_cast<size_t>(width) * bit_count + 31) / 32 * 4;
  if (static_cast<size_t>(data_offset) + row_size * height > b.size()) {
    return "pixel data goes past end of file (wrong row padding or data offset?)";
  }

  image.width = width;
  image.height = height;
  image.bit_count = bit_count;
  image.rgb.assign(static_cast<size_t>(width) * height, 0);
  for (int y = 0; y < height; ++y) {
    const int file_row = top_down ? y : height - 1 - y;
    const size_t row_at = data_offset + file_row * row_size;
    for (int x = 0; x < width; ++x) {
      uint32_t color = 0;
      if (bit_count <= 8) {
        const size_t bit = static_cast<size_t>(x) * bit_count;
        const uint8_t byte = b[row_at + bit / 8];
        const int shift = 8 - bit_count - static_cast<int>(bit % 8);
        const uint32_t index = (byte >> shift) & ((1u << bit_count) - 1);
        if (index >= palette.size()) {
          return "pixel (" + std::to_string(x) + ", " + std::to_string(y) +
                 ") uses palette index " + std::to_string(index) + " outside the palette";
        }
        color = palette[index];
      } else {
        const size_t at = row_at + static_cast<size_t>(x) * (bit_count / 8);
        color = Rgb(b[at + 2], b[at + 1], b[at]);
      }
      image.rgb[static_cast<size_t>(y) * width + x] = color;
    }
  }
  return "";
}

SandColor Classify(uint32_t rgb) {
  const int r = (rgb >> 16) & 0xFF;
  const int g = (rgb >> 8) & 0xFF;
  const int b = rgb & 0xFF;
  const int hi = std::max({r, g, b});
  const int lo = std::min({r, g, b});
  if (hi < 64) {
    return kBlack;
  }
  if (lo > 192) {
    return kWhite;
  }
  if (r >= 128 && g >= 128 && b < std::min(r, g) - 60) {
    return kYellow;
  }
  if (g >= 96 && g > r + 50 && g > b + 50) {
    return kGreen;
  }
  if (r >= 64 && b >= 64 && g < std::min(r, b) - 40) {
    return kPurple;
  }
  return kUnknown;
}

const char* ColorName(SandColor color) {
  switch (color) {
    case kBlack:
      return "black(0)";
    case kGreen:
      return "green(1)";
    case kPurple:
      return "purple(2)";
    case kYellow:
      return "yellow(3)";
    case kWhite:
      return "white(>3)";
    default:
      return "unknown";
  }
}
