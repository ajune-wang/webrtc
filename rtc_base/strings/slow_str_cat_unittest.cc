// TODO(kwiberg): Copyright blurb. I stole these tests from
// abseil-cpp/absl/strings/str_cat_test.cc

#include "rtc_base/strings/slow_str_cat.h"

#include <limits>

#include "test/gtest.h"

namespace {

// Test rtc::SlowStrCat of ints and longs of various sizes and signdedness.
TEST(StrCat, Ints) {
  const short s = -1;  // NOLINT(runtime/int)
  const uint16_t us = 2;
  const int i = -3;
  const unsigned int ui = 4;
  const long l = -5;                 // NOLINT(runtime/int)
  const unsigned long ul = 6;        // NOLINT(runtime/int)
  const long long ll = -7;           // NOLINT(runtime/int)
  const unsigned long long ull = 8;  // NOLINT(runtime/int)
  const ptrdiff_t ptrdiff = -9;
  const size_t size = 10;
  const intptr_t intptr = -12;
  const uintptr_t uintptr = 13;
  std::string answer;
  answer = rtc::SlowStrCat(s, us);
  EXPECT_EQ(answer, "-12");
  answer = rtc::SlowStrCat(i, ui);
  EXPECT_EQ(answer, "-34");
  answer = rtc::SlowStrCat(l, ul);
  EXPECT_EQ(answer, "-56");
  answer = rtc::SlowStrCat(ll, ull);
  EXPECT_EQ(answer, "-78");
  answer = rtc::SlowStrCat(ptrdiff, size);
  EXPECT_EQ(answer, "-910");
  answer = rtc::SlowStrCat(ptrdiff, intptr);
  EXPECT_EQ(answer, "-9-12");
  answer = rtc::SlowStrCat(uintptr, 0);
  EXPECT_EQ(answer, "130");
}

TEST(StrCat, Enums) {
  enum SmallNumbers { One = 1, Ten = 10 } e = Ten;
  EXPECT_EQ("10", rtc::SlowStrCat(e));
  EXPECT_EQ("-5", rtc::SlowStrCat(SmallNumbers(-5)));

  enum class Option { Boxers = 1, Briefs = -1 };

  EXPECT_EQ("-1", rtc::SlowStrCat(Option::Briefs));

  enum class Airplane : uint64_t {
    Airbus = 1,
    Boeing = 1000,
    Canary = 10000000000  // too big for "int"
  };

  EXPECT_EQ("10000000000", rtc::SlowStrCat(Airplane::Canary));

  enum class TwoGig : int32_t {
    TwoToTheZero = 1,
    TwoToTheSixteenth = 1 << 16,
    TwoToTheThirtyFirst = INT32_MIN
  };
  EXPECT_EQ("65536", rtc::SlowStrCat(TwoGig::TwoToTheSixteenth));
  EXPECT_EQ("-2147483648", rtc::SlowStrCat(TwoGig::TwoToTheThirtyFirst));
  EXPECT_EQ("-1", rtc::SlowStrCat(static_cast<TwoGig>(-1)));

  enum class FourGig : uint32_t {
    TwoToTheZero = 1,
    TwoToTheSixteenth = 1 << 16,
    TwoToTheThirtyFirst = 1U << 31  // too big for "int"
  };
  EXPECT_EQ("65536", rtc::SlowStrCat(FourGig::TwoToTheSixteenth));
  EXPECT_EQ("2147483648", rtc::SlowStrCat(FourGig::TwoToTheThirtyFirst));
  EXPECT_EQ("4294967295", rtc::SlowStrCat(static_cast<FourGig>(-1)));

  EXPECT_EQ("10000000000", rtc::SlowStrCat(Airplane::Canary));
}

TEST(StrCat, Basics) {
  std::string result;

  std::string strs[] = {"Hello", "Cruel", "World"};

  std::string stdstrs[] = {"std::Hello", "std::Cruel", "std::World"};

  absl::string_view pieces[] = {"Hello", "Cruel", "World"};

  const char* c_strs[] = {"Hello", "Cruel", "World"};

  int32_t i32s[] = {'H', 'C', 'W'};
  uint64_t ui64s[] = {12345678910LL, 10987654321LL};

  EXPECT_EQ(rtc::SlowStrCat(), "");

  result = rtc::SlowStrCat(false, true, 2, 3);
  EXPECT_EQ(result, "0123");

  result = rtc::SlowStrCat(-1);
  EXPECT_EQ(result, "-1");

  result = rtc::SlowStrCat(absl::SixDigits(0.5));
  EXPECT_EQ(result, "0.5");

  result = rtc::SlowStrCat(strs[1], pieces[2]);
  EXPECT_EQ(result, "CruelWorld");

  result = rtc::SlowStrCat(stdstrs[1], " ", stdstrs[2]);
  EXPECT_EQ(result, "std::Cruel std::World");

  result = rtc::SlowStrCat(strs[0], ", ", pieces[2]);
  EXPECT_EQ(result, "Hello, World");

  result = rtc::SlowStrCat(strs[0], ", ", strs[1], " ", strs[2], "!");
  EXPECT_EQ(result, "Hello, Cruel World!");

  result = rtc::SlowStrCat(pieces[0], ", ", pieces[1], " ", pieces[2]);
  EXPECT_EQ(result, "Hello, Cruel World");

  result = rtc::SlowStrCat(c_strs[0], ", ", c_strs[1], " ", c_strs[2]);
  EXPECT_EQ(result, "Hello, Cruel World");

  result = rtc::SlowStrCat("ASCII ", i32s[0], ", ", i32s[1], " ", i32s[2], "!");
  EXPECT_EQ(result, "ASCII 72, 67 87!");

  result = rtc::SlowStrCat(ui64s[0], ", ", ui64s[1], "!");
  EXPECT_EQ(result, "12345678910, 10987654321!");

  std::string one =
      "1";  // Actually, it's the size of this std::string that we want; a
            // 64-bit build distinguishes between size_t and uint64_t,
            // even though they're both unsigned 64-bit values.
  result =
      rtc::SlowStrCat("And a ", one.size(), " and a ", &result[2] - &result[0],
                      " and a ", one, " 2 3 4", "!");
  EXPECT_EQ(result, "And a 1 and a 2 and a 1 2 3 4!");

  // result = rtc::SlowStrCat("Single chars won't compile", '!');
  // result = rtc::SlowStrCat("Neither will nullptrs", nullptr);
  result = rtc::SlowStrCat("To output a char by ASCII/numeric value, use +: ",
                           '!' + 0);
  EXPECT_EQ(result, "To output a char by ASCII/numeric value, use +: 33");

  float f = 100000.5;
  result = rtc::SlowStrCat("A hundred K and a half is ", absl::SixDigits(f));
  EXPECT_EQ(result, "A hundred K and a half is 100000");

  f = 100001.5;
  result =
      rtc::SlowStrCat("A hundred K and one and a half is ", absl::SixDigits(f));
  EXPECT_EQ(result, "A hundred K and one and a half is 100002");

  double d = 100000.5;
  d *= d;
  result =
      rtc::SlowStrCat("A hundred K and a half squared is ", absl::SixDigits(d));
  EXPECT_EQ(result, "A hundred K and a half squared is 1.00001e+10");

  result = rtc::SlowStrCat(1, 2, 333, 4444, 55555, 666666, 7777777, 88888888,
                           999999999);
  EXPECT_EQ(result, "12333444455555666666777777788888888999999999");
}

// A minimal allocator that uses malloc().
template <typename T>
struct Mallocator {
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;

  size_type max_size() const {
    return size_t(std::numeric_limits<size_type>::max()) / sizeof(value_type);
  }
  template <typename U>
  struct rebind {
    typedef Mallocator<U> other;
  };
  Mallocator() = default;
  template <class U>
  Mallocator(const Mallocator<U>&) {}  // NOLINT(runtime/explicit)

  T* allocate(size_t n) { return static_cast<T*>(std::malloc(n * sizeof(T))); }
  void deallocate(T* p, size_t) { std::free(p); }
};
template <typename T, typename U>
bool operator==(const Mallocator<T>&, const Mallocator<U>&) {
  return true;
}
template <typename T, typename U>
bool operator!=(const Mallocator<T>&, const Mallocator<U>&) {
  return false;
}

TEST(StrCat, CustomAllocator) {
  using mstring =
      std::basic_string<char, std::char_traits<char>, Mallocator<char>>;
  const mstring str1("PARACHUTE OFF A BLIMP INTO MOSCONE!!");

  const mstring str2("Read this book about coffee tables");

  std::string result = rtc::SlowStrCat(str1, str2);
  EXPECT_EQ(result,
            "PARACHUTE OFF A BLIMP INTO MOSCONE!!"
            "Read this book about coffee tables");
}

TEST(StrCat, MaxArgs) {
  std::string result;
  // Test 10 up to 26 arguments, the old maximum
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a");
  EXPECT_EQ(result, "123456789a");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b");
  EXPECT_EQ(result, "123456789ab");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c");
  EXPECT_EQ(result, "123456789abc");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d");
  EXPECT_EQ(result, "123456789abcd");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e");
  EXPECT_EQ(result, "123456789abcde");
  result =
      rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e", "f");
  EXPECT_EQ(result, "123456789abcdef");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e",
                           "f", "g");
  EXPECT_EQ(result, "123456789abcdefg");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e",
                           "f", "g", "h");
  EXPECT_EQ(result, "123456789abcdefgh");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e",
                           "f", "g", "h", "i");
  EXPECT_EQ(result, "123456789abcdefghi");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e",
                           "f", "g", "h", "i", "j");
  EXPECT_EQ(result, "123456789abcdefghij");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e",
                           "f", "g", "h", "i", "j", "k");
  EXPECT_EQ(result, "123456789abcdefghijk");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e",
                           "f", "g", "h", "i", "j", "k", "l");
  EXPECT_EQ(result, "123456789abcdefghijkl");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e",
                           "f", "g", "h", "i", "j", "k", "l", "m");
  EXPECT_EQ(result, "123456789abcdefghijklm");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e",
                           "f", "g", "h", "i", "j", "k", "l", "m", "n");
  EXPECT_EQ(result, "123456789abcdefghijklmn");
  result = rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e",
                           "f", "g", "h", "i", "j", "k", "l", "m", "n", "o");
  EXPECT_EQ(result, "123456789abcdefghijklmno");
  result =
      rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e", "f",
                      "g", "h", "i", "j", "k", "l", "m", "n", "o", "p");
  EXPECT_EQ(result, "123456789abcdefghijklmnop");
  result =
      rtc::SlowStrCat(1, 2, 3, 4, 5, 6, 7, 8, 9, "a", "b", "c", "d", "e", "f",
                      "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q");
  EXPECT_EQ(result, "123456789abcdefghijklmnopq");
  // No limit thanks to C++11's variadic templates
  result = rtc::SlowStrCat(
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, "a", "b", "c", "d", "e", "f", "g", "h",
      "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w",
      "x", "y", "z", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
      "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z");
  EXPECT_EQ(result,
            "12345678910abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
}

}  // namespace
