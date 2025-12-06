# ccsocket

  `ccsocket`是一个为跨平台软件准备的简易封装`socket`库, 除目标平台标准库外不依赖任何其他第三方库.

  `ccsocket`将易用性、高效列为设计目的, 简单、实用、快速上手可以显著减少开发者对平台`API`的依赖.

  `ccsocket`创建的`socket`未经过包装, 因此开发者亦可调用平台特定的原生接口使用.

## 优势与特性

  * 跨平台支持

  * 易整合/嵌入

  * 简单易上手

  * 维护成本低

## 平台/环境支持

  * Windows

  * Linux

  * MacOS

  * FreeBSD/otherBSD

  * Solaris/illumos/openindiana

  * cygwin/msys2

  * openKylin

  同时支持`32bit`/`64bit`编译生成.

## 编译器支持

  * msvc

  * gcc

  * clang

  * musl

  * mingw

  默认使用`C`编译器, 可选择强制使用`C++`编译.

## 构建方式

### 1. cmake

* 命令行构建 - 调试可用的库 (当前目录切换到`ccsocket`目录)

  `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build -build`

* 命令行构建 - 生产可用的库 (当前目录切换到`ccsocket`目录)

  `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build -build`

* `cmake-gui` 图形化构建

  `source code` 填入 `ccsocket` 的完整目录, `build the binaries` 填入 `ccsocket` 的完整目录 + `build`.
  点击下面的`Configure`, 选择你生成的项目类型(如: `Visual Studio 2022`), 下拉选择`Win32/x64`. 再点击`Finished`按钮就开始配置. 如果出错则`Delete Cache`重来.
  点击`Generate`按钮就生成工程, 再点击`Open`就可以开始构建了.

更多使用方法, 请自行参考[cmake](https://cmake.org/documentation/)

### 2. premake5

1. 安装`premake5`放置到`/usr/bin`或`/usr/local/bin`, 然后就进入到`ccsocket`目录.

2. 根据你的平台使用命令`premake5 gmake/windows/linux`等命令配置好项目.

3. 使用`make`或者`make config=Release`生成出指定配置的库.

更多使用方法, 请自行参考[premake5](https://premake.github.io/docs/)

### 3. xmake

  安装`xmake`后进入到`ccsocket`目录, 执行`xmake`或`xmake build`即可完成构建.

## 使用指南

  [ccsocket wiki](//)

## 开源许可

  **MIT LICENSE**