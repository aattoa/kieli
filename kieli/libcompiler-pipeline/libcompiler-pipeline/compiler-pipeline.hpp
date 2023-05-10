#include <libutl/common/wrapper.hpp>
#include <libutl/common/pooled_string.hpp>
#include <libutl/diagnostics/diagnostics.hpp>


namespace compiler {

    using String     = utl::Pooled_string<struct _string_tag>;
    using Identifier = utl::Pooled_string<struct _identifier_tag>;

    struct [[nodiscard]] Shared_compilation_info {
        utl::diagnostics::Builder       diagnostics;
        utl::Wrapper_arena<utl::Source> source_arena = utl::Wrapper_arena<utl::Source>::with_page_size(8);
        String::Pool                    string_literal_pool;
        Identifier::Pool                identifier_pool;
    };
    using Compilation_info = utl::Strong<std::shared_ptr<Shared_compilation_info>>;

    struct [[nodiscard]] Compile_arguments {
        std::filesystem::path source_directory_path;
        std::string           main_file_name;
    };

    auto predefinitions_source(Compilation_info&) -> utl::Wrapper<utl::Source>;
    auto mock_compilation_info(utl::diagnostics::Level = utl::diagnostics::Level::suppress) -> Compilation_info;

}
