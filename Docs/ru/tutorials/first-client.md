---
layout: default
title: Первый игровой клиент
locale: ru
document_id: first-client-tutorial
permalink: /Docs/ru/tutorials/first-client.html
---

<!-- docs-translation: {"document_id":"first-client-tutorial","locale":"ru","source_path":"Docs/en/tutorials/first-client.md","source_sha256":"c96b9ffeaf641a1f2dc5af3b1d520d649740af3814044c84026f85cdde0f66c0"} -->

# Первый игровой клиент

Соберите и запустите настольный клиент, подключённый к серверу примера
[Minimal Multiplayer](../../../Examples/MinimalMultiplayer/README.ru.md), который принадлежит движку.

## Предварительные требования

Сначала завершите руководство по headless-проекту. В отдельном checkout FOnline
остаётся подмодулем `Engine/` рядом с файлом `CMakeLists.txt` примера.

В корне отдельного примера выполните:

```powershell
python validate.py
```

В Linux используйте `python3 validate.py`. Команда конфигурирует и собирает
настольный клиент, headless-клиент, headless-сервер и baker, после чего запускает
полный автоматизированный урок.

Сопровождающие движок могут проверить исходники непосредственно из этого
репозитория без материализации отдельного репозитория:

```powershell
cd Examples\MinimalMultiplayer
python validate.py
```

В Linux используйте ту же точку входа `python3 validate.py` из `Examples/MinimalMultiplayer`.

## Запуск видимого клиента

После успешного завершения `validate.py` откройте два терминала в корне
отдельного примера. Сначала запустите сервер:

```powershell
Build\windows\Binaries\Server-Windows-win64\FOMM_ServerHeadless.exe -ApplyConfig FOnlineMinimalMultiplayer.fomain
```

Затем запустите настольный клиент:

```powershell
Build\windows\Binaries\Client-Windows-win64\FOMM_Client.exe -ApplyConfig FOnlineMinimalMultiplayer.fomain
```

Имя сгенерированного платформенного каталога может отличаться в зависимости от
хоста и генератора. Клиент подключается к `127.0.0.1:4010`, входит в игру через
`Tutorial::EnterWorld()` и загружает `TutorialMap`. Когда карта появится,
нажмите пробел. Клиент вызовет `CollectSupply()`, сервер увеличит
`SuppliesCollected`, а клиент отобразит синхронизированное значение.

В журнале должны появиться следующие этапы:

```text
tutorial_client_connected
tutorial_server_world_ready
tutorial_client_map_loaded
tutorial_server_supply_collected=1
tutorial_client_supply_collected=1
```

## Граница между клиентом и сервером

Полное поведение находится в
[`Scripts/Tutorial.fos`](../../../Examples/MinimalMultiplayer/Scripts/Tutorial.fos):

1. На клиенте `Game.OnStart` привязывает шрифт движка и вызывает `Game.Connect()`.
2. На клиенте `Game.OnConnected` вызывает серверный remote call `EnterWorld()`.
3. Сервер создаёт локацию, карту, криттеров и предмет, авторизует игрока, а затем вызывает `WorldReady()`.
4. На клиенте `Game.OnMapLoaded` включает ввод и отрисовывает карту с небольшим интерфейсным слоем.
5. Пробел отправляет только намерение. Создание предмета и изменение реплицируемого свойства принадлежат серверу.

Клиент отвечает за представление и намерение, сервер — за авторитетное состояние мира и постоянные данные.

## Контракт конфигурации

[`FOnlineMinimalMultiplayer.fomain`](../../../Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain)
генерируется из текущих объявлений настроек движка, потому что при подготовке
дистрибутива Config baker требует полный набор серверных и клиентских настроек.
Решения проекта остаются явными в `generate_config.py`: изменяйте его
`OVERRIDES` или `PROJECT_SECTIONS`, повторно генерируйте `.fomain` и сохраняйте
проверку `--check` зелёной. При обычном запуске настройки применяются в таком
порядке:

1. значения по умолчанию из движка;
2. конфигурация проекта или встроенная конфигурация пакета;
3. выбранные sub-config;
4. доступный для записи кэш локальной конфигурации;
5. переопределения командной строки;
6. производные автоматические настройки.

Настольная цель размещает `FOMM_BakerLib` рядом с хостом, чтобы запуск без
пакета мог предварительно запечь изменённые исходники. Более строгий `Config`
baker для дистрибутива требует инициализации каждой сохраняемой настройки
проекта; `CheckTutorialConfig` обнаруживает рассогласование до запекания или
упаковки.

## Восстановление после ошибок

- `Connection refused`: сначала запустите сервер и убедитесь, что порт `4010` свободен.
- `Config file not found`: запускайте команду из корня примера или передайте полный путь в `-ApplyConfig`.
- Отсутствует `FOMM_ClientLib.dll` или `FOMM_BakerLib.dll`: пересоберите пресет `windows-check`; не копируйте DLL из другого проекта.
- Изменение карты или скрипта не учитывается: выполните `cmake --build --preset windows-check`, чтобы запечь ресурсы и повторить весь маршрут.

Продолжите с [первого изменения контента](first-content.md).
