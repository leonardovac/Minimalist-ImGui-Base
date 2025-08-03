<div align="center">
<h1> Minimalistic ImGui Base </h1>
<div>
<a href="https://en.wikipedia.org/wiki/C++23"><img src="https://img.shields.io/badge/Language-C%2B%2B23-f44c7c?style=flat-square" alt="Static Badge"></a>
<a href="https://github.com/leonardovac/Minimalistic-ImGui-Base/blob/main/LICENSE.txt"><img src="https://img.shields.io/badge/License-MIT-blue?style=flat-square" alt="Static Badge"></a>
<img alt="GitHub Repo stars" src="https://img.shields.io/github/stars/leonardovac/Minimalistic-ImGui-Base">
</div>
<div>
<a href="https://visitorbadge.io/status?path=https%3A%2F%2Fgithub.com%2Fleonardovac%2FMinimalistic-ImGui-Base">
<img src="https://api.visitorbadge.io/api/visitors?path=https%3A%2F%2Fgithub.com%2Fleonardovac%2FMinimalistic-ImGui-Base&label=Repo.%20Visits&countColor=%23ba68c8&style=flat-square" alt="Visitors"></a>
<a href="https://app.deepsource.com/gh/leonardovac/Minimalistic-ImGui-Base/"><img alt="DeepSource" title="DeepSource" src="https://app.deepsource.com/gh/leonardovac/Minimalistic-ImGui-Base.svg/?label=active+issues&show_trend=false&token=VEY-dCFd7Zvez753JFNRKMHy"/></a>
<a href="https://app.codacy.com/gh/leonardovac/Minimalistic-ImGui-Base/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade"><img alt="Codacy" title="Codacy" src="https://img.shields.io/codacy/grade/60d23119442344d7913494bbfbdc31f7?logo=codacy&style=flat-square"/></a>
<a href="https://www.codefactor.io/repository/github/leonardovac/Minimalistic-imgui-base"><img alt="CodeFactor" title="CodeFactor" src="https://img.shields.io/codefactor/grade/github/leonardovac/Minimalistic-imgui-base?logo=codefactor&style=flat-square"/></a>
</div>

| [Introduction](#-introduction) - [Requirements](#-requirements) - [Quick Start](#-quick-start) - [Features](#-features) |
| :----------------------------------------------------------: |
| [Examples](#-examples) - [Acknowledgements](#-acknowledgements) - [License](#-license) |
</div>
<br>

## 🌱 Introduction

Welcome to a *(definitely not)* minimalistic base/framework for your *debug tools* — inspired by the well-known [UniversalHookX](https://github.com/bruhmoment21/UniversalHookX), but even simpler and more user-friendly.

This is a simple project that I visit once in a while during my free time. Contributions are welcome!

## ⚙️ Requirements

**Hard Requirements:** 

- [Dear ImGui](https://github.com/ocornut/imgui)  
- [SafetyHook](https://github.com/cursey/safetyhook)

**Soft Requirements:**

- [Quill](https://github.com/odygrd/quill) (for logging)

**Optional:**  

- [Volk](https://github.com/zeux/volk) (only for Vulkan)

**Recommended:**  

- [vcpkg](https://vcpkg.io/en/) (or any C++ package manager)

## 🚀 Quick Start

1. **Clone the repository:**
    ```bash
    git clone https://github.com/leonardovac/Minimalistic-ImGui-Base.git
    ```

2. **Navigate to the project folder and run the setup script:**

>[!NOTE]   
>This script uses vcpkg to install dependencies (`x86`/`x64-windows-static`) and runs [git sparse-checkout](https://git-scm.com/docs/git-sparse-checkout) to reduce submodule size.

    ```bash 
    cd Minimalistic-ImGui-Base/scripts
    .\quick_setup.bat
    ```

3. **Build the project and use it!**

### ➕ Usage

#### Side-loading / Proxy

- Rename the resulting DLL to `version.dll`
- Place it in the target application's directory

#### Manual Injection

- Use your preferred DLL injector to inject the DLL into the target process

## 🎯 Features

- Simple and easy to use
- Automatic graphics API detection
- Logging system based on Quill
- Multiple hooking methods available out of the box:
  - *Inline*, *Mid-function*, *EAT*, *IAT*, *VMT*
- Memory helper functions for pattern scanning and manipulation
- Supports Discord and Steam overlays

**Supported backends:**

- [X] Direct3D 9 / 9EX
- [ ] Direct3D 10
- [X] Direct3D 11
- [X] Direct3D 12
- [X] OpenGL
- [X] Vulkan

## 📂 Examples

- [DOS2 Minimalist Menu](https://github.com/leonardovac/DOS2-Minimalist-Menu)

## 🐐 Acknowledgements

Special thanks to:

- [@ocornut](https://github.com/ocornut) for [Dear ImGui](https://github.com/ocornut/imgui) — an excellent immediate mode GUI library.  
- [@odygrd](https://github.com/odygrd) for [Quill](https://github.com/odygrd/quill) — a high-performance logging library.  
- [@cursey](https://github.com/cursey) for [SafetyHook](https://github.com/cursey/safetyhook) — a modern and safe hooking library for Windows.  
- [@bruhmoment21](https://github.com/bruhmoment21) for [UniversalHookX](https://github.com/bruhmoment21/UniversalHookX) — hooking framework.  
    
## ⚖ License

Minimalistic ImGui Base is licensed under the MIT License, see [LICENSE.txt](https://github.com/leonardovac/Minimalistic-ImGui-Base/blob/main/LICENSE.txt) for more information.