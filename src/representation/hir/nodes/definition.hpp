#ifndef KIELI_HIR_NODES_DEFINITION
#define KIELI_HIR_NODES_DEFINITION
#else
#error "This isn't supposed to be included by anything other than hir/hir.hpp"
#endif


template <>
struct ast::definition::Basic_function<hir::HIR_configuration> {
    std::vector<hir::Implicit_template_parameter> implicit_template_parameters;
    std::vector<hir::Function_parameter>          parameters;
    tl::optional<hir::Type>                      return_type;
    hir::Expression                               body;
    ast::Name                                     name;
    tl::optional<Self_parameter>                 self_parameter;
};


namespace hir {

    using Function_signature          = ast::Basic_function_signature          <HIR_configuration>;
    using Function_template_signature = ast::Basic_function_template_signature <HIR_configuration>;
    using Type_signature              = ast::Basic_type_signature              <HIR_configuration>;
    using Type_template_signature     = ast::Basic_type_template_signature     <HIR_configuration>;

    namespace definition {

        using Function       = ast::definition::Basic_function       <HIR_configuration>;
        using Struct         = ast::definition::Basic_struct         <HIR_configuration>;
        using Enum           = ast::definition::Basic_enum           <HIR_configuration>;
        using Alias          = ast::definition::Basic_alias          <HIR_configuration>;
        using Typeclass      = ast::definition::Basic_typeclass      <HIR_configuration>;
        using Implementation = ast::definition::Basic_implementation <HIR_configuration>;
        using Instantiation  = ast::definition::Basic_instantiation  <HIR_configuration>;
        using Namespace      = ast::definition::Basic_namespace      <HIR_configuration>;

        using Function_template       = ast::definition::Template<Function>;
        using Struct_template         = ast::definition::Template<Struct>;
        using Enum_template           = ast::definition::Template<Enum>;
        using Alias_template          = ast::definition::Template<Alias>;
        using Typeclass_template      = ast::definition::Template<Typeclass>;
        using Implementation_template = ast::definition::Template<Implementation>;
        using Instantiation_template  = ast::definition::Template<Instantiation>;
        using Namespace_template      = ast::definition::Template<Namespace>;

    }

    struct Definition : ast::Basic_definition<HIR_configuration> {
        using Basic_definition::Basic_definition;
        using Basic_definition::operator=;
    };

}
