#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#define VOLK_IMPLEMENTATION
#include <volk.h>
#include <windows.h>

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_win32.h"
#include "../menu.h"
#include "../overlay.h"
#include "../src/hooks.h"

namespace Overlay::Vulkan
{
	static VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex);
	static VkResult VKAPI_CALL vkAcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex);
	static VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain);
	static VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);

	namespace Interface
	{
		inline VkAllocationCallbacks* pAllocator {nullptr};
		inline VkInstance vkInstance {VK_NULL_HANDLE};

		inline VkPhysicalDevice vkPhysicalDevice {VK_NULL_HANDLE};
		static uint32_t queueFamily {UINT32_MAX};

		static VkDevice vkDevice {nullptr};
		static VkQueue vkQueue {nullptr};

        static VkDescriptorPool vkDescriptorPool = VK_NULL_HANDLE;
        static VkRenderPass vkRenderPass = VK_NULL_HANDLE;
		static std::vector<ImGui_ImplVulkanH_Frame> vkFrames(1, {});
		static VkExtent2D vkExtent{ 1920, 1080 }; // Initial default, will be updated before buffer creation
	}

	inline bool Init()
	{
		if (FAILED(volkInitialize())) return false;

		VkInstanceCreateInfo instanceCreateInfo = {};
		constexpr auto instanceExtension = "VK_KHR_surface";

		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.enabledExtensionCount = 1;
		instanceCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		instanceCreateInfo.ppEnabledExtensionNames = &instanceExtension;

		if (FAILED(vkCreateInstance(&instanceCreateInfo, Interface::pAllocator, &Interface::vkInstance))) return false;
		volkLoadInstance(Interface::vkInstance);

		Interface::vkPhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(Interface::vkInstance);
		if (Interface::vkPhysicalDevice == VK_NULL_HANDLE) return false;

		Interface::queueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(Interface::vkPhysicalDevice);
		if (Interface::queueFamily == UINT32_MAX) return false;

		constexpr auto deviceExtension = "VK_KHR_swapchain";
		constexpr float queuePriority = 1.0f;

		const VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, Interface::queueFamily, 1, &queuePriority };

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pQueueCreateInfos = &queueInfo;
		deviceCreateInfo.enabledExtensionCount = 1;
		deviceCreateInfo.ppEnabledExtensionNames = &deviceExtension;

		VkDevice vkDevice {nullptr};
		if (FAILED(vkCreateDevice(Interface::vkPhysicalDevice, &deviceCreateInfo, Interface::pAllocator, &vkDevice))) return false;

		void* pAcquireNextImageKHR = vkGetDeviceProcAddr(vkDevice, "vkAcquireNextImageKHR");
		void* pAcquireNextImage2KHR = vkGetDeviceProcAddr(vkDevice, "vkAcquireNextImage2KHR");
		void* pCreateSwapchainKHR = vkGetDeviceProcAddr(vkDevice, "vkCreateSwapchainKHR");
		void* pQueuePresentKHR = vkGetDeviceProcAddr(vkDevice, "vkQueuePresentKHR");

		HooksManager::Setup<InlineHook>(pAcquireNextImageKHR, FUNCTION(vkAcquireNextImageKHR));
		HooksManager::Setup<InlineHook>(pAcquireNextImage2KHR, FUNCTION(vkAcquireNextImage2KHR));
		HooksManager::Setup<InlineHook>(pCreateSwapchainKHR, FUNCTION(vkCreateSwapchainKHR));
		HooksManager::Setup<InlineHook>(pQueuePresentKHR, FUNCTION(vkQueuePresentKHR));
		return true;
	}

	static void LoadDevice(const VkDevice device)
	{
		if (Interface::vkDevice != device)
		{
			volkLoadDevice(device);
			Interface::vkDevice = device;
		}
	}

	static void CreateRenderTarget(const VkDevice device, const VkSwapchainKHR swapchain)
	{
		uint32_t imageCount;
		vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
		Interface::vkFrames.resize(imageCount, {});

		std::vector<VkImage> images(imageCount);
		vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

		if (Interface::vkRenderPass == VK_NULL_HANDLE)
		{
			VkAttachmentDescription attachment = {};
			attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
			attachment.samples = VK_SAMPLE_COUNT_1_BIT;
			attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colorAttachment = {};
			colorAttachment.attachment = 0;
			colorAttachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachment;

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &attachment;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;

			vkCreateRenderPass(device, &renderPassInfo, Interface::pAllocator, &Interface::vkRenderPass);
		}

		if (Interface::vkDescriptorPool == VK_NULL_HANDLE)
		{
			VkDescriptorPoolSize poolSizes[] = {
				{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
				{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
				{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
				{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
				{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
				{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
				{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
				{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
				{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
				{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
				{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
			};

			VkDescriptorPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			poolInfo.maxSets = 1000;
			poolInfo.poolSizeCount = IM_ARRAYSIZE(poolSizes);
			poolInfo.pPoolSizes = poolSizes;

			vkCreateDescriptorPool(device, &poolInfo, Interface::pAllocator, &Interface::vkDescriptorPool);
		}

		// Create per-frame resources
		for (uint32_t i = 0; i < images.size(); ++i)
		{
			Interface::vkFrames[i].Backbuffer = images[i];
			ImGui_ImplVulkanH_Frame* frame = &Interface::vkFrames[i];

			// Command pool
			VkCommandPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			poolInfo.queueFamilyIndex = Interface::queueFamily;
			vkCreateCommandPool(device, &poolInfo, Interface::pAllocator, &frame->CommandPool);

			// Command buffer
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = frame->CommandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;
			vkAllocateCommandBuffers(device, &allocInfo, &frame->CommandBuffer);

			// Fence
			VkFenceCreateInfo fenceInfo = {};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			vkCreateFence(device, &fenceInfo, Interface::pAllocator, &frame->Fence);

			// Image view
			VkImageViewCreateInfo viewInfo = {};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = frame->Backbuffer;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			vkCreateImageView(device, &viewInfo, Interface::pAllocator, &frame->BackbufferView);

			// Framebuffer
			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = Interface::vkRenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = &frame->BackbufferView;
			framebufferInfo.width = Interface::vkExtent.width;
			framebufferInfo.height = Interface::vkExtent.height;
			framebufferInfo.layers = 1;
			vkCreateFramebuffer(device, &framebufferInfo, Interface::pAllocator, &frame->Framebuffer);
		}
	}

	static void CleanupRenderTarget()
	{
		for (auto& vkFrame : Interface::vkFrames)
		{
			if (vkFrame.Fence)
			{
				vkDestroyFence(Interface::vkDevice, vkFrame.Fence, Interface::pAllocator);
				vkFrame.Fence = nullptr;
			}
			if (vkFrame.CommandBuffer)
			{
				vkFreeCommandBuffers(Interface::vkDevice, vkFrame.CommandPool, 1, &vkFrame.CommandBuffer);
				vkFrame.CommandBuffer = nullptr;
			}
			if (vkFrame.CommandPool)
			{
				vkDestroyCommandPool(Interface::vkDevice, vkFrame.CommandPool, Interface::pAllocator);
				vkFrame.CommandPool = nullptr;
			}
			if (vkFrame.BackbufferView)
			{
				vkDestroyImageView(Interface::vkDevice, vkFrame.BackbufferView, Interface::pAllocator);
				vkFrame.BackbufferView = nullptr;
			}
			if (vkFrame.Framebuffer)
			{
				vkDestroyFramebuffer(Interface::vkDevice, vkFrame.Framebuffer, Interface::pAllocator);
				vkFrame.Framebuffer = nullptr;
			}
		}
		Interface::vkFrames.clear();
	}

	inline void CleanupVulkan()
	{
		std::thread([]
			{
				HooksManager::Unhook(&vkAcquireNextImageKHR, &vkAcquireNextImage2KHR, &vkCreateSwapchainKHR, &vkQueuePresentKHR);

				DisableRendering();

				ImGui_ImplVulkan_Shutdown();
				Menu::CleanupImGui();

				CleanupRenderTarget();

				vkDestroyDescriptorPool(Interface::vkDevice, Interface::vkDescriptorPool, Interface::pAllocator);
				vkDestroyInstance(Interface::vkInstance, Interface::pAllocator);
				vkDestroyRenderPass(Interface::vkDevice, Interface::vkRenderPass, Interface::pAllocator);
				Interface::pAllocator = nullptr;
			}).detach();
	}

	static VkResult VKAPI_CALL vkAcquireNextImageKHR(const VkDevice device, const VkSwapchainKHR swapchain, const uint64_t timeout, const VkSemaphore semaphore, const VkFence fence, uint32_t* pImageIndex)
	{
		LoadDevice(device);
		static auto& original = HooksManager::GetOriginal(&vkAcquireNextImageKHR);
		return original.stdcall<VkResult>(device, swapchain, timeout, semaphore, fence, pImageIndex);
	}

	static VkResult VKAPI_CALL vkAcquireNextImage2KHR(const VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex)
	{
		LoadDevice(device);
		static auto& original = HooksManager::GetOriginal(&vkAcquireNextImage2KHR);
		return original.stdcall<VkResult>(device, pAcquireInfo, pImageIndex);
	}

	static VkResult VKAPI_CALL vkCreateSwapchainKHR(const VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
	{
		Interface::vkExtent = pCreateInfo->imageExtent;

		CleanupRenderTarget();
		static auto& original = HooksManager::GetOriginal(&vkCreateSwapchainKHR);
		const auto result = original.stdcall<VkResult>(device, pCreateInfo, pAllocator, pSwapchain);
		CreateRenderTarget(device, *pSwapchain);
		return result;
	}

	static VkResult VKAPI_CALL vkQueuePresentKHR(const VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
	{
		[&pPresentInfo]
		{
			if (!Overlay::bEnabled)
			{
				SetEvent(screenCleaner.eventPresentSkipped);
				return;
			}

			if (!Interface::vkDevice || !pPresentInfo->pSwapchains) return;

			if (!bInitialized)
			{
				// Setup Dear ImGui context
				Menu::SetupImGui();
				// Setup Platform backend
				if (!ImGui_ImplWin32_Init(hWindow)) return;

				RECT clientRect {};
				if (GetClientRect(Overlay::hWindow, &clientRect))
				{
					Interface::vkExtent = {
						.width = static_cast<uint32_t>(clientRect.right - clientRect.left),
						.height = static_cast<uint32_t>(clientRect.bottom - clientRect.top)
					};
				}

				vkGetDeviceQueue(Interface::vkDevice, Interface::queueFamily, 0, &Interface::vkQueue);
				if (!Interface::vkQueue) return;

				CreateRenderTarget(Interface::vkDevice, *pPresentInfo->pSwapchains);
				if (!Interface::vkFrames.begin()->Framebuffer) return;

				ImGui_ImplVulkan_InitInfo initInfo = {};
				initInfo.ApiVersion = VK_HEADER_VERSION_COMPLETE;
				initInfo.Instance = Interface::vkInstance;
				initInfo.PhysicalDevice = Interface::vkPhysicalDevice;
				initInfo.Device = Interface::vkDevice;
				initInfo.QueueFamily = Interface::queueFamily;
				initInfo.Queue = Interface::vkQueue;
				initInfo.DescriptorPool = Interface::vkDescriptorPool;
				initInfo.MinImageCount = 2;
				initInfo.ImageCount = 2;
				initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
				initInfo.RenderPass = Interface::vkRenderPass;
				initInfo.Allocator = Interface::pAllocator;
				// Setup Renderer backend
				ImGui_ImplVulkan_Init(&initInfo);
				bInitialized = true;
			}

			const ImGui_ImplVulkanH_Frame* frame = &Interface::vkFrames[*pPresentInfo->pImageIndices];
				
			vkWaitForFences(Interface::vkDevice, 1, &frame->Fence, VK_TRUE, ~0ull);
			vkResetFences(Interface::vkDevice, 1, &frame->Fence);

			// Reset command buffer and begin recording
			vkResetCommandBuffer(frame->CommandBuffer, 0);
			VkCommandBufferBeginInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			bufferInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(frame->CommandBuffer, &bufferInfo);

			// Begin render pass
			VkRenderPassBeginInfo passInfo = {};
			passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			passInfo.renderPass = Interface::vkRenderPass;
			passInfo.framebuffer = frame->Framebuffer;
			passInfo.renderArea.extent = Interface::vkExtent;
			vkCmdBeginRenderPass(frame->CommandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);

			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplWin32_NewFrame();

			Overlay::RenderLogic();

			// Recording primitives into command buffer
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame->CommandBuffer);

			// Submit command buffer
			vkCmdEndRenderPass(frame->CommandBuffer);
			vkEndCommandBuffer(frame->CommandBuffer);

			// Submit to the queue
			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &frame->CommandBuffer;

			vkQueueSubmit(Interface::vkQueue, 1, &submitInfo, frame->Fence);
		}();
		static auto& original = HooksManager::GetOriginal(&vkQueuePresentKHR);
		return original.stdcall<VkResult>(queue, pPresentInfo);
	}
}
