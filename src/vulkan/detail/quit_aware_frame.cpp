// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./quit_aware_frame.hpp"

#include <algorithm>
#include <utility>

#include <fnfelt/result.hpp>
#include <immer/array.hpp>

#include "../../sdl/event_types.hpp"
#include "../../sdl/quit_event.hpp"
#include "../../sdl/window_id_types.hpp"
#include "../errors.hpp"
#include "../quit_aware_frame_types.hpp"
#include "../submit_and_present_types.hpp"

namespace vulkandemo::vulkan::detail
{
auto stop_if_quit_requested(sdl::window_id window_id, immer::array<sdl::event> events)
    -> vulkan_result<immer::array<sdl::event>>
{
    namespace result = fnfelt::result;
    using result_type = vulkan_result<immer::array<sdl::event>>;
    if (std::ranges::any_of(
            events,
            [window_id](sdl::event const & event) { return sdl::should_quit(event, window_id); }))
    {
        return result::stopped<result_type>();
    }
    return result::success<result_type>(std::move(events));
}

auto make_quit_aware_frame(immer::array<sdl::event> events, CompletedPresentation completion)
    -> QuitAwareFrame
{
    return QuitAwareFrame{completion, std::move(events)};
}
}  // namespace vulkandemo::vulkan::detail
