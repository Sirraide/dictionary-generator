#include "dictgen-v8.hh"
#include <clopts.hh>

using namespace dict;
using namespace dict::gen;

namespace cmd {
using namespace command_line_options;
using options = clopts< // clang-format off
    option<"--driver", "Main JavaScript file to run", file<>, true>,
    help<>
>; // clang-format on
}

auto GetGlobalFunction(std::string_view name) -> Result<Local<v8::Function>> {
    auto ctx = Isolate()->GetCurrentContext();
    auto l = ctx->Global()->Get(ctx, Str(name));
    if (l.IsEmpty()) return Error("Global function '{}' not found", name);
    auto f = l.ToLocalChecked();
    if (not f->IsFunction()) return Error("Global '{}' is not a function; make sure to use the 'function' syntax", name);
    return f.As<v8::Function>();
}

auto gen::Main(int argc, char **argv) -> Result<> {
    auto opts = cmd::options::parse(argc, argv);
    Try(CompileAndRun(opts.get<"--driver">()->contents, opts.get<"--driver">()->path.string()));

    auto i = Isolate();
    auto ctx = i->GetCurrentContext();
    auto global = ctx->Global();
    auto h = Try(GetGlobalFunction("HandleUnknownMacro"));
    Local<Value> args[] { v8::Undefined(i), Str("pf") };
    h->Call(ctx, global, 2, args);
    return {};
}
