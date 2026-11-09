---
title: Сгенерированный справочник CLI BuildTools
document_id: generated-cli-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-cli-index","locale":"ru","source_path":"Docs/en/reference/buildtools/index.md","source_sha256":"5c837c613ba09a8a9ab1f8cb944f794d5f8d5b5151fe4218c1895d6845484db2"} -->

# Сгенерированный справочник CLI BuildTools

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/buildtools.py`, затем выполните `python BuildTools/docs_cli.py --write`.

[Индекс справочника](index.md) | [Команды](commands.md) | [Каноническая JSON-модель](../../../generated/cli.json) | [Контракт генерации](../metadata/)

Этот справочник создаётся из того же `argparse.ArgumentParser`, который использует исполняемая точка входа BuildTools. Поэтому изменение парсера делает устаревшими зафиксированную модель и страницы.

## Статус контракта

| Поле | Значение |
| --- | --- |
| Стабильность | <code>internal</code> |
| Начиная с версии | Не объявлено |
| Политика поддержки | Версионируемая линия поддержки CLI не объявлена; закрепляйте ревизию движка в автоматизации. |
| Исходный парсер | [BuildTools/buildtools.py](https://github.com/cvet/fonline/blob/master/BuildTools/buildtools.py) |
| Дайджест контракта | <code>5079db447e1798d3ebc361db85498e4e22ef94e756573587aea998593d44a363</code> |

## Покрытие

| Справочник | Записи | Назначение |
| --- | --- | --- |
| [Команды](commands.md) | 15 | Команды с 39 собственными аргументами. |
| Глобальные аргументы | 0 | Аргументы, принимаемые перед командой. |

## Справка верхнего уровня

```text
usage: buildtools.py [-h]
                     {env,build,validate,setup-mono,format-source,format-content,toolset,build-auxiliary,prepare-workspace,repair-checkout-case,package-web-debug,package-android-debug,managed-project-path,host-check,prepare-host-workspace} ...

Shared BuildTools helpers

positional arguments:
  {env,build,validate,setup-mono,format-source,format-content,toolset,build-auxiliary,prepare-workspace,repair-checkout-case,package-web-debug,package-android-debug,managed-project-path,host-check,prepare-host-workspace}
    env                 resolve BuildTools environment
    build               configure and build a target
    validate            configure and validate scenarios
    setup-mono          prepare mono runtime
    format-source       format engine source files
    format-content      format project .fomain, prototype and optional AngelScript files
    toolset             build an existing toolset target
    build-auxiliary     build a separately packaged auxiliary tool
    prepare-workspace   prepare shared workspace parts
    repair-checkout-case
                        realign working-tree entry names with the git index
    package-web-debug   package the local web debug client
    package-android-debug
                        package the local android debug client
    managed-project-path
                        print the generated managed C# project and solution paths
    host-check          check host prerequisites
    prepare-host-workspace
                        prepare host workspace and prerequisites

options:
  -h, --help            show this help message and exit
```

## Граница

Включено:

- команды верхнего уровня и аргументы, объявленные функцией create_parser() в BuildTools/buildtools.py
- значения argparse по умолчанию, варианты, кардинальность, описания, usage и вывод --help

Исключено из этого среза:

- командные строки вспомогательных скриптов вне BuildTools/buildtools.py
- объявление package.py и контракты payload
- семантика validation-целей и внутренние helper-функции Python

Исключённые поверхности остаются деталями реализации, пока для них не будут опубликованы владеющий контракт на основе парсера и политика совместимости.
