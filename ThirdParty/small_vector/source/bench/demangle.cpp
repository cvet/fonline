//=======================================================================
// Copyright (c) 2014 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include <cstdlib>
#include <memory>

#include "demangle.hpp"

#if defined (__GLIBCXX__) || defined (_LIBCPP_VERSION)

#include <cxxabi.h>

std::string
demangle (const char* name)
{
  struct deleter
  {
    void
    operator() (char *ptr) const
    {
      free (ptr);
    }
  };

  int status = 0;
  std::unique_ptr<char, deleter> demName (abi::__cxa_demangle (name, nullptr, nullptr, &status));
  return (status == 0) ? demName.get () : name;
}

#else

std::string
demangle (const char *name)
{
  return name;
}

#endif
