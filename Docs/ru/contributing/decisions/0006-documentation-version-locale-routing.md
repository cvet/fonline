---
layout: default
title: "ADR-0006: версия, локали и стабильные маршруты документации"
locale: ru
document_id: adr-documentation-version-locale-routing
permalink: /Docs/ru/contributing/decisions/0006-documentation-version-locale-routing.html
---

<!-- docs-translation: {"document_id":"adr-documentation-version-locale-routing","locale":"ru","source_path":"Docs/en/contributing/decisions/0006-documentation-version-locale-routing.md","source_sha256":"6a90c2be548426ba6e0bbd4644cf0c2eec7798b74a5fc5927dc6e7d81f0b8962"} -->

# ADR-0006: версия, локали и стабильные маршруты документации

- Статус: принято
- Дата: 2026-07-16
- Изменено: 2026-08-01 после проверенных групп физической миграции EN/RU
- Владельцы: документация, сборка и выпуск

## Контекст

FOnline публикует Markdown репозитория через GitHub Pages/Jekyll по адресу `https://fonline.ru`. ADR-0001 выбрал будущую структуру `Docs/en` и `Docs/ru`, ADR-0003 сделал тот же корпус доступным для ИИ-клиентов, а ADR-0004 добавил навигацию и поиск на основе манифеста. Эти решения ещё не определяли единый исполняемый контракт для следующих вопросов:

- значение отображаемой версии документации;
- момент, когда можно публиковать снимки выпусков;
- стабильный URL каждой текущей публичной страницы;
- английский целевой путь и путь русского зеркала каждой страницы для людей;
- перенаправления при перемещении плоского английского дерева;
- несколько legacy pages, сходящихся к одной канонической замене.

Перемещение файлов или начало перевода без такого контракта поставило бы redirects, language switching, search и AI retrieval в зависимость от записанных вручную сведений о путях.

## Проверенные исходные пути

- `Docs/documentation-manifest.json`
- `BuildTools/docs_ai_delivery.py`
- `BuildTools/docs_site.py`
- `BuildTools/docs_validate.py`
- `BuildTools/tests/test_docs_ai_delivery.py`
- `BuildTools/tests/test_docs_site.py`
- `BuildTools/tests/test_docs_site_layout.py`
- `BuildTools/tests/test_docs_validate.py`
- `_config.yml`
- `_layouts/default.html`
- `Docs/en/contributing/documentation/site-publication.md`
- `Docs/ProductionDocumentationPlan.md`
- `Docs/en/contributing/decisions/0001-github-pages-markdown-publication.md`
- `Docs/en/contributing/decisions/0004-manifest-backed-site-navigation-search.md`

## Решение

### Текущая документация

1. Неверсионированный сайт является каналом документации `current`.
2. `current` следует за rolling-веткой `master` и обозначается как `Current`, а не `Stable`.
3. Текущие публичные URL остаются неверсионированными и стабильными, пока их содержимое следует за последней опубликованной ревизией `master`.
4. Ссылки на исходники используют тот же ref `master`, что и отображаемая документация.
5. Для исторической проверки используются commit-addressable artifacts `_site` из GitHub Actions и ревизии репозитория.

### Документация выпусков

1. Снимки выпусков с тегами остаются отложенными, пока у движка нет поддерживаемой серии тегов и support matrix.
2. Будущий канал выпусков обязан использовать неизменяемые Git tags и зарезервированное семейство путей `/versions/{version}/`.
3. Для публикации первого снимка выпуска требуются явная политика поддержки и последующий ADR о поддерживаемых линиях, retention, selectors, canonical URLs и banners неподдерживаемых версий.
4. Инструменты документации не должны выводить стабильный выпуск из `VERSION`, имени ветки или доступного Git tag.

### Владение локалями

1. Английский (`en`) является каноническим. Немигрированные плоские английские файлы остаются исходниками до перемещения проверенной группы; мигрированные страницы каноничны под `Docs/en`.
2. Русский (`ru`) представляет собой цельное зеркало документов, заполняемое проверенными группами. Авторитетным снимком покрытия служит `Docs/generated/translation-status.json`; ни один абзац ADR не владеет числом переводов, поддерживаемым вручную.
3. Для страницы для людей, перемещённой под `Docs/en`, русский путь получается заменой `Docs/en/` на `Docs/ru/`.
4. Точки входа README репозитория и подсистем используют объявленные в манифесте парные пути, например `README.md` и `README.ru.md`.
5. Английские и русские страницы связываются стабильными идентификаторами документов, а не переведёнными заголовками.
6. Актуальность перевода использует нормализованный SHA-256 hash канонического английского содержимого. Для существующих переводов проверяются актуальные hashes, идентичные fenced code, явные locale metadata и language-preserving links; полное покрытие становится обязательным при производственном двуязычном запуске.
7. `translation-pending` разрешён только до производственного двуязычного запуска.

### Стабильные маршруты и миграция

1. `Docs/documentation-manifest.json` владеет versioning, localization, текущими исходными путями, стабильными идентификаторами документов, dispositions миграции и планируемыми целями.
2. `BuildTools/docs_site.py` генерирует `Docs/generated/document-routes.json`.
3. Каталог маршрутов записывает для каждой публичной страницы текущий URL, будущего канонического владельца, планируемый английский URL, путь русского зеркала, состояние миграции и обязательный legacy redirect.
4. Будущий target, общий для нескольких legacy pages, обязан иметь ровно одного канонического владельца без disposition `replace`. Остальные записи являются aliases, перенаправляющими к этому владельцу.
5. Перемещение запрещено, пока старый маршрут не сохранён как долговечная pointer page Markdown. Это сохраняет чтение и в репозитории GitHub, и в GitHub Pages/Jekyll без зависимости от неподдерживаемого redirect plugin или зафиксированного HTML.
6. URL текущих source paths остаются каноническими до фактического перемещения. После него новые маршруты `Docs/en` и `Docs/ru` становятся каноническими, а старый файл становится принадлежащей манифесту pointer/redirect-записью. Немигрированные планируемые URL являются резервированием, а не заявлением о существующей странице.
7. Навигация, поиск по каждой локали, доставка для ИИ, статус локализации и отрендеренные элементы версии и языка используют одну политику манифеста. Они не могут поддерживать независимые объявления локали или версии.

## Последствия

### Положительные

- Каждый планируемый английский и русский путь известен до перемещения файлов.
- Текущая карта публичных URL проверяема и генерируется из стабильных идентификаторов.
- Legacy aliases могут сходиться к одной странице, не создавая двух канонических владельцев.
- Результаты для людей и ИИ честно обозначают `master` как rolling current revision.
- Будущие снимки выпусков имеют зарезервированную архитектуру без неподтверждённого обещания стабильности.
- Решение остаётся в пределах Markdown репозитория и GitHub Pages/Jekyll.

### Издержки

- Перемещение публичной страницы требует и нового канонического файла, и старого Markdown pointer route.
- README-style entry points требуют явных пар локалей в манифесте, поскольку находятся вне `Docs/en`.
- Locale-aware navigation, hashes, поиск по локалям, переключение языка и долговечные pointers доказаны проверенными группами миграции; повторение процесса для остальных обязательных страниц всё ещё требует значительной работы.
- Выбор версии выпуска остаётся недоступным до появления управления выпусками движка.

## Отклонённые варианты

- **Считать `master` стабильной:** отклонено, поскольку текущий рефакторинг и отсутствие support matrix не оправдывают такого обещания.
- **Уже сейчас копировать документацию для каждого tag:** отклонено, поскольку хранение не создаёт политику поддержки.
- **Сначала перемещать файлы, затем восстанавливать redirects:** отклонено, поскольку старые публичные URL были бы потеряны из источника истины.
- **Использовать переведённые заголовки как ключи локалей:** отклонено, поскольку заголовки изменяются и не являются машинно-стабильными.
- **Добавить отдельное приложение документации:** отклонено ADR-0001; каноническими остаются Markdown и GitHub Pages/Jekyll.
- **Требовать HTTP redirect plugin:** отклонено, поскольку долговечные Markdown pointer pages работают и в GitHub, и в Jekyll и сохраняют миграцию проверяемой.

## Проверка

- `python BuildTools/tests/test_docs_ai_delivery.py`
- `python BuildTools/tests/test_docs_site.py`
- `python BuildTools/tests/test_docs_site_layout.py`
- `python BuildTools/tests/test_docs_site_artifact.py`
- `python BuildTools/tests/test_docs_browser.py`
- `python BuildTools/tests/test_docs_validate.py`
- `python BuildTools/docs_site.py --check`
- `python BuildTools/docs_ai_delivery.py --check`
- `python BuildTools/docs_localization.py --check`
- `python BuildTools/docs_validate.py`

Тесты отклоняют drift версии или source ref, некорректную политику локалей, отсутствующие пары README, коллизии маршрутов, неоднозначные canonical targets, устаревшие данные маршрутов и drift layout.

## Связанные документы

- [ADR-0001](0001-github-pages-markdown-publication.md)
- [ADR-0003](0003-manifest-backed-ai-documentation-delivery.md)
- [ADR-0004](0004-manifest-backed-site-navigation-search.md)
- [Публикация сайта документации](../documentation/site-publication.md)
- [Сопровождение документации](../documentation/)
- [ProductionDocumentationPlan.md](https://github.com/cvet/fonline/blob/master/Docs/ProductionDocumentationPlan.md)
- [documentation-manifest.json](../../../documentation-manifest.json)
