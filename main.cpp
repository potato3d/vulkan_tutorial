#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <array>
#include <unordered_set>
#include <iterator>
#include <fstream>

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if(func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if(func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

class HelloTriangleApplication
{
public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Tutorial", nullptr, nullptr);

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensionNames = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		_requiredInstanceExtensions.insert(_requiredInstanceExtensions.end(), glfwExtensionNames, glfwExtensionNames + glfwExtensionCount);
	}

	void initVulkan() {
		createInstance();
		createDebugMessenger();
		createSurface();
		choosePhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createSwapChainImageViews();
		createRenderPass();
		createFramebuffers();
		createGraphicsPipelineLayout();
		createGraphicsPipeline();
		createCommandPool();
		createCommandBuffers();
		createSyncObjects();
	}

	void mainLoop()
	{
		while(!glfwWindowShouldClose(_window))
		{
			glfwPollEvents();
			drawFrame();
		}
		vkDeviceWaitIdle(_device);
	}

	void cleanup()
	{
		destroySyncObjects();
		vkDestroyCommandPool(_device, _commandPool, nullptr);
		vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(_device, _graphicsPipelineLayout, nullptr);
		destroyFramebuffers();
		vkDestroyRenderPass(_device, _renderPass, nullptr);
		destroySwapChainImageViews();
		vkDestroySwapchainKHR(_device, _swapChain, nullptr);
		vkDestroyDevice(_device, nullptr);
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		destroyDebugMessenger();
		vkDestroyInstance(_instance, nullptr);
		glfwDestroyWindow(_window);
		glfwTerminate();
	}

	// VULKAN STUFF ------------------------------------------------------------------------------------------------------------------------------------------------------------------

	struct QueueFamilies
	{
		std::vector<VkQueueFamilyProperties> properties;
		std::vector<VkBool32> supportsPresentation;
		std::optional<uint32_t> graphics;
		std::optional<uint32_t> present;
	};

	struct SwapChainInfo
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	void checkRequiredInstanceExtensions()
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::unordered_set<std::string_view> availableExtensions;
		for(const auto& e : extensions)
		{
			availableExtensions.emplace(e.extensionName);
		}

		puts("available instance extensions:");
		for(const auto& extension : extensions)
		{
			puts(extension.extensionName);
		}
		putc('\n', stdout);

		puts("enabling the following instance extensions:");
		for(auto extension : _requiredInstanceExtensions)
		{
			if(!availableExtensions.contains(extension))
			{
				throw std::runtime_error("required instance extension not found: " + std::string{extension});
			}
			puts(extension);
		}
		putc('\n', stdout);
	}

	void checkRequiredValidationLayers()
	{
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> layers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

		std::unordered_set<std::string_view> availableLayers;

		puts("available validation layers:");
		for(const auto& layer : layers)
		{
			availableLayers.emplace(layer.layerName);
			puts(layer.layerName);
		}
		putc('\n', stdout);

		puts("enabling the following validation layers:");
		for(auto layer : _requiredValidationLayers)
		{
			if(!availableLayers.contains(layer))
			{
				throw std::runtime_error("required validation layer not found: " + std::string{layer});
			}
			puts(layer);
		}
		putc('\n', stdout);
	}

	VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo()
	{
		// TODO:
		/*
			There are a lot more settings for the behavior of validation layers than just the flags specified in the VkDebugUtilsMessengerCreateInfoEXT struct. Browse to the Vulkan SDK and go to the Config directory. There you will find a vk_layer_settings.txt file that explains how to configure the layers.
			To configure the layer settings for your own application, copy the file to the Debug and Release directories of your project and follow the instructions to set the desired behavior.
		*/
		VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo{};
		messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		messengerCreateInfo.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | */VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		messengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		messengerCreateInfo.pfnUserCallback = debugCallback;
		messengerCreateInfo.pUserData = nullptr; // optional
		return messengerCreateInfo;
	}

	void createInstance()
	{
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan Tutorial";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Batata";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		if constexpr(_enableValidationLayers)
		{
			_requiredInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		checkRequiredInstanceExtensions();
		checkRequiredValidationLayers();

		VkInstanceCreateInfo instanceCreateInfo{};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pApplicationInfo = &appInfo;
		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(_requiredInstanceExtensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = _requiredInstanceExtensions.data();
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(_requiredValidationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = _requiredValidationLayers.data();

		VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = getDebugMessengerCreateInfo();
		if constexpr(_enableValidationLayers)
		{
			instanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&messengerCreateInfo;
		}

		if(vkCreateInstance(&instanceCreateInfo, nullptr, &_instance) != VK_SUCCESS)
		{
			throw std::runtime_error{"failed to create instance"};
		}
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		fprintf(stderr, "validation: %s\n", pCallbackData->pMessage);
		return VK_FALSE;
	}

	void createDebugMessenger()
	{
		if constexpr(!_enableValidationLayers)
		{
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = getDebugMessengerCreateInfo();

		if(CreateDebugUtilsMessengerEXT(_instance, &messengerCreateInfo, nullptr, &_debugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create debug messenger");
		}

		puts("created debug messenger\n");
	}

	void destroyDebugMessenger()
	{
		if constexpr(!_enableValidationLayers)
		{
			return;
		}
		DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
	}

	void createSurface()
	{
		if(glfwCreateWindowSurface(_instance, _window, nullptr, &_surface) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface");
		}
	}

	bool checkPhysicalDeviceExtensions(VkPhysicalDevice physicalDevice, std::vector<VkExtensionProperties>& physicalDeviceExtensions)
	{
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

		physicalDeviceExtensions.resize(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, physicalDeviceExtensions.data());

		std::unordered_set<std::string_view> availableExtensions;

		for(const auto& e : physicalDeviceExtensions)
		{
			availableExtensions.emplace(e.extensionName);
		}

		for(auto extension : _requiredDeviceExtensions)
		{
			if(!availableExtensions.contains(extension))
			{
				return false;
			}
		}

		return true;
	}

	bool checkPhysicalDeviceQueueFamilies(VkPhysicalDevice physicalDevice, QueueFamilies& families)
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

		families.properties.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, families.properties.data());

		families.supportsPresentation.resize(queueFamilyCount);
		for(uint32_t i = 0; i < queueFamilyCount; ++i)
		{
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, _surface, &families.supportsPresentation[i]);
		}

		// greedily choose first graphics queue that also supports presentation if any
		// TODO: handle multiple graphics queues, how to choose which one to use?
		// TODO: for compute and transfer queues, decide if it is better to reuse graphics queue or prefer another queue
		for(uint32_t i = 0; i < queueFamilyCount; ++i)
		{
			if(families.properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				families.graphics = i;
			}
			if(families.supportsPresentation[i])
			{
				families.present = i;
			}
			if(families.graphics == families.present)
			{
				return true;
			}
		}

		return families.graphics.has_value() && families.present.has_value();
	}

	bool checkPhysicalDeviceSwapChain(VkPhysicalDevice physicalDevice, SwapChainInfo& info)
	{
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, _surface, &info.capabilities);

		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, _surface, &formatCount, nullptr);

		info.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, _surface, &formatCount, info.formats.data());

		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, _surface, &presentModeCount, nullptr);

		info.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, _surface, &presentModeCount, info.presentModes.data());

		return !info.formats.empty() && !info.presentModes.empty();
	}

	void choosePhysicalDevice()
	{
		uint32_t physicalDeviceCount = 0;
		vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, nullptr);

		if(physicalDeviceCount == 0)
		{
			throw std::runtime_error("failed to find any GPU with Vulkan support");
		}

		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, physicalDevices.data());

		std::vector<VkExtensionProperties> extensions;
		QueueFamilies families;
		SwapChainInfo swapChainInfo;
		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceFeatures features;

		// greedily choose first discrete gpu if any
		// TODO: more sophisticated logic to choose best device with fallbacks
		for(auto physicalDevice : physicalDevices)
		{
			if(!checkPhysicalDeviceExtensions(physicalDevice, extensions))
			{
				continue;
			}

			if(!checkPhysicalDeviceQueueFamilies(physicalDevice, families))
			{
				continue;
			}

			if(!checkPhysicalDeviceSwapChain(physicalDevice, swapChainInfo))
			{
				continue;
			}

			vkGetPhysicalDeviceProperties(physicalDevice, &properties);
			vkGetPhysicalDeviceFeatures(physicalDevice, &features);

			_physicalDevice = physicalDevice;
			_queueFamilies = families;
			_swapChainInfo = swapChainInfo;

			if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				break;
			}
		}

		if(_physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to find a suitable GPU");
		}

		printf("chosen device: %s\n", properties.deviceName);

		for(uint32_t idx = 0; idx < _queueFamilies.properties.size(); ++idx)
		{
			const auto& family = _queueFamilies.properties[idx];

			printf("queue family %d:", idx);
			if(family.queueFlags & VK_QUEUE_GRAPHICS_BIT) fputs(" graphics", stdout);
			if(family.queueFlags & VK_QUEUE_COMPUTE_BIT) fputs(" compute", stdout);
			if(family.queueFlags & VK_QUEUE_TRANSFER_BIT) fputs(" transfer", stdout);
			if(family.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) fputs(" sparse_binding", stdout);
			if(family.queueFlags & VK_QUEUE_PROTECTED_BIT) fputs(" protected", stdout);
			if(_queueFamilies.supportsPresentation[idx]) fputs(" present", stdout);
			printf(" (count: %d)", family.queueCount);
			putc('\n', stdout);
		}

		printf("chosen graphics queue family: %d\n", _queueFamilies.graphics.value());
		printf("chosen present  queue family: %d\n", _queueFamilies.present.value());
		putc('\n', stdout);

		puts("available device extensions:");
		for(const auto& e : extensions)
		{
			puts(e.extensionName);
		}
		putc('\n', stdout);

		puts("enabling the following device extensions:");
		for(auto extension : _requiredDeviceExtensions)
		{
			puts(extension);
		}
		putc('\n', stdout);
	}

	void createLogicalDevice()
	{
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::unordered_set<uint32_t> uniqueQueueFamilies = {_queueFamilies.graphics.value(), _queueFamilies.present.value()};

		float queuePriority = 1.0f;
		for(auto queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;

			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		if constexpr(_enableValidationLayers)
		{
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(_requiredValidationLayers.size());
			deviceCreateInfo.ppEnabledLayerNames = _requiredValidationLayers.data();
		}
		else
		{
			deviceCreateInfo.enabledLayerCount = 0;
		}

		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(_requiredDeviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = _requiredDeviceExtensions.data();

		if(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical device");
		}

		vkGetDeviceQueue(_device, _queueFamilies.graphics.value(), 0, &_graphicsQueue);
		vkGetDeviceQueue(_device, _queueFamilies.present.value(), 0, &_presentQueue);
	}

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for(const auto& availableFormat : availableFormats)
		{
			if(availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
				availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}
		return availableFormats.front();
	}

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		for(const auto& availablePresentMode : availablePresentModes)
		{
			if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width = 0;
			int height = 0;
			glfwGetFramebufferSize(_window, &width, &height);

			VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	void createSwapChain()
	{
		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(_swapChainInfo.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(_swapChainInfo.presentModes);
		VkExtent2D extent = chooseSwapExtent(_swapChainInfo.capabilities);

		uint32_t imageCount = _swapChainInfo.capabilities.minImageCount + 1;
		if(_swapChainInfo.capabilities.maxImageCount > 0 && imageCount > _swapChainInfo.capabilities.maxImageCount)
		{
			imageCount = _swapChainInfo.capabilities.maxImageCount;
		}

		printf("swapchain supports min of %d and max of %d images\n", _swapChainInfo.capabilities.minImageCount, _swapChainInfo.capabilities.maxImageCount);

		VkSwapchainCreateInfoKHR swapChainCreateInfo{};
		swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapChainCreateInfo.surface = _surface;
		swapChainCreateInfo.minImageCount = imageCount;
		swapChainCreateInfo.imageFormat = surfaceFormat.format;
		swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
		swapChainCreateInfo.imageExtent = extent;
		swapChainCreateInfo.imageArrayLayers = 1;
		swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // It is also possible that you'll render images to a separate image first to perform operations like post-processing. In that case you may use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT instead and use a memory operation to transfer the rendered image to a swap chain image.

		uint32_t queueFamilyIndices[] = {_queueFamilies.graphics.value(), _queueFamilies.present.value()};
		if(_queueFamilies.graphics != _queueFamilies.present)
		{
			swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapChainCreateInfo.queueFamilyIndexCount = static_cast<uint32_t>(std::size(queueFamilyIndices));
			swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapChainCreateInfo.queueFamilyIndexCount = 0; // Optional
			swapChainCreateInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		swapChainCreateInfo.preTransform = _swapChainInfo.capabilities.currentTransform;
		swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // ignore alpha
		swapChainCreateInfo.presentMode = presentMode;
		swapChainCreateInfo.clipped = VK_TRUE;
		swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

		if(vkCreateSwapchainKHR(_device, &swapChainCreateInfo, nullptr, &_swapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create swap chain");
		}

		printf("asked for %d images,", imageCount);
		vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, nullptr);
		printf(" got %d images\n\n", imageCount);

		_swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, _swapChainImages.data());

		_swapChainImageFormat = surfaceFormat.format;
		_swapChainExtent = extent;
	}

	void createSwapChainImageViews()
	{
		_swapChainImageViews.resize(_swapChainImages.size());

		for(size_t i = 0; i < _swapChainImages.size(); ++i)
		{
			VkImageViewCreateInfo imageViewCreateInfo{};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = _swapChainImages[i];
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = _swapChainImageFormat;
			imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;

			if(vkCreateImageView(_device, &imageViewCreateInfo, nullptr, &_swapChainImageViews[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create image view");
			}
		}
	}

	void destroySwapChainImageViews()
	{
		for(auto imageView : _swapChainImageViews)
		{
			vkDestroyImageView(_device, imageView, nullptr);
		}
	}

	void createRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = _swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0; // index to VkAttachmentDescription array
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachments[] = {colorAttachment};
		VkAttachmentReference colorAttachmentRefs[] = {colorAttachmentRef}; // The index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0) out vec4 outColor directive!

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = static_cast<uint32_t>(std::size(colorAttachmentRefs));
		subpass.pColorAttachments = colorAttachmentRefs;

		VkSubpassDescription subpasses[] = {subpass};

		// src and dst are indices to subpasses in VkSubpassDescription array
		// dst must always be higher than src
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // special value that indicates implicit subpass before or after our subpasses
		dependency.dstSubpass = 0; // our only subpass
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkSubpassDependency dependencies[] = {dependency};

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(std::size(colorAttachments));
		renderPassCreateInfo.pAttachments = colorAttachments;
		renderPassCreateInfo.subpassCount = static_cast<uint32_t>(std::size(subpasses));
		renderPassCreateInfo.pSubpasses = subpasses;
		renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(std::size(dependencies));
		renderPassCreateInfo.pDependencies = dependencies;

		if(vkCreateRenderPass(_device, &renderPassCreateInfo, nullptr, &_renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create render pass");
		}
	}

	void createFramebuffers()
	{
		_framebuffers.resize(_swapChainImageViews.size());

		for(size_t i = 0; i < _swapChainImageViews.size(); ++i)
		{
			VkImageView attachments[] = {_swapChainImageViews[i]};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = _renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(std::size(attachments));
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = _swapChainExtent.width;
			framebufferInfo.height = _swapChainExtent.height;
			framebufferInfo.layers = 1;

			if(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_framebuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create framebuffer");
			}
		}
	}

	void destroyFramebuffers()
	{
		for(auto framebuffer : _framebuffers)
		{
			vkDestroyFramebuffer(_device, framebuffer, nullptr);
		}
	}

	void createGraphicsPipelineLayout()
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 0; // Optional
		pipelineLayoutCreateInfo.pSetLayouts = nullptr; // Optional
		pipelineLayoutCreateInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutCreateInfo.pPushConstantRanges = nullptr; // Optional

		if(vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &_graphicsPipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	std::vector<uint32_t> readSPV(const std::string& filename)
	{
		std::vector<uint32_t> byteCode;

		std::ifstream file(filename, std::ios::binary | std::ios::ate);

		if(!file)
		{
			throw std::runtime_error("failed to read shader file: " + filename);
		}

		size_t fileSizeBytes = file.tellg();

		if(fileSizeBytes % sizeof(uint32_t) != 0)
		{
			throw std::runtime_error("bytecode not multiple of uint32_t: " + filename);
		}

		byteCode.resize(fileSizeBytes / sizeof(uint32_t));

		file.seekg(0);
		file.read(reinterpret_cast<char*>(byteCode.data()), byteCode.size() * sizeof(uint32_t));

		return byteCode;
	}

	VkShaderModule createShaderModule(const std::vector<uint32_t>& byteCode)
	{
		VkShaderModuleCreateInfo shaderModuleCreateInfo{};
		shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleCreateInfo.codeSize = byteCode.size() * sizeof(uint32_t);
		shaderModuleCreateInfo.pCode = byteCode.data();

		VkShaderModule shaderModule;

		if(vkCreateShaderModule(_device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create shader module");
		}

		return shaderModule;
	}

	void createGraphicsPipeline()
	{
		// vertex shader --------------------------------------------------------------------------

		auto vertShaderCode = readSPV("shaders/simple.vert.spv");
		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";
		// TODO: pSpecializationInfo allows you to specify values for shader constants. You can use a single shader module where its behavior can be configured at pipeline creation by specifying different values for the constants used in it. This is more efficient than configuring the shader using variables at render time, because the compiler can do optimizations like eliminating if statements that depend on these values.

		// fragment shader --------------------------------------------------------------------------

		auto fragShaderCode = readSPV("shaders/simple.frag.spv");
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		// shader stages --------------------------------------------------------------------------

		VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		// vertex input --------------------------------------------------------------------------

		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
		vertexInputCreateInfo.pVertexBindingDescriptions = nullptr; // Optional
		vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
		vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr; // Optional

		// input assembly --------------------------------------------------------------------------

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
		inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

		// viewport --------------------------------------------------------------------------

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(_swapChainExtent.width);
		viewport.height = static_cast<float>(_swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = _swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportCreateInfo{};
		viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportCreateInfo.viewportCount = 1;
		viewportCreateInfo.pViewports = &viewport;
		viewportCreateInfo.scissorCount = 1;
		viewportCreateInfo.pScissors = &scissor;

		// rasterization --------------------------------------------------------------------------

		VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo{};
		rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationCreateInfo.depthClampEnable = VK_FALSE;
		rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationCreateInfo.lineWidth = 1.0f;
		rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
		rasterizationCreateInfo.depthBiasConstantFactor = 0.0f; // Optional
		rasterizationCreateInfo.depthBiasClamp = 0.0f; // Optional
		rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f; // Optional

		// multisampling --------------------------------------------------------------------------

		VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo{};
		multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
		multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisamplingCreateInfo.minSampleShading = 1.0f; // Optional
		multisamplingCreateInfo.pSampleMask = nullptr; // Optional
		multisamplingCreateInfo.alphaToCoverageEnable = VK_FALSE; // Optional
		multisamplingCreateInfo.alphaToOneEnable = VK_FALSE; // Optional

		// depth/stencil --------------------------------------------------------------------------

		// TODO:

		// color blending --------------------------------------------------------------------------

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

		VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo{};
		colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendCreateInfo.logicOpEnable = VK_FALSE;
		colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlendCreateInfo.attachmentCount = 1;
		colorBlendCreateInfo.pAttachments = &colorBlendAttachment;
		colorBlendCreateInfo.blendConstants[0] = 0.0f; // Optional
		colorBlendCreateInfo.blendConstants[1] = 0.0f; // Optional
		colorBlendCreateInfo.blendConstants[2] = 0.0f; // Optional
		colorBlendCreateInfo.blendConstants[3] = 0.0f; // Optional

		// dynamic state --------------------------------------------------------------------------
		// TODO: ignoring for now, will use later

		VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT};

		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
		dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
		dynamicStateCreateInfo.pDynamicStates = dynamicStates;

		// --------------------------------------------------------------------------------------------
		// graphics pipeline
		// --------------------------------------------------------------------------------------------

		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(std::size(shaderStages));
		pipelineCreateInfo.pStages = shaderStages;
		pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
		pipelineCreateInfo.pViewportState = &viewportCreateInfo;
		pipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
		pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
		pipelineCreateInfo.pDepthStencilState = nullptr; // Optional
		pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
		pipelineCreateInfo.pDynamicState = nullptr; // Optional
		pipelineCreateInfo.layout = _graphicsPipelineLayout;
		pipelineCreateInfo.renderPass = _renderPass;
		pipelineCreateInfo.subpass = 0; // which subpass where this graphics pipeline will be used
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineCreateInfo.basePipelineIndex = -1; // Optional
		// These last two values are only used if deriving from an existing pipeline. In this case, must set the VK_PIPELINE_CREATE_DERIVATIVE_BIT flag in the flags field of VkGraphicsPipelineCreateInfo.

		// Note: this function is designed to take multiple VkGraphicsPipelineCreateInfo objects and create multiple VkPipeline objects in a single call
		// The second parameter, for which we've passed the VK_NULL_HANDLE argument, references an optional VkPipelineCache object.
		// A pipeline cache can be used to store and reuse data relevant to pipeline creation across multiple calls to vkCreateGraphicsPipelines and even across program executions if the cache is stored to a file.
		// This makes it possible to significantly speed up pipeline creation at a later time.
		if(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &_graphicsPipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create graphics pipeline");
		}

		// we're allowed to destroy the shader modules as soon as pipeline creation is finished
		vkDestroyShaderModule(_device, fragShaderModule, nullptr);
		vkDestroyShaderModule(_device, vertShaderModule, nullptr);
	}

	void createCommandPool()
	{
		VkCommandPoolCreateInfo poolCreateInfo{};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolCreateInfo.queueFamilyIndex = _queueFamilies.graphics.value();
		poolCreateInfo.flags = 0; // Optional (later probably use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT)

		if(vkCreateCommandPool(_device, &poolCreateInfo, nullptr, &_commandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool");
		}
	}

	void createCommandBuffers()
	{
		_commandBuffers.resize(_framebuffers.size());

		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = _commandPool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = static_cast<uint32_t>(_commandBuffers.size());

		if(vkAllocateCommandBuffers(_device, &allocateInfo, _commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}

		// begin recording command buffers --------------------------------------------------------

		for(size_t i = 0; i < _commandBuffers.size(); ++i)
		{
			// begin command buffer -------------------------------------------

			VkCommandBufferBeginInfo commandBufferBeginInfo{};
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; // TODO: later probably use VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT and maybe VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
			commandBufferBeginInfo.flags = 0; // Optional
			commandBufferBeginInfo.pInheritanceInfo = nullptr; // Optional

			if(vkBeginCommandBuffer(_commandBuffers[i], &commandBufferBeginInfo) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to begin recording command buffer");
			}

			// begin render pass -------------------------------------------

			VkRenderPassBeginInfo renderPassBeginInfo{};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = _renderPass;
			renderPassBeginInfo.framebuffer = _framebuffers[i];
			renderPassBeginInfo.renderArea.offset = {0, 0};
			renderPassBeginInfo.renderArea.extent = _swapChainExtent;

			VkClearValue clearColors[] = {{0.0f, 0.0f, 0.0f, 1.0f}};
			renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(std::size(clearColors));
			renderPassBeginInfo.pClearValues = clearColors;

			vkCmdBeginRenderPass(_commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			// bind graphics pipeline -------------------------------------------

			vkCmdBindPipeline(_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);

			// draw! -------------------------------------------

			vkCmdDraw(_commandBuffers[i], 3, 1, 0, 0);

			// end render pass -------------------------------------------

			vkCmdEndRenderPass(_commandBuffers[i]);

			// end command buffer -------------------------------------------

			if(vkEndCommandBuffer(_commandBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to record command buffer");
			}
		}

		// end recording command buffers --------------------------------------------------------
	}

	void createSyncObjects()
	{
		_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		_framesInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		_imagesInFlightFences.resize(_swapChainImages.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreCreateInfo{};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // so we can render the first frame

		for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(_device, &fenceCreateInfo, nullptr, &_framesInFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create sync objects");
			}
		}
	}

	void destroySyncObjects()
	{
		for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			vkDestroySemaphore(_device, _renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(_device, _imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(_device, _framesInFlightFences[i], nullptr);
		}
	}

	void drawFrame()
	{
		// wait until current frame is finished -----------------------------------------------------

		vkWaitForFences(_device, 1, &_framesInFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);

		// acquire next image from swap chain -----------------------------------------------------

		uint32_t imageIndex = 0;
		vkAcquireNextImageKHR(_device, _swapChain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

		// check if a previous frame is using this image -----------------------------------------------------

		if(_imagesInFlightFences[imageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(_device, 1, &_imagesInFlightFences[imageIndex], VK_TRUE, UINT64_MAX);
		}
		_imagesInFlightFences[imageIndex] = _framesInFlightFences[_currentFrame];

		// submit command buffers to graphics queue -----------------------------------------------------

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// these two arrays run in parallel
		VkSemaphore waitSemaphores[] = {_imageAvailableSemaphores[_currentFrame]};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = static_cast<uint32_t>(std::size(waitSemaphores));
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &_commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = {_renderFinishedSemaphores[_currentFrame]};
		submitInfo.signalSemaphoreCount = static_cast<uint32_t>(std::size(signalSemaphores));
		submitInfo.pSignalSemaphores = signalSemaphores;

		// remember to reset the fence that will be triggered when current frame finishes
		vkResetFences(_device, 1, &_framesInFlightFences[_currentFrame]);

		if(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _framesInFlightFences[_currentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer");
		}

		// present image on screen -----------------------------------------------------

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = static_cast<uint32_t>(std::size(signalSemaphores));
		presentInfo.pWaitSemaphores = signalSemaphores;

		// these two arrays run in parallel
		VkSwapchainKHR swapChains[] = {_swapChain};
		uint32_t imageIndices[] = {imageIndex};
		presentInfo.swapchainCount = static_cast<uint32_t>(std::size(swapChains));
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = imageIndices;

		presentInfo.pResults = nullptr; // Optional

		vkQueuePresentKHR(_presentQueue, &presentInfo);

		// goto next frame -----------------------------------------------------

		_currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

private:
	std::vector<const char*> _requiredInstanceExtensions;

	std::vector<const char*> _requiredValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	std::vector<const char*> _requiredDeviceExtensions = {
	    VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

#ifdef _DEBUG
	static constexpr bool _enableValidationLayers = true;
#else
	static constexpr bool _enableValidationLayers = false;
#endif

	VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;

	static constexpr uint32_t WIDTH = 800;
	static constexpr uint32_t HEIGHT = 600;
	GLFWwindow* _window = nullptr;
	VkSurfaceKHR _surface = VK_NULL_HANDLE;

	VkInstance _instance = VK_NULL_HANDLE;
	
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	QueueFamilies _queueFamilies;
	SwapChainInfo _swapChainInfo;

	VkDevice _device = VK_NULL_HANDLE;
	VkQueue _graphicsQueue = VK_NULL_HANDLE;
	VkQueue _presentQueue = VK_NULL_HANDLE;
	
	VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
	std::vector<VkImage> _swapChainImages; // depends on device
	VkFormat _swapChainImageFormat;
	VkExtent2D _swapChainExtent;
	std::vector<VkImageView> _swapChainImageViews; // one per swapchain image
	
	VkRenderPass _renderPass;

	std::vector<VkFramebuffer> _framebuffers; // one per swapchain image

	VkPipelineLayout _graphicsPipelineLayout;
	VkPipeline _graphicsPipeline;

	VkCommandPool _commandPool;
	std::vector<VkCommandBuffer> _commandBuffers; // one per swapchain image

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
	std::vector<VkSemaphore> _imageAvailableSemaphores; // one per frame in flight
	std::vector<VkSemaphore> _renderFinishedSemaphores; // one per frame in flight
	std::vector<VkFence> _framesInFlightFences; // one per frame in flight
	std::vector<VkFence> _imagesInFlightFences; // one per swapchain image. is there any frame in flight currently associated to a given swapchain image?
	uint32_t _currentFrame = 0;
};

int main()
{
	HelloTriangleApplication app;

	try
	{
		app.run();
	}
	catch(const std::exception& e)
	{
		fprintf(stderr, "%s\n", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}