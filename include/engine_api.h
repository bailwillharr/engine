#pragma once

#ifndef ENGINE_API
# ifdef _MSC_VER
#  ifdef ENGINE_EXPORTS
#   define ENGINE_API __declspec(dllexport)
#  else
#   define ENGINE_API __declspec(dllimport)
#  endif
# else
#  define ENGINE_API
# endif
#endif

#define ENGINE_API