#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <span>

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
		vkDestroyInstance(_instance, nullptr);
		glfwDestroyWindow(_window);
		glfwTerminate();
	}

	std::span<const char*> getExtensionsToEnable()
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
		std::span<const char*> glfwExtensions{glfwExtensionNames, glfwExtensionCount};

		puts("enabling the following extensions for glfw:");
		for(auto glfwExtension : glfwExtensions)
		{
			if(auto itr = std::find_if(vkExtensions.begin(), vkExtensions.end(), [&](const auto& e) {return e.extensionName == std::string_view{glfwExtension}; });
				itr == vkExtensions.end())
			{
				throw std::runtime_error("required glfw extension not found: " + std::string{glfwExtension});
			}
			puts(glfwExtension);
		}
		putc('\n', stdout);

		return glfwExtensions;
	}

	std::span<const char*> getValidationLayersToEnable()
	{
#ifdef NDEBUG
		return {};
#endif
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

		std::span<const char*> extensions = getExtensionsToEnable();
		std::span<const char*> layers = getValidationLayersToEnable();

		VkInstanceCreateInfo instanceCreateInfo{};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pApplicationInfo = &appInfo;
		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
		instanceCreateInfo.ppEnabledLayerNames = layers.data();

		if(vkCreateInstance(&instanceCreateInfo, nullptr, &_instance) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create instance!");
		}
	}

private:
	static constexpr uint32_t WIDTH = 800;
	static constexpr uint32_t HEIGHT = 600;
	GLFWwindow* _window;

	std::vector<const char*> _validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

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