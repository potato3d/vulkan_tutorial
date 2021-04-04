#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <array>
#include <unordered_set>
#include <iterator>

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
		createGraphicsPipeline();
	}

	void mainLoop()
	{
		while(!glfwWindowShouldClose(_window))
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
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
			swapChainCreateInfo.queueFamilyIndexCount = 2;
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

	void createGraphicsPipeline()
	{

	}

private:
	static constexpr uint32_t WIDTH = 800;
	static constexpr uint32_t HEIGHT = 600;
	GLFWwindow* _window = nullptr;

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

	VkSurfaceKHR _surface;

	VkInstance _instance = VK_NULL_HANDLE;
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	QueueFamilies _queueFamilies;
	SwapChainInfo _swapChainInfo;
	VkDevice _device = VK_NULL_HANDLE;
	VkQueue _graphicsQueue = VK_NULL_HANDLE;
	VkQueue _presentQueue = VK_NULL_HANDLE;
	VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
	std::vector<VkImage> _swapChainImages;
	VkFormat _swapChainImageFormat;
	VkExtent2D _swapChainExtent;
	std::vector<VkImageView> _swapChainImageViews;
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