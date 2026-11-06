/** Pure shader failure injection parser tests; no GL context required. */
#include "linden_common.h"
#include "../llshaderdiagnostics.h"
#include "../test/lltut.h"

namespace tut
{
    struct shader_diagnostics_data {};
    typedef test_group<shader_diagnostics_data> shader_diagnostics_group;
    typedef shader_diagnostics_group::object shader_diagnostics_object;
    shader_diagnostics_group shader_diagnostics_tests("LLShaderDiagnostics");

    template<> template<> void shader_diagnostics_object::test<1>()
    {
        std::vector<std::string> errors;
        auto entries = LLShaderInjection::parse("compile:aV.glsl:3;link:Program;binary:Other;overflow:Light:33;missing:lib/moduleF.glsl", errors);
        ensure("valid forms", errors.empty());
        ensure_equals("five entries", entries.size(), size_t(5));
        ensure_equals("compile level", entries[0].value, 3);
        ensure_equals("module path", entries[4].target, std::string("lib/moduleF.glsl"));
        ensure_equals("overflow channels", entries[3].value, 33);
    }

    template<> template<> void shader_diagnostics_object::test<2>()
    {
        std::vector<std::string> errors;
        auto entries = LLShaderInjection::parse("compile:a:-1;compile:a:2x;overflow:p:2147483648;missing:;link:p:2;unknown:x;compile:a;binary:;link:ok", errors);
        ensure_equals("only valid entry retained", entries.size(), size_t(1));
        ensure_equals("all invalid entries reported", errors.size(), size_t(8));
        ensure_equals("remaining target", entries[0].target, std::string("ok"));
    }

    template<> template<> void shader_diagnostics_object::test<3>()
    {
        std::vector<std::string> errors;
        ensure("empty disables injection", LLShaderInjection::parse("", errors).empty());
        ensure("empty tokens ignored", LLShaderInjection::parse(";;;", errors).empty());
        auto entries = LLShaderInjection::parse("compile:a:0;;overflow:p:0;link:Program with spaces;", errors);
        ensure("zero and program names with spaces valid", errors.empty());
        ensure_equals("three entries", entries.size(), size_t(3));
    }
}
