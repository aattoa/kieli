#ifndef KIELI_MIR_NODES_DEFINITION
#define KIELI_MIR_NODES_DEFINITION
#else
#error This isn't supposed to be included by anything other than mir/mir.hpp
#endif


namespace resolution {
    struct Namespace;
}


namespace mir {

    template <class Definition>
    struct Template {
        Definition                                                                 definition;
        std::vector<Template_parameter>                                            parameters;
        std::vector<utl::Wrapper<resolution::Definition_info<To_HIR<Definition>>>> instantiations;
    };


    struct [[nodiscard]] Self_parameter {
        mir::Mutability  mutability;
        bool             is_reference = false;
        utl::Source_view source_view;
    };

    struct Function {
        struct Signature {
            std::vector<mir::Function_parameter> parameters;
            mir::Type                            return_type;
            mir::Type                            function_type;
        };
        Signature                     signature;
        Expression                    body;
        ast::Name                     name;
        std::optional<Self_parameter> self_parameter;
    };
    using Function_template = Template<Function>;

    struct Struct {
        struct Member {
            ast::Name name;
            Type      type;
            bool      is_public;
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
        struct Function_signature {
            std::vector<mir::Type> parameters;
            mir::Type              return_type;
        };
        struct Function_template_signature {
            Function_signature                   function_signature;
            std::vector<mir::Template_parameter> template_parameters;
        };
        struct Type_signature {
            std::vector<Class_reference> classes;
        };
        struct Type_template_signature {
            Type_signature                       type_signature;
            std::vector<mir::Template_parameter> template_parameters;
        };
        utl::Flatmap<compiler::Identifier, Function_signature>          function_signatures;
        utl::Flatmap<compiler::Identifier, Function_template_signature> function_template_signatures;
        utl::Flatmap<compiler::Identifier, Type_signature>              type_signatures;
        utl::Flatmap<compiler::Identifier, Type_template_signature>     type_template_signatures;
        ast::Name                                                       name;
    };
    using Typeclass_template = Template<Typeclass>;

    struct Implementation {
        template <utl::instance_of<resolution::Definition_info> Info>
        using Map = utl::Flatmap<compiler::Identifier, utl::Wrapper<Info>>;

        struct Definitions {
            Map<resolution::Function_info>          functions;
            Map<resolution::Function_template_info> function_templates;
            Map<resolution::Struct_info>            structures;
            Map<resolution::Struct_template_info>   structure_templates;
            Map<resolution::Enum_info>              enumerations;
            Map<resolution::Enum_template_info>     enumeration_templates;
            Map<resolution::Alias_info>             aliases;
            Map<resolution::Alias_template_info>    alias_templates;
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