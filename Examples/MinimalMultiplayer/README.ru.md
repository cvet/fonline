---
permalink: /Examples/MinimalMultiplayer/README.ru.html
locale: ru
document_id: minimal-multiplayer-readme
---

<!-- docs-translation: {"document_id":"minimal-multiplayer-readme","locale":"ru","source_path":"Examples/MinimalMultiplayer/README.md","source_sha256":"ad90b82633801cdc08ce4ac47dcd8ca34b2497da81a3d371b7aa12c004d6258d"} -->

# Минимальный многопользовательский проект FOnline

Отдельный первый игровой проект после
[`MinimalProject`](../MinimalProject/README.ru.md), использующий только
публичные интерфейсы и ресурсы движка.

Итоговое состояние демонстрирует:

- нативный клиент, подключённый к headless-серверу;
- одну сгенерированную локацию и одну запечённую карту;
- криттера игрока, NPC и предмет на карте с fallback-спрайтами движка;
- клиент-серверное взаимодействие с предметом;
- одно свойство криттера `PublicSync Persistent`;
- парные клиентские и серверные remote call и события жизненного цикла;
- видимость криттеров по расстоянию через один небольшой публичный серверный Engine hook;
- английский и русский текст прототипов;
- серверный тест контента и полный видимый клиенту smoke-тест;
- нативные архивы клиента и сервера с inventory, хэшами и свидетельствами
  запуска готового пакета;
- один принадлежащий движку fixture исходника и текстуры SPARK для Mapper,
  Particle Preview, Particle Viewer и capture документации.

## Предварительные требования

- CMake 3.22 или новее;
- Python 3;
- Visual Studio с поддержкой C++ в Windows либо GCC и Ninja Multi-Config в
  Linux;
- checkout движка в `Engine/`:

```bash
git clone --recursive https://github.com/cvet/fonline-minimal-multiplayer.git
```

При запуске из репозитория движка, а не отдельного примера, до конфигурации
направьте `Engine` обратно на этот checkout:

```powershell
New-Item -ItemType Junction -Path Engine -Target ..\..
```

## Проверьте всё

Windows:

```powershell
python validate.py
```

Linux:

```bash
python3 validate.py
```

Команда конфигурирует проект, запекает ресурсы, собирает настольный и
headless-клиенты и headless-сервер, запускает изолированный тест контента,
запускает оба приложения с headless-клиентом, входит в игру, загружает карту,
подбирает предмет и проверяет запечённые метаданные.

## Проверьте нативные пакеты

После конфигурации соберите package acceptance target текущего хоста:

```powershell
cmake --build --preset windows-package
```

```bash
cmake --build --preset linux-package
```

Цель принудительно запекает конфигурацию `TutorialSmoke`, создаёт исходные
payload клиента и сервера и архивы ZIP или tar.gz, сравнивает inventory архива
и payload, хэширует каждый опубликованный архив и файл payload и запускает
взаимодействие готовых headless-сервера и клиента. Свидетельства записываются в
`Build/<host>/FOMM-Tutorial/` как `tutorial-packaging-manifest.json` и
`tutorial-package-runtime-report.json`.

Эта проверка квалифицирует только неподписанный fixture headless-архива
руководства. Она не заявляет acceptance установщика, подписи, магазина,
публичного развёртывания, постоянной базы данных, видимого renderer или звука,
обновления или rollback. Хост становится поддерживаемым package claim только
после того, как обязательное задание CI сохранит архивы и манифесты с адресом
commit.

## Запустите видимую версию

Один раз выполните конфигурацию и сборку:

```powershell
cmake --preset windows
cmake --build --preset windows-check
```

Затем запустите сервер и клиент в двух терминалах без `TutorialSmoke`:

```powershell
Set-Location Build\windows
.\Binaries\Server-Windows-win64\FOMM_ServerHeadless.exe -ApplyConfig ..\..\FOnlineMinimalMultiplayer.fomain
.\Binaries\Client-Windows-win64\FOMM_Client.exe -ApplyConfig ..\..\FOnlineMinimalMultiplayer.fomain
```

Точный каталог платформы может отличаться в зависимости от выбранного
генератора. Цель настольного клиента размещает `FOMM_BakerLib` рядом с
исполняемым файлом, поэтому перед ручным запуском изменённые исходники
запекаются заново. Клиент автоматически входит на учебную карту. Нажмите
пробел, чтобы подобрать локализованный предмет `TutorialSupply`; счётчик в
верхней панели синхронно увеличится.

## Карта исходников

| Путь | Владение |
|---|---|
| `generate_config.py` | полный набор значений по умолчанию настроек Engine и проверенные переопределения и секции руководства |
| `FOnlineMinimalMultiplayer.fomain` | сгенерированные готовые к упаковке runtime-настройки, пакеты ресурсов и тестовые sub-config |
| `Content/StarterContent.fopro` | прототипы игрока, проводника, запаса и локации |
| `Maps/TutorialMap.fomap` | мир из одной карты |
| `Scripts/Tutorial.fos` | жизненный цикл, вход, взаимодействие, отрисовка, метаданные и тест контента |
| `SourceExt/ServerExtension.cpp` | `CheckCritterVisibilityHook` для видимости по расстоянию на сервере туториала |
| `Particles/Documentation.spark` | минимальный зацикленный fixture авторинга SPARK |
| `assets/provenance.json` | машиночитаемая лицензия и точный хэш исходника принадлежащей движку текстуры частицы |
| `package-smoke.json` | сценарий запуска готовых сервера и клиента и семантические свидетельства |
| `tutorial-smoke.json` | порядок процессов, готовность, маркеры, сроки и сигнатуры ошибок |
| `run_tutorial_smoke.py` | проверка запечённых метаданных и точка входа общего gameplay runner движка |
| `verify_tutorial_package.py` | parity архива, SHA-256 inventory, запуск пакета и манифест свидетельств |
| `validate.py` | зависящая от хоста точка входа конфигурации и сборки |

В репозитории нет принадлежащего проекту каталога игровых изображений или
звука. Встроенные шрифты и fallback-спрайты движка сохраняют игровой маршрут
работоспособным. Единственная текстура частицы `Radiation.png` скопирована из
ресурсов движка под лицензией MIT и зафиксирована вместе с исходной ревизией,
путём, лицензией и SHA-256 в `assets/provenance.json`.

Smoke-манифест выполняется
`Engine/BuildTools/gameplay_test_runner.py`. Пример владеет командами процессов
и семантическими маркерами `tutorial_*`; Engine runner владеет проверкой
манифеста, упорядоченным запуском, capture вывода, общими сроками, очисткой и
JSON-отчётом в `Workspace/`. Переиспользуемый контракт описан в
[тестировании игрового процесса и интеграций](../../Docs/ru/how-to/testing/gameplay-and-integration.md).

Package verifier использует тот же runner после проверки parity payload и
архива. Грамматика пакета и граница release evidence описаны в
[Упаковке и выпуске](../../Docs/ru/how-to/release/packaging.md).

## Зафиксируйте рабочий процесс частиц Mapper

Соберите fixture исходника, запечённые ресурсы, Mapper и отдельный viewer:

```powershell
cmake --build Build\windows --config Release --target ForceBakeResources FOMM_Mapper FOMM_ParticleViewer
```

Запустите один из детерминированных профилей документации:

```powershell
Build\windows\Binaries\Mapper-Windows-win64\FOMM_Mapper.exe `
  -ApplyConfig FOnlineMinimalMultiplayer.fomain `
  -ApplySubConfig MapperDocumentationCapture

Build\windows\Binaries\Mapper-Windows-win64\FOMM_Mapper.exe `
  -ApplyConfig FOnlineMinimalMultiplayer.fomain `
  -ApplySubConfig MapperDocumentationSparkEditorCapture
```

Оба профиля открывают `TutorialMap`, фиксируют viewport `1280x800`, ждут
стабилизации UI, запрашивают полный кадр Mapper и завершаются после отложенного
capture. Первый выбирает `Documentation.spk` с фиксированными seed, scale и
prewarm; второй напрямую открывает авторский исходник `Documentation.spark`
через `Mapper.SparkEditorSource`, не перекрывая его окном Particle Preview.
Результат TGA
`MapperDocumentationCapture.tga` находится рядом с запущенным процессом.
Добавленные в репозиторий PNG документации, точные шаги взаимодействия, хэши
исходников и триггеры повторного capture находятся в
[`BuildTools/DocumentationScreenshots.json`](../../BuildTools/DocumentationScreenshots.json).

Этот профиль является свидетельством для переиспользуемой документации
инструментов. Он не входит в многопользовательский smoke-маршрут и не делает
SPARK обязательным backend для downstream-игр.

## Маршрут уроков

Используйте исходники в таком порядке:

1. [Первый игровой клиент](../../Docs/ru/tutorials/first-client.md)
2. [Первое изменение контента](../../Docs/ru/tutorials/first-content.md)
3. [Первый автоматизированный тест](../../Docs/ru/tutorials/first-test.md)

При обычном запуске значения по умолчанию Engine применяются до конфигурации
проекта, sub-config, локального cache, переопределений командной строки и
производных автоматических настроек. Поэтому `.fomain` записывает намеренные
решения проекта, а не копирует каждое runtime-значение по умолчанию.
Ориентированный на дистрибутив `Config` baker строже и требует полный контракт
сохраняемых настроек.
