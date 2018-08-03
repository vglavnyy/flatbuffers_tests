#include <ostream>
#include <utility>
#include <vector>
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/minireflect.h"
#include "flatbuffers/registry.h"
#include "flatbuffers/util.h"
#include "gtest/gtest.h"

// Use global define `FLATBUFFERS_FBS_DIR` for reference to fbs files insted of
// copy to binary dir.

// Parametric test.
// 0: test result
// 1: parser add-in (root_type name;)
// 2: json file name (starts '/') or embedded json
// 3: optional parser error message (if test failed)
// Examples:
// {1, "root_type Empty;", "/test.json", nullptr}
// {0, "root_type Empty;", "{", "", nullptr}

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

 public:
  virtual ~TestFixtureBase() = default;
};

// This will be the base fixture for all parameterized tests.
class ParamTestJsonParser : public TestFixtureBase,
                            public ::testing::WithParamInterface<TestConfig> {
 protected:
  TResult expected_{ TResult::ANY };
  const char *error_msg_ = nullptr;
  std::string json_file_;

  void SetUp() override {
    TestFixtureBase::SetUp();

    parser_.opts = std::get<1>(GetParam()).opts;

    const auto &param = std::get<0>(GetParam());
    expected_ = std::get<0>(param);
    auto parser_ext = std::get<1>(param);
    auto json = std::get<2>(param);
    error_msg_ = std::get<3>(param);

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

TEST_P(ParamTestJsonParser, StrictJsonCheck) {
  const auto done = parser_.Parse(json_file_.c_str());
  ASSERT_EQ(done, expected_) << parser_.error_;
  if (error_msg_) { EXPECT_STREQ(error_msg_, parser_.error_.c_str()); }
  if (false == done) return;

  // if not asserted yet, try to pack->unpack->pack
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

// pre-generate testset
// Test dataset from file [https://www.json.org/JSON_checker/test.zip]
// Use C++11 raw strings for embedded json: R"(some unescaped string)"
static std::vector<TestParam> json_org_dataset(bool strict);

static const auto json_org_dataset_strict = json_org_dataset(true);
static const auto json_org_dataset_nonstrict = json_org_dataset(false);

INSTANTIATE_TEST_CASE_P(
    json_org_default, ParamTestJsonParser,
    ::testing::Combine(::testing::ValuesIn(json_org_dataset_strict),
                       ::testing::Values(ParserTraits())));

INSTANTIATE_TEST_CASE_P(
    json_org_non_strict, ParamTestJsonParser,
    ::testing::Combine(::testing::ValuesIn(json_org_dataset_nonstrict),
                       ::testing::Values(ParserTraitsNonStrict())));

//====================================================================
// Test dataset from file [https://www.json.org/JSON_checker/test.zip]
// Use C++11 raw strings for embedded json: R"(some unescaped string)"
std::vector<TestParam> json_org_dataset(bool strict) {
  constexpr auto _DONE = TResult::DONE;
  constexpr auto _FAIL = TResult::FAIL;
  constexpr auto _ANY = TResult::ANY;
  const auto weak = !strict;
  // clang-format off
return {
  // fail1.json
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    R"("A JSON payload should be an object or array, not a string.")",
    "1:0: error: declaration expected" },
  // fail2.json
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    R"(["Unclosed array")", nullptr },
  // fail3.json
  { weak ? _DONE : _FAIL, R"(table t{f1:string;} root_type t;)",
    R"({unquoted_key: "keys must be quoted"})",
    weak ? "" : "1:0: error: expecting: string constant instead got: unquoted_key" },
  // fail4.json
  { weak ? _DONE : _FAIL, R"(table t{f1:string;} root_type t;)",
    R"(["extra comma",])", nullptr },
  // fail5.json
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    R"(["double extra comma",,])", nullptr },
  // fail6.json
  { _FAIL, R"(table t{f1:string;f2:string;} root_type t;)",
    R"([   , "<-- missing value"])",
    "1:0: error: expecting: string constant instead got: ," },
  // fail6-1.json
  { _FAIL, R"(table t{f1:string;f2:string;} root_type t;)",
    R"(["missing value ->>",  ])",
    weak ? nullptr : "1:0: error: expecting: string constant instead got: ]" },
  // fail7.json
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    R"(["Comma after the close"],)",
    "1:0: error: declaration expected" },
  // fail8.json
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    R"(["Extra close"]])",
    "1:0: error: declaration expected" },
  // fail9.json
  { weak ? _DONE : _FAIL, R"(table t{f1:string;} root_type t;)",
    R"({"f1": "Extra comma",})",
    weak ? nullptr : "1:0: error: expecting: string constant instead got: }" },
  // fail9-1.json
  { weak ? _DONE : _FAIL, R"(table t{} root_type t;)",
    R"({"unexpected": "Extra comma after unexpected field",})",
    weak ? nullptr : "1:0: error: expecting: string constant instead got: }" },
  // fail10.json
  { _FAIL, R"(table t{} root_type t;)",
    R"({"Extra value after close": true} "misplaced quoted value")", nullptr },
  // fail11.json
  { _FAIL, R"(table t{} root_type t;)",
    R"({"Illegal expression": 1 + 2})", nullptr },
  // fail12.json
  { _FAIL, R"(table t{} root_type t;)",
    R"({"Illegal invocation": alert()})", nullptr },
  // Incompatible fail13.json
  { _ANY, R"(table t{} root_type t;)",
    R"({"Numbers cannot have leading zeroes": 013})", nullptr },
  // Incompatible fail13-1.json
  { _ANY, R"(table t{f1:int;} root_type t;)",
    R"({"f1": 013})", nullptr },
  // Incompatible fail14.json
  { _ANY, R"(table t{} root_type t;)",
    R"({"Numbers cannot be hex": 0x14})", nullptr },
  // Incompatible fail14-1.json
  { _ANY, R"(table t{f1:int;} root_type t;)",
    R"({"f1": 0x14})", nullptr },
  // Incompatible fail15.json
  { _ANY, R"(table t{f1:string;} root_type t;)",
    R"(["Illegal backslash escape: \x15"])", nullptr },
  // fail16.json
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    R"([\naked])",
    "1:0: error: illegal character: \\" },
  // fail17.json
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    R"(["Illegal backslash escape: \017"])",
    "1:0: error: unknown escape code in string constant" },
  // Ignore fail18.json "[[[[[[[[[[[[[[[[[[[["Too deep"]]]]]]]]]]]]]]]]]]]]"
  // fail19.json
  { _FAIL, R"(table t{} root_type t;)",
    R"({"Missing colon" null})",
    "1:0: error: expecting: : instead got: null" },
  // fail20.json
  { _FAIL, R"(table t{} root_type t;)",
    R"({"Double colon":: null})",
    "1:0: error: cannot parse value starting with: :" },
  // fail21.json
  { _FAIL, R"(table t{} root_type t;)",
    R"({"Comma instead of colon", null})",
    "1:0: error: expecting: : instead got: ," },
  // fail22.json
  { _FAIL, R"(table t{f1:bool;} root_type t;)",
    R"(["Colon instead of comma": false])", nullptr },
  // fail23.json
  { _FAIL, R"(table t{f1:string;f2:bool;} root_type t;)",
    R"(["Bad value", truth])", nullptr },
  // Incompatible - fail24.json
  { _ANY, R"(table t{f1:string;} root_type t;)",
    R"(['single quote'])", nullptr },
  // fail25.json
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    R"(["	tab	character	in	string	"])", nullptr },
  // fail26.json
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    R"(["tab\   character\   in\  string\  "])", nullptr },
  // fail27.json (non-escaped line break)
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    "[\"line" "\nbreak\"]", nullptr },
  // fail28.json (non-escaped line break)
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    "[\"line\\" "\nbreak\"]", nullptr },
  // fail29.json
  { _FAIL, R"(table t{f1:float;} root_type t;)",
    R"([0e])", nullptr },
  // fail30.json
  { _FAIL, R"(table t{f1:float;} root_type t;)",
    R"([0e+])", nullptr },
  // fail31.json
  { _FAIL, R"(table t{f1:float;} root_type t;)",
    R"([0e+-1])", nullptr },
  // fail32.json
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    R"({"Comma instead if closing brace": true,)", nullptr },
  // fail33.json
  { _FAIL, R"(table t{f1:string;} root_type t;)",
    R"(["mismatch"})", nullptr },
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
  { _DONE, R"(table t{} root_type t;)",
    "/json.org/pass3.json", nullptr }
};
  // clang-format on
}

//====================================================================
// Dataset from [https://github.com/nst/JSONTestSuite]
std::vector<TestParam> seriot_dataset(bool strict) {
  constexpr auto _DONE = TResult::DONE;
  constexpr auto _FAIL = TResult::FAIL;
  constexpr auto _ANY = TResult::ANY;
  const auto weak = !strict;
  // clang-format off
  return {};
  // clang-format on
}
