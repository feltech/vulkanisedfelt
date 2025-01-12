// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <tuple>
#include <utility>

#include <vulkan/vulkan_core.h>

#include "../macros.hpp"
#include "../types.hpp"

namespace vulkandemo::draw::detail
{

/**
 * Creates an exclusive vertex buffer and memory, and maps and copies data to
 * it.
 *
 * @param device The device to create the buffer on.
 * @param memory_type_idx The memory type index to use for the memory.
 * @param vertices The vertices to copy into the buffer.
 *
 * @return A tuple of buffer handle and its associated memory handle.
 */
std::tuple<types::VulkanBufferPtr, types::VulkanDeviceMemoryPtr>
create_exclusive_vertex_buffer_and_memory(
	types::VulkanDevicePtr const & device,
	types::VulkanMemoryTypeIdx const memory_type_idx,
	types::ContiguousContainer auto const & vertices)
{
	std::span const host_vertex_memory = std::as_bytes(std::span{vertices});

	types::VulkanBufferPtr buffer_ptr = [&]
	{
		VkBufferCreateInfo const buffer_create_info{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.flags = 0,
			.size = host_vertex_memory.size(),
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		};

		VkBuffer out = nullptr;
		VK_CHECK(
			vkCreateBuffer(device.get(), &buffer_create_info, nullptr, &out),
			"Failed to create buffer");

		return types::make_buffer_ptr(device, out);
	}();

	VkMemoryRequirements const memory_requirements = [&]
	{
		VkMemoryRequirements out;
		vkGetBufferMemoryRequirements(device.get(), buffer_ptr.get(), &out);
		return out;
	}();

	auto [device_memory] = [&]
	{
		VkMemoryAllocateInfo const memory_allocate_info{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memory_requirements.size,
			.memoryTypeIndex = memory_type_idx,
		};

		VkDeviceMemory out = nullptr;
		VK_CHECK(
			vkAllocateMemory(device.get(), &memory_allocate_info, nullptr, &out),
			"Failed to allocate memory");

		return std::make_tuple(types::make_device_memory_ptr(device, out));
	}();

	std::byte * const mapped_memory = [&]
	{
		void * out = nullptr;
		VK_CHECK(
			vkMapMemory(device.get(), device_memory.get(), 0, memory_requirements.size, 0, &out),
			"Failed to map memory");
		return static_cast<std::byte *>(out);
	}();

	std::ranges::copy(host_vertex_memory, mapped_memory);

	return {std::move(buffer_ptr), std::move(device_memory)};
}
}  // namespace vulkandemo::draw::detail
