---
layout: default
title: Первое изменение контента
locale: ru
document_id: first-content-tutorial
permalink: /Docs/ru/tutorials/first-content.html
---

<!-- docs-translation: {"document_id":"first-content-tutorial","locale":"ru","source_path":"Docs/en/tutorials/first-content.md","source_sha256":"8da0c82371079add4a5dc05948b334fd2f56383caee460998674b118ee8e82dc"} -->

# Первое изменение контента

Измените локализованный текст прототипа и подтвердите результат запекания в
примере [Minimal Multiplayer](../../../Examples/MinimalMultiplayer/README.ru.md).

## Найдите исходники-владельцы

Урок намеренно отделяет авторские данные от поведения:

| Исходник | Ответственность |
|---|---|
| `Content/StarterContent.fopro` | прототипы игрока, NPC, предмета и локации, а также английский и русский текст |
| `Maps/TutorialMap.fomap` | идентификатор карты, размер и рабочий гекс |
| `Scripts/Tutorial.fos` | создание мира, взаимодействие и клиентское представление |
| `FOnlineMinimalMultiplayer.fomain` | сгенерированные входы baker, порядок языков и настройки среды выполнения |

Не редактируйте `Build/*`, `Baking/*`, сгенерированные метаданные или ресурсы
готового пакета для авторинга контента. Это выходные данные. Настройки,
принадлежащие проекту, находятся в `generate_config.py`; сгенерированный из них
`.fomain` должен оставаться воспроизводимым.

## Переименуйте учебный предмет

Откройте
[`Content/StarterContent.fopro`](../../../Examples/MinimalMultiplayer/Content/StarterContent.fopro)
и замените оба значения `$Text` у `TutorialSupply`. Не меняйте стабильное имя
прототипа:

```ini
[ProtoItem]
$Name = TutorialSupply
$Text engl = "Emergency cache"
$Text russ = "Аварийный контейнер"
Stackable = True
```

`Baking.BakeLanguages = engl russ` делает английский базой нормализации этого
примера. Значения обоих языков создаются вместе, чтобы контракт контента не
зависел неявно от fallback.

Проверяемый smoke-сценарий и сценарий запуска готового пакета намеренно считают
видимое английское имя публичным ожиданием. В том же изменении обновите
клиентский маркер и в `tutorial-smoke.json`, и в `package-smoke.json`:

```json
"tutorial_client_content=Emergency cache"
```

Проверка, обновлённая вместе с контентом, отличает намеренное переименование от
отсутствующего или устаревшего результата запекания.

## Запеките и проверьте

Выполните полную проверку из корня отдельного checkout примера:

```powershell
python validate.py
```

Ожидаемые семантические свидетельства включают:

```text
[tutorial-smoke] remote calls and replicated persistent property verified
[gameplay-test] scenario content-test: passed
tutorial_client_content=Emergency cache
tutorial_server_supply_collected=1
[gameplay-test] summary: suite=minimal-multiplayer-tutorial status=passed scenarios=2 passed=2 failed=0
```

В строках вывода процессов вокруг маркеров могут присутствовать метки runner.
Контрактом являются сами маркеры и итоговый статус `passed`.

Для checkout движка используйте
`python BuildTools/buildtools.py validate win64-tutorial-smoke` или эквивалент
для Linux.

Чтобы визуально проверить русский текст, задайте переопределение
`Client.Language` равным `russ` в `generate_config.py`, заново сгенерируйте
проверяемый `.fomain`, пересоберите пресет проверки и запустите обычный
настольный клиент. Затем восстановите язык по умолчанию или намеренно
зафиксируйте его как политику проекта; выбор языка не должен находиться в
исходниках движка.

## Безопасно добавляйте контент

Для нового прототипа или карты:

1. Добавьте авторскую секцию в каталог, который потребляет соответствующий
   `ResourcePack`.
2. Задайте стабильное `$Name`; последующее изменение сохраняемого идентификатора
   является миграцией, а не косметическим переименованием.
3. Создайте данные для каждого языка из `Baking.BakeLanguages`.
4. Ссылайтесь на объект через метаданные скрипта или проверенный `hstring` в
   соответствии с контрактом API-владельца.
5. Расширьте тест контента, затем запеките ресурсы и запустите самый узкий
   маршрут, который их потребляет.

Полные контракты описаны в руководствах [Формат прототипов](../how-to/content/prototype-format.md),
[Формат карт](../how-to/content/map-format.md) и
[Текст и локализация](../how-to/content/text-and-localization.md).

## Восстановление после ошибок

- `Unknown prototype`: убедитесь, что расширение указано в
  `Baking.ProtoFileExtensions`, а содержащий файл каталог подключён к пакету
  ресурсов `Protos`.
- Остаётся прежнее имя: не исправляйте сгенерированные данные или готовые
  пакеты вручную; повторите проверяемое запекание и найдите пакет-владелец в
  `Baking/Baking.report.json`.
- Тест контента проходит, но клиентский маркер отсутствует: проверяйте пакет
  ресурсов `Texts` и вызов `Game.GetText()` в среде выполнения, а не только
  существование прототипа.

Продолжите с [первого автоматизированного теста](first-test.md).
