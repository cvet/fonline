---
title: Стадии и hooks проекта CMake
document_id: generated-cmake-stages
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-cmake-stages","locale":"ru","source_path":"Docs/en/reference/cmake/stages.md","source_sha256":"8a3e4f5553d052c65ff5636d5fe51b0da4f352e782b46b32114d50376b14c011"} -->

# Стадии и hooks проекта CMake

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/cmake/ProjectInterface.json`, затем выполните `python BuildTools/docs_cmake.py --write`.

[Индекс справочника](index.md) | [Каноническая JSON-модель](../../../generated/cmake.json) | [Контракт генерации](../metadata/)

Подключающие проекты должны вызвать каждую точку входа ровно один раз в указанном ниже порядке. Повторный вызов стадии, нарушение порядка или пропуск предшественника прерывают настройку CMake.

Регистрируйте hooks через `AddStageHook(<stage> <Pre|Post> <macro-name>)` до выполнения целевой стадии. Hooks выполняются в порядке регистрации.

| Порядок | Стабильный ID | Стадия | Точка входа | Hooks | Источник | Ответственность |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | <a id="entry-cmake-stage-init-b661766d23"></a><code>cmake.stage.Init</code> | <code>Init</code> | <code>StartProjectGeneration</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/Init.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Init.cmake) | Объявляет параметры, определяет платформу и toolchain и создаёт общее состояние сборки. |
| 2 | <a id="entry-cmake-stage-projectoptions-ad152d8067"></a><code>cmake.stage.ProjectOptions</code> | <code>ProjectOptions</code> | <code>RegisterProjectOptions</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/ProjectOptions.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/ProjectOptions.cmake) | Нормализует комбинации параметров проекта и отклоняет несовместимые режимы сборки. |
| 3 | <a id="entry-cmake-stage-thirdparty-00fb30a4b1"></a><code>cmake.stage.ThirdParty</code> | <code>ThirdParty</code> | <code>AddThirdPartyLibraries</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/ThirdParty.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/ThirdParty.cmake) | Регистрирует встроенные сторонние зависимости и изоляцию поиска пакетов. |
| 4 | <a id="entry-cmake-stage-enginesources-7f3dd59abc"></a><code>cmake.stage.EngineSources</code> | <code>EngineSources</code> | <code>RegisterEngineSources</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/EngineSources.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/EngineSources.cmake) | Собирает списки исходников движка и подключающего проекта, а также входы сгенерированных ресурсов. |
| 5 | <a id="entry-cmake-stage-codegen-2e6e8aa291"></a><code>cmake.stage.Codegen</code> | <code>Codegen</code> | <code>SetupCodeGeneration</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/Codegen.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Codegen.cmake) | Определяет выходы сгенерированных метаданных и исходников и цели генерации кода. |
| 6 | <a id="entry-cmake-stage-corelibs-a2a0674d4e"></a><code>cmake.stage.CoreLibs</code> | <code>CoreLibs</code> | <code>BuildCoreLibraries</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/CoreLibs.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/CoreLibs.cmake) | Создаёт переиспользуемые библиотеки движка из зарегистрированных групп исходников. |
| 7 | <a id="entry-cmake-stage-applications-64ab5135be"></a><code>cmake.stage.Applications</code> | <code>Applications</code> | <code>BuildApplications</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/Applications.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Applications.cmake) | Создаёт включённые приложения клиента, сервера, Mapper, компилятора, baker, viewer и тестов. |
| 8 | <a id="entry-cmake-stage-scriptsandbaking-09008fbe74"></a><code>cmake.stage.ScriptsAndBaking</code> | <code>ScriptsAndBaking</code> | <code>SetupScriptsAndBaking</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/ScriptsAndBaking.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/ScriptsAndBaking.cmake) | Создаёт цели компиляции скриптов и запекания ресурсов. |
| 9 | <a id="entry-cmake-stage-packages-1f46cc2b55"></a><code>cmake.stage.Packages</code> | <code>Packages</code> | <code>BuildPackages</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/Packages.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Packages.cmake) | Создаёт цели пакетов из объявлений пакетов подключающего проекта. |
| 10 | <a id="entry-cmake-stage-finalize-5fc677c347"></a><code>cmake.stage.Finalize</code> | <code>Finalize</code> | <code>FinalizeProjectGeneration</code> | <code>Pre</code>, <code>Post</code> | [BuildTools/cmake/stages/Finalize.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Finalize.cmake) | Завершает организацию целей и проверяет, что каждая стадия выполнена ровно один раз. |
