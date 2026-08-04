---
layout: default
title: Исходный код FOnline Engine
permalink: /Source/README.ru.html
locale: ru
document_id: source-readme
---

<!-- docs-translation: {"document_id":"source-readme","locale":"ru","source_path":"Source/README.md","source_sha256":"f386f4c9f5f293ab0378eb7f6327993123f09f0144e488d7c85136f00e26618a"} -->

# Дерево исходного кода FOnline Engine

- `Applications/` - точки входа исполняемых файлов для генерируемых целей сборки.
- `Client/` - клиентская часть среды выполнения, используемая игровым клиентом и редакторами.
- `Common/` - код среды выполнения, общий для клиента, сервера и редакторов.
- `Tools/` - реализации Mapper, baker, viewer и инструментов разработчика.
- `Server/` - серверная часть среды выполнения, также встраиваемая в редакторы и тестовые цели.
- `Scripting/` - скриптовые бэкенды, поддержка генерации кода и переиспользуемые базовые скрипты.
- `Tests/` - детерминированные нативные тесты движка и их точка входа на уровне исходного кода.

## Подробная навигация по исходному коду

Актуальное руководство по точкам входа, направлению зависимостей и владению
слоями приведено в разделах [Дерево исходного кода](../Docs/ru/contributing/source-tree/),
[Архитектура](../Docs/ru/explanation/architecture/) и
[Приложения](../Docs/ru/reference/applications.md).
