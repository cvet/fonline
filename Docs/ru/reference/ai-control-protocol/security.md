---
title: Граница безопасности AiControl
document_id: generated-ai-control-protocol-security
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-ai-control-protocol-security","locale":"ru","source_path":"Docs/en/reference/ai-control-protocol/security.md","source_sha256":"f2946955ec0954f19e62c257bee15c16638332d1c08f14eb47b2e20ccaea9ebc"} -->

# Граница безопасности AiControl

> Сгенерированный справочник. Не редактируйте его напрямую. Измените `BuildTools/AiControlProtocol.json`, затем запустите `python BuildTools/docs_ai_control_protocol.py --write`.

[Обзор](index.md) | [Wire-контракт](wire.md) | [Методы](methods.md) | [Команды и события](commands-events.md) | [Безопасность](security.md) | [Интеграция и проверка](integration-validation.md) | [Канонический JSON](../../../generated/ai-control-protocol.json) | [Руководство](../../how-to/ai-control-protocol.md)

## Правила безопасности

| Стабильный ID | Правило | Требование | Обоснование | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-ai-control-protocol-security-disabled-default-cc8a9a4e17"></a><code>ai-control-protocol.security.disabled-default</code> | По умолчанию выключен | Встраиваемый проект обязан требовать явную настройку для запуска listener. | Listener управления — чувствительная с точки зрения безопасности функция разработки. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-security-loopback-default-9ef3468888"></a><code>ai-control-protocol.security.loopback-default</code> | Сначала loopback | Listeners и клиенты по умолчанию используют 127.0.0.1 и отказываются работать за пределами loopback без явного разрешения оператора. | В протоколе нет шифрования транспорта или идентификации peer, кроме общего token. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-security-remote-token-46ab4a63ad"></a><code>ai-control-protocol.security.remote-token</code> | Для удалённой работы нужен token | Пример или проектный listener, доступный за пределами loopback, требует непустой token в дополнение к явному разрешению удалённой работы. | Удалённый listener с пустым token является неаутентифицированным каналом управления процессом. | [Examples/AiControlSample/ai_control_sample.py](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/ai_control_sample.py) |
| <a id="entry-ai-control-protocol-security-plaintext-376f7db0e7"></a><code>ai-control-protocol.security.plaintext</code> | Нет TLS | Считайте общий token и все payload открытым текстом в TCP; если non-loopback-транспорт неизбежен, используйте аутентифицированный зашифрованный туннель. | Аутентификация token не обеспечивает конфиденциальность или защиту от повторов. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-security-secret-input-206c1da3bb"></a><code>ai-control-protocol.security.secret-input</code> | Token из окружения | Эталонные инструменты читают tokens из именованной переменной окружения и не принимают или не выводят сырой token в аргументах. | Командные строки, списки процессов, история shell и отчёты часто раскрывают секреты. | [BuildTools/ai_control_client.py](https://github.com/cvet/fonline/blob/master/BuildTools/ai_control_client.py) |
| <a id="entry-ai-control-protocol-security-shipping-build-5df2405dda"></a><code>ai-control-protocol.security.shipping-build</code> | Исключение из shipping-сборки | Проекты должны исключать listener и путь удалённых команд из production-клиентов на этапе компиляции, а не только выключать runtime-настройкой. | Удаление socket и пути команд сокращает поверхность атак и эвристик антивируса. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
| <a id="entry-ai-control-protocol-security-server-authority-b28f36d0f6"></a><code>ai-control-protocol.security.server-authority</code> | Сохранение server authority | Проектные действия используют обычный клиентский ввод или аутентифицированные gameplay RPC; bridge по умолчанию не даёт server authority или возможностей администратора. | AI QA должен проходить ту же границу валидации, что и игрок. | [Examples/AiControlSample/README.md](https://github.com/cvet/fonline/blob/master/Examples/AiControlSample/README.md) |
