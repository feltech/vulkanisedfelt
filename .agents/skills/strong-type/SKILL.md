---
name: strong-type
description: >-
  Use rollbear strong_type v14 for distinct domain values in vulkanisedfelt. Covers
  declarations, modifiers, explicit construction and unwrapping, ordering, and the
  migrated Vulkan type inventory. Use when adding or changing strong types or
  diagnosing their conversions and operators.
license: MIT
---

<!-- vulkanisedfelt
SPDX-License-Identifier: MIT
Copyright 2026 David Feltell
-->

# rollbear strong_type

## Version and integration

This guide describes **strong_type v14**, verified against the installed headers and the project's
migrated types. It supplies the ordinary API without requiring a source scan. For capabilities
outside this guide or a version change, consult the
[upstream documentation](https://github.com/rollbear/strong_type).

The dependency already exists in production: Conan `strong_type/v14`, CMake
`find_package(strong_type REQUIRED)`, and target `rollbear::strong_type`. It predates the fnfelt
migration; do not add a duplicate dependency or make it test-only.

Include the fine-grained header for each modifier used, plus `<strong_type/type.hpp>`.
`<strong_type/strong_type.hpp>` is the library's umbrella header; prefer the smaller headers in
project code.

## Declare a domain

```cpp
#include <cstdint>
#include <type_traits>
#include <utility>

#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

struct FamilyIndexTag;
struct QueueIndexTag;
using family_index = strong::type<std::uint32_t, FamilyIndexTag, strong::regular>;
using queue_index = strong::type<std::uint32_t, QueueIndexTag, strong::regular>;

constexpr auto family = family_index{2};
constexpr auto queue = queue_index{0};
static_assert(value_of(family) == 2);
static_assert(!std::is_convertible_v<std::uint32_t, family_index>);
static_assert(!std::is_convertible_v<family_index, std::uint32_t>);
static_assert(!std::is_convertible_v<family_index, queue_index>);
```

The underlying type, tag and modifier list determine the wrapper's identity. Give every distinct
domain its own tag, even if the underlying types match. A forward declaration is sufficient; keep
aliases `lower_case` and tags `CamelCase` in the domain namespace. Reusing the same complete
template arguments produces the same type despite a different alias name.

Construction from underlying values is explicit. There is no implicit scalar conversion,
cross-domain conversion or mixed scalar equality unless a modifier supplies one.
`family == family_index{2}` is valid; `family == 2` is not.

**Construction is not validation.** The forwarding constructor initializes the underlying value with
parentheses. Even `family_index{some_signed_or_wider_value}` can narrow or wrap; wrapper braces do
not enforce scalar list-initialization checks. Validate untrusted ranges and report domain errors
before constructing a wrapper.

`strong::regular` requires an underlying type that supports copy/move construction and assignment;
it is not suitable for move-only payloads. It supplies default construction and same-type equality.
In v14, its default constructor value-initializes the underlying value, so a default-constructed
numeric index is zero. This does not establish domain validity, and the modifier name alone does not
guarantee all `std::regular` requirements for arbitrary underlying types. Without a
default-construction modifier, the basic wrapper is not default-constructible. Avoid the explicit
`strong::uninitialized` escape hatch unless definite initialization is independently ensured.

## Unwrap at a boundary

Use **unqualified `value_of(wrapper)`**, found by ADL. It is a hidden friend; do not rely on
qualified `strong::value_of(wrapper)` lookup. The member form `wrapper.value_of()` also exists, but
the project prefers ADL.

| Expression                             | Result      |
| -------------------------------------- | ----------- |
| `value_of(wrapper)` for mutable lvalue | `T&`        |
| `value_of(wrapper)` for const lvalue   | `const T&`  |
| `value_of(std::move(wrapper))`         | `T&&`       |
| `value_of(std::move(const_wrapper))`   | `const T&&` |

These are references into the wrapper. Unwrapping an rvalue does not itself move the value; the
consumer decides whether to move. A reference obtained from a temporary wrapper dangles after the
temporary's lifetime ends. `auto raw = value_of(wrapper)` copies the underlying value;
`auto&& raw = value_of(wrapper)` retains a reference and needs the wrapper to stay alive.

Keep strong values through project APIs, records, optionals, Results and continuations. Unwrap where
the receiving interface requires the representation:

```cpp
// The Vulkan ABI and Vulkan structs remain C-shaped.
queue_create_info.queueFamilyIndex = value_of(family);
api.vk_get_device_queue(device.get(), value_of(family), value_of(queue), &handle);

// Immer subscripts require an ordinary index; the stored selection stays strong.
auto const& selected = physical_devices[value_of(selected_index)];
```

Plain local counters, enumeration capacities and retry counts need not become strong types. Keep
backend `Api`/`MockApi` method signatures identical to the Vulkan C API. Wrap raw observations when
they enter a project domain, rather than repeatedly unwrapping throughout policy code. Strong types
are value distinctions; they do not add validation or ownership of Vulkan resources.

## Choose only the operations the domain needs

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

Ordering alone does not provide equality. Our scores combine `strong::regular` with
`strong::strongly_ordered`, which supplies the comparisons needed by `std::ranges::less` and
`std::ranges::min_element`:

```cpp
#include <cstdint>

#include <strong_type/ordered.hpp>
#include <strong_type/regular.hpp>
#include <strong_type/type.hpp>

struct PolicyCostTag;
using policy_cost =
    strong::type<std::uint32_t, PolicyCostTag, strong::regular, strong::strongly_ordered>;

static_assert(policy_cost{1} < policy_cost{2});
```

Indices currently have equality but no ordering or arithmetic. Add a modifier when its operations
make sense for the domain, not merely to make an unrelated expression compile. For example, allowing
multiplication of two indices is usually undesirable. `strong::regular` already supplies equality;
adding `strong::equality` is redundant. Avoid `strong::implicitly_convertible_to<T>` and raw-value
comparison modifiers for domain indices: the library supports them, but they weaken the project's
explicit boundaries.

## Migrated inventory and maintenance

The aliases live in `src/vulkan/strong_types.hpp`, namespace `vulkandemo::vulkan`:

| Alias                   | Meaning                                                                      |
| ----------------------- | ---------------------------------------------------------------------------- |
| `queue_family_index`    | Position in a physical device's queue-family snapshot.                       |
| `queue_index`           | Position within the queues requested from one family at device creation.     |
| `physical_device_index` | Position in an instance's physical-device snapshot, including error context. |
| `queue_family_score`    | Lower-is-better queue-family policy cost.                                    |
| `physical_device_score` | Lower-is-better physical-device policy cost.                                 |

All five wrap `std::uint32_t`; only the two score types are ordered. Do not interchange the indices,
or treat an index as a stable identifier across unrelated or reordered snapshots. The device and
instance lifetime owners remain in `managed_handles.hpp`.

For a member that shares an alias's name, qualify the type, for example
`vulkan::queue_family_index queue_family_index;`, to avoid ambiguous type lookup. Existing selectors
return typed indices and score records; preserve those types when composing them.

When extending the inventory, introduce the alias with its first feature and update its producers,
consumers and expected values together. If history work is requested, keep each fixup with that
feature rather than introducing future domains early. Add compile-time nonconversion checks in the
corresponding embedded tests and retain the existing behavioral tests. Do not rewrite legacy
`src/types.hpp` or copy its modifiers/tags blindly: the prototype allows implicit conversions, and
its memory-type index accidentally shares an image-index tag.
