---
title: Сгенерированный интерфейс пакетов
document_id: generated-package-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-package-index","locale":"ru","source_path":"Docs/en/reference/packages/index.md","source_sha256":"da8fea67954a88af1c995bcf0557759ad2666d2f14f08da5d128b0e4de5f8c83"} -->

# Сгенерированный интерфейс пакетов

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/PackageInterface.json` или `BuildTools/package.py`, затем выполните `python BuildTools/docs_package.py --write`.

[Индекс](index.md) | [Объявление](declaration.md) | [Матрица](matrix.md) | [Содержимое](payloads.md) | [CLI](cli.md) | [Канонический JSON](../../../generated/package.json)

Этот справочник связывает объявление пакета CMake с контрактом упаковщика, который используется во время выполнения. Он описывает возможности движка, а не матрицу релиза встраивающего проекта.
Этот сгенерированный интерфейс пакетов является точкой входа для грамматики `DefinePackage`, целей `package.py`, платформ, pack tokens и эффектов payload.

## Решение между контрактами

Используйте следующую cross-contract последовательность:

1. Формулируйте package boundary явно: фиксируйте точную ревизию Engine и используйте `BuildTools/PackageInterface.json` как `internal` package contract; не заменяйте это утверждение общим revision-pinned заголовком.
2. Считайте допустимые dimensions package.py и payload effects только capability. Package matrices встраивающего проекта, release policy, secret provisioning, deployment topology и credentials подписи установщика остаются исключёнными и принадлежат проекту.
3. Рассматривайте support отдельно: `build_gated`, `smoke_gated`, `source_capable` и `not_in_public_matrix` являются разными состояниями.
4. Для каждого примера сообщайте вместе source status, visibility/state remote, наблюдённые required checks, точный Engine pin, политику update delivery и соответствующий Contract digest. Свидетельством публикации является только source со статусом `published`, remote `public` / `published` и наблюдёнными checks `passing`; private, reserved, source-staged, planned или unobserved repository им не является.

## Статус контракта

| Поле | Значение |
| --- | --- |
| Stability | <code>internal</code> |
| Since | Не объявлено |
| Support policy | Версионируемая линия поддержки пакетов не объявлена; встраивающие проекты должны фиксировать ревизию движка. |
| Manifest | [BuildTools/PackageInterface.json](https://github.com/cvet/fonline/blob/master/BuildTools/PackageInterface.json) |
| Packager | [BuildTools/package.py](https://github.com/cvet/fonline/blob/master/BuildTools/package.py) |
| Contract digest | <code>be83fb2d181b0a244ec30e08ddc5fe64ea10e717c3597e9df7c706e3f67aea98</code> |

## Покрытие

| Справочник | Записи | Назначение |
| --- | --- | --- |
| [Declaration](declaration.md) | 2 | CMake clauses and per-binary modifiers. |
| [Targets/platforms/packs](matrix.md) | 6 / 6 / 19 | Accepted runtime dimensions and support status. |
| [Payloads and artifacts](payloads.md) | 8 | Implemented output-producing pack tokens. |
| [Packager CLI](cli.md) | 15 | Exact internal package.py invocation contract. |

## Граница ответственности

Включено:

- DefinePackage declaration clauses and the per-binary POSTFIX modifier
- package.py targets, platforms, architecture keys, pack tokens, and payload effects
- implemented, placeholder, and unsupported package boundaries

Не входит в этот раздел:

- embedding-project package matrices and release policy
- project configuration key schema and secret provisioning
- resource-pack content selection and deployment topology
- installer signing credentials and external tool operation
