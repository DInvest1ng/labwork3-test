#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Цвета из условия. Числовое значение = количество песчинок (4 = «больше 3»).
enum SandColor {
  kUnknown = -1,
  kBlack = 0,
  kGreen = 1,
  kPurple = 2,
  kYellow = 3,
  kWhite = 4,
};

struct Image {
  int width = 0;
  int height = 0;
  int bit_count = 0;
  // Пиксели построчно сверху вниз, 0xRRGGBB.
  std::vector<uint32_t> rgb;

  uint32_t At(int x, int y) const;
  SandColor ColorAt(int x, int y) const;
};

// Читает BMP (BITMAPINFOHEADER и новее, без сжатия). Возвращает пустую строку
// при успехе или текст ошибки.
std::string ReadBmp(const std::filesystem::path& path, Image& image);

// Определяет цвет из условия по RGB. Точные оттенки в условии не заданы,
// поэтому сравнение нестрогое: (0,128,0) и (0,255,0) — оба зелёные.
SandColor Classify(uint32_t rgb);

const char* ColorName(SandColor color);
