// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./presentation_instance_extensions.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include <vulkan/vulkan_core.h>

#include <fnfelt/result.hpp>
#include <immer/array.hpp>
#include <immer/array_transient.hpp>

#include "./extension_types.hpp"
#include "./renderer_errors.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <variant>

#include <doctest/doctest.h>
#endif  // DOCTEST_CONFIG_DISABLE

namespace vulkandemo::vulkan
{
namespace
{
// Name-only discovery also works with 1.4 headers predating the KHR promotion.
constexpr std::string_view khr_surface_maintenance1 = "VK_KHR_surface_maintenance1";
constexpr std::string_view ext_surface_maintenance1 = "VK_EXT_surface_maintenance1";
}  // namespace

auto select_presentation_instance_extensions(
    immer::array<extension_name> required_names,
    immer::array<VkExtensionProperties> available_extensions)
    -> renderer_result<immer::array<extension_name>>
{
    namespace result = fnfelt::result;
    using result_type = renderer_result<immer::array<extension_name>>;
    auto extension_names = immer::array<extension_name>{}.transient();
    auto missing_extensions = immer::array<extension_name>{}.transient();
    auto is_available = [&](std::string_view name)
    {
        return std::ranges::any_of(
            available_extensions,
            [name](VkExtensionProperties const & extension)
            { return name == extension.extensionName; });
    };
    auto add_required = [&](extension_name name)
    {
        if (std::ranges::find(extension_names, name) != extension_names.end())
        {
            return;
        }
        std::string_view contents = value_of(name).view();
        extension_names.push_back(name);
        if (!is_available(contents))
        {
            missing_extensions.push_back(std::move(name));
        }
    };
    for (extension_name const & name : required_names)
    {
        add_required(name);
    }
    constexpr std::array shared_dependencies{
        std::string_view{VK_KHR_SURFACE_EXTENSION_NAME},
        std::string_view{VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME}};
    for (std::string_view name : shared_dependencies)
    {
        add_required(extension_name{name});
    }
    if (!missing_extensions.empty())
    {
        return result::error<result_type>(PresentationInstanceFailure{
            PresentationInstanceFailureReason::missing_required_extensions,
            std::move(missing_extensions).persistent()});
    }

    bool const has_khr = is_available(khr_surface_maintenance1);
    bool const has_ext = is_available(ext_surface_maintenance1);
    if (!has_khr && !has_ext)
    {
        return result::error<result_type>(PresentationInstanceFailure{
            PresentationInstanceFailureReason::no_maintenance1_path, {}});
    }
    if (has_khr)
    {
        add_required(extension_name{khr_surface_maintenance1});
    }
    if (has_ext)
    {
        add_required(extension_name{ext_surface_maintenance1});
    }
    return result::success<result_type>(std::move(extension_names).persistent());
}
}  // namespace vulkandemo::vulkan

#ifndef DOCTEST_CONFIG_DISABLE
namespace vulkandemo::vulkan
{
namespace
{
auto extension_properties(immer::array<std::string_view> names)
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

TEST_CASE("[pure] presentation instance extensions retain every available maintenance1 path")
{
    bool has_khr = false;
    bool has_ext = false;
    SUBCASE("KHR only")
    {
        has_khr = true;
    }
    SUBCASE("EXT only")
    {
        has_ext = true;
    }
    SUBCASE("both families")
    {
        has_khr = true;
        has_ext = true;
    }
    auto names =
        immer::array<std::string_view>{
            "VK_TEST_platform",
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME}
            .transient();
    if (has_khr)
    {
        names.push_back(khr_surface_maintenance1);
    }
    if (has_ext)
    {
        names.push_back(ext_surface_maintenance1);
    }

    renderer_result<immer::array<extension_name>> outcome = select_presentation_instance_extensions(
        {extension_name{"VK_TEST_platform"}, extension_name{VK_KHR_SURFACE_EXTENSION_NAME}},
        extension_properties(std::move(names).persistent()));

    REQUIRE(fnfelt::result::is_success(outcome));
    immer::array<extension_name> selected_extensions =
        fnfelt::result::get_success(std::move(outcome));
    REQUIRE_EQ(
        selected_extensions.size(), 3 + static_cast<int>(has_khr) + static_cast<int>(has_ext));
    CHECK_EQ(value_of(selected_extensions[0]).view(), "VK_TEST_platform");
    CHECK_EQ(value_of(selected_extensions[1]).view(), VK_KHR_SURFACE_EXTENSION_NAME);
    CHECK_EQ(
        value_of(selected_extensions[2]).view(), VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
    if (has_khr)
    {
        CHECK_EQ(value_of(selected_extensions[3]).view(), khr_surface_maintenance1);
    }
    if (has_ext)
    {
        CHECK_EQ(value_of(selected_extensions[has_khr ? 4 : 3]).view(), ext_surface_maintenance1);
    }
}

TEST_CASE("[pure] presentation instance extensions deduplicate platform names and dependencies")
{
    immer::array<extension_name> required_names{
        extension_name{ext_surface_maintenance1},
        extension_name{VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME},
        extension_name{"VK_TEST_platform"},
        extension_name{"VK_TEST_platform"},
        extension_name{VK_KHR_SURFACE_EXTENSION_NAME}};
    immer::array<VkExtensionProperties> available = extension_properties(
        {"VK_TEST_platform",
         VK_KHR_SURFACE_EXTENSION_NAME,
         VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
         khr_surface_maintenance1,
         ext_surface_maintenance1,
         khr_surface_maintenance1});

    renderer_result<immer::array<extension_name>> outcome =
        select_presentation_instance_extensions(required_names, available);

    REQUIRE(fnfelt::result::is_success(outcome));
    immer::array<extension_name> selected_extensions =
        fnfelt::result::get_success(std::move(outcome));
    REQUIRE_EQ(selected_extensions.size(), 5);
    CHECK_EQ(value_of(selected_extensions[0]).view(), ext_surface_maintenance1);
    CHECK_EQ(
        value_of(selected_extensions[1]).view(), VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
    CHECK_EQ(value_of(selected_extensions[2]).view(), "VK_TEST_platform");
    CHECK_EQ(value_of(selected_extensions[3]).view(), VK_KHR_SURFACE_EXTENSION_NAME);
    CHECK_EQ(value_of(selected_extensions[4]).view(), khr_surface_maintenance1);
    CHECK_EQ(required_names.size(), 5);
}

TEST_CASE(
    "[pure] presentation instance extensions report missing mandatory dependencies before "
    "alternatives")
{
    immer::array<VkExtensionProperties> available =
        extension_properties({khr_surface_maintenance1});

    renderer_result<immer::array<extension_name>> outcome = select_presentation_instance_extensions(
        {extension_name{"VK_TEST_platform"}, extension_name{"VK_TEST_platform"}}, available);

    REQUIRE(fnfelt::result::is_error(outcome));
    PresentationInstanceFailure failure =
        std::get<PresentationInstanceFailure>(fnfelt::result::get_error(outcome));
    CHECK_EQ(failure.reason, PresentationInstanceFailureReason::missing_required_extensions);
    REQUIRE_EQ(failure.missing_extensions.size(), 3);
    CHECK_EQ(value_of(failure.missing_extensions[0]).view(), "VK_TEST_platform");
    CHECK_EQ(value_of(failure.missing_extensions[1]).view(), VK_KHR_SURFACE_EXTENSION_NAME);
    CHECK_EQ(
        value_of(failure.missing_extensions[2]).view(),
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
}

TEST_CASE("[pure] presentation instance extensions distinguish unavailable alternative families")
{
    immer::array<VkExtensionProperties> available = extension_properties(
        {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME});

    renderer_result<immer::array<extension_name>> outcome =
        select_presentation_instance_extensions({}, available);

    REQUIRE(fnfelt::result::is_error(outcome));
    PresentationInstanceFailure failure =
        std::get<PresentationInstanceFailure>(fnfelt::result::get_error(outcome));
    CHECK_EQ(failure.reason, PresentationInstanceFailureReason::no_maintenance1_path);
    CHECK(failure.missing_extensions.empty());
}
}  // namespace vulkandemo::vulkan
#endif  // DOCTEST_CONFIG_DISABLE
