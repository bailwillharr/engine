#pragma once

#ifndef GFX_API
# ifdef _MSC_VER
#  ifdef GFX_EXPORTS
#   define GFX_API __declspec(dllexport)
#  else
#   define GFX_API __declspec(dllimport)
#  endif
# else
#  define GFX_API
# endif
#endif
