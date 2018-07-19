# Jinja2Cpp

[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![Standard](https://img.shields.io/badge/c%2B%2B-14-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![Build Status](https://travis-ci.org/flexferrum/Jinja2Cpp.svg?branch=master)](https://travis-ci.org/flexferrum/Jinja2Cpp)
[![Build status](https://ci.appveyor.com/api/projects/status/19v2k3bl63jxl42f/branch/master?svg=true)](https://ci.appveyor.com/project/flexferrum/Jinja2Cpp)
[![Github Releases](https://img.shields.io/github/release/flexferrum/Jinja2Cpp.svg)](https://github.com/flexferrum/Jinja2Cpp/releases)
[![Github Issues](https://img.shields.io/github/issues/flexferrum/Jinja2Cpp.svg)](http://github.com/flexferrum/Jinja2Cpp/issues)
[![GitHub License](https://img.shields.io/badge/license-Mozilla-blue.svg)](https://raw.githubusercontent.com/flexferrum/Jinja2Cpp/master/LICENSE)

C++ implementation of big subset of Jinja2 template engine features. This library was inspired by [Jinja2CppLight](https://github.com/hughperkins/Jinja2CppLight) project and brings support of mostly all Jinja2 templates features into C++ world. Unlike [inja](https://github.com/pantor/inja) lib, you have to build Jinja2Cpp, but it has only one dependence: boost.

# Table of contents

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->


- [Introduction](#introduction)
- [Getting started](#getting-started)
  - [More complex example](#more-complex-example)
    - [The simplest case](#the-simplest-case)
    - [Reflection](#reflection)
    - ['set' statement](#set-statement)
  - [Other features](#other-features)
- [Current Jinja2 support](#current-jinja2-support)
- [Supported compilers](#supported-compilers)
- [Build and install](#build-and-install)
  - [Additional CMake build flags](#additional-cmake-build-flags)
- [Link with you projects](#link-with-you-projects)
- [Changelog](#changelog)
  - [Version 0.6](#version-06)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Introduction

Main features of Jinja2Cpp:
- Easy-to-use public interface. Just load templates and render them.
- Conformance to [Jinja2 specification](http://jinja.pocoo.org/docs/2.10/)
- Partial support for both narrow- and wide-character strings both for templates and parameters.
- Built-in reflection for C++ types.
- Powerful full-featured Jinja2 expressions with filtering (via '|' operator) and 'if'-expressions.
- Basic control statements (set, for, if).
- Templates extention.

For instance, this simple code:

```c++
std::string source = R"(
{{ ("Hello", 'world') | join }}!!!
{{ ("Hello", 'world') | join(', ') }}!!!
{{ ("Hello", 'world') | join(d = '; ') }}!!!
{{ ("Hello", 'world') | join(d = '; ') | lower }}!!!
)";

Template tpl;

std::string result = tpl.RenderAsString(ValuesMap());
```

produces the result string:

```
Helloworld!!!
Hello, world!!!
Hello; world!!!
hello; world!!!
```

# Getting started

In order to use Jinja2Cpp in your project you have to:
* Clone the Jinja2Cpp repository
* Build it according with the instructions
* Link to your project.

Usage of Jinja2Cpp in the code is pretty simple:
1. Declare the jinja2::Template object:

```c++
jinja2::Template tpl;
```

2. Populate it with template:

```c++
tpl.Load("{{'Hello World' }}!!!");
```

3. Render the template:

```c++
std::cout << tpl.RenderAsString(jinja2::ValuesMap{}) << std::endl;
```

and get:

`
Hello World!!!
`

That's all!

## More complex example
Let's say you have the following enum:

```c++
enum Animals
{
    Dog,
    Cat,
    Monkey,
    Elephant
};
```

And you want to automatically produce string-to-enum and enum-to-string convertor. Like this:

```c++
inline const char* AnimalsToString(Animals e)
{
    switch (e)
    {
    case Dog:
        return "Dog";
    case Cat:
        return "Cat";
    case Monkey:
        return "Monkey";
    case Elephant:
        return "Elephant";
    }
    return "Unknown Item";
}
```

Of course, you can write this producer in the way like [this](https://github.com/flexferrum/autoprogrammer/blob/87a9dc8ff61c7bdd30fede249757b71984e4b954/src/generators/enum2string_generator.cpp#L140). It's too complicated for writing 'from scratch'. Actually, there is a better and simpler way.

### The simplest case

Firstly, you should define the simple jinja2 template (in the C++ manner):
```c++
std::string enum2StringConvertor = R"(
inline const char* {{enumName}}ToString({{enumName}} e)
{
    switch (e)
    {
{% for item in items %}
    case {{item}}:
        return "{{item}}";
{% endfor %}
    }
    return "Unknown Item";
})";
```
As you can see, this template is similar to the C++ sample code above, but some parts replaced by placeholders ("parameters"). These placeholders will be replaced with the actual text during template rendering process. In order to this happen, you should fill up the rendering parameters. This is a simple dictionary which maps the parameter name to the corresponding value:

```c++
jinja2::ValuesMap params {
    {"enumName", "Animals"},
    {"items", {"Dog", "Cat", "Monkey", "Elephant"}},
};
```
An finally, you can render this template with Jinja2Cpp library:

```c++
jinja2::Template tpl;
tpl.Load(enum2StringConvertor);
std::cout << tpl.RenderAsString(params);
```
And you will get on the console the conversion function mentioned above!

You can call 'Render' method many times, with different parameters set, from several threads. Everything will be fine: every time you call 'Render' you will get the result depended only on provided params. Also you can render some part of template many times (for different parameters) with help of 'for' loop and 'extend' statement (described below). It allows you to iterate through the list of items from the first to the last one and render the loop body for each item respectively. In this particular sample it allows to put as many 'case' blocks in conversion function as many items in the 'reflected' enum.

### Reflection
Let's imagine you don't want to fill the enum descriptor by hand, but want to fill it with help of some code parsing tool ([autoprogrammer](https://github.com/flexferrum/autoprogrammer) or [cppast](https://github.com/foonathan/cppast)). In this case you can define structure like this:

```c++
// Enum declaration description
struct EnumDescriptor
{
// Enumeration name
std::string enumName;
// Namespace scope prefix
std::string nsScope;
// Collection of enum items
std::vector<std::string> enumItems;
};
```
This structure holds the enum name, enum namespace scope prefix, and list of enum items (we need just names). Then, you can populate instances of this descriptor automatically using chosen tool (ex. here: [clang-based enum2string converter generator](https://github.com/flexferrum/flex_lib/blob/accu2017/tools/codegen/src/main.cpp) ). For our sample we can create the instance manually:
```c++
EnumDescriptor descr;
descr.enumName = "Animals";
descr.nsScope = "";
descr.enumItems = {"Dog", "Cat", "Monkey", "Elephant"};
```

And now you need to transfer data from this internal enum descriptor to Jinja2 value params map. Of course it's possible to do it by hands:
```c++
jinja2::ValuesMap params {
    {"enumName", descr.enumName},
    {"nsScope", descr.nsScope},
    {"items", {descr.enumItems[0], descr.enumItems[1], descr.enumItems[2], descr.enumItems[3]}},
};
```

But actually, with Jinja2Cpp you don't have to do it manually. Library can do it for you. You just need to define reflection rules. Something like this:

```c++
namespace jinja2
{
template<>
struct TypeReflection<EnumDescriptor> : TypeReflected<EnumDescriptor>
{
    static auto& GetAccessors()
    {
        static std::unordered_map<std::string, FieldAccessor> accessors = {
            {"name", [](const EnumDescriptor& obj) {return obj.name;}},
            {"nsScope", [](const EnumDescriptor& obj) { return obj.nsScope;}},
            {"items", [](const EnumDescriptor& obj) {return Reflect(obj.items);}},
        };

        return accessors;
    }
};
```
And in this case you need to correspondingly change the template itself and it's invocation:
```c++
std::string enum2StringConvertor = R"(
inline const char* {{enum.enumName}}ToString({{enum.enumName}} e)
{
    switch (e)
    {
{% for item in enum.items %}
    case {{item}}:
        return "{{item}}";
{% endfor %}
    }
    return "Unknown Item";
})";

// ...
    jinja2::ValuesMap params = {
        {"enum", jinja2::Reflect(descr)},
    };
// ...
```
Every specified field will be reflected into Jinja2Cpp internal data structures and can be accessed from the template without additional efforts. Quite simple! As you can see, you can use 'dot' notation to access named members of some parameter as well, as index notation like this: `enum['enumName']`. With index notation you can access to the particular item of a list: `enum.items[3]` or `enum.items[itemIndex]` or `enum['items'][itemIndex]`.

### 'set' statement
But what if enum `Animals` will be in the namespace?

```c++
namespace world
{
enum Animals
{
    Dog,
    Cat,
    Monkey,
    Elephant
};
}
```
In this case you need to prefix both enum name and it's items with namespace prefix in the generated code. Like this:
```c++
std::string enum2StringConvertor = R"(
inline const char* {{enum.enumName}}ToString({{enum.nsScope}}::{{enum.enumName}} e)
{
    switch (e)
    {
{% for item in enum.items %}
    case {{enum.nsScope}}::{{item}}:
        return "{{item}}";
{% endfor %}
    }
    return "Unknown Item";
})";
```
This template will produce 'world::' prefix for our new scoped enum (and enum itmes). And '::' for the ones in global scope. But you may want to eliminate the unnecessary global scope prefix. And you can do it this way:
```c++
{% set prefix = enum.nsScope + '::' if enum.nsScope else '' %}
std::string enum2StringConvertor = R"(inline const char* {{enum.enumName}}ToString({{prefix}}::{{enum.enumName}} e)
{
    switch (e)
    {
{% for item in enum.items %}
    case {{prefix}}::{{item}}:
        return "{{item}}";
{% endfor %}
    }
    return "Unknown Item";
})";
```
This template uses two significant jinja2 template features:
1. The 'set' statement. You can declare new variables in your template. And you can access them by the name.
2. if-expression. It works like a ternary '?:' operator in C/C++. In C++ the code from the sample could be written in this way:
```c++
std::string prefix = !descr.nsScope.empty() ? descr.nsScope + "::" : "";
```
I.e. left part of this expression (before 'if') is a true-branch of the statement. Right part (after 'else') - false-branch, which can be omitted. As a condition you can use any expression convertible to bool.

## Other features
The render procedure is stateless, so you can perform several renderings simultaneously in different threads. Even if you pass parameters:

```c++
    ValuesMap params = {
        {"intValue", 3},
        {"doubleValue", 12.123f},
        {"stringValue", "rain"},
        {"boolFalseValue", false},
        {"boolTrueValue", true},
    };

    std::string result = tpl.RenderAsString(params);
    std::cout << result << std::endl;
```

Parameters could have the following types:
- std::string/std::wstring
- integer (int64_t)
- double
- boolean (bool)
- Tuples (also known as arrays)
- Dictionaries (also known as maps)

Tuples and dictionaries can be mapped to the C++ types. So you can smoothly reflect your structures and collections into the template engine:

```c++
namespace jinja2
{
template<>
struct TypeReflection<reflection::EnumInfo> : TypeReflected<reflection::EnumInfo>
{
    static auto& GetAccessors()
    {
        static std::unordered_map<std::string, FieldAccessor> accessors = {
            {"name", [](const reflection::EnumInfo& obj) {return Reflect(obj.name);}},
            {"scopeSpecifier", [](const reflection::EnumInfo& obj) {return Reflect(obj.scopeSpecifier);}},
            {"namespaceQualifier", [](const reflection::EnumInfo& obj) { return obj.namespaceQualifier;}},
            {"isScoped", [](const reflection::EnumInfo& obj) {return obj.isScoped;}},
            {"items", [](const reflection::EnumInfo& obj) {return Reflect(obj.items);}},
        };

        return accessors;
    }
};

// ...
    jinja2::ValuesMap params = {
        {"enum", jinja2::Reflect(enumInfo)},
    };
```

In this cases method 'jinja2::reflect' reflects regular C++ type into jinja2 template param. If type is a user-defined class or structure then handwritten mapper 'TypeReflection<>' should be provided.

# Current Jinja2 support
Currently, Jinja2Cpp supports the limited number of Jinja2 features. By the way, Jinja2Cpp is planned to be full [jinja2 specification](http://jinja.pocoo.org/docs/2.10/templates/)-conformant. The current support is limited to:
- expressions. You can use almost every style of expressions: simple, filtered, conditional, and so on.
- big number of filters (**sort, default, first, last, length, max, min, reverse, unique, sum, attr, map, reject, rejectattr, select, selectattr, pprint, dictsort, abs, float, int, list, round, random, trim, title, upper, wordcount, replace, truncate, groupby, urlencode**)
- big number of testers (**eq, defined, ge, gt, iterable, le, lt, mapping, ne, number, sequence, string, undefined, in, even, odd, lower, upper**)
- limited number of functions (**range**, **loop.cycle**)
- 'if' statement (with 'elif' and 'else' branches)
- 'for' statement (with 'else' branch and 'if' part support)
- 'extends' statement
- 'set' statement
- recursive loops

# Supported compilers
Compilation of Jinja2Cpp tested on the following compilers (with C++14 enabled feature):
- Linux gcc 5.0
- Linux gcc 6.0
- Linux gcc 7.0
- Linux clang 5.0
- Microsoft Visual Studio 2015 x86
- Microsoft Visual Studio 2017 x86

# Build and install
Jinja2Cpp has got only one external dependency: boost library (at least version 1.55). Because of types from boost are used inside library, you should compile both your projects and Jinja2Cpp library with similar compiler settings. Otherwise ABI could be broken.

In order to compile Jinja2Cpp you need:

1. Install CMake build system (at least version 3.0)
2. Clone jinja2cpp repository and update submodules:

```
> git clone https://github.com/flexferrum/Jinja2Cpp.git
> git submodule -q update --init
```

3. Create build directory:

```
> cd Jinja2Cpp
> mkdir build
```

4. Run CMake and build the library:

```
> cd build
> cmake .. -DCMAKE_INSTALL_PREFIX=<path to install folder>
> cmake --build . --target all
```
"Path to install folder" here is a path to the folder where you want to install Jinja2Cpp lib.

5. Install library:

```
> cmake --build . --target install
```

6. Also you can run the tests:

```
> ctest -C Release
```

## Additional CMake build flags
You can define (via -D command line CMake option) the following build flags:

* **WITH_TESTS** (default TRUE) - build or not Jinja2Cpp tests.
* **MSVC_RUNTIME_TYPE** (default /MD) - MSVC runtime type to link with (if you use Microsoft Visual Studio compiler).
* **LIBRARY_TYPE** Could be STATIC (default for Windows platform) or SHARED (default for Linux). Specify the type of Jinja2Cpp library to build.

# Link with you projects
Jinja2Cpp is shipped with cmake finder script. So you can:

1. Include Jinja2Cpp cmake scripts to the project:
```cmake
list (APPEND CMAKE_MODULE_PATH ${JINJA2CPP_INSTALL_DIR}/cmake)
```

2. Use regular 'find' script:
```cmake
find_package(Jinja2Cpp)
```

3. Add found paths to the project settings:
```cmake
#...
include_directories(
    #...
    ${JINJA2CPP_INCLUDE_DIR}
    )

target_link_libraries(YourTarget
    #...
    ${JINJA2CPP_LIBRARY}
    )
#...
```

or just link with Jinja2Cpp target:
```cmake
#...
target_link_libraries(YourTarget
    #...
    Jinja2Cpp
    )
#...
```

# Changelog
## Version 0.6
* A lot of filters has been implemented. Full set of supported filters listed here: https://github.com/flexferrum/Jinja2Cpp/issues/7
* A lot of testers has been implemented. Full set of supported testers listed here: https://github.com/flexferrum/Jinja2Cpp/issues/8
* 'Contatenate as string' operator ('~') has been implemented
* For-loop with 'if' condition has been implemented
* Fixed some bugs in parser
