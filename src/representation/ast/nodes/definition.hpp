#ifndef KIELI_AST_NODES_DEFINITION
#define KIELI_AST_NODES_DEFINITION
#else
#error "This isn't supposed to be included by anything other than ast/ast.hpp"
#endif


namespace hir {
    struct HIR_configuration;
}

namespace ast {

    struct [[nodiscard]] Self_parameter {
        Mutability        mutability;
        utl::Strong<bool> is_reference;
        utl::Source_view  source_view;
    };


    template <tree_configuration Configuration>
    struct Basic_function_signature {
        std::vector<Basic_function_parameter<Configuration>> parameters;
        tl::optional<Self_parameter>                         self_parameter;
        tl::optional<typename Configuration::Type>           return_type;
        Name                                                 name;
    };

    template <tree_configuration Configuration>
    struct Basic_function_template_signature {
        Basic_function_signature<Configuration>              function_signature;
        std::vector<Basic_template_parameter<Configuration>> template_parameters;
    };

    template <tree_configuration Configuration>
    struct Basic_type_signature {
        std::vector<Basic_class_reference<Configuration>> classes;
        Name                                              name;
    };

    template <tree_configuration Configuration>
    struct Basic_type_template_signature {
        Basic_type_signature<Configuration>                  type_signature;
        std::vector<Basic_template_parameter<Configuration>> template_parameters;
    };

    using Function_signature          = Basic_function_signature          <AST_configuration>;
    using Function_template_signature = Basic_function_template_signature <AST_configuration>;
    using Type_signature              = Basic_type_signature              <AST_configuration>;
    using Type_template_signature     = Basic_type_template_signature     <AST_configuration>;


    namespace definition {
        template <tree_configuration Configuration>
        struct Basic_function {
            Basic_function_signature<Configuration> signature;
            typename Configuration::Expression      body;
        };
        template <tree_configuration Configuration>
        struct Basic_struct_member {
            Name                         name;
            typename Configuration::Type type;
            utl::Strong<bool>            is_public;
            utl::Source_view             source_view;
        };
        template <tree_configuration Configuration>
        struct Basic_struct {
            using Member = Basic_struct_member<Configuration>;
            std::vector<Member> members;
            Name                name;
        };
        template <tree_configuration Configuration>
        struct Basic_enum_constructor {
            Name                                       name;
            tl::optional<typename Configuration::Type> payload_type;
            utl::Source_view                           source_view;
        };
        template <tree_configuration Configuration>
        struct Basic_enum {
            using Constructor = Basic_enum_constructor<Configuration>;
            std::vector<Constructor> constructors;
            Name                     name;
        };
        template <tree_configuration Configuration>
        struct Basic_alias {
            Name                         name;
            typename Configuration::Type type;
        };
        template <tree_configuration Configuration>
        struct Basic_typeclass {
            std::vector<Basic_function_signature         <Configuration>> function_signatures;
            std::vector<Basic_function_template_signature<Configuration>> function_template_signatures;
            std::vector<Basic_type_signature             <Configuration>> type_signatures;
            std::vector<Basic_type_template_signature    <Configuration>> type_template_signatures;
            Name                                                          name;
        };
        template <tree_configuration Configuration>
        struct Basic_implementation {
            typename Configuration::Type                    type;
            std::vector<typename Configuration::Definition> definitions;
        };
        template <tree_configuration Configuration>
        struct Basic_instantiation {
            Basic_class_reference<Configuration>            typeclass;
            typename Configuration::Type                    self_type;
            std::vector<typename Configuration::Definition> definitions;
        };
        template <tree_configuration Configuration>
        struct Basic_namespace {
            std::vector<typename Configuration::Definition> definitions;
            Name                                            name;
        };

        using Function       = Basic_function       <AST_configuration>;
        using Struct         = Basic_struct         <AST_configuration>;
        using Enum           = Basic_enum           <AST_configuration>;
        using Alias          = Basic_alias          <AST_configuration>;
        using Typeclass      = Basic_typeclass      <AST_configuration>;
        using Implementation = Basic_implementation <AST_configuration>;
        using Instantiation  = Basic_instantiation  <AST_configuration>;
        using Namespace      = Basic_namespace      <AST_configuration>;

        template <class>
        struct Template;

        template <tree_configuration Configuration, template <tree_configuration> class Definition>
        struct Template<Definition<Configuration>> {
            Definition<Configuration>                            definition;
            std::vector<Basic_template_parameter<Configuration>> parameters;
        };

        using Function_template       = Template<Function>;
        using Struct_template         = Template<Struct>;
        using Enum_template           = Template<Enum>;
        using Alias_template          = Template<Alias>;
        using Typeclass_template      = Template<Typeclass>;
        using Implementation_template = Template<Implementation>;
        using Instantiation_template  = Template<Instantiation>;
        using Namespace_template      = Template<Namespace>;
    }


    template <tree_configuration Configuration>
    struct Basic_definition {
        using Variant = std::variant<
            definition::Basic_function       <Configuration>,
            definition::Basic_struct         <Configuration>,
            definition::Basic_enum           <Configuration>,
            definition::Basic_alias          <Configuration>,
            definition::Basic_typeclass      <Configuration>,
            definition::Basic_implementation <Configuration>,
            definition::Basic_instantiation  <Configuration>,
            definition::Basic_namespace      <Configuration>,

            definition::Template<definition::Basic_function       <Configuration>>,
            definition::Template<definition::Basic_struct         <Configuration>>,
            definition::Template<definition::Basic_enum           <Configuration>>,
            definition::Template<definition::Basic_alias          <Configuration>>,
            definition::Template<definition::Basic_typeclass      <Configuration>>,
            definition::Template<definition::Basic_implementation <Configuration>>,
            definition::Template<definition::Basic_instantiation  <Configuration>>,
            definition::Template<definition::Basic_namespace      <Configuration>>>;

        Variant          value;
        utl::Source_view source_view;

        Basic_definition(Variant&& value, utl::Source_view const view) noexcept
            : value { std::move(value) }
            , source_view { view } {}
    };

    struct Definition : Basic_definition<AST_configuration> {
        using Basic_definition::Basic_definition;
        using Basic_definition::operator=;
    };

}
