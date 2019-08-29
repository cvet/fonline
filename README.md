# FOnline Engine

[![Build Status](https://ci.fonline.ru/buildStatus/icon?job=fonline/master)](https://ci.fonline.ru/blue/organizations/jenkins/fonline/activity)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/6c9c1cddf6ba4b58bfa94c729a73f315)](https://www.codacy.com/app/cvet/fonline?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=cvet/fonline&amp;utm_campaign=Badge_Grade)

## Goal

Friendly engine for fallout-like icometric games for develop/play alone or with friends

## About (not actually ready marked as *)

* C++14
* Editor and Server target platforms
  * Windows
  * Linux
  * macOS*
* Client target platforms
  * Windows
  * Linux
  * macOS
  * iOS
  * Android
  * Web
  * PS4*
* Online or singleplayer*
* Supporting of Fallout 1/2/Tactics, Arcanum, Boldur's Gate and other isometric games asset formats
* Supporting of 3d characters in modern graphic formats
* Hexagonal / Square map tiling
* ...write more

## WIP features

* Multifunctional editor
* C# as scripting language (Mono)
* Singleplayer mode
* Parallelism for server
* Documentation
* First release with fixed API and further backward comparability

## Repository structure

* *BuildScripts* - scripts for automatical build in command line or any ci/cd system
* *Other* - historical stuff, deprecated and not used
* *Resources* - resources for build applications but not related to code
* *SdkPlaceholder* - all this stuff merged with build output in resulted sdk zip
* *Source* - fonline specific code
* *SourceTools* - some tools for formatting code or count it
* *ThirdParty* - external dependecies of engine, included in repository

## Git LFS

Before clone make sure that you install Git LFS
https://git-lfs.github.com/

## Help and support

* [English-speaking community](https://fodev.net)
* [Russian-speaking community](https://fonline.ru)
