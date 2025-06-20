/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <folly/Optional.h>

#include <algorithm>
#include <initializer_list>
#include <iomanip>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <folly/Portability.h>
#include <folly/portability/GMock.h>
#include <folly/portability/GTest.h>

FOLLY_GNU_DISABLE_WARNING("-Wself-move")

using std::shared_ptr;
using std::unique_ptr;

namespace {

struct HashableStruct {};
struct UnhashableStruct {};

} // namespace

namespace std {

template <>
struct hash<HashableStruct> {
  [[maybe_unused]] size_t operator()(const HashableStruct&) const noexcept {
    return 0;
  }
};

} // namespace std

namespace folly {

namespace {

template <class V>
std::ostream& operator<<(std::ostream& os, const Optional<V>& v) {
  if (v) {
    os << "Optional(" << v.value() << ')';
  } else {
    os << "None";
  }
  return os;
}

struct NoDefault {
  NoDefault(int, int) {}
  char a, b, c;
};

} // namespace

static_assert(sizeof(Optional<char>) == 2);
static_assert(sizeof(Optional<int>) == 8);
static_assert(sizeof(Optional<NoDefault>) == 4);
static_assert(sizeof(Optional<char>) == sizeof(std::optional<char>));
static_assert(sizeof(Optional<short>) == sizeof(std::optional<short>));
static_assert(sizeof(Optional<int>) == sizeof(std::optional<int>));
static_assert(sizeof(Optional<double>) == sizeof(std::optional<double>));

static_assert(is_hashable_v<folly::Optional<HashableStruct>>);
static_assert(!is_hashable_v<folly::Optional<UnhashableStruct>>);

TEST(Optional, ConstexprConstructible) {
  // Use FOLLY_STORAGE_CONSTEXPR to work around MSVC not taking this.
  static FOLLY_STORAGE_CONSTEXPR Optional<int> opt;
  // NOTE: writing `opt = none` instead of `opt(none)` causes gcc to reject this
  // code, claiming that the (non-constexpr) move ctor of `Optional` is being
  // invoked.
  static FOLLY_STORAGE_CONSTEXPR Optional<int> opt2(none);

  EXPECT_FALSE(opt.has_value());
  EXPECT_FALSE(opt2.has_value());
}

TEST(Optional, NoDefault) {
  Optional<NoDefault> x;
  EXPECT_FALSE(x);
  x.emplace(4, 5);
  EXPECT_TRUE(bool(x));
  x.reset();
  EXPECT_FALSE(x);
}

TEST(Optional, Emplace) {
  Optional<std::vector<int>> opt;
  auto& values1 = opt.emplace(3, 4);
  EXPECT_THAT(values1, testing::ElementsAre(4, 4, 4));
  auto& values2 = opt.emplace(2, 5);
  EXPECT_THAT(values2, testing::ElementsAre(5, 5));
}

TEST(Optional, EmplaceInitializerList) {
  Optional<std::vector<int>> opt;
  auto& values1 = opt.emplace({3, 4, 5});
  EXPECT_THAT(values1, testing::ElementsAre(3, 4, 5));
  auto& values2 = opt.emplace({4, 5, 6});
  EXPECT_THAT(values2, testing::ElementsAre(4, 5, 6));
}

TEST(Optional, Reset) {
  Optional<int> opt(3);
  opt.reset();
  EXPECT_FALSE(opt);
}

TEST(Optional, String) {
  Optional<std::string> maybeString;
  EXPECT_FALSE(maybeString);
  maybeString = "hello";
  EXPECT_TRUE(bool(maybeString));
}

TEST(Optional, Const) {
  { // default construct
    Optional<const int> opt;
    EXPECT_FALSE(bool(opt));
    opt.emplace(4);
    EXPECT_EQ(*opt, 4);
    opt.emplace(5);
    EXPECT_EQ(*opt, 5);
    opt.reset();
    EXPECT_FALSE(bool(opt));
  }
  { // copy-constructed
    const int x = 6;
    Optional<const int> opt(x);
    EXPECT_EQ(*opt, 6);
  }
  { // move-constructed
    const int x = 7;
    Optional<const int> opt(std::move(x));
    EXPECT_EQ(*opt, 7);
  }
  // no assignment allowed
}

TEST(Optional, Simple) {
  Optional<int> opt;
  EXPECT_FALSE(bool(opt));
  EXPECT_EQ(42, opt.value_or(42));
  opt = 4;
  EXPECT_TRUE(bool(opt));
  EXPECT_EQ(4, *opt);
  EXPECT_EQ(4, opt.value_or(42));
  opt = 5;
  EXPECT_EQ(5, *opt);
  opt.reset();
  EXPECT_FALSE(bool(opt));
}

namespace {

class MoveTester {
 public:
  /* implicit */ MoveTester(const char* s) : s_(s) {}
  MoveTester(const MoveTester&) = default;
  MoveTester(MoveTester&& other) noexcept {
    s_ = std::move(other.s_);
    other.s_ = "";
  }
  MoveTester& operator=(const MoveTester&) = default;
  MoveTester& operator=(MoveTester&& other) noexcept {
    s_ = std::move(other.s_);
    other.s_ = "";
    return *this;
  }

 private:
  friend bool operator==(const MoveTester& o1, const MoveTester& o2);
  std::string s_;
};

bool operator==(const MoveTester& o1, const MoveTester& o2) {
  return o1.s_ == o2.s_;
}

} // namespace

TEST(Optional, valueOrRvalueArg) {
  Optional<MoveTester> opt;
  MoveTester dflt = "hello";
  EXPECT_EQ("hello", opt.value_or(dflt));
  EXPECT_EQ("hello", dflt);
  EXPECT_EQ("hello", opt.value_or(std::move(dflt)));
  EXPECT_EQ("", dflt);
  EXPECT_EQ("world", opt.value_or("world"));

  dflt = "hello";
  // Make sure that the const overload works on const objects
  const auto& optc = opt;
  EXPECT_EQ("hello", optc.value_or(dflt));
  EXPECT_EQ("hello", dflt);
  EXPECT_EQ("hello", optc.value_or(std::move(dflt)));
  EXPECT_EQ("", dflt);
  EXPECT_EQ("world", optc.value_or("world"));

  dflt = "hello";
  opt = "meow";
  EXPECT_EQ("meow", opt.value_or(dflt));
  EXPECT_EQ("hello", dflt);
  EXPECT_EQ("meow", opt.value_or(std::move(dflt)));
  EXPECT_EQ("hello", dflt); // only moved if used
}

TEST(Optional, valueOrNoncopyable) {
  Optional<std::unique_ptr<int>> opt;
  std::unique_ptr<int> dflt(new int(42));
  EXPECT_EQ(42, *std::move(opt).value_or(std::move(dflt)));
}

struct ExpectingDeleter {
  explicit ExpectingDeleter(int expected_) : expected(expected_) {}
  int expected;
  void operator()(const int* ptr) {
    EXPECT_EQ(*ptr, expected);
    delete ptr;
  }
};

TEST(Optional, valueMove) {
  auto ptr =
      Optional<std::unique_ptr<int, ExpectingDeleter>>(
          {new int(42), ExpectingDeleter{1337}})
          .value();
  *ptr = 1337;
}

TEST(Optional, dereferenceMove) {
  auto ptr = *Optional<std::unique_ptr<int, ExpectingDeleter>>(
      {new int(42), ExpectingDeleter{1337}});
  *ptr = 1337;
}

TEST(Optional, EmptyConstruct) {
  Optional<int> opt;
  EXPECT_FALSE(bool(opt));
  Optional<int> test1(opt);
  EXPECT_FALSE(bool(test1));
  Optional<int> test2(std::move(opt));
  EXPECT_FALSE(bool(test2));
}

TEST(Optional, InPlaceConstruct) {
  using A = std::pair<int, double>;
  Optional<A> opt(std::in_place, 5, 3.2);
  EXPECT_TRUE(bool(opt));
  EXPECT_EQ(5, opt->first);
}

TEST(Optional, InPlaceNestedConstruct) {
  using A = std::pair<int, double>;
  Optional<Optional<A>> opt(std::in_place, std::in_place, 5, 3.2);
  EXPECT_TRUE(bool(opt));
  EXPECT_TRUE(bool(*opt));
  EXPECT_EQ(5, (*opt)->first);
}

TEST(Optional, Unique) {
  Optional<unique_ptr<int>> opt;

  opt.reset();
  EXPECT_FALSE(bool(opt));
  // empty->emplaced
  opt.emplace(new int(5));
  EXPECT_TRUE(bool(opt));
  EXPECT_EQ(5, **opt);

  opt.reset();
  // empty->moved
  opt = std::make_unique<int>(6);
  EXPECT_EQ(6, **opt);
  // full->moved
  opt = std::make_unique<int>(7);
  EXPECT_EQ(7, **opt);

  // move it out by move construct
  Optional<unique_ptr<int>> moved(std::move(opt));
  EXPECT_TRUE(bool(moved));
  EXPECT_FALSE(bool(opt));
  EXPECT_EQ(7, **moved);

  EXPECT_TRUE(bool(moved));
  opt = std::move(moved); // move it back by move assign
  EXPECT_FALSE(bool(moved));
  EXPECT_TRUE(bool(opt));
  EXPECT_EQ(7, **opt);
}

TEST(Optional, Shared) {
  shared_ptr<int> ptr;
  Optional<shared_ptr<int>> opt;
  EXPECT_FALSE(bool(opt));
  // empty->emplaced
  opt.emplace(new int(5));
  EXPECT_TRUE(bool(opt));
  ptr = opt.value();
  EXPECT_EQ(ptr.get(), opt->get());
  EXPECT_EQ(2, ptr.use_count());
  opt.reset();
  EXPECT_EQ(1, ptr.use_count());
  // full->copied
  opt = ptr;
  EXPECT_EQ(2, ptr.use_count());
  EXPECT_EQ(ptr.get(), opt->get());
  opt.reset();
  EXPECT_EQ(1, ptr.use_count());
  // full->moved
  opt = std::move(ptr);
  EXPECT_EQ(1, opt->use_count());
  EXPECT_EQ(nullptr, ptr.get());
  {
    Optional<shared_ptr<int>> copied(opt);
    EXPECT_EQ(2, opt->use_count());
    Optional<shared_ptr<int>> moved(std::move(opt));
    EXPECT_EQ(2, moved->use_count());
    moved.emplace(new int(6));
    EXPECT_EQ(1, moved->use_count());
    copied = moved;
    EXPECT_EQ(2, moved->use_count());
  }
}

TEST(Optional, Order) {
  std::vector<Optional<int>> vect{
      {none},
      {3},
      {1},
      {none},
      {2},
  };
  std::vector<Optional<int>> expected{
      {none},
      {none},
      {1},
      {2},
      {3},
  };
  std::sort(vect.begin(), vect.end());
  EXPECT_EQ(vect, expected);
}

TEST(Optional, Swap) {
  Optional<std::string> a;
  Optional<std::string> b;

  swap(a, b);
  EXPECT_FALSE(a.has_value());
  EXPECT_FALSE(b.has_value());

  a = "hello";
  EXPECT_TRUE(a.has_value());
  EXPECT_FALSE(b.has_value());
  EXPECT_EQ("hello", a.value());

  swap(a, b);
  EXPECT_FALSE(a.has_value());
  EXPECT_TRUE(b.has_value());
  EXPECT_EQ("hello", b.value());

  a = "bye";
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ("bye", a.value());

  swap(a, b);
  EXPECT_TRUE(a.has_value());
  EXPECT_TRUE(b.has_value());
  EXPECT_EQ("hello", a.value());
  EXPECT_EQ("bye", b.value());
}

TEST(Optional, Comparisons) {
  Optional<int> o_;
  Optional<int> o1(1);
  Optional<int> o2(2);

  EXPECT_TRUE(o_ <= (o_));
  EXPECT_TRUE(o_ == (o_));
  EXPECT_TRUE(o_ >= (o_));

  EXPECT_TRUE(o1 < o2);
  EXPECT_TRUE(o1 <= o2);
  EXPECT_TRUE(o1 <= (o1));
  EXPECT_TRUE(o1 == (o1));
  EXPECT_TRUE(o1 != o2);
  EXPECT_TRUE(o1 >= (o1));
  EXPECT_TRUE(o2 >= o1);
  EXPECT_TRUE(o2 > o1);

  EXPECT_FALSE(o2 < o1);
  EXPECT_FALSE(o2 <= o1);
  EXPECT_FALSE(o2 <= o1);
  EXPECT_FALSE(o2 == o1);
  EXPECT_FALSE(o1 != (o1));
  EXPECT_FALSE(o1 >= o2);
  EXPECT_FALSE(o1 >= o2);
  EXPECT_FALSE(o1 > o2);

  /* folly::Optional explicitly doesn't support comparisons with contained value
  EXPECT_TRUE(1 < o2);
  EXPECT_TRUE(1 <= o2);
  EXPECT_TRUE(1 <= o1);
  EXPECT_TRUE(1 == o1);
  EXPECT_TRUE(2 != o1);
  EXPECT_TRUE(1 >= o1);
  EXPECT_TRUE(2 >= o1);
  EXPECT_TRUE(2 > o1);

  EXPECT_FALSE(o2 < 1);
  EXPECT_FALSE(o2 <= 1);
  EXPECT_FALSE(o2 <= 1);
  EXPECT_FALSE(o2 == 1);
  EXPECT_FALSE(o2 != 2);
  EXPECT_FALSE(o1 >= 2);
  EXPECT_FALSE(o1 >= 2);
  EXPECT_FALSE(o1 > 2);
  */

  // std::optional does support comparison with contained value, which can
  // lead to confusion when a bool is contained
  std::optional<int> boi(3);
  EXPECT_TRUE(boi < 5);
  EXPECT_TRUE(boi <= 4);
  EXPECT_TRUE(boi == 3);
  EXPECT_TRUE(boi != 2);
  EXPECT_TRUE(boi >= 1);
  EXPECT_TRUE(boi > 0);
  EXPECT_TRUE(1 < boi);
  EXPECT_TRUE(2 <= boi);
  EXPECT_TRUE(3 == boi);
  EXPECT_TRUE(4 != boi);
  EXPECT_TRUE(5 >= boi);
  EXPECT_TRUE(6 > boi);

  std::optional<bool> bob(false);
  EXPECT_TRUE((bool)bob);
  EXPECT_TRUE(bob == false); // well that was confusing
  EXPECT_FALSE(bob != false);
}

TEST(Optional, HeterogeneousComparisons) {
  using opt8 = Optional<uint8_t>;
  using opt64 = Optional<uint64_t>;

  EXPECT_TRUE(opt8(4) == uint64_t(4));
  EXPECT_FALSE(opt8(8) == uint64_t(4));
  EXPECT_FALSE(opt8() == uint64_t(4));

  EXPECT_TRUE(uint64_t(4) == opt8(4));
  EXPECT_FALSE(uint64_t(4) == opt8(8));
  EXPECT_FALSE(uint64_t(4) == opt8());

  EXPECT_FALSE(opt8(4) != uint64_t(4));
  EXPECT_TRUE(opt8(8) != uint64_t(4));
  EXPECT_TRUE(opt8() != uint64_t(4));

  EXPECT_FALSE(uint64_t(4) != opt8(4));
  EXPECT_TRUE(uint64_t(4) != opt8(8));
  EXPECT_TRUE(uint64_t(4) != opt8());

  EXPECT_TRUE(opt8() == opt64());
  EXPECT_TRUE(opt8(4) == opt64(4));
  EXPECT_FALSE(opt8(8) == opt64(4));
  EXPECT_FALSE(opt8() == opt64(4));
  EXPECT_FALSE(opt8(4) == opt64());

  EXPECT_FALSE(opt8() != opt64());
  EXPECT_FALSE(opt8(4) != opt64(4));
  EXPECT_TRUE(opt8(8) != opt64(4));
  EXPECT_TRUE(opt8() != opt64(4));
  EXPECT_TRUE(opt8(4) != opt64());

  EXPECT_TRUE(opt8() < opt64(4));
  EXPECT_TRUE(opt8(4) < opt64(8));
  EXPECT_FALSE(opt8() < opt64());
  EXPECT_FALSE(opt8(4) < opt64(4));
  EXPECT_FALSE(opt8(8) < opt64(4));
  EXPECT_FALSE(opt8(4) < opt64());

  EXPECT_FALSE(opt8() > opt64(4));
  EXPECT_FALSE(opt8(4) > opt64(8));
  EXPECT_FALSE(opt8() > opt64());
  EXPECT_FALSE(opt8(4) > opt64(4));
  EXPECT_TRUE(opt8(8) > opt64(4));
  EXPECT_TRUE(opt8(4) > opt64());

  EXPECT_TRUE(opt8() <= opt64(4));
  EXPECT_TRUE(opt8(4) <= opt64(8));
  EXPECT_TRUE(opt8() <= opt64());
  EXPECT_TRUE(opt8(4) <= opt64(4));
  EXPECT_FALSE(opt8(8) <= opt64(4));
  EXPECT_FALSE(opt8(4) <= opt64());

  EXPECT_FALSE(opt8() >= opt64(4));
  EXPECT_FALSE(opt8(4) >= opt64(8));
  EXPECT_TRUE(opt8() >= opt64());
  EXPECT_TRUE(opt8(4) >= opt64(4));
  EXPECT_TRUE(opt8(8) >= opt64(4));
  EXPECT_TRUE(opt8(4) >= opt64());
}

TEST(Optional, NoneComparisons) {
  using opt = Optional<int>;
  EXPECT_TRUE(opt() == none);
  EXPECT_TRUE(none == opt());
  EXPECT_FALSE(opt(1) == none);
  EXPECT_FALSE(none == opt(1));

  EXPECT_FALSE(opt() != none);
  EXPECT_FALSE(none != opt());
  EXPECT_TRUE(opt(1) != none);
  EXPECT_TRUE(none != opt(1));

  EXPECT_FALSE(opt() < none);
  EXPECT_FALSE(none < opt());
  EXPECT_FALSE(opt(1) < none);
  EXPECT_TRUE(none < opt(1));

  EXPECT_FALSE(opt() > none);
  EXPECT_FALSE(none > opt());
  EXPECT_FALSE(none > opt(1));
  EXPECT_TRUE(opt(1) > none);

  EXPECT_TRUE(opt() <= none);
  EXPECT_TRUE(none <= opt());
  EXPECT_FALSE(opt(1) <= none);
  EXPECT_TRUE(none <= opt(1));

  EXPECT_TRUE(opt() >= none);
  EXPECT_TRUE(none >= opt());
  EXPECT_TRUE(opt(1) >= none);
  EXPECT_FALSE(none >= opt(1));
}

TEST(Optional, Conversions) {
  Optional<bool> mbool;
  Optional<short> mshort;
  Optional<char*> mstr;
  Optional<int> mint;

  // These don't compile
  // bool b = mbool;
  // short s = mshort;
  // char* c = mstr;
  // int x = mint;
  // char* c(mstr);
  // short s(mshort);
  // int x(mint);

  // intended explicit operator bool, for if (opt).
  bool b(mbool);
  EXPECT_FALSE(b);

  // Truthy tests work and are not ambiguous
  if (mbool && mshort && mstr && mint) { // only checks not-empty
    if (*mbool && *mshort && *mstr && *mint) { // only checks value
    }
  }

  mbool = false;
  EXPECT_TRUE(bool(mbool));
  EXPECT_FALSE(*mbool);

  mbool = true;
  EXPECT_TRUE(bool(mbool));
  EXPECT_TRUE(*mbool);

  mbool = none;
  EXPECT_FALSE(bool(mbool));

  // No conversion allowed; does not compile
  // EXPECT_TRUE(mbool == false);
}

TEST(Optional, Pointee) {
  Optional<int> x;
  EXPECT_FALSE(get_pointer(x));
  x = 1;
  EXPECT_TRUE(get_pointer(x));
  *get_pointer(x) = 2;
  EXPECT_TRUE(*x == 2);
  x = none;
  EXPECT_FALSE(get_pointer(x));
}

namespace {
class ConstructibleWithArgsOnly {
 public:
  explicit ConstructibleWithArgsOnly(int, double) {}

  ConstructibleWithArgsOnly() = delete;
  ConstructibleWithArgsOnly(const ConstructibleWithArgsOnly&) = delete;
  ConstructibleWithArgsOnly(ConstructibleWithArgsOnly&&) = delete;
  ConstructibleWithArgsOnly& operator=(const ConstructibleWithArgsOnly&) =
      delete;
  ConstructibleWithArgsOnly& operator=(ConstructibleWithArgsOnly&&) = delete;
};

class ConstructibleWithInitializerListAndArgsOnly {
 public:
  ConstructibleWithInitializerListAndArgsOnly(
      std::initializer_list<int>, double) {}

  ConstructibleWithInitializerListAndArgsOnly() = delete;
  ConstructibleWithInitializerListAndArgsOnly(
      const ConstructibleWithInitializerListAndArgsOnly&) = delete;
  ConstructibleWithInitializerListAndArgsOnly(
      ConstructibleWithInitializerListAndArgsOnly&&) = delete;
  ConstructibleWithInitializerListAndArgsOnly& operator=(
      const ConstructibleWithInitializerListAndArgsOnly&) = delete;
  ConstructibleWithInitializerListAndArgsOnly& operator=(
      ConstructibleWithInitializerListAndArgsOnly&&) = delete;
};
} // namespace

TEST(Optional, MakeOptional) {
  // const L-value version
  const std::string s("abc");
  auto optStr = folly::make_optional(s);
  ASSERT_TRUE(optStr.has_value());
  EXPECT_EQ(*optStr, "abc");
  *optStr = "cde";
  EXPECT_EQ(s, "abc");
  EXPECT_EQ(*optStr, "cde");

  // L-value version
  std::string s2("abc");
  auto optStr2 = folly::make_optional(s2);
  ASSERT_TRUE(optStr2.has_value());
  EXPECT_EQ(*optStr2, "abc");
  *optStr2 = "cde";
  // it's vital to check that s2 wasn't clobbered
  EXPECT_EQ(s2, "abc");

  // L-value reference version
  std::string& s3(s2);
  auto optStr3 = folly::make_optional(s3);
  ASSERT_TRUE(optStr3.has_value());
  EXPECT_EQ(*optStr3, "abc");
  *optStr3 = "cde";
  EXPECT_EQ(s3, "abc");

  // R-value ref version
  unique_ptr<int> pInt(new int(3));
  auto optIntPtr = folly::make_optional(std::move(pInt));
  EXPECT_TRUE(pInt.get() == nullptr);
  ASSERT_TRUE(optIntPtr.has_value());
  EXPECT_EQ(**optIntPtr, 3);

  // variadic version
  {
    auto&& optional = make_optional<ConstructibleWithArgsOnly>(int{}, double{});
    std::ignore = optional;
  }
  {
    using Type = ConstructibleWithInitializerListAndArgsOnly;
    auto&& optional = make_optional<Type>({int{}}, double{});
    std::ignore = optional;
  }
}

TEST(Optional, InitializerListConstruct) {
  using Type = ConstructibleWithInitializerListAndArgsOnly;
  auto&& optional = Optional<Type>{std::in_place, {int{}}, double{}};
  std::ignore = optional;
}

TEST(Optional, TestDisambiguationMakeOptionalVariants) {
  {
    auto optional = make_optional<int>(1);
    std::ignore = optional;
  }
  {
    auto optional = make_optional(1);
    std::ignore = optional;
  }
}

TEST(Optional, SelfAssignment) {
  Optional<int> a = 42;
  a = static_cast<decltype(a)&>(a); // suppress self-assign warning
  ASSERT_TRUE(a.has_value() && a.value() == 42);

  Optional<int> b = 23333333;
  b = static_cast<decltype(b)&&>(b); // suppress self-move warning
  ASSERT_TRUE(b.has_value() && b.value() == 23333333);
}

namespace {

class ContainsOptional {
 public:
  ContainsOptional() {}
  explicit ContainsOptional(int x) : opt_(x) {}
  bool hasValue() const { return opt_.has_value(); }
  int value() const { return opt_.value(); }

  ContainsOptional(const ContainsOptional& other) = default;
  ContainsOptional& operator=(const ContainsOptional& other) = default;
  ContainsOptional(ContainsOptional&& other) = default;
  ContainsOptional& operator=(ContainsOptional&& other) = default;

 private:
  Optional<int> opt_;
};

} // namespace

/**
 * Test that a class containing an Optional can be copy and move assigned.
 */
TEST(Optional, AssignmentContained) {
  {
    ContainsOptional source(5), target;
    target = source;
    EXPECT_TRUE(target.hasValue());
    EXPECT_EQ(5, target.value());
  }

  {
    ContainsOptional source(5), target;
    target = std::move(source);
    EXPECT_TRUE(target.hasValue());
    EXPECT_EQ(5, target.value());
    EXPECT_FALSE(source.hasValue());
  }

  {
    ContainsOptional opt_uninit, target(10);
    target = opt_uninit;
    EXPECT_FALSE(target.hasValue());
  }
}

TEST(Optional, Exceptions) {
  Optional<int> empty;
  EXPECT_THROW(empty.value(), OptionalEmptyException);
}

TEST(Optional, NoThrowDefaultConstructible) {
  EXPECT_TRUE(std::is_nothrow_default_constructible<Optional<bool>>::value);
}

namespace {

struct NoDestructor {};

struct WithDestructor {
  ~WithDestructor();
};

} // namespace

TEST(Optional, TriviallyDestructible) {
  // These could all be static_asserts but EXPECT_* give much nicer output on
  // failure.
  EXPECT_TRUE(std::is_trivially_destructible<Optional<NoDestructor>>::value);
  EXPECT_TRUE(std::is_trivially_destructible<Optional<int>>::value);
  EXPECT_FALSE(std::is_trivially_destructible<Optional<WithDestructor>>::value);
}

TEST(Optional, Hash) {
  // Test it's usable in std::unordered map (compile time check)
  std::unordered_map<Optional<int>, Optional<int>> obj;
  // Also check the std::hash template can be instantiated by the compiler
  std::hash<Optional<int>>()(none);
  std::hash<Optional<int>>()(3);
}

namespace {

struct WithConstMember {
  /* implicit */ WithConstMember(int val) : x(val) {}
  const int x;
};

// Make this opaque to the optimizer by preventing inlining.
FOLLY_NOINLINE void replaceWith2(Optional<WithConstMember>& o) {
  o.emplace(2);
}

} // namespace

TEST(Optional, ConstMember) {
  // Verify that the compiler doesn't optimize out the second load of
  // o->x based on the assumption that the field is const.
  //
  // Current Optional implementation doesn't defend against that
  // assumption, thus replacing an optional where the object has const
  // members is technically UB and would require wrapping each access
  // to the storage with std::launder, but this prevents useful
  // optimizations.
  //
  // Implementations of std::optional in both libstdc++ and libc++ are
  // subject to the same UB. It is then reasonable to believe that
  // major compilers don't rely on the constness assumption.
  Optional<WithConstMember> o(1);
  int sum = 0;
  sum += o->x;
  replaceWith2(o);
  sum += o->x;
  EXPECT_EQ(sum, 3);
}

TEST(Optional, NoneMatchesNullopt) {
  auto op = make_optional<int>(10);
  op = {};
  EXPECT_FALSE(op.has_value());

  op = make_optional<int>(20);
  op = none;
  EXPECT_FALSE(op.has_value());
}

TEST(Optional, StdOptionalConversions) {
  folly::Optional<int> f = 42;
  std::optional<int> s = static_cast<std::optional<int>>(f);
  EXPECT_EQ(*s, 42);
  EXPECT_EQ(s, f.toStdOptional());
  EXPECT_TRUE(f);

  f = static_cast<folly::Optional<int>>(s);
  EXPECT_EQ(*f, 42);
  EXPECT_TRUE(s);

  folly::Optional<int> e;
  EXPECT_FALSE(static_cast<std::optional<int>>(e));
  EXPECT_FALSE(e.toStdOptional());

  const folly::Optional<int> fc = 12;
  s = static_cast<std::optional<int>>(fc);
  EXPECT_EQ(*s, 12);

  folly::Optional<std::unique_ptr<int>> fp = std::make_unique<int>(42);
  std::optional<std::unique_ptr<int>> sp(std::move(fp));
  EXPECT_EQ(**sp, 42);
  EXPECT_FALSE(fp);
  fp = static_cast<folly::Optional<std::unique_ptr<int>>>(std::move(sp));
  EXPECT_EQ(**fp, 42);
  EXPECT_FALSE(sp);

  folly::Optional<std::unique_ptr<int>> fp2 = std::make_unique<int>(42);
  auto sp2 = std::move(fp2).toStdOptional();
  EXPECT_EQ(**sp2, 42);
  EXPECT_FALSE(fp2);
}

TEST(Optional, MovedFromOptionalIsEmpty) {
  // moved-from folly::Optional is empty, unlike std::optional!
  folly::Optional<int> x = 1;
  EXPECT_TRUE(x);
  auto y = std::move(x);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_FALSE(x);
  EXPECT_TRUE(y);
  folly::Optional<int> z;
  z = std::move(y);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_FALSE(y);
  EXPECT_TRUE(z);

  folly::Optional<std::string> str("hello");
  EXPECT_TRUE(str);
  auto str2 = std::move(str);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_FALSE(str);
  EXPECT_TRUE(str2);
  folly::Optional<std::string> str3;
  str3 = std::move(str2);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_FALSE(str2);
  EXPECT_TRUE(str3);
}
} // namespace folly
