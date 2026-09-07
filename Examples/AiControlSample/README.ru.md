---
layout: default
title: Образец протокола AiControl
document_id: ai-control-sample-readme
locale: ru
permalink: /Examples/AiControlSample/README.ru.html
---

<!-- docs-translation: {"document_id":"ai-control-sample-readme","locale":"ru","source_path":"Examples/AiControlSample/README.md","source_sha256":"d11c363a4d4954befcc3510e2435a5b3191e269e96b50533e77624835ccd273f"} -->

# Образец протокола AiControl

Этот запускаемый пример демонстрирует независимый от проекта контракт
транспорта и жизненного цикла AiControl. Это намеренно не игра, и FOnline в
него не встроен: сервер заменяет клиентский цикл проекта, чтобы поведение
протокола можно было проверять без заимствования игровых схем Last Frontier
или TLA.

Пример доказывает:

- фрейминг запросов и ответов в виде UTF-8 JSON с разделением переводами строк;
- авторизацию каждой связи отдельным общим токеном;
- конверты `ping`, `status`, `observe`, `events` и `act`;
- ограниченные очереди команд и событий;
- sequence id принятых команд и последующие события `command_completed`;
- принадлежащие проекту наблюдения и имена действий.

Запустите полный smoke из корня Engine:

```powershell
python Examples\AiControlSample\run_protocol_smoke.py
```

Для ручной проверки моста поместите токен в переменную окружения и запустите
пример. При использовании `--port 0` строка готовности сообщает выбранный порт:

```powershell
$env:FONLINE_AI_TOKEN = 'local-development-token'
python Examples\AiControlSample\ai_control_sample.py --port 43011
python BuildTools\ai_control_client.py --port 43011 status
python BuildTools\ai_control_client.py --port 43011 act --type move --x 7 --y 9
python BuildTools\ai_control_client.py --port 43011 events
python BuildTools\ai_control_client.py --port 43011 observe
```

По умолчанию CLI читает токен из `FONLINE_AI_TOKEN` и никогда не принимает его
как аргумент командной строки. Обе программы отклоняют адреса вне loopback,
если оператор явно не разрешил их; пример дополнительно требует непустой токен.
Транспорт не использует TLS, поэтому loopback остаётся рекомендуемой границей.

Перед адаптацией примера прочитайте
[AiControlProtocol.md](../../Docs/AiControlProtocol.md). Реальный встраивающий
проект обязан заменить обработчик наблюдений и действий примера, сохранить
обычную серверную авторитетность, исключить listener из shipping-клиентов на
этапе компиляции и отдельно проверить получившуюся нативную/скриптовую
интеграцию.
