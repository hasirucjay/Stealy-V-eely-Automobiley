// Polyfill for __std_find_not_ch_1, which is an internal MSVC STL symbol
// introduced in VS 2022 17.13+. Needed when linking against a static library
// (Soup.lib) built with a newer toolset than the one used to compile this project.

#include <cstdint>
#include <cstring>

extern "C" const void* __cdecl __std_find_not_ch_1(const void* _First, const void* _Last, uint8_t _Val) noexcept
{
    auto _Ptr = static_cast<const uint8_t*>(_First);
    const auto _End = static_cast<const uint8_t*>(_Last);
    for (; _Ptr != _End; ++_Ptr)
    {
        if (*_Ptr != _Val)
        {
            return _Ptr;
        }
    }
    return _Last;
}
