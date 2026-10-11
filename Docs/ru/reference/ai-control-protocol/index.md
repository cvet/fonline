---
title: Сгенерированный справочник протокола AiControl
document_id: generated-ai-control-protocol-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-ai-control-protocol-index","locale":"ru","source_path":"Docs/en/reference/ai-control-protocol/index.md","source_sha256":"ab4ad1691fff340a296176f4fad47fb4e56d9e7aa069f43e170531a5e0ab2643"} -->

# Сгенерированный справочник протокола AiControl

> Сгенерированный справочник. Не редактируйте его напрямую. Измените `BuildTools/AiControlProtocol.json`, затем запустите `python BuildTools/docs_ai_control_protocol.py --write`.

[Обзор](index.md) | [Wire-контракт](wire.md) | [Методы](methods.md) | [Команды и события](commands-events.md) | [Безопасность](security.md) | [Интеграция и проверка](integration-validation.md) | [Канонический JSON](../../../generated/ai-control-protocol.json) | [Руководство](../../how-to/ai-control-protocol.md)

Этот справочник определяет принадлежащий Engine и нейтральный к проекту конверт для опциональных bridges наблюдения и управления ИИ. Он не определяет игровую схему, пространство имён MCP, listener в основном runtime или обход server authority.

## Состояние контракта

| Поле | Значение |
| --- | --- |
| Стабильность | <code>experimental</code> |
| Политика поддержки | Конверт версионируется, но остаётся экспериментальным; встраиваемые проекты должны закреплять ревизию Engine и владеть своими схемами наблюдений и действий. |
| Версия протокола | <code>1</code> |
| Маркер JSON-RPC | <code>2.0</code> |
| Endpoint по умолчанию | <code>127.0.0.1:43011</code> |
| Максимальный JSON payload | <code>1048576</code> |
| Стабильные элементы | 50 |
| Манифест-источник | [BuildTools/AiControlProtocol.json](https://github.com/cvet/fonline/blob/master/BuildTools/AiControlProtocol.json) |
| Digest контракта | <code>cea32aa5325a0f7601c93c0b40d599865eac7b5471093ec61ade8eb4baf070dd</code> |

| Справочник | Назначение |
| --- | --- |
| [Wire-контракт](wire.md) | Framing, конверты, порядок и коды ошибок. |
| [Методы](methods.md) | Шесть транспортных методов и их результаты. |
| [Команды и события](commands-events.md) | Общие поля команд и асинхронное завершение. |
| [Безопасность](security.md) | Loopback, tokens, shipping-сборки и authority. |
| [Интеграция и проверка](integration-validation.md) | Ответственность проекта и исполняемые свидетельства. |

## Граница

Включено:

- UTF-8 JSON с одним сообщением на строку поверх потока байтов TCP
- конверты request, result и error по форме JSON-RPC
- методы авторизации, liveness, status, наблюдения, событий и действий
- ограниченные transport, очередь команд и история событий
- жизненный цикл принятой команды и её асинхронного завершения
- граница угроз с loopback по умолчанию и эталонная проверка

Исключено:

- listener, скомпилированный в основной runtime FOnline
- проектные поля наблюдений, имена игровых действий, семантика сущностей и критерии готовности
- имена MCP-инструментов, рецепты оркестрации, игровые политики и prompts моделей
- обход server authority, команды администратора, TLS, discovery и публикация в интернете
