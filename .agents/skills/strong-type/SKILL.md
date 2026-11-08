---
name: strong-type
description: >-
  Use the rollbear strong_type C++ library for distinct domain values. Covers
  declarations, modifiers, explicit construction, unwrapping, ordering and reference
  lifetimes. Use when adding or changing strong types or diagnosing their
  conversions and operators.
license: MIT
---

<!-- SPDX-License-Identifier: MIT
Copyright 2026 David Feltell
-->

# rollbear strong_type

## Scope and headers

This guide describes **strong_type v14**. For another release or capabilities outside this guide,
consult the [upstream documentation](https://github.com/rollbear/strong_type).

Include `<strong_type/type.hpp>` and the fine-grained header for each modifier used.
`<strong_type/strong_type.hpp>` is the umbrella header.

## Declare distinct domains

```cpp
#include <cstdint>
#include <type_traits>

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

struct RowIndexTag;
struct ColumnIndexTag;
using row_index = strong::type<std::uint32_t, RowIndexTag, strong::regular>;
using column_index = strong::type<std::uint32_t, ColumnIndexTag, strong::regular>;

constexpr auto row = row_index{2};
constexpr auto column = column_index{0};
static_assert(value_of(row) == 2);
static_assert(!std::is_convertible_v<std::uint32_t, row_index>);
static_assert(!std::is_convertible_v<row_index, std::uint32_t>);
static_assert(!std::is_convertible_v<row_index, column_index>);
```

The underlying type, tag and modifier list determine the wrapper's identity. Give distinct domains
different tags, even when their representations match. A forward declaration of a tag is sufficient.
Two aliases with identical template arguments name the same type.

Construction from underlying values is explicit. There is no implicit scalar conversion,
cross-domain conversion or mixed scalar equality unless a modifier supplies one.
`row == row_index{2}` is valid; `row == 2` is not.

Use a typed expression for default arguments, because copy-list-initialization cannot use the
explicit constructor:

```cpp
void process_rows(row_index first = row_index{0});  // Not: first = {0}.
```

**Construction is not validation.** The forwarding constructor initializes the underlying value with
parentheses. Even `row_index{some_signed_or_wider_value}` can narrow or wrap; wrapper braces do not
enforce scalar list-initialization checks. Validate untrusted ranges before constructing a wrapper.

`strong::regular` requires an underlying type supporting copy/move construction and assignment; it
is not suitable for move-only payloads. It supplies default construction and same-type equality. In
v14, its default constructor value-initializes the underlying value, so a default-constructed
numeric index is zero. That does not establish domain validity, and the modifier name alone does not
guarantee all `std::regular` requirements for arbitrary payloads. Without a default-construction
modifier, the basic wrapper is not default-constructible. Avoid `strong::uninitialized` unless
definite initialization is independently ensured.

## Unwrap at a representation boundary

Use unqualified `value_of(wrapper)`, found by ADL. It is a hidden friend; do not rely on qualified
`strong::value_of(wrapper)` lookup. The member form `wrapper.value_of()` also exists.

| Expression                             | Result      |
| -------------------------------------- | ----------- |
| `value_of(wrapper)` for mutable lvalue | `T&`        |
| `value_of(wrapper)` for const lvalue   | `const T&`  |
| `value_of(std::move(wrapper))`         | `T&&`       |
| `value_of(std::move(const_wrapper))`   | `const T&&` |

These references point into the wrapper. Unwrapping an rvalue does not itself move the value; the
consumer decides whether to move. A reference obtained from a temporary wrapper dangles after the
temporary's lifetime ends. `auto raw = value_of(wrapper)` copies the underlying value;
`auto&& raw = value_of(wrapper)` retains a reference and requires the wrapper to stay alive. Include
`<utility>` when using `std::move`.

Keep values wrapped while their domain matters. Unwrap when an interface requires the underlying
representation:

```cpp
void consume_row_number(std::uint32_t);

void process_row(row_index selected_row)
{
    consume_row_number(value_of(selected_row));
}
```

Wrapping a raw observation when it enters a domain avoids repeated conversions within calculations.
A strong type distinguishes values; it does not by itself validate them or manage external
resources.

## Choose the required operations

| Modifier                    | Header under `strong_type/` | Capability                                                                                              |
| --------------------------- | --------------------------- | ------------------------------------------------------------------------------------------------------- |
| `strong::regular`           | `regular.hpp`               | Default construction and same-type equality; requires underlying copy/move construction and assignment. |
| `strong::strongly_ordered`  | `ordered.hpp`               | `<=>` producing `std::strong_ordering`, requiring suitable underlying ordering.                         |
| `strong::ordered`           | `ordered.hpp`               | Separate `<`, `<=`, `>`, `>=` operators; does not add equality.                                         |
| `strong::incrementable`     | `incrementable.hpp`         | Prefix/postfix `++`.                                                                                    |
| `strong::bicrementable`     | `bicrementable.hpp`         | Prefix/postfix `++` and `--`.                                                                           |
| `strong::arithmetic`        | `arithmetic.hpp`            | Same-type arithmetic and compound assignments; underlying operations must support them.                 |
| `strong::boolean`           | `boolean.hpp`               | Explicit conversion to `bool`.                                                                          |
| `strong::convertible_to<T>` | `convertible_to.hpp`        | Explicit conversion to the requested type.                                                              |
| `strong::ostreamable`       | `ostreamable.hpp`           | Stream insertion of the underlying value.                                                               |
| `strong::hashable`          | `hashable.hpp`              | `std::hash` support using the underlying hash.                                                          |

Ordering alone does not provide equality. Combining `strong::regular` with
`strong::strongly_ordered` supplies comparisons suitable for `std::ranges::less` and
`std::ranges::min_element`:

```cpp
#include <cstdint>

#include <strong_type/ordered.hpp>
#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

struct CostTag;
using cost = strong::type<std::uint32_t, CostTag, strong::regular, strong::strongly_ordered>;

static_assert(cost{1} < cost{2});
```

Add a modifier when the operation makes sense for the domain. For example, multiplying two indices
is usually undesirable. `strong::regular` already supplies equality, so adding `strong::equality` is
redundant. `strong::implicitly_convertible_to<T>` and raw-value comparison modifiers deliberately
relax boundaries; use them only when that interoperability is intended.
