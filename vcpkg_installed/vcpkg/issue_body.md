Package: glslang:x64-windows@16.3.0

**Host Environment**

- Host: x64-windows
- Compiler: MSVC 19.50.35728.0
- CMake Version: 4.3.2
-    vcpkg-tool version: 2026-04-08-e0612b42ce44e55a0e630f2ee9d3c533a63d8bc1
    vcpkg-scripts version: 8defa4b8 2026-05-13 (2 days ago)

**To Reproduce**

`vcpkg install `

**Failure logs**

```
-- Note: glslang only supports static library linkage. Building static library.
Downloading https://github.com/KhronosGroup/glslang/archive/16.3.0.tar.gz -> KhronosGroup-glslang-16.3.0.tar.gz
error: curl operation failed with error code 7 (Could not connect to server).
error: Not a transient network error, won't retry download from https://github.com/KhronosGroup/glslang/archive/16.3.0.tar.gz
note: If you are using a proxy, please ensure your proxy settings are correct.
Possible causes are:
1. You are actually using an HTTP proxy, but setting HTTPS_PROXY variable to `https://address:port`.
This is not correct, because `https://` prefix claims the proxy is an HTTPS proxy, while your proxy (v2ray, shadowsocksr, etc...) is an HTTP proxy.
Try setting `http://address:port` to both HTTP_PROXY and HTTPS_PROXY instead.
2. If you are using Windows, vcpkg will automatically use your Windows IE Proxy Settings set by your proxy software. See: https://github.com/microsoft/vcpkg-tool/pull/77
The value set by your proxy might be wrong, or have same `https://` prefix issue.
3. Your proxy's remote server is out of service.
If you believe this is not a temporary download server failure and vcpkg needs to be changed to download this file from a different location, please submit an issue to https://github.com/Microsoft/vcpkg/issues
CMake Error at scripts/cmake/vcpkg_download_distfile.cmake:136 (message):
  Download failed, halting portfile.
Call Stack (most recent call first):
  scripts/cmake/vcpkg_from_github.cmake:120 (vcpkg_download_distfile)
  ports/glslang/portfile.cmake:3 (vcpkg_from_github)
  scripts/ports.cmake:206 (include)



```

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "name": "dataleakguard",
  "version-string": "1.0.0",
  "dependencies": [
    "sdl2",
    "nlohmann-json",
    "vulkan-headers",
    "vulkan-loader",
    "glslang"
  ]
}

```
</details>
