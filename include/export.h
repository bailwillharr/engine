#pragma once

#ifndef DECLSPEC
# ifdef _MSC_VER
#  ifdef ENGINE_EXPORTS
#   define DECLSPEC __declspec(dllexport)
#  else
#   define DECLSPEC __declspec(dllimport)
#  endif
# else
#  define DECLSPEC
# endif
#endif