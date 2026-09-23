---
title: Методы AiControl
document_id: generated-ai-control-protocol-methods
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-ai-control-protocol-methods","locale":"ru","source_path":"Docs/en/reference/ai-control-protocol/methods.md","source_sha256":"0b29f9329e82acd40014900fcb35a95a116ed227b7b9b5bb04455ef9a5fbeede"} -->

# Методы AiControl

> Сгенерированный справочник. Не редактируйте его напрямую. Измените `BuildTools/AiControlProtocol.json`, затем запустите `python BuildTools/docs_ai_control_protocol.py --write`.

[Обзор](index.md) | [Wire-контракт](wire.md) | [Методы](methods.md) | [Команды и события](commands-events.md) | [Безопасность](security.md) | [Интеграция и проверка](integration-validation.md) | [Канонический JSON](../../../generated/ai-control-protocol.json) | [Руководство](../../how-to/ai-control-protocol.md)

| Стабильный ID | Метод | Параметры | Результат | Контракт | Источник |
| --- | --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-method-auth-a69cc82e95"></a><code>ai-control-protocol.method.auth</code> | <code>auth</code> | <code>&#123;token: string&#125;</code> | <code>&#123;authorized: boolean&#125;</code> | Аутентифицирует текущее соединение настроенным общим token; неудачная попытка оставляет его неавторизованным, а следующая попытка может завершиться успешно. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-method-ping-87dbd1fcf9"></a><code>ai-control-protocol.method.ping</code> | <code>ping</code> | <code>&#123;&#125;</code> | <code>&#123;ok: true&#125;</code> | Сообщает о доступности bridge после авторизации. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-method-status-0ad2a41458"></a><code>ai-control-protocol.method.status</code> | <code>status</code> | <code>&#123;&#125;</code> | <code>&#123;running, host, port, queuedCommands, maxQueuedCommands, events, maxEvents, observationSeq, lastError&#125;</code> | Возвращает состояние транспорта, заполнение и пределы очереди/событий, sequence последнего наблюдения и последнюю ошибку bridge. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-method-observe-56481e1dba"></a><code>ai-control-protocol.method.observe</code> | <code>observe</code> | <code>&#123;&#125;</code> | <code>&#123;observationSeq: integer, observation: object&#125;</code> | Возвращает последний полный проектный снимок наблюдения и монотонно возрастающий sequence его замены. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-method-events-226d53f902"></a><code>ai-control-protocol.method.events</code> | <code>events</code> | <code>&#123;afterSeq?: integer, limit?: integer&#125;</code> | <code>&#123;latestSeq: integer, events: [&#123;seq: integer, event: object&#125;]&#125;</code> | Возвращает сохранённые события с seq больше afterSeq по возрастанию; ограничивает limit диапазоном 1..500. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-method-act-938cfa4f33"></a><code>ai-control-protocol.method.act</code> | <code>act</code> | <code>project command object with non-empty type</code> | <code>&#123;accepted: true, commandSeq: integer&#125;</code> | Ставит в очередь одну проектную команду и возвращает её sequence; принятие не означает завершение или игровой успех. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
