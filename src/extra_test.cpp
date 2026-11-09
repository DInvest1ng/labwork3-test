// Тесты на поведение, которое в условии НЕ описано явно, но разумно.
// Если у вас решено иначе — это не обязательно ошибка. Запуск без них:
//   ctest --test-dir build -R Spec

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "bmp_reader.h"
#include "runner.h"

namespace {

class ExtraFlags : public ProgramTest {};
class ExtraInput : public ProgramTest {
 protected:
  void ExpectRejected(const std::string& name) {
    RunFails({"-i", DataPath(name), "-o", out_.string()});
  }
};

TEST_F(ExtraFlags, LongFormWithEquals) {
  const RunResult result =
      RunProgram({"--input=" + DataPath("stable.tsv"), "--output=" + out_.string(),
                  "--max-iter=0", "--freq=5"});
  ASSERT_TRUE(result.exited_normally) << result.command;
  EXPECT_EQ(result.code, 0) << result.command;
  EXPECT_EQ(ListBmps(out_).size(), 1u);
}

TEST_F(ExtraFlags, InputFileDoesNotExist) {
  RunFails({"-i", DataPath("no-such-file.tsv"), "-o", out_.string()});
}

TEST_F(ExtraFlags, OutputIsExistingFile) {
  const std::filesystem::path file = dir_ / "file";
  std::ofstream(file) << "x";
  RunFails({"-i", DataPath("stable.tsv"), "-o", file.string()});
}

TEST_F(ExtraFlags, MaxIterOverflow) {
  RunFails({"-i", DataPath("stable.tsv"), "-o", out_.string(), "-m",
            "99999999999999999999999"});
}

TEST_F(ExtraInput, DuplicatesAreSummed) {
  // 0 0 2 / 0 0 1 / 0 0 1 -> одна клетка с 4 песчинками
  RunOk("duplicates.tsv", {"-m", "0"});
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(GridMatches(image, {{4}}));
}

TEST_F(ExtraInput, WindowsLineEndingsAndBlankLines) {
  RunOk("crlf.tsv");
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_TRUE(GridMatches(image, {{1, 2}, {3, 0}}));
}

TEST_F(ExtraInput, ZeroSandCellCountsForGridSize) {
  // 0 5 0 / 0 0 4 -> сетка 1x6 (условие: «минимальный прямоугольник,
  // содержащий все указанные ячейки»)
  RunOk("zero-sand.tsv", {"-m", "0"});
  Image image;
  ASSERT_NO_FATAL_FAILURE(ReadSingle(image));
  EXPECT_EQ(image.width, 1);
  EXPECT_EQ(image.height, 6);
}

TEST_F(ExtraInput, EmptyFile) {
  ExpectRejected("empty.tsv");
  ExpectRejected("blank-lines.tsv");
}

TEST_F(ExtraInput, BrokenLines) {
  ExpectRejected("not-number.tsv");
  ExpectRejected("fraction.tsv");
  ExpectRejected("missing-value.tsv");
  ExpectRejected("extra-value.tsv");
  ExpectRejected("bad-last-line.tsv");
}

TEST_F(ExtraInput, ValuesOutOfRange) {
  ExpectRejected("negative-sand.tsv");
  ExpectRejected("big-coord.tsv");
  ExpectRejected("sand-overflow.tsv");
}

}  // namespace
