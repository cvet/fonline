---
title: Wire-контракт AiControl
document_id: generated-ai-control-protocol-wire
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-ai-control-protocol-wire","locale":"ru","source_path":"Docs/en/reference/ai-control-protocol/wire.md","source_sha256":"f07a9dd0d04b2a24ce7dbd50b985a88674ad22d62254dbf0ebd615b17a3e4172"} -->

# Wire-контракт AiControl

> Сгенерированный справочник. Не редактируйте его напрямую. Измените `BuildTools/AiControlProtocol.json`, затем запустите `python BuildTools/docs_ai_control_protocol.py --write`.

[Обзор](index.md) | [Wire-контракт](wire.md) | [Методы](methods.md) | [Команды и события](commands-events.md) | [Безопасность](security.md) | [Интеграция и проверка](integration-validation.md) | [Канонический JSON](../../../generated/ai-control-protocol.json) | [Руководство](../../how-to/ai-control-protocol.md)

| Стабильный ID | Правило | Требование | Обоснование | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-wire-tcp-stream-a1496d2000"></a><code>ai-control-protocol.wire.tcp-stream</code> | Поток байтов TCP | Bridge принимает явно настроенный TCP endpoint; клиенты не должны предполагать наличие discovery, TLS, HTTP или WebSocket framing. | Общие проектные реализации являются локальными каналами инструментов, а не протоколом интернет-сервиса. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-wire-ndjson-62b9249f69"></a><code>ai-control-protocol.wire.ndjson</code> | Один объект JSON в строке | Каждый запрос и ответ является одним объектом JSON, завершённым LF; peers обрабатывают строки в порядке соединения. | Строчный framing допускает потоковую обработку, удобен для инспекции и используется обоими проверенными проектными bridges. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-wire-utf8-bbcd1f1cb5"></a><code>ai-control-protocol.wire.utf8</code> | Кодировка UTF-8 | Строки JSON кодируются и декодируются как строгий UTF-8; неверные данные получают parse error или приводят к закрытию соединения. | Проектные наблюдения и payload действий могут содержать локализованный текст. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-wire-line-limit-f2c1dfbe4d"></a><code>ai-control-protocol.wire.line-limit</code> | Ограниченный размер строки | JSON payload запроса или ответа не превышает 1 МиБ до LF; слишком большой ввод отклоняется, а соединение может быть закрыто. | Даже локальному каналу автоматизации нужны детерминированные пределы памяти. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-wire-request-envelope-e3a81d64d9"></a><code>ai-control-protocol.wire.request-envelope</code> | Конверт запроса | Объект запроса содержит jsonrpc=2.0, выбранный вызывающей стороной id, непустую строку method и объект params. | Небольшой конверт по форме JSON-RPC даёт корреляцию, не заявляя полную спецификацию JSON-RPC. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-wire-response-envelope-bfea6842b8"></a><code>ai-control-protocol.wire.response-envelope</code> | Конверт ответа | Ответ повторяет jsonrpc=2.0 и id запроса, затем содержит ровно одно из result или error; error содержит целочисленный code и строковый message. | Строгая корреляция не позволяет одному шагу автоматизации получить результат другого шага. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-wire-sequential-connection-09dc2b0d93"></a><code>ai-control-protocol.wire.sequential-connection</code> | Последовательная обработка запросов | Клиент отправляет запрос и получает соответствующий ответ до повторного использования соединения; клиенты не должны требовать multiplexing или нескольких одновременных запросов. | Контракт остаётся реализуемым одним проектным listener и не создаёт скрытых гонок порядка. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |

## Коды ошибок

| Стабильный ID | Код | Имя | Применение | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-error-parse-12e2c3421b"></a><code>ai-control-protocol.error.parse</code> | <code>-32700</code> | Ошибка разбора | Отклоняет неверный UTF-8 JSON. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-error-invalid-request-c140fc72db"></a><code>ai-control-protocol.error.invalid-request</code> | <code>-32600</code> | Некорректный запрос | Отклоняет не-объект, неверный конверт, отсутствующий method или слишком большой запрос. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-error-method-not-found-416e6e9008"></a><code>ai-control-protocol.error.method-not-found</code> | <code>-32601</code> | Метод не найден | Отклоняет неизвестный метод протокола. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-error-invalid-params-24842d0a4b"></a><code>ai-control-protocol.error.invalid-params</code> | <code>-32602</code> | Некорректные параметры | Отклоняет отсутствующий тип команды или структурно неверные параметры метода. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-error-unauthorized-afd4c4e043"></a><code>ai-control-protocol.error.unauthorized</code> | <code>-32001</code> | Нет авторизации | Если token настроен, отклоняет все методы, кроме auth, пока соединение не авторизовано. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-error-queue-full-1c63470f94"></a><code>ai-control-protocol.error.queue-full</code> | <code>-32002</code> | Очередь команд заполнена | Отклоняет act, когда в ограниченной проектной очереди команд нет места. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
