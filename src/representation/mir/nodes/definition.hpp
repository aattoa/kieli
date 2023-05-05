#ifndef KIELI_MIR_NODES_DEFINITION
#define KIELI_MIR_NODES_DEFINITION
#else
#error "This isn't supposed to be included by anything other than mir/mir.hpp"
#endif


namespace resolution {
    struct [[nodiscard]] Namespace;
    class  [[nodiscard]] Scope;
}


namespace mir {

    template <class Definition>
    struct Template {
        Definition                                                                 definition;
        std::vector<Template_parameter>                                            parameters;
        std::vector<utl::Wrapper<resolution::Definition_info<To_HIR<Definition>>>> instantiations;
    };

    struct [[nodiscard]] Self_parameter {
        mir::Mutability   mutability;
        utl::Strong<bool> is_reference;
        utl::Source_view  source_view;
    };

    struct Function {
        struct Signature {
            std::vector<mir::Template_parameter> template_parameters; // empty when not a template
            std::vector<mir::Function_parameter> parameters;
            tl::optional<Self_parameter>         self_parameter;
            ast::Name                            name;
            mir::Type                            return_type;
            mir::Type                            function_type;

            [[nodiscard]] auto is_template() const noexcept -> bool;
        };
        Signature                                            signature;
        Expression                                           body;
        std::vector<utl::Wrapper<resolution::Function_info>> template_instantiations; // empty when not a template
    };

    struct Struct {
        struct Member { // NOLINT
            ast::Name         name;
            Type              type;
            utl::Strong<bool> is_public;
        };
        std::vector<Member>                 members;
        ast::Name                           name;
        utl::Wrapper<resolution::Namespace> associated_namespace;
    };
    using Struct_template = Template<Struct>;

    struct Enum {
        std::vector<Enum_constructor>       constructors;
        ast::Name                           name;
        utl::Wrapper<resolution::Namespace> associated_namespace;
    };
    using Enum_template = Template<Enum>;

    struct Alias {
        Type      aliased_type;
        ast::Name name;
    };
    using Alias_template = Template<Alias>;

    struct Typeclass {
        struct Type_signature {
            std::vector<Class_reference> classes;
        };
        struct Type_template_signature {
            Type_signature                       type_signature;
            std::vector<mir::Template_parameter> template_parameters;
        };
        utl::Flatmap<compiler::Identifier, Function::Signature>         function_signatures;
        utl::Flatmap<compiler::Identifier, Type_signature>              type_signatures;
        utl::Flatmap<compiler::Identifier, Type_template_signature>     type_template_signatures;
        ast::Name                                                       name;
    };
    using Typeclass_template = Template<Typeclass>;

    struct Implementation {
        template <utl::instance_of<resolution::Definition_info> Info>
        using Map = utl::Flatmap<compiler::Identifier, utl::Wrapper<Info>>;

        struct Definitions {
            Map<resolution::Function_info>        functions;
            Map<resolution::Struct_info>          structures;
            Map<resolution::Struct_template_info> structure_templates;
            Map<resolution::Enum_info>            enumerations;
            Map<resolution::Enum_template_info>   enumeration_templates;
            Map<resolution::Alias_info>           aliases;
            Map<resolution::Alias_template_info>  alias_templates;
        };
        Definitions definitions;
        Type        self_type;
    };
    using Implementation_template = Template<Implementation>;

    struct Instantiation {
        using Definitions = Implementation::Definitions;
        Definitions     definitions;
        Class_reference class_reference;
        Type            self_type;
    };
    using Instantiation_template = Template<Instantiation>;

}
