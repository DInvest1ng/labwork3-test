#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "bmp_reader.h"

struct RunResult {
  bool exited_normally = false;  // false = упала (segfault, abort, ...)
  int code = 0;
  std::string command;
};

// Запускает тестируемую программу с аргументами. stdout/stderr глушатся.
RunResult RunProgram(const std::vector<std::string>& args);

std::string DataPath(const std::string& name);

// Все *.bmp в директории, отсортированные по номеру итерации в имени
// (последняя группа цифр), чтобы "999.bmp" шёл раньше "1000.bmp".
std::vector<std::filesystem::path> ListBmps(const std::filesystem::path& dir);

// Сетка цветов картинки: grid[y][x].
using Grid = std::vector<std::vector<int>>;
Grid ToGrid(const Image& image);

// Ожидаемая сетка в количестве песчинок; всё, что > 3, считается белым.
// Ориентация оси Y в условии не задана, поэтому принимается и
// отражённая по вертикали картинка.
::testing::AssertionResult GridMatches(const Image& image, const Grid& expected);

bool HasColor(const Image& image, SandColor color);
bool AllColorsKnown(const Image& image);
uint64_t TotalSand(const Image& image);  // только для стабильного состояния
bool MirroredLeftRight(const Image& image);
bool MirroredTopBottom(const Image& image);
bool MirroredDiagonal(const Image& image);
bool SandOnEveryBorder(const Image& image);

// Базовый класс: у каждого теста своя чистая временная директория.
class ProgramTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  // Запускает -i data/<input> -o <out> <extra>, ожидает код 0.
  void RunOk(const std::string& input, const std::vector<std::string>& extra = {});
  // Ожидает, что программа завершится сама (не упадёт) с ненулевым кодом.
  void RunFails(const std::vector<std::string>& args);

  // Ровно одна картинка в out; читает её.
  void ReadSingle(Image& image);
  // Последняя по номеру картинка.
  void ReadLast(Image& image);
  void Read(const std::filesystem::path& path, Image& image);

  std::filesystem::path dir_;
  std::filesystem::path out_;
};
