// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include <tuple>

#include <immer/box.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include <fnfelt/interop/stdexec.hpp>
#include <fnfelt/monad/io.hpp>
#include <fnfelt/monad/reader_io.hpp>
#include <fnfelt/monad/state_io.hpp>

int main()
{
    namespace io = fnfelt::monad::io;
    namespace reader_io = fnfelt::monad::reader_io;
    namespace state_io = fnfelt::monad::state_io;
    using state_type = immer::box<int>;

    auto actions = io::sequence(io::just(20), io::just(22));
    auto read = reader_io::lift<state_type>(actions);
    auto program = state_io::lift<state_type>(read).store([](int first, int second, state_type)
                                                          { return state_type{first + second}; });
    auto [values, state] = program.build(state_type{0}).sync_wait(lf::lazy_pool{2});
    auto [first, second] = values;
    return first + second == 42 && *state == 42 ? 0 : 1;
}
