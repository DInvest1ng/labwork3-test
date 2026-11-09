// Тесты только на то, что прямо написано в условии (README лабы).

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "bmp_reader.h"
#include "runner.h"

namespace {

class SpecFlags : public ProgramTest {};
class SpecOutput : public ProgramTest {};
class SpecModel : public ProgramTest {
 protected:
  // Точное число итераций без доступа к stdout:
  //   --freq N     -> ровно 1 картинка  => итераций <= N
  //   --freq N-1   -> ровно 2 картинки  => итераций >= N
  // Возвращает последнюю картинку второго запуска (итоговое состояние).
  void ExpectIterations(const std::string& input, uint64_t n, Image& final_image) {
    ASSERT_NO_FATAL_FAILURE(RunOk(input, {"-f", std::to_string(n)}));
    EXPECT_EQ(ListBmps(out_).size(), 1u)
        << "with --freq " << n << ": expected only the final image => more than " << n
        << " iterations?";
    std::filesystem::remove_all(out_);
    ASSERT_NO_FATAL_FAILURE(RunOk(input, {"-f", std::to_string(n - 1)}));
    EXPECT_EQ(ListBmps(out_).size(), 2u)
        << "with --freq " << n - 1 << ": expected 2 images => fewer than " << n
        << " iterations?";
    ASSERT_NO_FATAL_FAILURE(ReadLast(final_image));
  }

  void ExpectStableSymmetricPicture(const Image& image, int width, int height,
                                    uint64_t sand) {
    EXPECT_EQ(image.width, width);
    EXPECT_EQ(image.height, height);
    EXPECT_FALSE(HasColor(image, kWhite)) << "final state must be stable (no white)";
    EXPECT_EQ(TotalSand(image), sand) << "sand must not be lost";
    EXPECT_TRUE(MirroredLeftRight(image));
    EXPECT_TRUE(MirroredTopBottom(image));
    EXPECT_TRUE(SandOnEveryBorder(image)) << "grid grew more than needed";
  }
};

// ---------- аргументы командной строки ----------

TEST_F(SpecFlags, ShortForm) {
  RunOk("stable.tsv", {"-m", "100", "-f", "10"});
  EXPECT_EQ(ListBmps(out_).size(), 1u);
}

TEST_F(SpecFlags, LongForm) {
  const RunResult result = RunProgram({"--input", DataPath("stable.tsv"), "--output",
                                       out_.string(), "--max-iter", "0", "--freq", "5"});
  ASSERT_TRUE(result.exited_normally) << result.command;
  EXPECT_EQ(result.code, 0) << result.command;
  EXPECT_EQ(ListBmps(out_).size(), 1u);
}

TEST_F(SpecFlags, FlagsInAnyOrder) {
  const RunResult result =
      RunProgram({"-f", "0", "-o", out_.string(), "-m", "5", "-i", DataPath("stable.tsv")});
  ASSERT_TRUE(result.exited_normally) << result.command;
  EXPECT_EQ(result.code, 0) << result.command;
}

TEST_F(SpecFlags, NoArguments) {
  RunFails({});
}

TEST_F(SpecFlags, MissingOutput) {
  RunFails({"-i", DataPath("stable.tsv")});
}

TEST_F(SpecFlags, MissingInput) {
  RunFails({"-o", out_.string()});
}

TEST_F(SpecFlags, FlagWithoutValue) {
  RunFails({"-i", DataPath("stable.tsv"), "-o"});
  RunFails({"-i", DataPath("stable.tsv"), "-o", out_.string(), "-m"});
}

TEST_F(SpecFlags, UnknownFlag) {
  RunFails({"-i", DataPath("stable.tsv"), "-o", out_.string(), "--size", "3"});
  RunFails({"-i", DataPath("stable.tsv"), "-o", out_.string(), "-x"});
}

TEST_F(SpecFlags, NegativeValues) {
  RunFails({"-i", DataPath("stable.tsv"), "-o", out_.string(), "-f", "-1"});
  RunFails({"-i", DataPath("stable.tsv"), "-o", out_.string(), "-m", "-5"});
}

TEST_F(SpecFlags, NotIntegerValues) {
  RunFails({"-i", DataPath("stable.tsv"), "-o", out_.string(), "-m", "abc"});
  RunFails({"-i", DataPath("stable.tsv"), "-o", out_.string(), "-f", "1.5"});
  RunFails({"-i", DataPath("stable.tsv"), "-o", out_.string(), "-m", "10abc"});
  RunFails({"-i", DataPath("stable.tsv"), "-o", out_.string(), "-f", ""});
}

TEST_F(SpecFlags, CreatesNestedOutputDirectory) {
  out_ = dir_ / "a" / "b" / "c";
  RunOk("stable.tsv");
  EXPECT_TRUE(std::filesystem::is_directory(out_));
  EXPECT_EQ(ListBmps(out_).size(), 1u);
}

TEST_F(SpecFlags, ExistingOutputDirectory) {
  std::filesystem::create_directories(out_);
  RunOk("stable.tsv");
  EXPECT_EQ(ListBmps(out_).size(), 1u);
}

// ---------- формат и сохранение картинок ----------

TEST_F(SpecOutput, AtMostFourBitsPerPixel) {
  RunOk("stable.tsv");
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_LE(image.bit_count, 4);
}

TEST_F(SpecOutput, StableSavesInitialState) {
  RunOk("stable.tsv");
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(GridMatches(image, {{1, 2}, {3, 0}}));
}

TEST_F(SpecOutput, AllFiveColors) {
  // fractal без итераций: 1x1, 65536 песчинок -> белый.
  RunOk("fractal.tsv", {"-m", "0"});
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(GridMatches(image, {{4}}));
  std::filesystem::remove_all(out_);
  RunOk("stable.tsv");
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(HasColor(image, kBlack));
  EXPECT_TRUE(HasColor(image, kGreen));
  EXPECT_TRUE(HasColor(image, kPurple));
  EXPECT_TRUE(HasColor(image, kYellow));
}

TEST_F(SpecOutput, ZeroMaxIterSavesInitialState) {
  RunOk("multiple-piles.tsv", {"-m", "0"});
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(GridMatches(image, {
                                     {16, 0, 0, 0, 0},
                                     {0, 0, 4, 0, 0},
                                     {0, 0, 0, 0, 12},
                                 }));
}

TEST_F(SpecOutput, FreqZeroSavesOnlyLast) {
  RunOk("square.tsv", {"-f", "0"});
  EXPECT_EQ(ListBmps(out_).size(), 1u);
}

TEST_F(SpecOutput, LastStateIsNotSavedTwice) {
  RunOk("single-pile.tsv", {"-f", "1"});
  EXPECT_EQ(ListBmps(out_).size(), 1u);
}

TEST_F(SpecOutput, FreqOneSavesEveryIteration) {
  // multiple-piles стабилизируется за несколько итераций; при -f 1 картинок
  // столько же, сколько итераций, и все размеры не убывают.
  RunOk("multiple-piles.tsv", {"-f", "1"});
  const std::vector<std::filesystem::path> files = ListBmps(out_);
  ASSERT_GE(files.size(), 2u);
  int previous_w = 0;
  int previous_h = 0;
  for (size_t i = 0; i < files.size(); ++i) {
    Image image;
    ASSERT_NO_FATAL_FAILURE(Read(files[i], image));
    EXPECT_GE(image.width, previous_w) << files[i];
    EXPECT_GE(image.height, previous_h) << files[i];
    previous_w = image.width;
    previous_h = image.height;
    if (i + 1 < files.size()) {
      EXPECT_TRUE(HasColor(image, kWhite)) << files[i] << " is not last, must be unstable";
    } else {
      EXPECT_FALSE(HasColor(image, kWhite)) << files[i] << " is last, must be stable";
    }
  }
}

TEST_F(SpecOutput, FreqSavesEveryNIterationsAndLast) {
  // square: 3165 итераций -> 1000, 2000, 3000, 3165
  RunOk("square.tsv", {"-f", "1000"});
  const std::vector<std::filesystem::path> files = ListBmps(out_);
  ASSERT_EQ(files.size(), 4u);
  int previous = 0;
  for (const std::filesystem::path& file : files) {
    Image image;
    ASSERT_NO_FATAL_FAILURE(Read(file, image));
    EXPECT_GE(image.width, previous) << file;
    EXPECT_EQ(image.width, image.height) << file;
    previous = image.width;
  }
  EXPECT_EQ(previous, 113);
}

TEST_F(SpecOutput, MaxIterOnFreqBoundary) {
  RunOk("four-piles.tsv", {"-m", "10", "-f", "5"});
  EXPECT_EQ(ListBmps(out_).size(), 2u) << "expected images after 5 and 10";
}

TEST_F(SpecOutput, MaxIterBetweenSaves) {
  RunOk("four-piles.tsv", {"-m", "12", "-f", "5"});
  EXPECT_EQ(ListBmps(out_).size(), 3u) << "expected images after 5, 10 and 12";
}

TEST_F(SpecOutput, FreqBiggerThanIterations) {
  RunOk("multiple-piles.tsv", {"-f", "1000000"});
  EXPECT_EQ(ListBmps(out_).size(), 1u);
}

TEST_F(SpecOutput, MaxIterStopsEarly) {
  RunOk("fractal.tsv", {"-m", "100"});
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(HasColor(image, kWhite)) << "after 100 iterations fractal is not stable yet";
}

TEST_F(SpecOutput, MaxIterAboveNeededIsFine) {
  RunOk("multiple-piles.tsv", {"-m", "1000000"});
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_FALSE(HasColor(image, kWhite));
}

// ---------- правила модели ----------

TEST_F(SpecModel, SinglePileBecomesCross) {
  RunOk("single-pile.tsv");
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(GridMatches(image, {
                                     {0, 1, 0},
                                     {1, 0, 1},
                                     {0, 1, 0},
                                 }));
}

TEST_F(SpecModel, MultiplePilesFirstIteration) {
  RunOk("multiple-piles.tsv", {"-m", "1"});
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(GridMatches(image, {
                                     {0, 1, 0, 0, 0, 0, 0},
                                     {1, 12, 1, 1, 0, 0, 0},
                                     {0, 1, 1, 0, 1, 1, 0},
                                     {0, 0, 0, 1, 1, 8, 1},
                                     {0, 0, 0, 0, 0, 1, 0},
                                 }));
}

TEST_F(SpecModel, MultiplePilesStabilize) {
  RunOk("multiple-piles.tsv");
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_FALSE(HasColor(image, kWhite));
  EXPECT_EQ(TotalSand(image), 32u);
  EXPECT_TRUE(SandOnEveryBorder(image));
}

// Правило 3: песок, полученный за итерацию, обваливается только на следующей.
TEST_F(SpecModel, RowWaitsForNextIteration) {
  // 3 4 3 в строку
  RunOk("row.tsv", {"-m", "1"});
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(GridMatches(image, {
                                     {0, 1, 0},
                                     {4, 0, 4},
                                     {0, 1, 0},
                                 }));
  std::filesystem::remove_all(out_);
  RunOk("row.tsv");
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_FALSE(HasColor(image, kWhite));
  EXPECT_EQ(TotalSand(image), 10u);
}

// Известный результат: 16 песчинок в одной точке.
TEST_F(SpecModel, SixteenGrainsPattern) {
  RunOk("sixteen.tsv");
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(GridMatches(image, {
                                     {0, 0, 1, 0, 0},
                                     {0, 2, 1, 2, 0},
                                     {1, 1, 0, 1, 1},
                                     {0, 2, 1, 2, 0},
                                     {0, 0, 1, 0, 0},
                                 }));
}

TEST_F(SpecModel, Int16LimitCoordinates) {
  RunOk("int16-limits.tsv");
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(GridMatches(image, {{1}}));
}

// Таблица из README: размер и число итераций.
TEST_F(SpecModel, FractalMatchesReadme) {
  Image image;
  ASSERT_NO_FATAL_FAILURE(ExpectIterations("fractal.tsv", 62118, image));
  ExpectStableSymmetricPicture(image, 189, 189, 65536);
  EXPECT_TRUE(MirroredDiagonal(image));
}

TEST_F(SpecModel, TwinPilesMatchesReadme) {
  Image image;
  ASSERT_NO_FATAL_FAILURE(ExpectIterations("twin-piles.tsv", 31148, image));
  ExpectStableSymmetricPicture(image, 233, 143, 65536);
}

TEST_F(SpecModel, FourPilesMatchesReadme) {
  Image image;
  ASSERT_NO_FATAL_FAILURE(ExpectIterations("four-piles.tsv", 16460, image));
  ExpectStableSymmetricPicture(image, 181, 181, 65536);
  EXPECT_TRUE(MirroredDiagonal(image));
}

TEST_F(SpecModel, SquareMatchesReadme) {
  Image image;
  ASSERT_NO_FATAL_FAILURE(ExpectIterations("square.tsv", 3165, image));
  ExpectStableSymmetricPicture(image, 113, 113, 65u * 65u * 6u);
  EXPECT_TRUE(MirroredDiagonal(image));
}

}  // namespace
