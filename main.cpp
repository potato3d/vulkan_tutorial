#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstdlib>
#include <vector>

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
		destroyDebugMessenger();
		vkDestroyInstance(_instance, nullptr);
		glfwDestroyWindow(_window);
		glfwTerminate();
	}

	// VULKAN STUFF ------------------------------------------------------------------------------------------------------------------------------------------------------------------

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
		VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo{};
		messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		messengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
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

		puts("created debug messenger");
	}

	void destroyDebugMessenger()
	{
		if constexpr(!_enableValidationLayers)
		{
			return;
		}
		DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
	}

	std::vector<const char*> getExtensionsToEnable()
	{
		uint32_t vkExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, nullptr);
		std::vector<VkExtensionProperties> vkExtensions(vkExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, vkExtensions.data());

		puts("available extensions:");
		for(const auto& extension : vkExtensions)
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
			if(auto itr = std::find_if(vkExtensions.begin(), vkExtensions.end(), [&](const auto& e) {return e.extensionName == std::string_view{extension}; });
				itr == vkExtensions.end())
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

private:
	static constexpr uint32_t WIDTH = 800;
	static constexpr uint32_t HEIGHT = 600;
	GLFWwindow* _window;

	std::vector<const char*> _validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

#ifdef _DEBUG
	static constexpr bool _enableValidationLayers = true;
#else
	static constexpr bool _enableValidationLayers = false;
#endif

	VkDebugUtilsMessengerEXT _debugMessenger;

	VkInstance _instance;
};

int main() {
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