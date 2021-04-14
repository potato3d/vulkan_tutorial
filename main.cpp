#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // change projection computations to Vulkan [0, 1] standard range instead of OpenGL [-1, 1] standard range.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <array>
#include <unordered_set>
#include <iterator>
#include <fstream>
#include <chrono>
#include <functional>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

template<typename Func>
class ScopeExit
{
public:
	~ScopeExit() { _f(); }
	Func _f;
};

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
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->_windowResized = true;
	}

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Tutorial", nullptr, nullptr);
		
		glfwSetWindowUserPointer(_window, this);
		glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);

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
		createDescriptorSetLayout();
		createGraphicsPipelineLayout();
		createGraphicsPipeline();
		createCommandPool();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
		createSyncObjects();
		// TODO: maybe should create buffers before descriptor set layout and pool so that we can then create the graphics pipeline with every information we need
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
		cleanupSwapChain();
		destroySyncObjects();
		destroyIndexBuffer();
		destroyVertexBuffer();
		vkDestroySampler(_device, _textureSampler, nullptr);
		vkDestroyImageView(_device, _textureImageView, nullptr);
		destroyTextureImage();
		vkDestroyCommandPool(_device, _commandPool, nullptr);
		vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
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

	struct Vertex
	{
		glm::vec2 pos;
		glm::vec3 color;
		glm::vec2 texCoord;

		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

			// position
			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, pos);

			// color
			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, color);

			// texture coordinate
			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

			return attributeDescriptions;
		}
	};

	struct UniformBufferObject
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
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

		// TODO: https://vulkan.lunarg.com/doc/view/1.1.126.0/windows/best_practices.html
		VkValidationFeatureEnableEXT validationEnables[] = {VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT};
		VkValidationFeaturesEXT validationFeatures = {};
		validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
		validationFeatures.enabledValidationFeatureCount = 1;
		validationFeatures.pEnabledValidationFeatures = validationEnables;

		VkInstanceCreateInfo instanceCreateInfo{};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pApplicationInfo = &appInfo;
		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(_requiredInstanceExtensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = _requiredInstanceExtensions.data();
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(_requiredValidationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = _requiredValidationLayers.data();
		instanceCreateInfo.pNext = &validationFeatures;

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

	bool checkPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures& features)
	{
		vkGetPhysicalDeviceFeatures(physicalDevice, &features);
		return features.samplerAnisotropy;
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

			if(!checkPhysicalDeviceFeatures(physicalDevice, features))
			{
				continue;
			}

			vkGetPhysicalDeviceProperties(physicalDevice, &properties);			

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
		deviceFeatures.samplerAnisotropy = VK_TRUE; // request feature for texture sampling

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

	VkImageView createImageView(VkImage image, VkFormat format)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView = VK_NULL_HANDLE;
		if(vkCreateImageView(_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image view");
		}

		return imageView;
	}

	void createSwapChainImageViews()
	{
		_swapChainImageViews.resize(_swapChainImages.size());

		for(size_t i = 0; i < _swapChainImages.size(); ++i)
		{
			_swapChainImageViews[i] = createImageView(_swapChainImages[i], _swapChainImageFormat);
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
		// TODO: It is actually possible to bind multiple descriptor sets simultaneously.
		// You need to specify a descriptor layout for each descriptor set when creating the pipeline layout. 
		// Shaders can then reference specific descriptor sets like this:
		//    layout(set = 0, binding = 0) uniform UniformBufferObject { ... }
		// You can use this feature to put descriptors that vary per-object and descriptors that are shared into separate descriptor sets.
		// In that case you avoid rebinding most of the descriptors across draw calls which is potentially more efficient.
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &_descriptorSetLayout;
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

		auto vertShaderCode = readSPV("shaders/ubo_texture.vert.spv");
		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";
		// TODO: pSpecializationInfo allows you to specify values for shader constants. You can use a single shader module where its behavior can be configured at pipeline creation by specifying different values for the constants used in it. This is more efficient than configuring the shader using variables at render time, because the compiler can do optimizations like eliminating if statements that depend on these values.

		// fragment shader --------------------------------------------------------------------------

		auto fragShaderCode = readSPV("shaders/simple_texture.frag.spv");
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		// shader stages --------------------------------------------------------------------------

		VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		// vertex input --------------------------------------------------------------------------

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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

			// bind vertex buffer -------------------------------------------

			VkBuffer vertexBuffers[] = {_vertexBuffer};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(_commandBuffers[i], 0, 1, vertexBuffers, offsets); // TODO: link the binding parameters with the bindings we used during buffer creation

			// bind index buffer -------------------------------------------

			vkCmdBindIndexBuffer(_commandBuffers[i], _indexBuffer, 0, VK_INDEX_TYPE_UINT16);

			// bind descriptor sets -------------------------------------------

			// TODO: 4th parameter must match the layout(set) used in the shader?
			vkCmdBindDescriptorSets(_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipelineLayout, 0, 1, &_descriptorSets[i], 0, nullptr);

			// draw! -------------------------------------------

			vkCmdDrawIndexed(_commandBuffers[i], static_cast<uint32_t>(_indices.size()), 1, 0, 0, 0);

			// end render pass -------------------------------------------

			vkCmdEndRenderPass(_commandBuffers[i]);

			// end command buffer -------------------------------------------

			if(vkEndCommandBuffer(_commandBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to end recording command buffer");
			}
		}

		// end recording command buffers --------------------------------------------------------
	}

	void createSyncObjects()
	{
		_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		_framesInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

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

	void updateFPS()
	{
		static char buffer[256] = {};

		double currentTime = glfwGetTime();
		double delta = currentTime - _fpsLastTime;
		_fpsFrameCount++;
		if(delta >= 0.5)
		{
			double fps = double(_fpsFrameCount) / delta;
			double mps = (delta * 1000.0) / double(_fpsFrameCount);

			sprintf_s(buffer, std::size(buffer) - 1, "Vulkan Tutorial - %.2f fps | %.2f ms", fps, mps);

			glfwSetWindowTitle(_window, buffer);

			_fpsFrameCount = 0;
			_fpsLastTime = currentTime;
		}
	}

	void drawFrame()
	{
		updateFPS();

		// wait until current frame is finished -----------------------------------------------------

		vkWaitForFences(_device, 1, &_framesInFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);
		vkResetFences(_device, 1, &_framesInFlightFences[_currentFrame]);

		// acquire next image from swap chain -----------------------------------------------------

		uint32_t imageIndex = 0;
		VkResult result = vkAcquireNextImageKHR(_device, _swapChain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

		if(result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapChain();
			return;
		}
		else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image");
		}

		// update UBOs -----------------------------------------------------

		// The updateUniformBuffer takes care of screen resizing, so we don't need to recreate the descriptor set in recreateSwapChain.
		updateUniformBuffer(imageIndex);

		// submit command buffers to graphics queue -----------------------------------------------------

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// these two arrays run in parallel
		VkSemaphore imageAvailableSemaphores[] = {_imageAvailableSemaphores[_currentFrame]};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = static_cast<uint32_t>(std::size(imageAvailableSemaphores));
		submitInfo.pWaitSemaphores = imageAvailableSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &_commandBuffers[imageIndex];

		// if graphics and present queues are the same we don't need a semaphore for explicit synchronization
		VkSemaphore renderFinishedSemaphores[] = {_renderFinishedSemaphores[_currentFrame]};
		if(_queueFamilies.graphics != _queueFamilies.present)
		{
			submitInfo.signalSemaphoreCount = static_cast<uint32_t>(std::size(renderFinishedSemaphores));
			submitInfo.pSignalSemaphores = renderFinishedSemaphores;
		}

		if(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _framesInFlightFences[_currentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer");
		}

		// present image on screen -----------------------------------------------------

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		// if graphics and present queues are the same we don't need a semaphore for explicit synchronization
		if(_queueFamilies.graphics != _queueFamilies.present)
		{
			presentInfo.waitSemaphoreCount = static_cast<uint32_t>(std::size(renderFinishedSemaphores));
			presentInfo.pWaitSemaphores = renderFinishedSemaphores;
		}

		// these two arrays run in parallel
		VkSwapchainKHR swapChains[] = {_swapChain};
		uint32_t imageIndices[] = {imageIndex};
		presentInfo.swapchainCount = static_cast<uint32_t>(std::size(swapChains));
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = imageIndices;

		presentInfo.pResults = nullptr; // Optional

		result = vkQueuePresentKHR(_presentQueue, &presentInfo);

		if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _windowResized)
		{
			_windowResized = false;
			recreateSwapChain();
		}
		else if(result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to present swap chain image");
		}

		// goto next frame -----------------------------------------------------

		_currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void cleanupSwapChain()
	{
		vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
		destroyUniformBuffers();
		vkFreeCommandBuffers(_device, _commandPool, static_cast<uint32_t>(_commandBuffers.size()), _commandBuffers.data());
		vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(_device, _graphicsPipelineLayout, nullptr);
		vkDestroyRenderPass(_device, _renderPass, nullptr);
		destroyFramebuffers();
		destroySwapChainImageViews();
		vkDestroySwapchainKHR(_device, _swapChain, nullptr);
	}

	void recreateSwapChain()
	{
		// TODO: review this entire process, check other demos and source codes, etc.
		// I'm sure this can be simplified and made more efficient

		int width = 0, height = 0;
		glfwGetFramebufferSize(_window, &width, &height);
		while(width == 0 || height == 0)
		{
			glfwGetFramebufferSize(_window, &width, &height);
			glfwWaitEvents();
		}

		// TODO: It is possible to create a new swap chain while drawing commands on an image from the old swap chain are still in-flight.
		// You need to pass the previous swap chain to the oldSwapChain field in the VkSwapchainCreateInfoKHR struct and destroy the old swap chain as soon as you've finished using it.
		vkDeviceWaitIdle(_device);

		cleanupSwapChain();

		if(!checkPhysicalDeviceSwapChain(_physicalDevice, _swapChainInfo))
		{
			throw std::runtime_error("swapchain is not compatible anymore");
		}

		createSwapChain();
		createSwapChainImageViews();
		createRenderPass();
		createGraphicsPipelineLayout();
		createGraphicsPipeline();
		createFramebuffers();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
	}

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);

		// TODO: Right now we'll only concern ourselves with the type of memory and not the heap it comes from, but you can imagine that this can affect performance.
		for(uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
		{
			if((typeFilter & (1 << i)) &&
				(memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type");
	}

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
	{
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = size;
		bufferCreateInfo.usage = usage;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if(vkCreateBuffer(_device, &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create buffer");
		}

		// TODO: It should be noted that in a real world application, you're not supposed to actually call vkAllocateMemory for every individual buffer.
		// The maximum number of simultaneous memory allocations is limited by the maxMemoryAllocationCount physical device limit, which may be as low as 4096 even on high end hardware like an NVIDIA GTX 1080.
		// The right way to allocate memory for a large number of objects at the same time is to create a custom allocator that splits up a single allocation among many different objects by using the offset parameters that we've seen in many functions.
		// You can either implement such an allocator yourself, or use the VulkanMemoryAllocator library provided by the GPUOpen initiative: https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

		VkMemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.allocationSize = memRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if(vkAllocateMemory(_device, &memoryAllocateInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate buffer memory");
		}

		// TODO: Since this memory is allocated specifically for this buffer, the offset is simply 0.
		// If the offset is non-zero, then it is required to be divisible by memRequirements.alignment
		vkBindBufferMemory(_device, buffer, bufferMemory, 0);
	}

	VkCommandBuffer beginSingleShotCommands()
	{
		// TODO: We must first allocate a temporary command buffer.
		// You may wish to create a separate command pool for these kinds of short-lived buffers, because the implementation may be able to apply memory allocation optimizations.
		// Or reuse the same command buffer already created for drawing.
		VkCommandBufferAllocateInfo bufferAllocateInfo{};
		bufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		bufferAllocateInfo.commandPool = _commandPool;
		bufferAllocateInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		vkAllocateCommandBuffers(_device, &bufferAllocateInfo, &commandBuffer);

		VkCommandBufferBeginInfo commandBufferBeginInfo{};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to begin recording command buffer");
		}

		return commandBuffer;
	}

	void endSingleShotCommands(VkCommandBuffer commandBuffer)
	{
		if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to end recording command buffer");
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		if(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit copy command buffer");
		}
		// TODO: We could use a fence and wait with vkWaitForFences, or simply wait for the transfer queue to become idle with vkQueueWaitIdle.
		// A fence would allow you to schedule multiple transfers simultaneously and wait for all of them complete, instead of executing one at a time.
		// That may give the driver more opportunities to optimize.
		vkQueueWaitIdle(_graphicsQueue);

		vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
	}

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBuffer commandBuffer = beginSingleShotCommands();

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0; // Optional
		copyRegion.dstOffset = 0; // Optional
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		endSingleShotCommands(commandBuffer);
	}

	void createVertexBuffer()
	{
		// TODO: the driver may not immediately copy the data into the buffer memory, for example because of caching.
		// It is also possible that writes to the buffer are not visible in the mapped memory yet.
		// There are two ways to deal with that problem:
		// - Use a memory heap that is host coherent, indicated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		// - Call vkFlushMappedMemoryRanges after writing to the mapped memory, and call vkInvalidateMappedMemoryRanges before reading from the mapped memory
		// We went for the first approach, which ensures that the mapped memory always matches the contents of the allocated memory.
		// Do keep in mind that this may lead to slightly worse performance than explicit flushing, but we'll see why that doesn't matter in the next chapter.

		// Flushing memory ranges or using a coherent memory heap means that the driver will be aware of our writes to the buffer, but it doesn't mean that they are actually visible on the GPU yet.
		// The transfer of data to the GPU is an operation that happens in the background and the specification simply tells us that it is guaranteed to be complete as of the next call to vkQueueSubmit.

		VkDeviceSize bufferSize = sizeof(decltype(_vertices)::value_type) * _vertices.size();

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data = nullptr;
		vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, _vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(_device, stagingBufferMemory);

		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer, _vertexBufferMemory);

		copyBuffer(stagingBuffer, _vertexBuffer, bufferSize);

		vkFreeMemory(_device, stagingBufferMemory, nullptr);
		vkDestroyBuffer(_device, stagingBuffer, nullptr);
	}

	void destroyVertexBuffer()
	{
		vkFreeMemory(_device, _vertexBufferMemory, nullptr);
		vkDestroyBuffer(_device, _vertexBuffer, nullptr);
	}

	void createIndexBuffer()
	{
		// TODO: Remember you should allocate multiple resources like buffers from a single memory allocation, but in fact you should go a step further.
		// Driver developers recommend that you also store multiple buffers, like the vertex and index buffer, into a single VkBuffer and use offsets in commands like vkCmdBindVertexBuffers.
		// The advantage is that your data is more cache friendly in that case, because it's closer together.
		// It is even possible to reuse the same chunk of memory for multiple resources if they are not used during the same render operations, provided that their data is refreshed, of course.
		// This is known as aliasing and some Vulkan functions have explicit flags to specify that you want to do this.

		VkDeviceSize bufferSize = sizeof(decltype(_indices)::value_type) * _indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data = nullptr;
		vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, _indices.data(), (size_t)bufferSize);
		vkUnmapMemory(_device, stagingBufferMemory);

		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _indexBuffer, _indexBufferMemory);

		copyBuffer(stagingBuffer, _indexBuffer, bufferSize);

		vkDestroyBuffer(_device, stagingBuffer, nullptr);
		vkFreeMemory(_device, stagingBufferMemory, nullptr);
	}

	void destroyIndexBuffer()
	{
		vkFreeMemory(_device, _indexBufferMemory, nullptr);
		vkDestroyBuffer(_device, _indexBuffer, nullptr);
	}

	void createDescriptorSetLayout()
	{
		// Each descriptor set may have multiple resources of the same or different type.
		// What type of resources can be bound through a descriptor set is defined in a descriptor set layout.
		// There, through a VkDescriptorSetLayoutBinding structure, You specify a given type of a resource (for example sampler, storage image or uniform buffer) and the number of resources of this type accessed as an array inside shader.
		// You can also specify multiple resources of the same or different types as separate layout entries (multiple VkDescriptorSetLayoutBinding entries specified during layout creation).
		// Each such descriptor must use a separate, different binding.
		// And the same binding must be used inside shader to access given resource:
		//   layout(set = S, binding = B) uniform <variable_type> <variable_name>;
		// So, to sum that up:
		//   You can have an(homogenous) array of a single descriptor type
		//   You can have several "bindings" which each can hold any descriptor type resource(or their array)
		//   And as one more layer of indirection you can have several descriptor sets each with its own bindings
		// Rule of thumb is probably "less is more".
		// If you do not need the resources to have separate types or names, use array.
		// If you do not need separate set, use only one set.

		// example shader code:
		//
		//	// binding to a single sampled image descriptor in set 0
		//	layout(set = 0, binding = 0) uniform texture2D mySampledImage;
		//
		//	// binding to an array of sampled image descriptors in set 0
		//	layout(set = 0, binding = 1) uniform texture2D myArrayOfSampledImages[12];
		//
		//	// binding to a single uniform buffer descriptor in set 1
		//	layout(set = 1, binding = 0) uniform myUniformBuffer
		//	{
		//		 vec4 myElement[32];
		//	};
		// 
		// correspinding example API code:
		//
		//	const VkDescriptorSetLayoutBinding myDescriptorSetLayoutBinding[] =
		//	{
		//		// binding to a single image descriptor
		//		{
		//			0,                                      // binding
		//			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,       // descriptorType
		//			1,                                      // descriptorCount
		//			VK_SHADER_STAGE_FRAGMENT_BIT,           // stageFlags
		//			NULL                                    // pImmutableSamplers
		//		},
		//		// binding to an array of image descriptors
		//		{
		//			1,                                      // binding
		//			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,       // descriptorType
		//			12,                                     // descriptorCount
		//			VK_SHADER_STAGE_FRAGMENT_BIT,           // stageFlags
		//			NULL                                    // pImmutableSamplers
		//		},
		//		// binding to a single uniform buffer descriptor
		//		{
		//			0,                                      // binding
		//			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,      // descriptorType
		//			1,                                      // descriptorCount
		//			VK_SHADER_STAGE_FRAGMENT_BIT,           // stageFlags
		//			NULL                                    // pImmutableSamplers
		//		}
		//	};
		//	const VkDescriptorSetLayoutCreateInfo myDescriptorSetLayoutCreateInfo[] =
		//	{
		//		// Create info for first descriptor set with two descriptor bindings
		//		{
		//			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,    // sType
		//			NULL,                                                   // pNext
		//			0,                                                      // flags
		//			2,                                                      // bindingCount
		//			&myDescriptorSetLayoutBinding[0]                        // pBindings
		//		},
		//		// Create info for second descriptor set with one descriptor binding
		//		{
		//			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,    // sType
		//			NULL,                                                   // pNext
		//			0,                                                      // flags
		//			1,                                                      // bindingCount
		//			&myDescriptorSetLayoutBinding[2]                        // pBindings
		//		}
		//	};
		//	VkDescriptorSetLayout myDescriptorSetLayout[2];
		//	//
		//	// Create first descriptor set layout
		//	//
		//	myResult = vkCreateDescriptorSetLayout(
		//		myDevice,
		//		&myDescriptorSetLayoutCreateInfo[0],
		//		NULL,
		//		&myDescriptorSetLayout[0]);
		//	//
		//	// Create second descriptor set layout
		//	//
		//	myResult = vkCreateDescriptorSetLayout(
		//		myDevice,
		//		&myDescriptorSetLayoutCreateInfo[1],
		//		NULL,
		//		&myDescriptorSetLayout[1]);

		// TODO: It is possible for the shader variable to represent an array of uniform buffer objects, and descriptorCount specifies the number of values in the array.
		// This could be used to specify a transformation for each of the bones in a skeleton for skeletal animation, for example.
		// Our MVP transformation is in a single uniform buffer object, so we're using a descriptorCount of 1.
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0; // // must match layout(binding) used in the shader
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerLayoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding bindings[] = {uboLayoutBinding, samplerLayoutBinding};

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.bindingCount = static_cast<uint32_t>(std::size(bindings));
		layoutCreateInfo.pBindings = bindings;

		if(vkCreateDescriptorSetLayout(_device, &layoutCreateInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor set layout");
		}
	}

	void createUniformBuffers()
	{
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		_uniformBuffers.resize(_swapChainImages.size());
		_uniformBuffersMemory.resize(_swapChainImages.size());

		for(size_t i = 0; i < _swapChainImages.size(); i++)
		{
			createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _uniformBuffers[i], _uniformBuffersMemory[i]);
		}
	}

	void destroyUniformBuffers()
	{
		for(size_t i = 0; i < _swapChainImages.size(); i++)
		{
			vkDestroyBuffer(_device, _uniformBuffers[i], nullptr);
			vkFreeMemory(_device, _uniformBuffersMemory[i], nullptr);
		}
	}

	void updateUniformBuffer(uint32_t currentImage)
	{
		// TODO: Using a UBO this way is not the most efficient way to pass frequently changing values to the shader.
		// A more efficient way to pass a small buffer of data to shaders are push constants.
		// Check these out in the future.

		// TODO: For host-to-device memory operations, you need to perform a form of synchronization known as a "domain operation".
		// Fortunately, vkQueueSubmit automatically performs a domain operation on any host writes made visible before the vkQueueSubmit call.
		// So if you write stuff to GPU-visible memory, then call vkQueueSubmit (either in the same thread or via CPU-side inter-thread communication), any commands in that submit call (or later ones) will see the values you wrote.
		// Assuming you have made them visible. Writes to host - coherent memory are always visible to the GPU, but writes to non - coherent memory must be made visible via a call to vkFlushMappedMemoryRanges.
		// If you want to write to memory asynchronously to the GPU process that reads it, you'll need to use an event.
		// You write to the memory, make it visible if needs be, then set the event.
		// The GPU commands that read from it would wait on the event, using VK_ACCESS_HOST_WRITE_BIT as the source access, and VK_PIPELINE_STAGE_HOST_BIT as the source stage.
		// The destination access and stage are determined by how you plan to read from it.

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), _swapChainExtent.width / (float)_swapChainExtent.height, 0.1f, 10.0f);

		void* data;
		vkMapMemory(_device, _uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(_device, _uniformBuffersMemory[currentImage]);
	}

	void createDescriptorPool()
	{
		VkDescriptorPoolSize poolSizes[2] = {};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(_swapChainImages.size());
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(_swapChainImages.size());

		VkDescriptorPoolCreateInfo poolCreateInfo{};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.poolSizeCount = 2;
		poolCreateInfo.pPoolSizes = poolSizes;
		poolCreateInfo.maxSets = static_cast<uint32_t>(_swapChainImages.size());

		if(vkCreateDescriptorPool(_device, &poolCreateInfo, nullptr, &_descriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor pool");
		}
	}

	void createDescriptorSets()
	{
		std::vector<VkDescriptorSetLayout> layouts(_swapChainImages.size(), _descriptorSetLayout);
		
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = _descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32_t>(_swapChainImages.size());
		descriptorSetAllocateInfo.pSetLayouts = layouts.data();

		_descriptorSets.resize(layouts.size(), VK_NULL_HANDLE);
		if(vkAllocateDescriptorSets(_device, &descriptorSetAllocateInfo, _descriptorSets.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate descriptor sets");
		}

		for(size_t i = 0; i < _swapChainImages.size(); ++i)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = _uniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = _textureImageView;
			imageInfo.sampler = _textureSampler;

			// The pBufferInfo field is used for descriptors that refer to buffer data, pImageInfo is used for descriptors that refer to image data, and pTexelBufferView is used for descriptors that refer to buffer views.
			VkWriteDescriptorSet descriptorWrites[2] = {};
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = _descriptorSets[i];
			descriptorWrites[0].dstBinding = 0; // must match layout(binding) used in the shader
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].dstArrayElement = 0; // if the descriptor is an array, start index in array
			descriptorWrites[0].descriptorCount = 1; // if the descriptor is an array, number of elements to update in array
			descriptorWrites[0].pBufferInfo = &bufferInfo; // descriptorCount structures

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = _descriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(_device, static_cast<uint32_t>(std::size(descriptorWrites)), descriptorWrites, 0, nullptr);
		}
	}

	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
	{
		// TODO: It is possible that the VK_FORMAT_R8G8B8A8_SRGB format is not supported by the graphics hardware.
		// You should have a list of acceptable alternatives and go with the best one that is supported.
		// However, support for this particular format is so widespread that we'll skip this step.
		// Using different formats would also require annoying conversions. 
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.extent.width = width;
		imageCreateInfo.extent.height = height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.flags = 0; // Optional

		if(vkCreateImage(_device, &imageCreateInfo, nullptr, &_textureImage) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(_device, _textureImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if(vkAllocateMemory(_device, &allocInfo, nullptr, &_textureImageMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(_device, _textureImage, _textureImageMemory, 0);
	}

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkCommandBuffer commandBuffer = beginSingleShotCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;
		
		if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else
		{
			throw std::invalid_argument("unsupported layout transition");
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage,
			destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		endSingleShotCommands(commandBuffer);
	}

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
	{
		VkCommandBuffer commandBuffer = beginSingleShotCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = {0, 0, 0};
		region.imageExtent = {width, height, 1};

		vkCmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		endSingleShotCommands(commandBuffer);
	}

	void createTextureImage()
	{
		int texWidth = 0, texHeight = 0, texChannels = 0;
		stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		ScopeExit se{[&] {stbi_image_free(pixels); }};
		
		if(!pixels)
		{
			stbi_image_free(pixels);
			throw std::runtime_error("failed to load texture image from file");
		}

		VkDeviceSize imageSize = texWidth * texHeight * 4;

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;

		createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data = nullptr;
		vkMapMemory(_device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(_device, stagingBufferMemory);

		// TODO: All of the helper functions that submit commands so far have been set up to execute synchronously by waiting for the queue to become idle.
		// For practical applications it is recommended to combine these operations in a single command buffer and execute them asynchronously for higher throughput, especially the transitions and copy in the createTextureImage function. 

		createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _textureImage, _textureImageMemory);

		transitionImageLayout(_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		copyBufferToImage(stagingBuffer, _textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

		transitionImageLayout(_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(_device, stagingBuffer, nullptr);
		vkFreeMemory(_device, stagingBufferMemory, nullptr);
	}

	void destroyTextureImage()
	{
		vkDestroyImage(_device, _textureImage, nullptr);
		vkFreeMemory(_device, _textureImageMemory, nullptr);
	}

	void createTextureImageView()
	{
		_textureImageView = createImageView(_textureImage, VK_FORMAT_R8G8B8A8_SRGB);
	}

	void createTextureSampler()
	{
		// TODO: avoid asking this again (already did this when choosing best device)
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(_physicalDevice, &properties);

		VkSamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
		samplerCreateInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 0.0f;

		if(vkCreateSampler(_device, &samplerCreateInfo, nullptr, &_textureSampler) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create texture sampler");
		}
	}

private:
	double _fpsLastTime = 0.0;
	uint32_t _fpsFrameCount = 0;

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

	bool _windowResized = false;

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
	
	VkRenderPass _renderPass = VK_NULL_HANDLE;

	std::vector<VkFramebuffer> _framebuffers; // one per swapchain image

	VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout _graphicsPipelineLayout = VK_NULL_HANDLE;
	VkPipeline _graphicsPipeline = VK_NULL_HANDLE;

	VkCommandPool _commandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> _commandBuffers; // one per swapchain image // TODO: only because we are pre-recording. if the command buffers were filled each frame, we could reduce to one per frame in flight, instead of one per swapchain image

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
	std::vector<VkSemaphore> _imageAvailableSemaphores; // one per frame in flight
	std::vector<VkSemaphore> _renderFinishedSemaphores; // one per frame in flight
	std::vector<VkFence> _framesInFlightFences; // one per frame in flight
	uint32_t _currentFrame = 0;

	const std::vector<Vertex> _vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
	};
	const std::vector<uint16_t> _indices = {
        0, 1, 2, 2, 3, 0
	};
	VkBuffer _vertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory _vertexBufferMemory = VK_NULL_HANDLE;
	VkBuffer _indexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory _indexBufferMemory = VK_NULL_HANDLE;

	std::vector<VkBuffer> _uniformBuffers; // one per swapchain image // TODO: only because we are pre-recording. if the command buffers were filled each frame, we could reduce to one per frame in flight, instead of one per swapchain image
	std::vector<VkDeviceMemory> _uniformBuffersMemory; // one per swapchain image // TODO: only because we are pre-recording. if the command buffers were filled each frame, we could reduce to one per frame in flight, instead of one per swapchain image
	
	VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> _descriptorSets;

	VkImage _textureImage;
	VkDeviceMemory _textureImageMemory;
	VkImageView _textureImageView;
	VkSampler _textureSampler;

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