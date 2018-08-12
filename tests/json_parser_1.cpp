#include <ostream>
#include <utility>
#include <vector>
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/minireflect.h"
#include "flatbuffers/registry.h"
#include "flatbuffers/util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_generated.h"

// Use global define `FLATBUFFERS_FBS_DIR` for reference to fbs files insted of
// copy to binary dir.

// Parametric test.
// 0: test result
// 1: parser add-in (root_type fbt.name;)
// 2: json file name (starts '/') or embedded json
// 3: optional parser error message which tested by substring checking
// Examples:
// {1, "root_type fbt.Empty;", "/test.json", nullptr}
// {0, "root_type fbt.Empty;", "{", "", nullptr}

enum class TResult {
  FAIL = 0,
  DONE = 1,
  // implementation defined
  ANY = 2
};
// helpers
constexpr static inline bool operator==(const TResult a, const bool b) {
  return (a == TResult::ANY) ? true : a == (b ? TResult::DONE : TResult::FAIL);
}
constexpr static inline bool operator==(const bool a, const TResult b) {
  return b == a;
}
static inline ::std::ostream &operator<<(::std::ostream &os, const TResult &e) {
  switch (e) {
    case TResult::FAIL: return os << "FAIL";
    case TResult::DONE: return os << "DONE";
    case TResult::ANY: return os << "ANY";
    default: return os << e;
  }
}

struct ParserTraits {
  flatbuffers::IDLOptions opts;
  ParserTraits() {
    // request strict json by default
    opts.skip_unexpected_fields_in_json = true;
    opts.strict_json = true;
  }
  ParserTraits(ParserTraits &&) = default;
  ParserTraits(const ParserTraits &) = default;
};

static inline auto ParserTraitsNonStrict() {
  ParserTraits traits;
  traits.opts.strict_json = false;
  return traits;
}

static inline ::std::ostream &operator<<(::std::ostream &os,
                                         const ParserTraits &traits) {
  return os << "IDLOptions {"
            << "not implemented"
            << "}";
}
using TestParam = std::tuple<TResult, const char *, const char *, const char *>;
using TestConfig = std::tuple<TestParam, ParserTraits>;

// base fixture cass
class TestFixtureBase : public ::testing::Test {
 protected:
  flatbuffers::Parser parser_;

  const char **const fbs_include_dir() const {
    static const char *fbs_include_directories_[] = { FLATBUFFERS_FBS_DIR,
                                                      nullptr };
    return fbs_include_directories_;
  }

  TestFixtureBase() {
    ParserTraits traits;
    parser_.opts = traits.opts;
  }

  void SetUp() override {
    std::string schemafile;
    auto full_fname =
        flatbuffers::ConCatPathFileName(FLATBUFFERS_FBS_DIR, "test.fbs");

    ASSERT_TRUE(flatbuffers::LoadFile(full_fname.c_str(), false, &schemafile));
    ASSERT_TRUE(parser_.Parse(schemafile.c_str(), fbs_include_dir()))
        << parser_.error_;
  }

  // Print current content of parser's builder to a string.
  // Then parse this string and print again.
  // After that compare both string.
  void ParserPrintDecodePrintTest();

 public:
  virtual ~TestFixtureBase() = default;
};

class TestJsonParser : public TestFixtureBase {
 public:
  virtual ~TestJsonParser() = default;
};

// This will be the base fixture for all parameterized tests.
class ParamTestJsonParser : public TestFixtureBase,
                            public ::testing::WithParamInterface<TestConfig> {
 protected:
  TResult expected_{ TResult::ANY };
  const char *error_substr_ = nullptr;
  std::string json_file_;

  void SetUp() override {
    TestFixtureBase::SetUp();

    parser_.opts = std::get<1>(GetParam()).opts;

    const auto &param = std::get<0>(GetParam());
    expected_ = std::get<0>(param);
    auto parser_ext = std::get<1>(param);
    auto json = std::get<2>(param);
    error_substr_ = std::get<3>(param);

    // finalize parser
    if (parser_ext && std::strlen(parser_ext)) {
      ASSERT_TRUE(parser_.Parse(parser_ext)) << parser_.error_;
    }

    if (json) {
      // file name always starts from '/'
      if (*json == '/') {
        auto full_fname =
            flatbuffers::ConCatPathFileName(JSON_SAMPLES_DIR, json);
        ASSERT_TRUE(
            flatbuffers::LoadFile(full_fname.c_str(), false, &json_file_));
      } else {
        // set content from params
        json_file_ = json;
      }
    }
  }

 public:
  virtual ~ParamTestJsonParser() = default;
};

void TestFixtureBase::ParserPrintDecodePrintTest() {
  std::string text_1;
  ASSERT_TRUE(flatbuffers::GenerateText(
      parser_, parser_.builder_.GetBufferPointer(), &text_1));
  ASSERT_TRUE(parser_.Parse(text_1.c_str())) << parser_.error_;
  std::string text_2;
  ASSERT_TRUE(flatbuffers::GenerateText(
      parser_, parser_.builder_.GetBufferPointer(), &text_2));
  ASSERT_FALSE(text_2.empty());
  ASSERT_STRCASEEQ(text_1.c_str(), text_2.c_str());
}

TEST_F(TestJsonParser, LeadingZerosResearchTest) {
  // bool parse problem
  ASSERT_TRUE(parser_.Parse("root_type fbt.tIntInt;")) << parser_.error_;
  const auto json = "[0999, 001987]";
  const auto done = parser_.Parse(json);
  ASSERT_EQ(done, true) << parser_.error_;
  using T = fbt::tIntInt;
  flatbuffers::Verifier verifier(parser_.builder_.GetBufferPointer(),
    parser_.builder_.GetSize());
  ASSERT_TRUE(verifier.VerifyBuffer<T>());
  auto t =
    flatbuffers::GetRoot<T>(parser_.builder_.GetBufferPointer());
  ASSERT_TRUE(t->Verify(verifier));
  ASSERT_EQ(t->f1(), 999);
  ASSERT_EQ(t->f2(), 1987);
}

TEST_F(TestJsonParser, BoolResearchTest) {
  // bool parse problem
  ASSERT_TRUE(parser_.Parse("root_type fbt.tBool;")) << parser_.error_;
  const auto json = "[true]";
  const auto done = parser_.Parse(json);
  ASSERT_EQ(done, true) << parser_.error_;

  flatbuffers::Verifier verifier(parser_.builder_.GetBufferPointer(),
                                 parser_.builder_.GetSize());
  ASSERT_TRUE(verifier.VerifyBuffer<fbt::tBool>());
  auto t =
      flatbuffers::GetRoot<fbt::tBool>(parser_.builder_.GetBufferPointer());
  ASSERT_TRUE(t->Verify(verifier));
  ASSERT_TRUE(t->f1());
}

TEST_P(ParamTestJsonParser, SimpleJsonCheck) {
  const auto done = parser_.Parse(json_file_.c_str());
  ASSERT_EQ(done, expected_) << parser_.error_;
  if (error_substr_) {
    EXPECT_THAT(parser_.error_.c_str(), ::testing::HasSubstr(error_substr_));
  }
  // leave test if parser failed.
  if (false == done) return;
  // Additional tests:
  ParserPrintDecodePrintTest();
}

// pre-generate testset
// Test dataset from file [https://www.json.org/JSON_checker/test.zip]
// Use C++11 raw strings for embedded json: R"(some unescaped string)"
static std::vector<TestParam> json_org_dataset(bool strict);

std::vector<TestParam> seriot_dataset(bool strict);

static const auto json_org_dataset_strict = json_org_dataset(true);
static const auto json_org_dataset_nonstrict = json_org_dataset(false);

static const auto seriot_dataset_strict = seriot_dataset(true);
static const auto seriot_dataset_nonstrict = seriot_dataset(false);

INSTANTIATE_TEST_CASE_P(
    json_org_default, ParamTestJsonParser,
    ::testing::Combine(::testing::ValuesIn(json_org_dataset_strict),
                       ::testing::Values(ParserTraits())));

INSTANTIATE_TEST_CASE_P(
    json_org_non_strict, ParamTestJsonParser,
    ::testing::Combine(::testing::ValuesIn(json_org_dataset_nonstrict),
                       ::testing::Values(ParserTraitsNonStrict())));

INSTANTIATE_TEST_CASE_P(
    seriot_default, ParamTestJsonParser,
    ::testing::Combine(::testing::ValuesIn(seriot_dataset_strict),
                       ::testing::Values(ParserTraits())));

// INSTANTIATE_TEST_CASE_P(
//  seriot_non_strict, ParamTestJsonParser,
//  ::testing::Combine(::testing::ValuesIn(seriot_dataset_nonstrict),
//    ::testing::Values(ParserTraitsNonStrict())));

//====================================================================
// Modified test dataset from [https://www.json.org/JSON_checker/test.zip]
// FlatBuffers doesn't support a typed field name with spaces or utf characters.
// Grammar rules for an indentifier is: ident = [a-zA-Z_][a-zA-Z0-9_]*
// Use C++11 raw strings for embedded json: R"(some unescaped string)".
std::vector<TestParam> json_org_dataset(bool strict) {
  constexpr auto _DONE = TResult::DONE;
  constexpr auto _FAIL = TResult::FAIL;
  constexpr auto _ANY = TResult::ANY;
  const auto weak = !strict;
  // clang-format off
return {
  // fail1.json
  { _FAIL, "root_type fbt.tEmpty;",
    R"("A JSON payload should be an object or array, not a string.")",
    "error: declaration expected" },
  // fail2.json
  { _FAIL, "root_type fbt.tStr;", R"(["Unclosed array")", nullptr },
  // fail3.json
  { weak ? _DONE : _FAIL, "root_type fbt.tStr;",
    R"({unquoted_key: "keys must be quoted"})",
    weak ? "" : "error: expecting: string constant instead got: unquoted_key" },
  // fail4.json
  { weak ? _DONE : _FAIL, "root_type fbt.tStr;", R"(["extra comma",])", nullptr },
  // fail5.json
  { _FAIL, "root_type fbt.tStr;", R"(["double extra comma",,])", nullptr },
  // fail6.json
  { _FAIL, "root_type fbt.tStrStr;", R"([   , "<-- missing value"])",
    "error: expecting: string constant instead got: ," },
  // fail6-1.json
  { _FAIL, "root_type fbt.tStrStr;", R"(["missing value ->>",  ])",
    weak ? nullptr : "error: expecting: string constant instead got: ]" },
  // fail7.json
  { _FAIL, "root_type fbt.tStr;", R"(["Comma after the close"],)",
    "error: declaration expected" },
  // fail8.json
  { _FAIL, "root_type fbt.tStr;", R"(["Extra close"]])",
    "error: declaration expected" },
  // fail9.json
  { weak ? _DONE : _FAIL, "root_type fbt.tStr;", R"({"f1": "Extra comma",})",
    weak ? nullptr : "error: expecting: string constant instead got: }" },
  // fail9-1.json
  { weak ? _DONE : _FAIL, "root_type fbt.tEmpty;",
    R"({"unexpected": "Extra comma after unexpected field",})",
    weak ? nullptr : "error: expecting: string constant instead got: }" },
  // fail10.json
  { _FAIL, "root_type fbt.tEmpty;",
    R"({"Extra value after close": true} "misplaced quoted value")", nullptr },
  // fail11.json
  { _FAIL, "root_type fbt.tEmpty;", R"({"Illegal expression": 1 + 2})", nullptr },
  // fail12.json
  { _FAIL, "root_type fbt.tEmpty;", R"({"Illegal invocation": alert()})", nullptr },
  // Incompatible fail13.json
  { _ANY, "root_type fbt.tEmpty;",
    R"({"Numbers cannot have leading zeroes": 013})", nullptr },
  // Incompatible fail13-1.json
  { _ANY, "root_type fbt.tInt;", R"({"f1": 013})", nullptr },
  // Incompatible fail14.json
  { _ANY, "root_type fbt.tEmpty;", R"({"Numbers cannot be hex": 0x14})", nullptr },
  // Incompatible fail14-1.json
  { _ANY, "root_type fbt.tInt;", R"({"f1": 0x14})", nullptr },
  // Incompatible fail15.json
  { _ANY, "root_type fbt.tStr;", R"(["Illegal backslash escape: \x15"])", nullptr },
  // fail16.json
  { _FAIL, "root_type fbt.tStr;", R"([\naked])", "error: illegal character: \\" },
  // fail17.json
  { _FAIL, "root_type fbt.tStr;", R"(["Illegal backslash escape: \017"])",
    "error: unknown escape code in string constant" },
  // Ignore fail18.json "[[[[[[[[[[[[[[[[[[[["Too deep"]]]]]]]]]]]]]]]]]]]]"
  // fail19.json
  { _FAIL, "root_type fbt.tEmpty;", R"({"Missing colon" null})",
    "error: expecting: : instead got: null" },
  // fail20.json
  { _FAIL, "root_type fbt.tEmpty;", R"({"Double colon":: null})",
    "error: cannot parse value starting with: :" },
  // fail21.json
  { _FAIL, "root_type fbt.tEmpty;", R"({"Comma instead of colon", null})",
    "error: expecting: : instead got: ," },
  // fail22.json
  { _FAIL, "root_type fbt.tBool;", R"(["Colon instead of comma": false])", nullptr },
  // fail23.json
  { _FAIL, "root_type fbt.tStrBool;", R"(["Bad value", truth])", nullptr },
  // Incompatible - fail24.json
  { _ANY, "root_type fbt.tStr;", R"(['single quote'])", nullptr },
  // fail25.json
  { _FAIL, "root_type fbt.tStr;", R"(["	tab	character	in	string	"])", nullptr },
  // fail26.json
  { _FAIL, "root_type fbt.tStr;", R"(["tab\   character\   in\  string\  "])",
    nullptr },
  // fail27.json (non-escaped line break)
  { _FAIL, "root_type fbt.tStr;", "[\"line" "\nbreak\"]", nullptr },
  // fail28.json (non-escaped line break)
  { _FAIL, "root_type fbt.tStr;", "[\"line\\" "\nbreak\"]", nullptr },
  // fail29.json
  { _FAIL, "root_type fbt.tFloat;", R"([0e])", nullptr },
  // fail30.json
  { _FAIL, "root_type fbt.tFloat;", R"([0e+])", nullptr },
  // fail31.json
  { _FAIL, "root_type fbt.tFloat;", R"([0e+-1])", nullptr },
  // fail32.json
  { _FAIL, "root_type fbt.tStr;", R"({"Comma instead if closing brace": true,)",
    nullptr },
  // fail33.json
  { _FAIL, "root_type fbt.tStr;", R"(["mismatch"})", nullptr },
  // pass1.json
  { _DONE,
  R"(
  table t{}
  table tt{
    f1 : string;
    f2 : t;
    f3 : t;
    f4 : [string];
    f5 : int;
    f6 : bool;
    f7 : bool;
    f8 : [string];
    f9 : t;
    f10: float; f11: float; f13: float; f14: float; f15: float;
    f16: float; f17: float; f18: float; f19: float; f20: float;
    f21: string;
  }
  root_type tt;
  )",
  "/json.org/pass1.json", nullptr },
  // pass2.json
  // Ignore: [[[[[[[[[[[[[[[[[[["Not too deep"]]]]]]]]]]]]]]]]]]]
  // pass3.json
  { _DONE, "root_type fbt.tEmpty;",
    "/json.org/pass3.json", nullptr }
};
  // clang-format on
}

// FlatBuffers Test Root generator
#define FBRT(rtype) "root_type fbt." rtype ";"
// Name of test file form nst testset
#define NSTF(fname) "/nst.JSONTestSuite/" fname ".json"

//====================================================================
// Dataset from [https://github.com/nst/JSONTestSuite]
std::vector<TestParam> seriot_dataset(bool strict) {
  constexpr auto _DONE = TResult::DONE;
  constexpr auto _FAIL = TResult::FAIL;
  constexpr auto _IMPL = TResult::ANY;
  const auto _F_D_ = strict ? _FAIL : _DONE;
  const auto _F_F_ = strict ? _FAIL : _FAIL;

  const auto weak = !strict;
  return {
    { _FAIL, FBRT("tIntBool"), NSTF("n_array_1_true_without_comma"), nullptr },
    { _FAIL, FBRT("tIntBool"), NSTF("n_array_1_true_without_comma"), nullptr },
    { _FAIL, FBRT("tStr"), NSTF("n_array_a_invalid_utf8"), nullptr },
    { _FAIL, FBRT("tStrInt"), NSTF("n_array_colon_instead_of_comma"), nullptr },
    { _FAIL, FBRT("tStr"), NSTF("n_array_comma_after_close"), nullptr },
    { _FAIL, FBRT("tStrInt"), NSTF("n_array_comma_and_number"), nullptr },
    { _FAIL, FBRT("tIntInt"), NSTF("n_array_double_comma"), nullptr },
    { _FAIL, FBRT("tIntIntInt"), NSTF("n_array_double_comma"), nullptr },
    { _FAIL, FBRT("tStrStrStr"), NSTF("n_array_double_extra_comma"), nullptr },
    { _FAIL, FBRT("tStrStr"), NSTF("n_array_double_extra_comma"), nullptr },
    { _FAIL, FBRT("tStr"), NSTF("n_array_extra_close"), nullptr },
    { _F_D_, FBRT("tStr"), NSTF("n_array_extra_comma"), nullptr },
    { _FAIL, FBRT("tStr"), NSTF("n_array_incomplete_invalid_value"), nullptr },
    { _FAIL, FBRT("tStr"), NSTF("n_array_incomplete"), nullptr },
    { _FAIL, FBRT("tIntVInt"), NSTF("n_array_inner_array_no_comma"), nullptr },
    { _FAIL, FBRT("tStr"), NSTF("n_array_invalid_utf8"), nullptr },
    { _FAIL, FBRT("tIntInt"), NSTF("n_array_items_separated_by_semicolon"),
      nullptr },
    { _FAIL, FBRT("tIntInt"), NSTF("n_array_just_comma"), nullptr },
    { _F_F_, FBRT("tInt"), NSTF("n_array_just_comma"), nullptr },
    { _FAIL, FBRT("tInt"), NSTF("n_array_just_minus"), nullptr },
    { _FAIL, FBRT("tFloat"), NSTF("n_array_just_minus"), nullptr },
    { _FAIL, FBRT("tStr"), NSTF("n_array_just_minus"), nullptr },
    { _FAIL, FBRT("tStrStr"), NSTF("n_array_missing_value"), nullptr },
    { _FAIL, FBRT("tStrIntInt"), NSTF("n_array_newlines_unclosed"), nullptr },
    { _F_D_, FBRT("tInt"), NSTF("n_array_number_and_comma"), nullptr },
    { _FAIL, FBRT("tIntInt"), NSTF("n_array_number_and_comma"), nullptr },
    { _FAIL, FBRT("tInt"), NSTF("n_array_number_and_several_commas"), nullptr },
    { _FAIL, FBRT("tIntInt"), NSTF("n_array_number_and_several_commas"),
      nullptr },
    { _FAIL, FBRT("tStr"), NSTF("n_array_spaces_vertical_tab_formfeed"),
      nullptr },
    { _FAIL, FBRT("tStr"), NSTF("n_array_star_inside"), nullptr },
    { _FAIL, FBRT("tInt"), NSTF("n_array_unclosed_trailing_comma"), nullptr },
    { _FAIL, FBRT("tIntInt"), NSTF("n_array_unclosed_trailing_comma"), nullptr },
    { _FAIL, FBRT("ttEmpty"), NSTF("n_array_unclosed_with_object_inside"), nullptr },
    { _FAIL, FBRT("tStr"), NSTF("n_array_unclosed"), nullptr },
    { _FAIL, FBRT("tBool"), NSTF("n_incomplete_false"), nullptr },
    { _FAIL, FBRT("tStr"), NSTF("n_incomplete_null"), nullptr },
    { _FAIL, FBRT("tBool"), NSTF("n_incomplete_true"), nullptr },
    { _FAIL, FBRT("tFloat"), NSTF("n_multidigit_number_then_00"), nullptr },
    { _FAIL, FBRT("tFloat"), NSTF("n_number_-1.0."), nullptr },
    { _FAIL, FBRT("tInt"), NSTF("n_number_-01"), nullptr },
    { _FAIL, FBRT("tFloat"), NSTF("n_number_-01"), nullptr },
    { _FAIL, FBRT("tInt"), NSTF("n_number_-2."), nullptr },
    // Float point number "-2." is valid for C/C++ and valid for flatbuffers
    { _IMPL, FBRT("tFloat"), NSTF("n_number_-2."), nullptr },
    { _FAIL, FBRT("tFloat"), NSTF("n_number_-NaN"), nullptr },
    { _FAIL, FBRT("tInt"), NSTF("n_number_.-1"), nullptr },
    { _FAIL, FBRT("tFloat"), NSTF("n_number_.-1"), nullptr },
    { _FAIL, FBRT("tFloat"), NSTF("n_number_.2e-3"), nullptr },
  };
}
