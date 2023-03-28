#include "utl/utilities.hpp"
#include "reification_internals.hpp"


namespace {

    struct Pattern_reification_visitor {
        reification::Context& context;
        mir::Pattern   const& this_pattern;
        utl::Safe_usize       this_type_size;

        auto increment_frame_offset() -> void {
            context.current_frame_offset +=
                utl::safe_cast<reification::Frame_offset::Underlying_integer>(this_type_size.get());
        }

        auto recurse() noexcept {
            return std::bind_front(&reification::Context::reify_pattern, &context);
        }


        auto operator()(mir::pattern::Name const& name) -> cir::Pattern::Variant {
            context.variable_frame_offsets.add(name.variable_tag, context.current_frame_offset);
            increment_frame_offset();
            return cir::pattern::Exhaustive {};
        }
        auto operator()(mir::pattern::Wildcard const&) -> cir::Pattern::Variant {
            increment_frame_offset();
            return cir::pattern::Exhaustive{};
        }
        auto operator()(mir::pattern::Tuple const& tuple) -> cir::Pattern::Variant {
            return cir::pattern::Tuple { .field_patterns = utl::map(recurse(), tuple.field_patterns) };
        }

        template <class T>
        auto operator()(mir::pattern::Literal<T>) -> cir::Pattern::Variant {
            utl::todo();
        }
        auto operator()(mir::pattern::Slice const&) -> cir::Pattern::Variant {
            utl::todo();
        }
        auto operator()(mir::pattern::Guarded const&) -> cir::Pattern::Variant {
            utl::todo();
        }
        auto operator()(mir::pattern::Enum_constructor const&) -> cir::Pattern::Variant {
            utl::todo();
        }
        auto operator()(mir::pattern::As const&) -> cir::Pattern::Variant {
            utl::todo();
        }
    };

}


auto reification::Context::reify_pattern(mir::Pattern const& pattern) -> cir::Pattern {
    cir::Type pattern_type = reify_type(pattern.type);
    return cir::Pattern {
        .value = std::visit(
            Pattern_reification_visitor { *this, pattern, pattern_type.size },
            pattern.value),
        .type = pattern_type,
        .source_view = pattern.source_view
    };
}