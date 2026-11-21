// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./swapchain_maintenance1_selection.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace vulkandemo::vulkan
{
auto select_swapchain_maintenance1_extension(
    immer::array<extension_name> enabled_instance_extensions,
    immer::array<VkExtensionProperties> device_extensions) -> std::optional<extension_name>
{
    auto is_enabled = [&](std::string_view name)
    {
        return std::ranges::any_of(
            enabled_instance_extensions,
            [name](extension_name const & enabled) { return value_of(enabled).view() == name; });
    };
    auto is_advertised = [&](std::string_view name)
    {
        return std::ranges::any_of(
            device_extensions,
            [name](VkExtensionProperties const & extension)
            { return name == extension.extensionName; });
    };
    if (!is_enabled(VK_KHR_SURFACE_EXTENSION_NAME) ||
        !is_enabled(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME) ||
        !is_advertised(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
    {
        return std::nullopt;
    }

    // Ordered alternatives preserve KHR preference without storing duplicate family flags.
    constexpr std::array families{
        std::pair{
            std::string_view{"VK_KHR_surface_maintenance1"},
            std::string_view{"VK_KHR_swapchain_maintenance1"}},
        std::pair{
            std::string_view{"VK_EXT_surface_maintenance1"},
            std::string_view{"VK_EXT_swapchain_maintenance1"}}};
    auto selected = std::ranges::find_if(
        families,
        [&](auto family) { return is_enabled(family.first) && is_advertised(family.second); });
    if (selected == families.end())
    {
        return std::nullopt;
    }
    return extension_name{selected->second};
}
}  // namespace vulkandemo::vulkan

#ifndef DOCTEST_CONFIG_DISABLE
#include <doctest/doctest.h>
#include <immer/array_transient.hpp>

namespace vulkandemo::vulkan
{
namespace
{
auto maintenance_device_extensions(immer::array<std::string_view> names)
    -> immer::array<VkExtensionProperties>
{
    auto properties = immer::array<VkExtensionProperties>{}.transient();
    for (std::string_view name : names)
    {
        VkExtensionProperties property{};
        std::ranges::copy(name, property.extensionName);
        properties.push_back(property);
    }
    return std::move(properties).persistent();
}
}  // namespace

TEST_CASE("[pure] maintenance1 extension candidates prefer matching KHR and fall back to EXT")
{
    bool has_khr_device = true;
    bool has_khr_instance = true;
    bool has_ext = true;
    SUBCASE("both complete paths prefer KHR") {}
    SUBCASE("KHR-only support needs no EXT path")
    {
        has_ext = false;
    }
    SUBCASE("EXT-only device uses the enabled EXT path")
    {
        has_khr_device = false;
    }
    SUBCASE("EXT-only instance uses the advertised EXT path")
    {
        has_khr_instance = false;
    }
    auto enabled =
        immer::array<extension_name>{
            extension_name{VK_KHR_SURFACE_EXTENSION_NAME},
            extension_name{VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME}}
            .transient();
    auto advertised = immer::array<std::string_view>{VK_KHR_SWAPCHAIN_EXTENSION_NAME}.transient();
    if (has_ext)
    {
        enabled.push_back(extension_name{"VK_EXT_surface_maintenance1"});
        advertised.push_back("VK_EXT_swapchain_maintenance1");
    }
    if (has_khr_instance)
    {
        enabled.push_back(extension_name{"VK_KHR_surface_maintenance1"});
    }
    if (has_khr_device)
    {
        advertised.push_back("VK_KHR_swapchain_maintenance1");
    }

    std::optional selected_extension = select_swapchain_maintenance1_extension(
        std::move(enabled).persistent(),
        maintenance_device_extensions(std::move(advertised).persistent()));

    REQUIRE(selected_extension.has_value());
    CHECK_EQ(
        value_of(*selected_extension).view(),
        has_khr_device && has_khr_instance ? "VK_KHR_swapchain_maintenance1"
                                           : "VK_EXT_swapchain_maintenance1");
}

TEST_CASE("[pure] maintenance1 candidates require matching families and shared dependencies")
{
    bool has_surface = true;
    bool has_capabilities2 = true;
    bool has_swapchain = true;
    bool has_khr_device = true;
    bool has_khr_instance = true;
    SUBCASE("missing instance surface")
    {
        has_surface = false;
    }
    SUBCASE("missing surface capabilities2")
    {
        has_capabilities2 = false;
    }
    SUBCASE("missing device swapchain")
    {
        has_swapchain = false;
    }
    SUBCASE("KHR instance cannot enable an EXT-only device")
    {
        has_khr_device = false;
    }
    SUBCASE("EXT instance cannot enable a KHR-only device")
    {
        has_khr_instance = false;
    }
    auto enabled = immer::array<extension_name>{}.transient();
    auto advertised = immer::array<std::string_view>{}.transient();
    if (has_surface)
    {
        enabled.push_back(extension_name{VK_KHR_SURFACE_EXTENSION_NAME});
    }
    if (has_capabilities2)
    {
        enabled.push_back(extension_name{VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME});
    }
    enabled.push_back(
        extension_name{
            has_khr_instance ? "VK_KHR_surface_maintenance1" : "VK_EXT_surface_maintenance1"});
    if (has_swapchain)
    {
        advertised.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    advertised.push_back(
        has_khr_device ? "VK_KHR_swapchain_maintenance1" : "VK_EXT_swapchain_maintenance1");

    std::optional selected_extension = select_swapchain_maintenance1_extension(
        std::move(enabled).persistent(),
        maintenance_device_extensions(std::move(advertised).persistent()));

    CHECK_FALSE(selected_extension.has_value());
}

TEST_CASE("[pure] maintenance1 extension candidates are absent without a device extension")
{
    immer::array enabled{
        extension_name{VK_KHR_SURFACE_EXTENSION_NAME},
        extension_name{VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME},
        extension_name{"VK_KHR_surface_maintenance1"},
        extension_name{"VK_EXT_surface_maintenance1"}};

    std::optional selected_extension = select_swapchain_maintenance1_extension(
        enabled, maintenance_device_extensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME}));

    CHECK_FALSE(selected_extension.has_value());
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
