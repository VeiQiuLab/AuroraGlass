#include <auroraglass/version.h>
#include <string_view>

int main()
{
    constexpr auto v = AuroraGlass::GetAuroraGlassVersion();
    static_assert(v.major == 0);
    static_assert(v.minor == 8);
    static_assert(v.patch == 0);
    return v.string == std::string_view{"0.8.0"} ? 0 : 1;
}
