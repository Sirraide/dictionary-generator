#include <dictgen/frontend.hh>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <print>

using namespace dict;

namespace {
struct TestOps : LanguageOps {
    [[nodiscard]] auto to_ipa(str) -> Result<std::string> override { return "/ipa/"; }
};
}

static void Check(str input, str output_raw) {
    TestOps ops;
    TypstBackend typ{ops};
    Generator gen{typ};
    input.trim();
    auto output = output_raw.remove_all(" \n\r\t\v\f");
    gen.parse(input);
    auto res = gen.emit_to_string();
    res.backend_output = str(res.backend_output).remove_all(" \n\r\t\v\f");
    if (res.has_error) throw std::runtime_error(res.backend_output);
    if (res.backend_output != output) {
        std::println("A: ⟨{}⟩", res.backend_output);
        std::println("B: ⟨{}⟩", output);
    }
    CHECK(res.backend_output == output);
}

TEST_CASE("Typst backend: some ULTRAFRENCH entries") {
    Check(
        "aub’heír|v. (in)tr.|obéir|To obey (+\\s{part} sbd.)",
        "#dictionary-entry(("
            "word: [aub’heír], "
            "pos: [v. (in)tr.], "
            "etym: [obéir], "
            "forms: [], "
            "ipa: [/ipa/],"
            "prim_def: (def: [To obey (\\+#smallcaps[part] sbd.).], comment: [], examples: ()),"
            "senses: ()"
        "))"
    );

    Check(
        "ánvé|v. tr.|animer|+\\s{acc} To bring to life, animate",
        "#dictionary-entry(("
            "word:[ánvé],"
            "pos:[v.tr.],"
            "etym:[animer],"
            "forms:[],"
            "ipa: [/ipa/],"
            "prim_def:("
                "def:[\\+#smallcaps[acc]Tobringtolife,animate.],"
                "comment:[],"
                "examples:()"
            "),"
            "senses:()"
        "))"
    );

    Check(
        "A|B|C|D\\\\ E\\comment F\\ex G\\comment H",
        "#dictionary-entry(("
            "word:[A],"
            "pos:[B],"
            "etym:[C],"
            "forms:[],"
            "ipa: [/ipa/],"
            "prim_def:("
                "def:[D.],"
                "comment:[],"
                "examples:()"
            "),"
            "senses:("
                "("
                    "def:[E.],"
                    "comment:[F.],"
                    "examples:("
                        "("
                            "text:[G.],"
                            "comment:[H.]"
                        "),"
                    ")"
                "),"
            ")"
        "))"
    );

    Check(
        "a|b|c|\\\\d",
        "#dictionary-entry(("
            "word:[a],"
            "pos:[b],"
            "etym:[c],"
            "forms:[],"
            "ipa: [/ipa/],"
            "prim_def:("
                "def:[],"
                "comment:[],"
                "examples:()"
            "),"
            "senses:("
                "("
                    "def:[d.],"
                    "comment:[],"
                    "examples:()"
                "),"
            ")"
        "))"
    );

    Check("a > b", "#dictionary-reference([a],[b])");
}

