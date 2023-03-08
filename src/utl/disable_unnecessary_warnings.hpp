#pragma once


#ifdef _MSC_VER // MS warnings

#pragma push_macro("DISABLE_HELPER")
#pragma push_macro("DISABLE")
#undef DISABLE_HELPER
#undef DISABLE

// The helper is necessary because _Pragma does not support string concatenation
#define DISABLE_HELPER(s) _Pragma(#s)
#define DISABLE(n) DISABLE_HELPER(warning(disable: n))


DISABLE(4061) // enumerator is not explicitly handled by a case label
DISABLE(4063) // case n is not a valid value for switch

DISABLE(4355) // 'this' used in member initializer list
DISABLE(4371) // layout of class may have changed from a previous version of the compiler

DISABLE(4456) // declaration hides previous local declaration
DISABLE(4459) // declaration hides global declaration

DISABLE(4514) // unreferenced inline function has been removed

DISABLE(4623) // default constructor was implicitly defined as deleted
DISABLE(4625) // copy constructor was implicitly defined as deleted
DISABLE(4626) // copy assignment operator was implicitly defined as deleted

DISABLE(4710) // function not inlined
DISABLE(4711) // function selected for automatic inline expansion

DISABLE(4820) // n bytes of padding added after data member

DISABLE(4866) // left-to-right evaluation not enforced
DISABLE(4868) // left-to-right evaluation not enforced in braced init list

DISABLE(5026) // move constructor was implicitly defined as deleted
DISABLE(5027) // move assignment operator was implicitly defined as deleted

DISABLE(5045) // spectre mitigation inserted if /Qspectre is specified


#pragma pop_macro("DISABLE_HELPER")
#pragma pop_macro("DISABLE")

#endif // defined _MSC_VER