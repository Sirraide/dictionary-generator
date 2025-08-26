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

#define TRY(...) Try(Catch([&]{ return __VA_ARGS__; }))

namespace {
struct Ops : LanguageOps {
    TexParser* current_parser = nullptr;
    v8::UniquePersistent<Function> impl_handle_unknown_macro;
    v8::UniquePersistent<Function> impl_to_ipa;
    v8::UniquePersistent<Function> impl_preprocess_full_entry;
    v8::UniquePersistent<v8::ObjectTemplate> templ_parser;
    v8::UniquePersistent<v8::ObjectTemplate> templ_node;

    Ops() = default;

    /// LanguageOps overrides.
    auto handle_unknown_macro(TexParser&, std::string_view macro) -> Result<Node*> override;
    auto to_ipa(std::string_view) -> Result<std::string> override;
    auto preprocess_full_entry(std::vector<std::u32string>&) -> Result<> override;

    /// Run a callback and catch any exceptions.
    auto Catch(auto cb) -> Result<Local<Value>>;
};

Ops* o;
} // namespace

static bool CheckParser(std::string_view fn) {
    if (o->current_parser == nullptr) {
        Throw("'{}' called outside of HandleUnknownMacro()", fn);
        return false;
    }

    return true;
}

static auto GetNode(Local<Value> v) -> Node* {
    Assert(o->current_parser);
    if (v->IsUndefined() or v->IsNull()) return o->current_parser->empty();
    if (not v->IsObject()) return nullptr;
    auto obj = v.As<v8::Object>();

    // FIXME: Figure out how to check this properly because this doesn’t work.
    // if (obj->GetPrototype() != templ_node.Get(Isolate())) return nullptr;
    if (obj->InternalFieldCount() != 1) return nullptr;
    return static_cast<Node*>(obj->GetInternalField(0).As<v8::External>()->Value());
}

static auto Wrap(Node* n) -> Local<Value> {
    auto i = Isolate();
    auto inst = o->templ_node.Get(i)->NewInstance(i->GetCurrentContext()).ToLocalChecked();
    inst->SetInternalField(0, v8::External::New(i, n));
    return inst;
}

static void Impl_TexParser_escaped(const FunctionCallbackInfo<Value>& info) {
    if (not CheckParser("escaped()")) return;
    if (info.Length() != 1 or not info[0]->IsString()) {
        Throw("escaped() takes one string argument, but got: {}", info[0]);
        return;
    }

    return info.GetReturnValue().Set(Wrap(o->current_parser->text(ToString(info[0]))));
}

static void Impl_TexParser_group(const FunctionCallbackInfo<Value>& info) {
    if (not CheckParser("group()")) return;

    std::vector<Node*> nodes;
    for (int i = 0; i < info.Length(); i++) {
        auto n = GetNode(info[i]);
        if (not n) {
            Throw("group(): argument #{} is not a valid node: {}", i + 1, info[i]);
            return;
        }
        nodes.push_back(n);
    }

    info.GetReturnValue().Set(Wrap(o->current_parser->make<ContentNode>(std::move(nodes))));
}

static void Impl_TexParser_parse_arg(const FunctionCallbackInfo<Value>& info) {
    if (not CheckParser("parse_arg()")) return;
    if (info.Length() != 0) {
        Throw("parse_arg() takes no arguments");
        return;
    }

    auto res = o->current_parser->parse_arg();
    if (not res.has_value()) {
        Throw("Failed to parse argument: {}", res.error());
        return;
    }

    info.GetReturnValue().Set(Wrap(res.value()));
}

static void Impl_TexParser_raw(const FunctionCallbackInfo<Value>& info) {
    if (not CheckParser("raw()")) return;
    if (info.Length() != 1 or not info[0]->IsString()) {
        Throw("raw() takes one string argument, but got: {}", info[0]);
        return;
    }

    return info.GetReturnValue().Set(Wrap(o->current_parser->text(ToString(info[0]), true)));
}

static void Impl_Node_toString(const FunctionCallbackInfo<Value>& info) {
    auto n = GetNode(info.Holder());
    if (not n) return;
    info.GetReturnValue().Set(Str(std::format("<#node:{}>", static_cast<void*>(n))));
}

static auto CreateParserTemplate(v8::Isolate* i) -> v8::UniquePersistent<v8::ObjectTemplate> {
    auto templ = v8::ObjectTemplate::New(i);
    templ->Set(i, "escaped", v8::FunctionTemplate::New(i, Impl_TexParser_escaped));
    templ->Set(i, "group", v8::FunctionTemplate::New(i, Impl_TexParser_group));
    templ->Set(i, "parse_arg", v8::FunctionTemplate::New(i, Impl_TexParser_parse_arg));
    templ->Set(i, "raw", v8::FunctionTemplate::New(i, Impl_TexParser_raw));
    templ->SetImmutableProto();
    return v8::UniquePersistent<v8::ObjectTemplate>{i, templ};
}

static auto CreateNodeTemplate(v8::Isolate* i) -> v8::UniquePersistent<v8::ObjectTemplate> {
    auto templ = v8::ObjectTemplate::New(i);
    templ->SetInternalFieldCount(1);
    templ->SetImmutableProto();
    templ->Set(i, "toString", v8::FunctionTemplate::New(i, Impl_Node_toString));
    return v8::UniquePersistent<v8::ObjectTemplate>{i, templ};
}

static auto GetGlobalFunction(std::string_view name) -> Result<v8::UniquePersistent<Function>> {
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

auto Ops::Catch(auto cb) -> Result<Local<Value>> {
    v8::TryCatch tc{Isolate()};
    auto res = cb();
    if (res.IsEmpty()) {
        if (tc.HasCaught() and not tc.Message().IsEmpty())
            return Error("{}", ToString(tc.Message()->Get()));
        return Error("Unknown error");
    }

    Assert(not tc.HasCaught());
    return res.ToLocalChecked();
}

auto Ops::handle_unknown_macro(TexParser& parser, std::string_view macro) -> Result<Node*> {
    tempset current_parser = &parser;
    auto i = Isolate();
    auto ctx = i->GetCurrentContext();
    auto p = templ_parser.Get(i)->NewInstance(ctx).ToLocalChecked();
    Local<Value> args[] { p, Str(macro) };
    auto res = TRY(impl_handle_unknown_macro.Get(i)->Call(ctx, ctx->Global(), 2, args));
    auto n = GetNode(res);
    if (not n) return Error("HandleUnknownMacro(): return value is not a valid node: {}", res);
    return n;
}

auto Ops::to_ipa(std::string_view word) -> Result<std::string> {
    auto i = Isolate();
    auto ctx = i->GetCurrentContext();
    Local<Value> arg[]{Str(word)};
    return ToString(TRY(impl_to_ipa.Get(i)->Call(ctx, ctx->Global(), 1, arg)));
}

auto Ops::preprocess_full_entry(std::vector<std::u32string>& fields) -> Result<> {
    auto i = Isolate();
    if (impl_preprocess_full_entry.Get(i)->Experimental_IsNopFunction())
        return {};

    auto ctx = i->GetCurrentContext();
    auto args = v8::Array::New(i, int(fields.size()));
    Local<Value> arg[]{args};
    for (auto [index, s] : vws::enumerate(fields)) args->Set(ctx, u32(index), Str(text::ToUTF8(s))).Check();
    auto res = TRY(impl_preprocess_full_entry.Get(i)->Call(ctx, ctx->Global(), 1, arg));
    if (not res->IsUndefined()) return Error(
        "PreprocessFullEntry() should not return a value; modify the input array instead"
    );

    fields.clear();
    for (u32 n = 0; n < args->Length(); n++) {
        auto el = args->Get(ctx, n);
        if (el.IsEmpty()) continue;
        fields.push_back(text::ToUTF32(ToString(el.ToLocalChecked())));
    }

    return {};
}

auto gen::Main(int argc, char** argv) -> Result<int> {
    auto i = Isolate();
    auto opts = cmd::options::parse(argc, argv);
    Try(CompileAndRun(opts.get<"--driver">()->contents, opts.get<"--driver">()->path.string()));

    Ops ops;
    o = &ops;
    ops.impl_handle_unknown_macro = Try(GetGlobalFunction("HandleUnknownMacro"));
    ops.impl_to_ipa = Try(GetGlobalFunction("ToIPA"));
    ops.impl_preprocess_full_entry = Try(GetGlobalFunction("PreprocessFullEntry"));
    ops.templ_parser = CreateParserTemplate(i);
    ops.templ_node = CreateNodeTemplate(i);

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
