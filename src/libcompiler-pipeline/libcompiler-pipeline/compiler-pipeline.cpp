#include <libutl/common/utilities.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>


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

auto compiler::mock_compilation_info(utl::diagnostics::Level const level) -> Compilation_info {
    return std::make_shared<compiler::Shared_compilation_info>(compiler::Shared_compilation_info {
        .diagnostics = utl::diagnostics::Builder {
            utl::diagnostics::Builder::Configuration { .note_level = level, .warning_level = level, }
        },
        .source_arena = utl::Source::Arena::with_page_size(1),
    });
}


auto compiler::Name_dynamic::as_upper() const noexcept -> Name_upper {
    utl::always_assert(is_upper.get());
    return { identifier, source_view };
}
auto compiler::Name_dynamic::as_lower() const noexcept -> Name_lower {
    utl::always_assert(!is_upper.get());
    return { identifier, source_view };
}
compiler::Name_upper::operator Name_dynamic() const noexcept {
    return { identifier, source_view, true };
}
compiler::Name_lower::operator Name_dynamic() const noexcept {
    return { identifier, source_view, false };
}

auto compiler::Name_dynamic::operator==(compiler::Name_dynamic const& other) const noexcept -> bool {
    return identifier == other.identifier;
}
auto compiler::Name_lower::operator==(compiler::Name_lower const& other) const noexcept -> bool {
    return identifier == other.identifier;
}
auto compiler::Name_upper::operator==(compiler::Name_upper const& other) const noexcept -> bool {
    return identifier == other.identifier;
}
