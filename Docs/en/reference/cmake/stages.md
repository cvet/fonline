---
title: CMake Project Stages and Hooks
document_id: generated-cmake-stages
locale: en
generated: true
---

# CMake Project Stages and Hooks

> Generated reference. Do not edit this page directly. Update `BuildTools/cmake/ProjectInterface.json`, then run `python BuildTools/docs_cmake.py --write`.

[Reference index](index.md) | [Canonical JSON model](../../../generated/cmake.json) | [Generation contract](../metadata/)

Embedding projects must call every entrypoint exactly once in the order below. Calling a stage twice, calling it out of order, or skipping a predecessor aborts CMake configure.

Register hooks with `AddStageHook(<stage> <Pre|Post> <macro-name>)` before the target stage executes. Hooks run in registration order.

| Order | Stable ID | Stage | Entrypoint | Hooks | Source | Responsibility |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | <a id="entry-cmake-stage-init-b661766d23"></a><code>cmake.stage.Init</code> | <code>Init</code> | <code>StartProjectGeneration</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/Init.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Init.cmake) | Declare options, detect the platform/toolchain, and establish common build state. |
| 2 | <a id="entry-cmake-stage-projectoptions-ad152d8067"></a><code>cmake.stage.ProjectOptions</code> | <code>ProjectOptions</code> | <code>RegisterProjectOptions</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/ProjectOptions.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/ProjectOptions.cmake) | Normalize project option combinations and reject incompatible build modes. |
| 3 | <a id="entry-cmake-stage-thirdparty-00fb30a4b1"></a><code>cmake.stage.ThirdParty</code> | <code>ThirdParty</code> | <code>AddThirdPartyLibraries</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/ThirdParty.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/ThirdParty.cmake) | Register bundled third-party dependencies and package lookup isolation. |
| 4 | <a id="entry-cmake-stage-enginesources-7f3dd59abc"></a><code>cmake.stage.EngineSources</code> | <code>EngineSources</code> | <code>RegisterEngineSources</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/EngineSources.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/EngineSources.cmake) | Aggregate engine and embedding-project source lists and generated resource inputs. |
| 5 | <a id="entry-cmake-stage-codegen-2e6e8aa291"></a><code>cmake.stage.Codegen</code> | <code>Codegen</code> | <code>SetupCodeGeneration</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/Codegen.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Codegen.cmake) | Define generated metadata/source outputs and code-generation targets. |
| 6 | <a id="entry-cmake-stage-corelibs-a2a0674d4e"></a><code>cmake.stage.CoreLibs</code> | <code>CoreLibs</code> | <code>BuildCoreLibraries</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/CoreLibs.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/CoreLibs.cmake) | Create reusable engine libraries from the registered source groups. |
| 7 | <a id="entry-cmake-stage-applications-64ab5135be"></a><code>cmake.stage.Applications</code> | <code>Applications</code> | <code>BuildApplications</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/Applications.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Applications.cmake) | Create enabled client, server, mapper, compiler, baker, viewer, and test applications. |
| 8 | <a id="entry-cmake-stage-scriptsandbaking-09008fbe74"></a><code>cmake.stage.ScriptsAndBaking</code> | <code>ScriptsAndBaking</code> | <code>SetupScriptsAndBaking</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/ScriptsAndBaking.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/ScriptsAndBaking.cmake) | Create script compilation and resource baking targets. |
| 9 | <a id="entry-cmake-stage-packages-1f46cc2b55"></a><code>cmake.stage.Packages</code> | <code>Packages</code> | <code>BuildPackages</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/Packages.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Packages.cmake) | Create package targets from embedding-project package declarations. |
| 10 | <a id="entry-cmake-stage-finalize-5fc677c347"></a><code>cmake.stage.Finalize</code> | <code>Finalize</code> | <code>FinalizeProjectGeneration</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/Finalize.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Finalize.cmake) | Finalize target organization and verify that every stage ran exactly once. |
