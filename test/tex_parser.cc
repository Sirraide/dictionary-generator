#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <dictgen/backends.hh>

using namespace dict;

namespace {
struct TestOps : LanguageOps {
    auto handle_unknown_macro(TexParser&, std::string_view macro) -> Result<Node::Ptr> override;
    [[nodiscard]] auto to_ipa(std::string_view) -> Result<std::string> override { return "[[ipa]]"; }
};
}

auto TestOps::handle_unknown_macro(TexParser& p, std::string_view macro) -> Result<Node::Ptr> {
    if (macro == "/") return p.text("Found /!");
    if (macro == "definedintestops") return p.text("This is our test macro");
    if (macro == "xyz") {
        auto arg = Try(p.parse_arg());
        return p.group(
            p.text("<foo>", true),
            std::move(arg),
            p.text("</foo>", true)
        );
    }

    return LanguageOps::handle_unknown_macro(p, macro);
}

static auto Convert(std::string_view input, bool strip_macros = false) -> std::string {
    TestOps ops;
    JsonBackend j{ops, false};
    j.current_word = "<f-w>the-current-word</f-w>";
    auto text = j.tex_to_html(input, strip_macros);
    if (j.has_error) throw std::runtime_error(j.errors);
    return text;
}

TEST_CASE("Parse plain text") {
    CHECK(Convert("") == "");
    CHECK(Convert("aa") == "aa");
    CHECK(Convert("aabbcc") == "aabbcc");
    CHECK(Convert("Sphinx of black quartz, judge my vows!") == "Sphinx of black quartz, judge my vows!");
}

TEST_CASE("Braces are skipped") {
    CHECK(Convert("{}") == "");
    CHECK(Convert("a{b}c") == "abc");
    CHECK(Convert("{{a}}{b}{{c}}") == "abc");
    CHECK(Convert("{{{{{{a}}{b}{{c}}}}}}") == "abc");
}

TEST_CASE("Mismatched braces are an error") {
    CHECK_THROWS(Convert("{"));
    CHECK_THROWS(Convert("{{}"));
    CHECK_THROWS(Convert("}"));
    CHECK_THROWS(Convert("{}}"));
    CHECK_THROWS(Convert("{}{"));
    CHECK_THROWS(Convert("{}{}}"));
}

TEST_CASE("Maths is ‘rendered’ verbatim") {
    CHECK(Convert("$a$") == "$a$");
}

TEST_CASE("Escaping braces works") {
    CHECK(Convert("\\{") == "{");
    CHECK(Convert("\\}") == "}");
    CHECK(Convert("{\\{}") == "{");
    CHECK(Convert("{\\}}") == "}");
    CHECK(Convert("\\{{}") == "{");
    CHECK(Convert("\\}{}") == "}");
}

TEST_CASE("Single-character macros") {
    CHECK(Convert("\\-") == "&shy;");
    CHECK(Convert("\\ ") == " ");
    CHECK(Convert("\\&") == "&amp;");
    CHECK(Convert("\\$") == "$");
    CHECK(Convert("\\%") == "%");
    CHECK(Convert("\\#") == "#");
    CHECK(Convert("\\{") == "{");
    CHECK(Convert("\\}") == "}");

    CHECK(Convert("{\\-}") == "&shy;");
    CHECK(Convert("{\\ }") == " ");
    CHECK(Convert("{\\&}") == "&amp;");
    CHECK(Convert("{\\$}") == "$");
    CHECK(Convert("{\\%}") == "%");
    CHECK(Convert("{\\#}") == "#");
    CHECK(Convert("{\\{}") == "{");
    CHECK(Convert("{\\}}") == "}");

    CHECK_THROWS(Convert("\\@"));
}

TEST_CASE("Double backslash is not a valid escape sequence") {
    CHECK_THROWS(Convert("\\\\"));
    CHECK_THROWS(Convert("{\\\\}"));
}

TEST_CASE("Unknown single-character macros are passed to the lang ops") {
    CHECK(Convert("\\/") == "Found /!");
    CHECK_THROWS(Convert("\\@"));
}

TEST_CASE("Unknown macros are passed to the lang ops") {
    CHECK(Convert("\\definedintestops") == "This is our test macro");
    CHECK(Convert("\\xyz{bar}") == "<foo>bar</foo>");
    CHECK_THROWS(Convert("\\definitelynotdefined"));
}

TEST_CASE("Single-argument macros") {
    CHECK(Convert("\\s{a}{b}") == "<f-s>a</f-s>b");
    CHECK(Convert("\\s{a{c}}{b}") == "<f-s>ac</f-s>b");
    CHECK(Convert("\\s{a{\\s{c}}}{b}") == "<f-s>a<f-s>c</f-s></f-s>b");
}

TEST_CASE("Builtin macros") {
    CHECK(Convert("\\par") == "</p><p>");
    CHECK(Convert("\\ldots") == "&hellip;");
    CHECK(Convert("\\this") == "<f-w>the-current-word</f-w>");
    CHECK(Convert("\\Sup{foo}bar") == "<sup>foo</sup>bar");
    CHECK(Convert("\\Sub{foo}bar") == "<sub>foo</sub>bar");
}

TEST_CASE("Refs and labels are dropped") {
    CHECK(Convert("x\\ref{...abab{\\w{ss}}}y") == "xy");
    CHECK(Convert("x\\label{...abab{\\w{ss}}}y") == "xy");
}

TEST_CASE("Comment and sense macros are dropped") {
    CHECK(Convert("\\comment a \\ex b \\comment c") == "a b c");
}
