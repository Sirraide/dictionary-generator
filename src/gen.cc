#include "dictgen-v8.hh"

#include <clopts.hh>
#include <dictgen/backends.hh>
#include <dictgen/core.hh>
#include <dictgen/frontend.hh>

using namespace dict;
using namespace dict::gen;

namespace cmd {
using namespace command_line_options;
using options = clopts< // clang-format off
    positional<"dictionary", "Input dictionary file", file<>>,
    option<"--driver", "Main JavaScript file to run", file<>, true>,
    option<"--emit", "What backend to use", values<"json+html", "tex", "typst">, true>,
    help<>
>; // clang-format on
}

namespace {
struct Ops : LanguageOps {
    v8::Isolate* i;
    v8::UniquePersistent<Function> impl_handle_unknown_macro;
    v8::UniquePersistent<Function> impl_to_ipa;
    v8::UniquePersistent<Function> impl_preprocess_full_entry;

    Ops() = default;
    static auto Create(v8::Isolate* i) -> Result<Ops>;

    /// LanguageOps overrides.
    auto handle_unknown_macro(TexParser&, std::string_view macro) -> Result<Node::Ptr> override;
    auto to_ipa(std::string_view) -> Result<std::string> override;
    auto preprocess_full_entry(std::vector<std::u32string>&) -> Result<> override;
};
} // namespace

auto GetGlobalFunction(std::string_view name) -> Result<v8::UniquePersistent<Function>> {
    auto Err = [&] {
        return Error(
            "Could not find global function '{}'; make sure to use the 'function' syntax",
            name
        );
    };

    auto i = Isolate();
    auto ctx = i->GetCurrentContext();
    auto l = ctx->Global()->Get(ctx, Str(name));
    if (l.IsEmpty()) return Err();
    auto f = l.ToLocalChecked();
    if (not f->IsFunction()) Err();
    return v8::UniquePersistent<Function>(i, f.As<Function>());
}

auto Ops::Create(v8::Isolate* i) -> Result<Ops> {
    Ops o;
    o.i = i;
    o.impl_handle_unknown_macro = Try(GetGlobalFunction("HandleUnknownMacro"));
    o.impl_to_ipa = Try(GetGlobalFunction("ToIPA"));
    o.impl_preprocess_full_entry = Try(GetGlobalFunction("PreprocessFullEntry"));
    return std::move(o);
}

auto Ops::handle_unknown_macro(TexParser&, std::string_view macro) -> Result<Node::Ptr> {
    Todo();
}

auto Ops::to_ipa(std::string_view word) -> Result<std::string> {
    v8::TryCatch tc{i};
    auto ctx = i->GetCurrentContext();
    Local<Value> arg[]{Str(word)};
    auto ipa = impl_to_ipa.Get(i)->Call(ctx, ctx->Global(), 1, arg);
    if (ipa.IsEmpty()) {
        if (tc.HasCaught() and not tc.Message().IsEmpty())
            return Error("{}", ToString(tc.Message()->Get()));
        return Error("ToIPA failed");
    }

    return ToString(ipa.ToLocalChecked());
}

auto Ops::preprocess_full_entry(std::vector<std::u32string>&) -> Result<> {
    if (impl_preprocess_full_entry.Get(i)->Experimental_IsNopFunction())
        return {};

    Todo();
}

auto gen::Main(int argc, char** argv) -> Result<int> {
    auto opts = cmd::options::parse(argc, argv);
    Try(CompileAndRun(opts.get<"--driver">()->contents, opts.get<"--driver">()->path.string()));

    auto ops = Try(Ops::Create(Isolate()));
    auto backend = [&] -> std::unique_ptr<Backend> {
        auto e = opts.get<"--emit">();
        if (*e == "json+html") return std::make_unique<JsonBackend>(ops, false);
        if (*e == "tex") return std::make_unique<TeXBackend>(ops, opts.get<"dictionary">()->path.string());
        if (*e == "typst") return std::make_unique<TypstBackend>(ops);
        Unreachable();
    }();

    Generator g{*backend};
    g.parse(opts.get<"dictionary">()->contents);
    return g.emit();
}
