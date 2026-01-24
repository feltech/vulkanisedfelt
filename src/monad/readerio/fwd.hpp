#pragma once
#include <type_traits>

namespace vulkandemo::monad::readerio
{
struct readerio_tag_t
{
};

template <typename F>
concept Action = !std::is_void_v<F>;

template <Action>
struct ReaderIO;
}  // namespace vulkandemo::monad::readerio
