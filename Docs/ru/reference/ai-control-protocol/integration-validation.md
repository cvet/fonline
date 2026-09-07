---
title: Интеграция и проверка AiControl
document_id: generated-ai-control-protocol-integration-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-ai-control-protocol-integration-validation","locale":"ru","source_path":"Docs/en/reference/ai-control-protocol/integration-validation.md","source_sha256":"c6f6bbc9e6bbe2bb595d916db335ed897e8561ef0727b45693fc8ba81b8c7af2"} -->

# Интеграция и проверка AiControl

> Сгенерированный справочник. Не редактируйте его напрямую. Измените `BuildTools/AiControlProtocol.json`, затем запустите `python BuildTools/docs_ai_control_protocol.py --write`.

[Обзор](index.md) | [Wire-контракт](wire.md) | [Методы](methods.md) | [Команды и события](commands-events.md) | [Безопасность](security.md) | [Интеграция и проверка](integration-validation.md) | [Канонический JSON](../../../generated/ai-control-protocol.json) | [Руководство](../../how-to/ai-control-protocol.md)

## Правила интеграции

| Стабильный ID | Правило | Требование | Обоснование | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-integration-native-extension-e36ecf4ff0"></a><code>ai-control-protocol.integration.native-extension</code> | Listener принадлежит проекту | Реализуйте listener как опциональное native-расширение встраиваемого проекта, пока в runtime Engine не появится проверенный владелец. | Текущий протокол переиспользуем, но политика listener и shipping-риски остаются ответственностью проекта. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-integration-loop-ownership-48c18d7fea"></a><code>ai-control-protocol.integration.loop-ownership</code> | Извлечение команд в игровом цикле | Сетевой поток только проверяет запросы act и ставит их в очередь; клиентский цикл проекта-владельца извлекает команды и изменяет состояние клиента. | Игровые объекты и состояние script runtime не являются socket-thread-safe. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-integration-observation-schema-f76de10379"></a><code>ai-control-protocol.integration.observation-schema</code> | Версионирование проектных наблюдений | Объект наблюдения содержит принадлежащую проекту версию схемы и достаточные для адаптера метаданные готовности/действий; протокол Engine не определяет игровые поля. | Last Frontier и TLA обоснованно предоставляют разные игровые модели. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-integration-command-completion-dc8b830972"></a><code>ai-control-protocol.integration.command-completion</code> | Событие завершения | Каждая принятая команда в итоге создаёт command_completed с commandSeq, success и message, включая неподдерживаемые или неудачные проектные действия. | Принятие доказывает только вставку в очередь; агентам нужен связанный терминальный результат. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-integration-mcp-boundary-61085ca8b1"></a><code>ai-control-protocol.integration.mcp-boundary</code> | Граница MCP-адаптера | MCP-адаптер может отображать проектные наблюдения/действия в семантические инструменты, но пространство имён tools, запуск, память, prompts и игровые политики принадлежат проекту. | Совместимость транспорта не должна ложно стандартизировать QA-поверхность одной игры. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-integration-bounded-state-40117587d7"></a><code>ai-control-protocol.integration.bounded-state</code> | Ограниченные очереди и история | Очередь команд и сохранённая история событий имеют положительные настраиваемые пределы, видимые через status; заполненная очередь отклоняет команды, не перезаписывая их. | Остановившийся клиентский цикл не должен вызывать неограниченный рост памяти или молчаливую потерю команд. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |

## Правила проверки

| Стабильный ID | Правило | Требование | Обоснование | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-validation-protocol-smoke-cb6a17829f"></a><code>ai-control-protocol.validation.protocol-smoke</code> | Smoke-тест протокола | Запустите эталонный клиент с примером сервера и докажите auth, liveness, status, наблюдение, неверный ввод, принятие действия, завершение, обновление состояния и поведение event cursor. | Одна отрисованная схема не доказывает состояние соединения и асинхронный жизненный цикл. | [Examples/AiControlSample/run_protocol_smoke.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/run_protocol_smoke.py) |
| <a id="entry-ai-control-protocol-validation-malformed-peer-87c73605b1"></a><code>ai-control-protocol.validation.malformed-peer</code> | Неверные ответы peer | Клиентские тесты отклоняют неверный JSON, неподдерживаемые конверты, несовпадающие id, одновременные result/error и слишком длинные строки. | Автоматизация должна завершаться закрыто, а не принимать неоднозначные данные. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-validation-security-30298ca936"></a><code>ai-control-protocol.validation.security</code> | Тесты границы безопасности | Тесты доказывают отказ для non-loopback, отдельную для соединения авторизацию, отклонение неверного token и отсутствие token в аргументах командной строки. | Требования безопасности должны оставаться исполняемыми при развитии helper. | [Examples/AiControlSample/run_protocol_smoke.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/run_protocol_smoke.py) |
| <a id="entry-ai-control-protocol-validation-project-native-1a7e43bc67"></a><code>ai-control-protocol.validation.project-native</code> | Native-интеграция проекта | Настоящий проект отдельно собирает native bridge, запускает настоящий клиент, проверяет извлечение очереди в клиентском цикле и корректные shutdown/reconnect. | Пример Python доказывает протокол, а не native runtime FOnline. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-validation-gameplay-authority-b2fddff3ee"></a><code>ai-control-protocol.validation.gameplay-authority</code> | Обычный игровой путь | Проектные тесты показывают, что типичные действия проходят обычную серверную валидацию, а отклонённые игровые действия завершаются ошибкой. | Успешный smoke-тест протокола не доказывает игровую авторизацию или anti-cheat-границы. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-validation-shipping-artifact-764a4733a6"></a><code>ai-control-protocol.validation.shipping-artifact</code> | Инспекция shipping-артефакта | Release-проверка подтверждает, что production-клиенты не могут открыть listener AiControl и не содержат проектную реализацию удалённых команд. | Выключение по умолчанию слабее отсутствия в поставляемом бинарном файле. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |

## Команды проверки

```powershell
python Examples\AiControlSample\run_protocol_smoke.py
python BuildTools\tests\test_ai_control_protocol.py
python BuildTools\docs_ai_control_protocol.py --check
```
