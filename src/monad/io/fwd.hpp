#pragma once
namespace vulkandemo::monad::io
{
struct io_tag_t
{
};

template <typename F>
concept Action = !std::is_void_v<F>;

template <Action>
struct IO;
}  // namespace vulkandemo::monad::io
