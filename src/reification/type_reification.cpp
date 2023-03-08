#include "utl/utilities.hpp"
#include "reification_internals.hpp"


namespace {

    using namespace reification;

    struct Type_reification_visitor {
        Context&  context;
        mir::Type this_type;

        auto recurse() const noexcept {
            return std::bind_front(&Context::reify_type, &context);
        }


        auto operator()(mir::type::General_unification_variable const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Integral_unification_variable const&) -> cir::Type {
            utl::todo();
        }


        auto operator()(mir::type::Integer const integer) -> cir::Type {
            static constexpr auto types = std::to_array({
                &Context::i8_type, &Context::i16_type, &Context::i32_type, &Context::i64_type,
                &Context::u8_type, &Context::u16_type, &Context::u32_type, &Context::u64_type,
            });
            static_assert(types.size() == static_cast<utl::Usize>(mir::type::Integer::_integer_count));
            return (context.*types[static_cast<utl::Usize>(integer)])(this_type.source_view);
        }

        auto operator()(mir::type::Boolean const&) -> cir::Type {
            return context.boolean_type(this_type.source_view);
        }
        auto operator()(mir::type::Floating const&) -> cir::Type {
            return context.floating_type(this_type.source_view);
        }
        auto operator()(mir::type::String const&) -> cir::Type {
            return context.string_type(this_type.source_view);
        }
        auto operator()(mir::type::Character const&) -> cir::Type {
            return context.character_type(this_type.source_view);
        }

        auto operator()(utl::one_of<mir::type::Pointer, mir::type::Reference> auto const& pointer) -> cir::Type {
            // The structured binding avoids having to spell out the field
            // names, which are different for reference and pointer nodes.
            auto [mutability, pointed_to_type] = pointer;
            return {
                .value       = wrap_type(cir::type::Pointer { .pointed_to_type = context.reify_type(pointed_to_type) }),
                .size        = sizeof(void*),
                .source_view = this_type.source_view
            };
        }

        auto operator()(mir::type::Tuple const& tuple) -> cir::Type {
            auto field_types = tuple.field_types
                             | std::views::transform(recurse())
                             | std::ranges::to<std::vector>();

            auto size = std::transform_reduce(field_types.begin(),
                                              field_types.end(),
                                              utl::Safe_usize {},
                                              std::plus {},
                                              std::mem_fn(&cir::Type::size));

            return {
                .value       = wrap_type(cir::type::Tuple { .field_types = std::move(field_types) }),
                .size        = size,
                .source_view = this_type.source_view
            };
        }

        auto operator()(mir::type::Array const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Enumeration const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Structure const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Function const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Self_placeholder const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Slice const&) -> cir::Type {
            utl::todo();
        }
        auto operator()(mir::type::Template_parameter_reference const&) -> cir::Type {
            utl::todo();
        }
    };

}


auto reification::Context::reify_type(mir::Type type) -> cir::Type {
    return std::visit(Type_reification_visitor { *this, type }, *type.value);
}