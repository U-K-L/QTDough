#include "QTDoughApplication.h"
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_system.h>
#include <SDL_syswm.h>
#include <stdio.h>
#include <stdexcept>
#include <iostream>
#include <set>

//extern SDL_Window *SDLWindow;
int QTDoughApplication::Run() {
    InitSDLWindow();
	InitVulkan();
	return 0;
}

void QTDoughApplication::InitSDLWindow()
{
    //The surface contained by the window
    //Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    }
    else
    {
        //Create window
        QTSDLWindow = SDL_CreateWindow("QTDough", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if (QTSDLWindow == NULL)
        {
            printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        }
        else
        {
            //Get window surface
            _screenSurface = SDL_GetWindowSurface(QTSDLWindow);

            //Fill the surface white
            SDL_FillRect(_screenSurface, NULL, SDL_MapRGB(_screenSurface->format, 0xFF, 0xFF, 0xFF));

            //Update the surface
            SDL_UpdateWindowSurface(QTSDLWindow);

            //Hack to get window to stay up
            SDL_Event e; bool quit = false; while (quit == false) { while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) quit = true; } }
        }
    }
}

void QTDoughApplication::InitVulkan()
{
	CreateInstance();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateWindowSurface();
}

void QTDoughApplication::CreateInstance()
{
    unsigned int extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    VkApplicationInfo appInfo{};
    VkInstanceCreateInfo createInfo{};
    
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, NULL); //Get the count.
    const char** extensions = (const char**)malloc(sizeof(char*) * extensionCount); //Well justified ;p
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, extensions); //Get the extensions.

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "QTDough";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "QTDoughEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledLayerCount = 0;

    auto sdlExtensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(sdlExtensions.size());
    createInfo.ppEnabledExtensionNames = sdlExtensions.data();

    if (enableValidationLayers && !CheckValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    //Finally create the instance.
    VkResult result = vkCreateInstance(&createInfo, nullptr, &_vkInstance);
    if (vkCreateInstance(&createInfo, nullptr, &_vkInstance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

}

bool QTDoughApplication::CheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

void QTDoughApplication::PickPhysicalDevice()
{
    
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(_vkInstance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(_vkInstance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (IsDeviceSuitable(device)) {
            _physicalDevice = device;
            break;
        }
    }

    if (_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU! Ensure you have GPU that supports Ray Tracing Acceleration");
    }
}

bool QTDoughApplication::IsDeviceSuitable(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {};
    rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &rayTracingFeatures;

    vkGetPhysicalDeviceFeatures2(device, &deviceFeatures2);
    QueueFamilyIndices indices = FindQueueFamilies(device);

    bool extensionsSupported = CheckDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;

    //return true;
    /*
    return rayTracingFeatures.rayTracingPipeline &&
           indices.graphicsFamily.has_value();
           */
}

std::vector<const char*> QTDoughApplication::GetRequiredExtensions() {
    uint32_t extensionCount = 0;

    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, NULL);
    const char** sdlExtensions = (const char**)malloc(sizeof(char*) * extensionCount);
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, sdlExtensions);

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}


QueueFamilyIndices QTDoughApplication::FindQueueFamilies(VkPhysicalDevice device) {

    std::optional<uint32_t> graphicsFamily;
    graphicsFamily = 0;

    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    VkBool32 presentSupport = false;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;

            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _vkSurface, &presentSupport);
        }

        if (presentSupport) {
            indices.presentFamily = i;
        }

        i++;
    }
    
    return indices;
}

void QTDoughApplication::CreateLogicalDevice()
{
    QueueFamilyIndices indices = FindQueueFamilies(_physicalDevice);

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    _createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(_physicalDevice, &deviceFeatures);

    _createInfo.pQueueCreateInfos = &queueCreateInfo;
    _createInfo.queueCreateInfoCount = 1;

    _createInfo.pEnabledFeatures = &deviceFeatures;

    _createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    _createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        _createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        _createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        _createInfo.enabledLayerCount = 0;
    }

    //Finally instantiate this device.
    if (vkCreateDevice(_physicalDevice, &_createInfo, nullptr, &_logicalDevice) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(_logicalDevice, indices.graphicsFamily.value(), 0, &_vkGraphicsQueue);
}

void QTDoughApplication::CreateWindowSurface()
{

    if (SDL_Vulkan_CreateSurface(QTSDLWindow, _vkInstance, &_vkSurface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }

    QueueFamilyIndices indices = FindQueueFamilies(_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    _createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    _createInfo.pQueueCreateInfos = queueCreateInfos.data();

    vkGetDeviceQueue(_logicalDevice, indices.presentFamily.value(), 0, &_presentQueue);
}


bool QTDoughApplication::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SwapChainSupportDetails QTDoughApplication::QuerySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    uint32_t formatCount;
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _vkSurface, &details.capabilities);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, _vkSurface, &formatCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, _vkSurface, &presentModeCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, _vkSurface, &formatCount, details.formats.data());
    }

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, _vkSurface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void QTDoughApplication::Cleanup()
{
    vkDestroyInstance(_vkInstance, nullptr);
    vkDestroyDevice(_logicalDevice, nullptr);
    vkDestroySurfaceKHR(_vkInstance, _vkSurface, nullptr);
    SDL_DestroyWindow(QTSDLWindow);
    SDL_Quit();
}