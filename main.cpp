#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION

#include <iostream>
#include <vector>
#include <volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Vulkan_LoadLibrary(nullptr);
    volkInitializeCustom(reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr()));

    VkInstance instance; {
        VkApplicationInfo applicationInfo{
            VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "2DEngine", 0, nullptr, 0, VK_API_VERSION_1_3
        };
        uint32_t extension_count;
        char const *const*extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);

        const VkInstanceCreateInfo create_info{
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &applicationInfo, 0, nullptr,
            extension_count, extensions,
        };
        SDL_assert(!vkCreateInstance(&create_info, nullptr, &instance));
        volkLoadInstance(instance);
    }

    uint32_t physical_device_count = 1;
    VkPhysicalDevice physical_device;
    SDL_assert(!vkEnumeratePhysicalDevices(instance, &physical_device_count, &physical_device));

    SDL_Window *window{SDL_CreateWindow("2DEngine", 800, 600, SDL_WINDOW_VULKAN)};
    SDL_assert(window);

    VkSurfaceKHR surface;
    SDL_assert(SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface));

    float queue_priority{0};
    VkDeviceQueueCreateInfo queue_create_info{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, 0, 1, &queue_priority
    };

    auto device_extension{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const VkDeviceCreateInfo create_info{
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, 0, 1, &queue_create_info, 0, nullptr, 1, &device_extension,
        nullptr
    };

    VkDevice device;
    SDL_assert(!vkCreateDevice(physical_device, &create_info, nullptr, &device));

    VkQueue queue;
    vkGetDeviceQueue(device, 0, 0, &queue);

    VkSurfaceCapabilitiesKHR surface_capabilities;
    SDL_assert(!vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities));

    VkSwapchainKHR swapchain;
    uint32_t queue_family_index{0};
    const VkSwapchainCreateInfoKHR swapchain_info{
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, nullptr, 0, surface, surface_capabilities.minImageCount + 1,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_COLORSPACE_SRGB_NONLINEAR_KHR, surface_capabilities.currentExtent.width,
        surface_capabilities.currentExtent.height, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE,
        1, &queue_family_index, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_PRESENT_MODE_FIFO_KHR, true, nullptr
    };
    SDL_assert(!vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain));

    uint32_t image_count;
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
    auto images{new VkImage[image_count]};
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, images);

    VkCommandPool command_pool;
    const VkCommandPoolCreateInfo command_pool_info{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0, queue_family_index
    };
    SDL_assert(!vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool));

    std::vector<VkCommandBuffer> command_buffers{image_count};
    const VkCommandBufferAllocateInfo command_buffer_info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        image_count
    };
    SDL_assert(!vkAllocateCommandBuffers(device, &command_buffer_info, command_buffers.data()));

    std::vector<VkSemaphore> image_available_semaphores{image_count};
    std::vector<VkSemaphore> render_finished_semaphores{image_count};
    std::vector<VkFence> fences{image_count};

    constexpr VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
    constexpr VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
    for (uint32_t i{}; i < image_count; ++i) {
        SDL_assert(!vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphores[i]));
        SDL_assert(!vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphores[i]));
        SDL_assert(!vkCreateFence(device, &fence_info, nullptr, &fences[i]));
    }

    // clear the screen
    for (uint32_t i{}; i < image_count; ++i) {
        constexpr VkCommandBufferBeginInfo begin_info{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            nullptr
        };
        VkCommandBuffer command_buffer{command_buffers[i]};
        VkSemaphore image_available_semaphore{image_available_semaphores[i]};
        VkSemaphore render_finished_semaphore{render_finished_semaphores[i]};

        SDL_assert(!vkBeginCommandBuffer(command_buffer, &begin_info));

        const VkImageMemoryBarrier barrier{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, images[i],
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &barrier);

        constexpr VkClearColorValue clear_color{{1.0f, 0.0f, 0.0f, 1.0f}};
        constexpr VkImageSubresourceRange subresource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(command_buffer, images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1,
                             &subresource_range);

        const VkImageMemoryBarrier barrier2{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED, images[i], {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &barrier2);

        SDL_assert(!vkEndCommandBuffer(command_buffer));

        constexpr VkPipelineStageFlags wait_stages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        const VkSubmitInfo submit_info{
            VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, &wait_stages, 1, &command_buffer,
            1, &render_finished_semaphore
        };
        SDL_assert(!vkQueueSubmit(queue, 1, &submit_info, fences[i]));
    }

    vkWaitForFences(device, image_count, fences.data(), true, UINT64_MAX);

    auto semaphore{image_available_semaphores[0]};

    for (bool running{true}; running;) {
        for (SDL_Event event; SDL_PollEvent(&event);)
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;

                default: ;
            }

        uint32_t image_index;
        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE,
                              &image_index);

        VkPresentInfoKHR present_info{
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, &semaphore, 1, &swapchain,
            &image_index, nullptr
        };

        vkQueuePresentKHR(queue, &present_info);
    }

    return EXIT_SUCCESS;
}
