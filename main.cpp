#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <array>
#include <unordered_set>

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
	}

	void initVulkan() {
		createInstance();
		createDebugMessenger();
		createSurface();
		choosePhysicalDevice();
		createLogicalDevice();
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

	std::vector<const char*> getExtensionsToEnable()
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		puts("available extensions:");
		for(const auto& extension : extensions)
		{
			puts(extension.extensionName);
		}
		putc('\n', stdout);

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensionNames = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> requiredExtensions{glfwExtensionNames, glfwExtensionNames + glfwExtensionCount};

		if constexpr(_enableValidationLayers)
		{
			requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		puts("enabling the following extensions:");
		for(auto extension : requiredExtensions)
		{
			if(auto itr = std::find_if(extensions.begin(), extensions.end(), [&](const auto& e) {return e.extensionName == std::string_view{extension}; });
				itr == extensions.end())
			{
				throw std::runtime_error("required extension not found: " + std::string{extension});
			}
			puts(extension);
		}
		putc('\n', stdout);

		return requiredExtensions;
	}

	std::vector<const char*> getValidationLayersToEnable()
	{
		uint32_t vkLayerCount = 0;
		vkEnumerateInstanceLayerProperties(&vkLayerCount, nullptr);

		std::vector<VkLayerProperties> vkLayers(vkLayerCount);
		vkEnumerateInstanceLayerProperties(&vkLayerCount, vkLayers.data());
		
		puts("available validation layers:");
		for(const auto& layer : vkLayers)
		{
			puts(layer.layerName);
		}
		putc('\n', stdout);
		
		puts("enabling the following validation layers:");
		for(auto layer : _validationLayers)
		{
			if(auto itr = std::find_if(vkLayers.begin(), vkLayers.end(), [&](const auto& l) {return l.layerName == std::string_view{layer}; });
				itr == vkLayers.end())
			{
				throw std::runtime_error("required validation layer not found: " + std::string{layer});
			}
			puts(layer);
		}
		putc('\n', stdout);

		return _validationLayers;
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

		std::vector<const char*> extensions = getExtensionsToEnable();
		std::vector<const char*> layers = getValidationLayersToEnable();

		VkInstanceCreateInfo instanceCreateInfo{};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pApplicationInfo = &appInfo;
		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
		instanceCreateInfo.ppEnabledLayerNames = layers.data();

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

		VkPhysicalDeviceProperties chosenPhysicalDeviceProperties;

		// greedily choose first discrete gpu if any
		// TODO: more sophisticated logic to choose best device with fallbacks
		for(auto physicalDevice : physicalDevices)
		{
			VkPhysicalDeviceProperties physicalDeviceProperties;
			vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

			VkPhysicalDeviceFeatures physicalDeviceFeatures;
			vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

			QueueFamilies families = getQueueFamilies(physicalDevice);

			if(families.graphics.has_value() && families.present.has_value())
			{
				chosenPhysicalDeviceProperties = physicalDeviceProperties;
				_physicalDevice = physicalDevice;
				_queueFamilies = families;

				if(physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					break;
				}
			}
		}

		if(_physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to find a suitable GPU");
		}

		printf("chosen device: %s\n", chosenPhysicalDeviceProperties.deviceName);

		for(uint32_t idx = 0; idx < _queueFamilies.properties.size(); ++idx)
		{
			const auto& family = _queueFamilies.properties[idx];

			printf("queue family %d:", idx);
			if(family.queueFlags & VK_QUEUE_GRAPHICS_BIT) fputs(" graphics", stdout);
			if(family.queueFlags & VK_QUEUE_COMPUTE_BIT) fputs(" compute", stdout);
			if(family.queueFlags & VK_QUEUE_TRANSFER_BIT) fputs(" transfer", stdout);
			if(family.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) fputs(" sparse_binding", stdout);
			if(family.queueFlags & VK_QUEUE_PROTECTED_BIT) fputs(" protected", stdout);
			printf(" (count: %d)", family.queueCount);
			putc('\n', stdout);
		}

		printf("chosen graphics queue family: %d\n", _queueFamilies.graphics.value());
		printf("chosen present  queue family: %d\n", _queueFamilies.present.value());
	}

	QueueFamilies getQueueFamilies(VkPhysicalDevice physicalDevice)
	{
		QueueFamilies families;
		
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
				break;
			}
		}

		return families;
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
		deviceCreateInfo.enabledExtensionCount = 0;

		if constexpr(_enableValidationLayers)
		{
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(_validationLayers.size());
			deviceCreateInfo.ppEnabledLayerNames = _validationLayers.data();
		}
		else
		{
			deviceCreateInfo.enabledLayerCount = 0;
		}

		if(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical device");
		}

		vkGetDeviceQueue(_device, _queueFamilies.graphics.value(), 0, &_graphicsQueue);
		vkGetDeviceQueue(_device, _queueFamilies.present.value(), 0, &_presentQueue);
	}

private:
	static constexpr uint32_t WIDTH = 800;
	static constexpr uint32_t HEIGHT = 600;
	GLFWwindow* _window = nullptr;

	std::vector<const char*> _validationLayers = {
		"VK_LAYER_KHRONOS_validation"
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
	VkDevice _device = VK_NULL_HANDLE;
	VkQueue _graphicsQueue = VK_NULL_HANDLE;
	VkQueue _presentQueue = VK_NULL_HANDLE;
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