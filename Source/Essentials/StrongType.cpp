// FOnline Engine
// SPDX-License-Identifier: MIT

#include "StrongType.h"

FO_BEGIN_NAMESPACE

using test_strong_type = strong_type<int32_t, struct test_strong_type_, strong_type_bool_test_tag, strong_type_arithmetics_tag>;
static_assert(some_strong_type<test_strong_type>);

FO_END_NAMESPACE
