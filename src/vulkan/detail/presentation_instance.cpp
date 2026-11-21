// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_instance.hpp"

#include <utility>

namespace vulkandemo::vulkan::detail
{
auto make_presentation_instance(immer::array<extension_name> extension_names, instance_ptr instance)
    -> PresentationInstance
{
    return PresentationInstance{std::move(instance), std::move(extension_names)};
}
}  // namespace vulkandemo::vulkan::detail
