---
title: Проектные helper-команды CMake
document_id: generated-cmake-helpers
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-cmake-helpers","locale":"ru","source_path":"Docs/en/reference/cmake/helpers.md","source_sha256":"2edff110cddaa1b27d040c845d3ab50d9cfbe93565ab043b29ba871a27b2b275"} -->

# Проектные helper-команды CMake

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/cmake/ProjectInterface.json`, затем выполните `python BuildTools/docs_cmake.py --write`.

[Индекс справочника](index.md) | [Каноническая JSON-модель](../../../generated/cmake.json) | [Контракт генерации](../metadata/)

Эти команды образуют выбранную поверхность helper-команд для проекта. Остальные команды в `BuildTools/cmake` остаются внутренними, пока не будут добавлены в manifest интерфейса.

| Стабильный ID | Сигнатура | Вид | Допустимые роли | Источник | Описание |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-cmake-helper-setoption-e646d30cc4"></a><code>cmake.helper.SetOption</code> | <code>SetOption(&lt;name&gt; &lt;value&gt;)</code> | <code>function</code> | - | [BuildTools/Init.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/Init.cmake) | Задаёт значение подключающего проекта по умолчанию, только если переменная ещё не определена. |
| <a id="entry-cmake-helper-addstagehook-a926dde5d7"></a><code>cmake.helper.AddStageHook</code> | <code>AddStageHook(&lt;stage&gt; &lt;Pre&#124;Post&gt; &lt;macro-name&gt;)</code> | <code>macro</code> | - | [BuildTools/Init.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/Init.cmake) | Регистрирует макрос проекта на границе стадии до её выполнения. |
| <a id="entry-cmake-helper-registerfindpackagehandler-668bfe3e6f"></a><code>cmake.helper.RegisterFindPackageHandler</code> | <code>RegisterFindPackageHandler(&lt;package&gt; &lt;handler-macro&gt;)</code> | <code>macro</code> | - | [BuildTools/Init.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/Init.cmake) | Регистрирует явный обработчик поиска пакета, используемый перехватчиком find_package на стадии ThirdParty. |
| <a id="entry-cmake-helper-addenginesources-8a1a5c856f"></a><code>cmake.helper.AddEngineSources</code> | <code>AddEngineSources(&lt;role&gt; &lt;path-or-glob&gt; [&lt;role&gt; &lt;path-or-glob&gt; ...])</code> | <code>macro</code> | <code>COMMON</code>, <code>SERVER</code>, <code>CLIENT</code>, <code>MAPPER</code>, <code>BAKER</code>, <code>TESTS</code> | [BuildTools/cmake/helpers/Build.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/helpers/Build.cmake) | Добавляет нативные исходники подключающего проекта с разделением по ролям до RegisterEngineSources. |
| <a id="entry-cmake-helper-addprojectlibraries-917d80c201"></a><code>cmake.helper.AddProjectLibraries</code> | <code>AddProjectLibraries(ROLES &lt;role&gt; [...] LIBRARIES &lt;target-or-library&gt; [...])</code> | <code>macro</code> | <code>COMMON</code>, <code>SERVER</code>, <code>CLIENT</code>, <code>MAPPER</code>, <code>BAKER</code>, <code>TESTS</code> | [BuildTools/cmake/helpers/Build.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/helpers/Build.cmake) | Компонует принадлежащие проекту цели или явно заданные платформенные библиотеки только с использующими их ролями движка до BuildCoreLibraries. |
| <a id="entry-cmake-helper-addnativeincludedir-60a1b8f1db"></a><code>cmake.helper.AddNativeIncludeDir</code> | <code>AddNativeIncludeDir(&lt;directory&gt; [...])</code> | <code>macro</code> | - | [BuildTools/cmake/helpers/Build.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/helpers/Build.cmake) | Добавляет относительные к проекту каталоги include, когда включены нативные скрипты. |
| <a id="entry-cmake-helper-addbakingtarget-5d644391c3"></a><code>cmake.helper.AddBakingTarget</code> | <code>AddBakingTarget(&lt;target&gt; [SUB_CONFIG &lt;name&gt;] [FORCE] [COMMENT &lt;text&gt;])</code> | <code>function</code> | - | [BuildTools/cmake/helpers/Build.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/helpers/Build.cmake) | Создаёт проектную цель запекания с необязательным subconfig, принудительным режимом и обновлением build hash после SetupScriptsAndBaking. |
