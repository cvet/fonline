---
title: Команды и события AiControl
document_id: generated-ai-control-protocol-commands-events
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-ai-control-protocol-commands-events","locale":"ru","source_path":"Docs/en/reference/ai-control-protocol/commands-events.md","source_sha256":"cb796cf889ab0c0d44ecb2709b405ed115ddcbf6d6f898f15fe3bd25f71b3945"} -->

# Команды и события AiControl

> Сгенерированный справочник. Не редактируйте его напрямую. Измените `BuildTools/AiControlProtocol.json`, затем запустите `python BuildTools/docs_ai_control_protocol.py --write`.

[Обзор](index.md) | [Wire-контракт](wire.md) | [Методы](methods.md) | [Команды и события](commands-events.md) | [Безопасность](security.md) | [Интеграция и проверка](integration-validation.md) | [Канонический JSON](../../../generated/ai-control-protocol.json) | [Руководство](../../how-to/ai-control-protocol.md)

| Стабильный ID | Поле | Тип | Контракт | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-command-type-9b682a6215"></a><code>ai-control-protocol.command.type</code> | <code>type</code> | <code>string</code> | Обязательный непустой discriminator проектной команды. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-target-id-bebe0b0729"></a><code>ai-control-protocol.command.target-id</code> | <code>targetId</code> | <code>project identifier</code> | Необязательный идентификатор основной цели. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-item-id-c0a823f488"></a><code>ai-control-protocol.command.item-id</code> | <code>itemId</code> | <code>project identifier</code> | Необязательный идентификатор предмета. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-aux-id-1681634d1b"></a><code>ai-control-protocol.command.aux-id</code> | <code>auxId</code> | <code>project identifier</code> | Необязательный вспомогательный идентификатор. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-x-236370e0e6"></a><code>ai-control-protocol.command.x</code> | <code>x</code> | <code>integer</code> | Необязательная проектная координата X мира или сетки. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-y-e2be376e13"></a><code>ai-control-protocol.command.y</code> | <code>y</code> | <code>integer</code> | Необязательная проектная координата Y мира или сетки. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-screen-x-21053d00ef"></a><code>ai-control-protocol.command.screen-x</code> | <code>screenX</code> | <code>integer</code> | Необязательная экранная координата X. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-screen-y-7347a1f652"></a><code>ai-control-protocol.command.screen-y</code> | <code>screenY</code> | <code>integer</code> | Необязательная экранная координата Y. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-int-arg-e0c52d8389"></a><code>ai-control-protocol.command.int-arg</code> | <code>intArg</code> | <code>integer</code> | Необязательный целочисленный payload с проектным смыслом. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-string-arg-84674f9aae"></a><code>ai-control-protocol.command.string-arg</code> | <code>stringArg</code> | <code>string</code> | Необязательный строковый payload с проектным смыслом. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-command-append-0c0a34c5f9"></a><code>ai-control-protocol.command.append</code> | <code>append</code> | <code>boolean</code> | Необязательный запрос проектной семантики append вместо replace. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |

## Конверт завершения

Принятая команда возвращает `commandSeq`. Позднее проект добавляет событие с `type=command_completed`, тем же `commandSeq`, логическим `success` и понятным проекту `message`. Принятие никогда не означает успех.
