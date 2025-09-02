#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <dictgen/frontend.hh>
#include <dictgen/backends.hh>

using namespace dict;

namespace {
struct TestOps : LanguageOps {
    [[nodiscard]] auto to_ipa(str) -> Result<std::string> override { return "[[ipa]]"; }
};
}

static auto Emit(str input) -> EmitResult {
    TestOps ops;
    JsonBackend backend{ops, false};
    Generator gen{backend};
    gen.parse(input);
    return gen.emit_to_string();
}

static void CheckContains(str input, const std::string& substr) {
    auto [output, has_error] = Emit(input);
    CHECK(not has_error);
    CHECK_THAT(std::string(str(output).trim()), Catch::Matchers::ContainsSubstring(substr));
}

static void CheckExact(str input, str expected) {
    auto [output, has_error] = Emit(input);
    CHECK(not has_error);
    CHECK(std::string(str(output).trim()) == expected);
}

static void CheckError(str input, const std::string& substr) {
    auto [output, has_error] = Emit(input);
    CHECK(has_error);
    CHECK(std::string(str(output).trim()) == substr);
}

TEST_CASE("JSON backend: Disallow \\comment and \\ex if the definition is empty") {
    CheckError(
        "x|y|z|\\comment abcd",
        "In Line 1: \\comment is not allowed in an empty sense or empty primary definition. Use \\textit{...} instead."
    );

    CheckError(
        "x|y|z|\\\\\\comment abcd",
        "In Line 1: \\comment is not allowed in an empty sense or empty primary definition. Use \\textit{...} instead."
    );

    CheckError(
        "x|y|z|\\ex abcd",
        "In Line 1: \\ex is not allowed in an empty sense or empty primary definition."
    );

    CheckError(
        "x|y|z|\\\\\\ex abcd",
        "In Line 1: \\ex is not allowed in an empty sense or empty primary definition."
    );
}

TEST_CASE("JSON backend: search normalisation") {
    TestOps ops;
    JsonBackend J{ops, false};
    CHECK(J.NormaliseForSearch("abcd") == "abcd");
    CHECK(J.NormaliseForSearch("ábćd") == "abcd");
    CHECK(J.NormaliseForSearch("ạ́́bć̣́d") == "abcd");
    CHECK(J.NormaliseForSearch("  a  bc’’' '‘‘..-d-") == "d bc a");
    CHECK(J.NormaliseForSearch("łŁlL") == "llll");
    CHECK(J.NormaliseForSearch("®©™@ç") == "rctmc");
    CHECK(J.NormaliseForSearch("ḍriłv́ẹ́âǎ") == "drilveaa");
    CHECK(J.NormaliseForSearch("+-/*!?\"$%&'()[]{},._^`<>:;=~\\@") == "");
}

TEST_CASE("Bogus entries") {
    CheckError(
        "\\\\a|||",
        "In Line 1: '\\\\' cannot be used in the lemma"
    );

    CheckError(
        "\\comment|||",
        "In Line 1: '\\comment' cannot be used in the lemma"
    );

    CheckError(
        "\\ex|||",
        "In Line 1: '\\ex' cannot be used in the lemma"
    );

    CheckError(
        "foo",
        "In Line 1: An entry must contain at least one '|' or '>'"
    );

    CheckError(
        "\\comment > b",
        "In Line 1: '\\comment' cannot be used in a reference entry"
    );

    CheckError(
        "a > \\comment",
        "In Line 1: '\\comment' cannot be used in a reference entry"
    );

    CheckError(
        "\\ex > b",
        "In Line 1: '\\ex' cannot be used in a reference entry"
    );

    CheckError(
        "a > \\ex",
        "In Line 1: '\\ex' cannot be used in a reference entry"
    );

    CheckError(
        "\\\\ > b",
        "In Line 1: '\\\\' cannot be used in a reference entry"
    );

    CheckError(
        "a > \\\\",
        "In Line 1: '\\\\' cannot be used in a reference entry"
    );
}
