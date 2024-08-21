#include <libcompiler/ast/ast.hpp>

namespace kieli::ast {
    auto display(Arena const& arena, Definition const& definition) -> std::string;
    auto display(Arena const& arena, Expression const& expression) -> std::string;
    auto display(Arena const& arena, Pattern const& pattern) -> std::string;
    auto display(Arena const& arena, Type const& type) -> std::string;
} // namespace kieli::ast
