#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <dictgen/frontend.hh>
#include <dictgen/backends.hh>

using namespace dict;

struct TestOps : LanguageOps {
    [[nodiscard]] auto to_ipa(std::string_view) -> Result<std::string> override { return "[[ipa]]"; }
};

auto Emit(std::string_view input) -> EmitResult {
    TestOps ops;
    JsonBackend backend{ops, false};
    Generator gen{backend};
    gen.parse(input);
    return gen.emit_to_string();
}

void CheckContains(std::string_view input, const std::string& substr) {
    auto [output, has_error] = Emit(input);
    CHECK(not has_error);
    CHECK_THAT(output, Catch::Matchers::ContainsSubstring(substr));
}

void CheckExact(std::string_view input, std::string_view expected) {
    auto [output, has_error] = Emit(input);
    CHECK(not has_error);
    CHECK(output == expected);
}

void CheckError(std::string_view input, const std::string& substr) {
    auto [output, has_error] = Emit(input);
    CHECK(has_error);
    CHECK_THAT(output, Catch::Matchers::ContainsSubstring(substr));
}

TEST_CASE("JSON backend: Disallow \\comment and \\ex if the definition is empty") {
    CheckError(
        "x|y|z|\\comment abcd",
        "\\comment is not allowed in an empty sense or empty primary definition"
    );

    CheckError(
        "x|y|z|\\\\\\comment abcd",
        "\\comment is not allowed in an empty sense or empty primary definition"
    );

    CheckError(
        "x|y|z|\\ex abcd",
        "\\ex is not allowed in an empty sense or empty primary definition"
    );

    CheckError(
        "x|y|z|\\\\\\ex abcd",
        "\\ex is not allowed in an empty sense or empty primary definition"
    );
}
