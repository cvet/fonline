---
title: Грамматика объявления пакетов
document_id: generated-package-declaration
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-package-declaration","locale":"ru","source_path":"Docs/en/reference/packages/declaration.md","source_sha256":"c4b279fec1814e88588d32d9f3d865af926dec6cf52307f0daa5e33398407d36"} -->

# Грамматика объявления пакетов

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/PackageInterface.json` или `BuildTools/package.py`, затем выполните `python BuildTools/docs_package.py --write`.

[Индекс](index.md) | [Объявление](declaration.md) | [Матрица](matrix.md) | [Содержимое](payloads.md) | [CLI](cli.md) | [Канонический JSON](../../../generated/package.json)

Объявляет именованную цель пакета и содержимое одного или нескольких бинарных файлов.

Stable ID: `package.declaration.DefinePackage`

Source: [BuildTools/cmake/helpers/Build.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/helpers/Build.cmake); consumer: [BuildTools/cmake/stages/Packages.cmake](https://github.com/cvet/fonline/blob/master/BuildTools/cmake/stages/Packages.cmake).

```cmake
DefinePackage(<name>
    CONFIG <config>
    BINARY <target> <platform> <arch[+arch...]> <pack[+pack...]> [POSTFIX <value>]
    [BINARY ...]
)
```

| Стабильный ID | Предложение | Аргументы | Обязательно | Повторяемо | Назначение |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-package-declaration-definepackage-clause-config-4d3ab33f4f"></a><code>package.declaration.DefinePackage.clause.CONFIG</code> | <code>CONFIG</code> | <code>&lt;config&gt;</code> | да | нет | Задаёт конфигурацию по умолчанию для всего пакета. |
| <a id="entry-package-declaration-definepackage-clause-binary-94febbf53b"></a><code>package.declaration.DefinePackage.clause.BINARY</code> | <code>BINARY</code> | <code>&lt;target&gt; &lt;platform&gt; &lt;arch&gt; &lt;pack&gt;</code> | да | yes | Добавляет содержимое для одной комбинации цели, платформы и архитектуры; за ним необязательно могут следовать POSTFIX и значение, применяемые вместе только к этой записи бинарного файла. |

## Модификаторы отдельного бинарного файла

| Стабильный ID | Модификатор | Значение | По умолчанию | Назначение |
| --- | --- | --- | --- | --- |
| <a id="entry-package-binary-option-postfix-5979921a13"></a><code>package.binary-option.POSTFIX</code> | <code>POSTFIX</code> | <code>string</code> | <code>empty</code> | Выбирает соответствующий бинарный вход с суффиксом и изолирует для этой записи каталог цели, имя упакованной сборки, содержимое runtime-обновления и имя MSI, не затрагивая соседние записи BINARY. |

`DefinePackage` требует `CONFIG`. Каждое предложение `BINARY` превращается в отдельный вызов `package.py`. `POSTFIX` необязателен и относится только к непосредственно предшествующему `BINARY`; общего значения для всего пакета нет.
