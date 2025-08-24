#include <dictgen/frontend.hh>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <print>

using namespace dict;

struct TestOps : LanguageOps {
    auto handle_unknown_macro(TexParser&, std::string_view macro) -> Result<Node::Ptr> override;
    [[nodiscard]] auto to_ipa(std::string_view) -> Result<std::string> override { return "[[ipa]]"; }
};

void Check(std::string_view input, std::string_view output) {
    TestOps ops;
    TypstBackend typ{ops};
    Generator gen{typ};
    input = stream(input).trim().text();
    output = stream(output).trim().text();
    gen.parse(input);
    auto res = gen.emit_to_string();
    res.backend_output = stream(res.backend_output).trim().text();
    if (res.has_error) throw std::runtime_error(res.backend_output);
    CHECK(res.backend_output == output);
}


TEST_CASE("Typst backend: some ULTRAFRENCH entries") {
    Check(
        "aub’heír|v. (in)tr.|obéir|To obey (+\\s{part} sbd.)",
        "#dictionary-entry([aub’heír],[v. (in)tr.],[obéir],[],dictionary-sense([To obey (\\+#smallcaps[part] sbd.).],[]))"
    );

    Check(
        "ánvé|v. tr.|animer|+\\s{acc} To bring to life, animate",
        "#dictionary-entry([ánvé],[v. tr.],[animer],[],dictionary-sense([\\+#smallcaps[acc] To bring to life, animate.],[]))"
    );

    Check(
        "A|B|C|D\\\\ E\\comment F\\ex G\\comment H",
        "#dictionary-entry("
            "[A],[B],[C],[],"
            "dictionary-sense([D.],[]),"
            "dictionary-sense("
                "[E.],"
                "dictionary-comment[F.],"
                "dictionary-example([G.],dictionary-comment[H.])"
            ")"
        ")"
    );
}

