#include "utl/utilities.hpp"
#include "compiler.hpp"


namespace {
    constexpr std::string_view source_string = R"(
        namespace std {
            class Copy { fn copy(&self): Self }
            class Drop { fn drop(&self): () }
            fn id[X](x: X) = x
        }
    )";
}


auto compiler::predefinitions_source(Compilation_info& compilation_info) -> utl::Wrapper<utl::Source> {
    return compilation_info.get()->source_arena.wrap("[predefined]", std::string(source_string));
}
