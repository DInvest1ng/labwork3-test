#include "runner.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace {

std::string Quote(const std::string& s) {
  return "\"" + s + "\"";
}

std::string BinaryPath() {
  const char* env = std::getenv("LABWORK3_BIN");
  if (env != nullptr && env[0] != '\0') {
    return env;
  }
  return LABWORK3_BIN;
}

long long IterationNumber(const std::filesystem::path& path) {
  const std::string name = path.stem().string();
  long long number = -1;
  size_t i = name.size();
  while (i > 0 && !std::isdigit(static_cast<unsigned char>(name[i - 1]))) {
    --i;
  }
  size_t end = i;
  while (i > 0 && std::isdigit(static_cast<unsigned char>(name[i - 1]))) {
    --i;
  }
  if (i < end) {
    number = std::stoll(name.substr(i, std::min<size_t>(end - i, 18)));
  }
  return number;
}

int Cell(const Grid& grid, int x, int y) {
  return grid[y][x];
}

}  // namespace

RunResult RunProgram(const std::vector<std::string>& args) {
  std::string command = Quote(BinaryPath());
  for (const std::string& arg : args) {
    command += " " + Quote(arg);
  }
  RunResult result;
  result.command = command;
#ifdef _WIN32
  command = "\"" + command + " >NUL 2>&1\"";
  result.code = std::system(command.c_str());
  result.exited_normally = result.code >= 0 && result.code < 256;
#else
  command += " >/dev/null 2>&1";
  const int status = std::system(command.c_str());
  result.exited_normally = WIFEXITED(status);
  result.code = result.exited_normally ? WEXITSTATUS(status) : -1;
#endif
  return result;
}

std::string DataPath(const std::string& name) {
  return std::string(DATA_DIR) + "/" + name;
}

std::vector<std::filesystem::path> ListBmps(const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> files;
  if (!std::filesystem::is_directory(dir)) {
    return files;
  }
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    std::string ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (entry.is_regular_file() && ext == ".bmp") {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end(),
            [](const std::filesystem::path& a, const std::filesystem::path& b) {
              const long long na = IterationNumber(a);
              const long long nb = IterationNumber(b);
              if (na != nb) {
                return na < nb;
              }
              return a.filename() < b.filename();
            });
  return files;
}

Grid ToGrid(const Image& image) {
  Grid grid(image.height, std::vector<int>(image.width));
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      grid[y][x] = image.ColorAt(x, y);
    }
  }
  return grid;
}

::testing::AssertionResult GridMatches(const Image& image, const Grid& expected) {
  const int height = static_cast<int>(expected.size());
  const int width = static_cast<int>(expected[0].size());
  if (image.width != width || image.height != height) {
    return ::testing::AssertionFailure()
           << "image is " << image.width << "x" << image.height << ", expected " << width
           << "x" << height;
  }
  const Grid actual = ToGrid(image);
  bool straight = true;
  bool flipped = true;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int want = std::min(Cell(expected, x, y), 4);
      if (Cell(actual, x, y) != want) {
        straight = false;
      }
      if (Cell(actual, x, height - 1 - y) != want) {
        flipped = false;
      }
    }
  }
  if (straight || flipped) {
    return ::testing::AssertionSuccess();
  }
  ::testing::AssertionResult failure = ::testing::AssertionFailure();
  failure << "colors differ (also tried vertical flip).\nexpected (sand):\n";
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      failure << " " << std::min(Cell(expected, x, y), 4);
    }
    failure << "\n";
  }
  failure << "actual (sand by color, -1 = unknown color):\n";
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      failure << " " << Cell(actual, x, y);
    }
    failure << "\n";
  }
  return failure;
}

bool HasColor(const Image& image, SandColor color) {
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      if (image.ColorAt(x, y) == color) {
        return true;
      }
    }
  }
  return false;
}

bool AllColorsKnown(const Image& image) {
  return !HasColor(image, kUnknown);
}

uint64_t TotalSand(const Image& image) {
  uint64_t total = 0;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const int color = image.ColorAt(x, y);
      if (color > 0) {
        total += color;
      }
    }
  }
  return total;
}

bool MirroredLeftRight(const Image& image) {
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      if (image.ColorAt(x, y) != image.ColorAt(image.width - 1 - x, y)) {
        return false;
      }
    }
  }
  return true;
}

bool MirroredTopBottom(const Image& image) {
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      if (image.ColorAt(x, y) != image.ColorAt(x, image.height - 1 - y)) {
        return false;
      }
    }
  }
  return true;
}

bool MirroredDiagonal(const Image& image) {
  if (image.width != image.height) {
    return false;
  }
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      if (image.ColorAt(x, y) != image.ColorAt(y, x)) {
        return false;
      }
    }
  }
  return true;
}

// Сетка растёт только когда песок выходит за край, поэтому на каждом краю
// итоговой сетки должна быть хотя бы одна песчинка.
bool SandOnEveryBorder(const Image& image) {
  bool top = false;
  bool bottom = false;
  bool left = false;
  bool right = false;
  for (int x = 0; x < image.width; ++x) {
    top = top || image.ColorAt(x, 0) != kBlack;
    bottom = bottom || image.ColorAt(x, image.height - 1) != kBlack;
  }
  for (int y = 0; y < image.height; ++y) {
    left = left || image.ColorAt(0, y) != kBlack;
    right = right || image.ColorAt(image.width - 1, y) != kBlack;
  }
  return top && bottom && left && right;
}

void ProgramTest::SetUp() {
  const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
  dir_ = std::filesystem::temp_directory_path() /
         (std::string("sandpile_tests_") + info->test_suite_name() + "_" + info->name());
  std::filesystem::remove_all(dir_);
  std::filesystem::create_directories(dir_);
  out_ = dir_ / "out";
}

void ProgramTest::TearDown() {
  std::error_code ignored;
  std::filesystem::remove_all(dir_, ignored);
}

void ProgramTest::RunOk(const std::string& input, const std::vector<std::string>& extra) {
  std::vector<std::string> args = {"-i", DataPath(input), "-o", out_.string()};
  args.insert(args.end(), extra.begin(), extra.end());
  const RunResult result = RunProgram(args);
  ASSERT_TRUE(result.exited_normally) << "program crashed: " << result.command;
  ASSERT_EQ(result.code, 0) << result.command;
}

void ProgramTest::RunFails(const std::vector<std::string>& args) {
  const RunResult result = RunProgram(args);
  EXPECT_TRUE(result.exited_normally) << "program crashed instead of reporting error: "
                                      << result.command;
  EXPECT_NE(result.code, 0) << "expected non-zero exit code: " << result.command;
}

void ProgramTest::Read(const std::filesystem::path& path, Image& image) {
  const std::string error = ReadBmp(path, image);
  ASSERT_EQ(error, "") << path;
  ASSERT_TRUE(AllColorsKnown(image))
      << path << ": some pixels are not black/green/purple/yellow/white";
}

void ProgramTest::ReadSingle(Image& image) {
  const std::vector<std::filesystem::path> files = ListBmps(out_);
  ASSERT_EQ(files.size(), 1u) << "expected exactly one .bmp in " << out_;
  Read(files[0], image);
}

void ProgramTest::ReadLast(Image& image) {
  const std::vector<std::filesystem::path> files = ListBmps(out_);
  ASSERT_FALSE(files.empty()) << "no .bmp in " << out_;
  Read(files.back(), image);
}
