---
title: CMake Project Helpers
document_id: generated-cmake-helpers
locale: en
generated: true
---

# CMake Project Helpers

> Generated reference. Do not edit this page directly. Update `BuildTools/cmake/ProjectInterface.json`, then run `python BuildTools/docs_cmake.py --write`.

[Reference index](index.md) | [Canonical JSON model](../../../generated/cmake.json) | [Generation contract](../metadata/)

These commands are the selected project-facing helper surface. Other commands under `BuildTools/cmake` remain internal unless they are added to the interface manifest.

| Stable ID | Signature | Kind | Allowed roles | Source | Description |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-cmake-helper-setoption-e646d30cc4"></a><code>cmake.helper.SetOption</code> | <code>SetOption(&lt;name&gt; &lt;value&gt;)</code> | <code>function</code> | - | [BuildTools/Init.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/Init.cmake) | Set an embedding-project default only when the variable is not already defined. |
| <a id="entry-cmake-helper-addstagehook-a926dde5d7"></a><code>cmake.helper.AddStageHook</code> | <code>AddStageHook(&lt;stage&gt; &lt;Pre&#124;Post&gt; &lt;macro-name&gt;)</code> | <code>macro</code> | - | [BuildTools/Init.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/Init.cmake) | Register a project macro at a stage boundary before that stage executes. |
| <a id="entry-cmake-helper-registerfindpackagehandler-668bfe3e6f"></a><code>cmake.helper.RegisterFindPackageHandler</code> | <code>RegisterFindPackageHandler(&lt;package&gt; &lt;handler-macro&gt;)</code> | <code>macro</code> | - | [BuildTools/Init.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/Init.cmake) | Register the explicit package lookup handler used by the ThirdParty-stage find_package interceptor. |
| <a id="entry-cmake-helper-addenginesources-8a1a5c856f"></a><code>cmake.helper.AddEngineSources</code> | <code>AddEngineSources(&lt;role&gt; &lt;path-or-glob&gt; [&lt;role&gt; &lt;path-or-glob&gt; ...])</code> | <code>macro</code> | <code>COMMON</code>, <code>SERVER</code>, <code>CLIENT</code>, <code>MAPPER</code>, <code>BAKER</code> | [BuildTools/cmake/helpers/Build.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/helpers/Build.cmake) | Add role-scoped embedding-project native sources before RegisterEngineSources. |
| <a id="entry-cmake-helper-addprojectlibraries-917d80c201"></a><code>cmake.helper.AddProjectLibraries</code> | <code>AddProjectLibraries(ROLES &lt;role&gt; [...] LIBRARIES &lt;target-or-library&gt; [...])</code> | <code>macro</code> | <code>COMMON</code>, <code>SERVER</code>, <code>CLIENT</code>, <code>MAPPER</code>, <code>BAKER</code> | [BuildTools/cmake/helpers/Build.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/helpers/Build.cmake) | Link project-owned targets or explicit platform libraries only to their consuming engine roles before BuildCoreLibraries. |
| <a id="entry-cmake-helper-addnativeincludedir-60a1b8f1db"></a><code>cmake.helper.AddNativeIncludeDir</code> | <code>AddNativeIncludeDir(&lt;directory&gt; [...])</code> | <code>macro</code> | - | [BuildTools/cmake/helpers/Build.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/helpers/Build.cmake) | Add project-relative include directories when Native scripting is enabled. |
| <a id="entry-cmake-helper-addbakingtarget-5d644391c3"></a><code>cmake.helper.AddBakingTarget</code> | <code>AddBakingTarget(&lt;target&gt; [SUB_CONFIG &lt;name&gt;] [FORCE] [COMMENT &lt;text&gt;])</code> | <code>function</code> | - | [BuildTools/cmake/helpers/Build.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/helpers/Build.cmake) | Create a project baking target with an optional subconfig, force mode, and build-hash update after SetupScriptsAndBaking. |
