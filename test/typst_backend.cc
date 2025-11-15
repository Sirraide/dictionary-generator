#include <dictgen/frontend.hh>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <print>

using namespace dict;

namespace {
struct TestOps : LanguageOps {
    [[nodiscard]] auto to_ipa(str input) -> Result<std::string> override { return std::format("//{}//", input); }
    auto handle_unknown_macro(TexParser& p, str macro) -> Result<Node::Ptr> override {
        if (macro == "raw") return p.formatting("#raw-typst[$a$_b_*c*]");
        if (macro == "L") return p.formatting("#super[L]");
        return LanguageOps::handle_unknown_macro(p, macro);
    }
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
            "ipa: [//aub’heír//],"
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
            "ipa: [//ánvé//],"
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
            "ipa: [//A//],"
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
            "ipa: [//a//],"
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

TEST_CASE("Typst backend should not escape formatting") {
    TestOps ops;
    TypstBackend b{ops};
    CHECK(b.convert("\\raw") == "#raw-typst[$a$_b_*c*]");
}

TEST_CASE("Typst: \\- works properly") {
    TestOps ops;
    TypstBackend b{ops};
    CHECK(b.convert("a\\-b") == "a-?b");
}

TEST_CASE("Typst: formatting in word") {
    Check(
        "aub’heír\\L|v. (in)tr.|obéir|To obey (+\\s{part} sbd.)",
        "#dictionary-entry(("
            "word: [aub’heír#super[L]], "
            "pos: [v. (in)tr.], "
            "etym: [obéir], "
            "forms: [], "
            "ipa: [//aub’heír//],"
            "prim_def: (def: [To obey (\\+#smallcaps[part] sbd.).], comment: [], examples: ()),"
            "senses: ()"
        "))"
    );
}

TEST_CASE("Typst: TeX conversion is also applied in reference entries") {
    Check(
        "ac’hes > \\w{a} + \\w{c’hes}",
        "#dictionary-reference([ac’hes], [#lemma[a] \\+ #lemma[c’hes]])"
    );
}
