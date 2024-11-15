#include <libcompiler/ast/ast.hpp>

namespace kieli::ast {
    auto display(Arena const& arena, definition::Function const& function) -> std::string;
    auto display(Arena const& arena, definition::Enumeration const& enumeration) -> std::string;
    auto display(Arena const& arena, definition::Alias const& alias) -> std::string;
    auto display(Arena const& arena, definition::Concept const& concept_) -> std::string;
    auto display(Arena const& arena, definition::Submodule const& submodule) -> std::string;
    auto display(Arena const& arena, Definition const& definition) -> std::string;
    auto display(Arena const& arena, Expression const& expression) -> std::string;
    auto display(Arena const& arena, Pattern const& pattern) -> std::string;
    auto display(Arena const& arena, Type const& type) -> std::string;
} // namespace kieli::ast
