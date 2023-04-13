#pragma once

#include "utl/safe_integer.hpp"
#include "vm/virtual_machine.hpp"
#include "representation/mir/mir.hpp"
#include "representation/cir/cir.hpp"


namespace reification {

    using Frame_offset = utl::Safe_integer<vm::Local_offset_type>;

    struct Reification_constants {
        utl::Wrapper<cir::Type::Variant> unit_type;
        utl::Wrapper<cir::Type::Variant> boolean_type;
        utl::Wrapper<cir::Type::Variant> string_type;
        utl::Wrapper<cir::Type::Variant> character_type;
        utl::Wrapper<cir::Type::Variant> i8_type;
        utl::Wrapper<cir::Type::Variant> i16_type;
        utl::Wrapper<cir::Type::Variant> i32_type;
        utl::Wrapper<cir::Type::Variant> i64_type;
        utl::Wrapper<cir::Type::Variant> u8_type;
        utl::Wrapper<cir::Type::Variant> u16_type;
        utl::Wrapper<cir::Type::Variant> u32_type;
        utl::Wrapper<cir::Type::Variant> u64_type;
        utl::Wrapper<cir::Type::Variant> floating_type;

        explicit Reification_constants(cir::Node_arena&);
    };

    struct [[nodiscard]] Context {
        compiler::Compilation_info                          compilation_info;
        cir::Node_arena                                     node_arena;
        Reification_constants                               constants;
        utl::Source                                         source;
        utl::Flatmap<mir::Local_variable_tag, Frame_offset> variable_frame_offsets;
        Frame_offset                                        current_frame_offset;

        explicit Context(
            utl::Source               && source,
            cir::Node_arena           && node_arena,
            compiler::Compilation_info&& compilation_info) noexcept
            : compilation_info { std::move(compilation_info) }
            , node_arena       { std::move(node_arena) }
            , constants        { this->node_arena }
            , source           { std::move(source) } {}


        template <class Node>
        auto wrap(Node&& node) -> utl::Wrapper<Node>
            requires requires { node_arena.wrap<Node>(std::move(node)); }
                && (!std::is_reference_v<Node>)
        {
            return node_arena.wrap<Node>(std::move(node));
        }
        [[nodiscard]]
        auto wrap() noexcept {
            return [this]<class Arg>(Arg&& arg) -> utl::Wrapper<Arg>
                requires requires { wrap(std::move(arg)); }
                     && (!std::is_reference_v<Arg>)
            {
                return wrap(std::move(arg));
            };
        }

        // `wrap_type(x)` is shorthand for `utl::wrap(cir::Type::Variant { x })`
        auto wrap_type(cir::Type::Variant&& value) -> utl::Wrapper<cir::Type::Variant> {
            return wrap(std::move(value));
        }

        auto reify_expression(mir::Expression const&) -> cir::Expression;
        auto reify_pattern   (mir::Pattern    const&) -> cir::Pattern;
        auto reify_type      (mir::Type             ) -> cir::Type;

        [[noreturn]]
        auto error(utl::Source_view, utl::diagnostics::Message_arguments) -> void;

        auto unit_type     (utl::Source_view) -> cir::Type;
        auto i8_type       (utl::Source_view) -> cir::Type;
        auto i16_type      (utl::Source_view) -> cir::Type;
        auto i32_type      (utl::Source_view) -> cir::Type;
        auto i64_type      (utl::Source_view) -> cir::Type;
        auto u8_type       (utl::Source_view) -> cir::Type;
        auto u16_type      (utl::Source_view) -> cir::Type;
        auto u32_type      (utl::Source_view) -> cir::Type;
        auto u64_type      (utl::Source_view) -> cir::Type;
        auto floating_type (utl::Source_view) -> cir::Type;
        auto character_type(utl::Source_view) -> cir::Type;
        auto boolean_type  (utl::Source_view) -> cir::Type;
        auto string_type   (utl::Source_view) -> cir::Type;
        auto size_type     (utl::Source_view) -> cir::Type;
    };

}