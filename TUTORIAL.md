# FOnline Engine Tutorial

> Document under development.  
> Estimated finishing date is middle of 2021.

## Table of Content

...generate content...

## Introduction
## Recommended development pipeline

There are four levels for control build process:  
Visual Studio extension -> Build scripts -> CMake build tool -> Native build tool

### Visual Studio extension

In editor go to the Extensions tab and then find and install 'FOnline' extension.  
Extension activates automatically when editor finds any file that contains `fonline` in name of any file at workspace root.  

Lets see on image what this extension included:  
![VSCode Example](https://github.com/cvet/fonline/blob/master/BuildTools/vscode-example.png?raw=true "Visual Studio Code FOnline Extension Example")
* Green rectangle - button to show up Actions panel (in red rectangle)
* Red rectangle - actions panel where you control whole process, from preparing workspace to make final packages
* Blue rectangle - fast shortcut for some useful for now operation (currently binded to compilation but futher will be customizable)
* Yellow rectangle - feedback from executed actions (terminal or output panels)

All other features come with stocked Visual Studio Code editor.  
You are free to install and tune your editor instance as you wish.

### Build scripts
### CMake build tool
### Native build tool

Commonly is Visual Studio under Windows platform and Makefiles under Linux.

## Build automation
## Scripting
### How to choose language for scripting

...languages pros and cons...

### Native C++
### AngelScript
### Mono C#
## Video tutorials
