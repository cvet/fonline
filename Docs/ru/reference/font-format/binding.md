---
title: Контракт привязки шрифтов
document_id: generated-font-format-binding
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-font-format-binding","locale":"ru","source_path":"Docs/en/reference/font-format/binding.md","source_sha256":"579c41bf7ee289690c7faf20af6034079ac9a482a558c5cc6ed62ba5cb1b8227"} -->

# Контракт привязки шрифтов

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/FontFormatInterface.json`, затем выполните `python BuildTools/docs_font_format.py --write`.

[Индекс](index.md) | [Форматы](formats.md) | [FOFNT](fofnt.md) | [BMFont](bmfont.md) | [Привязка](binding.md) | [Компоновка](layout.md) | [Отрисовка](rendering.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/font-format.json) | [Руководство](../../how-to/content/font-format.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-font-format-binding-extension-dispatch-e6553c421b"></a><code>font-format.binding.extension-dispatch</code> | Точная диспетчеризация расширения | Вызывайте Game.BindFont с точным путём в нижнем регистре с суффиксом .fofnt или .fnt; любой другой суффикс создаёт скриптовое исключение. | Диспетчеризация использует регистрозависимые проверки ends_with и не определяет формат по содержимому. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-font-format-binding-raw-copy-2eb2c5df1d"></a><code>font-format.binding.raw-copy</code> | Граница raw-copy для дескриптора | Оставьте fofnt и fnt в Baking.RawCopyFileExtensions, чтобы байты дескриптора без изменений попали в запечённые ресурсы. | Отдельного baker дескрипторов шрифтов нет; RawCopyBaker сохраняет путь и байты. | [Source/Common/Settings.inc](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.inc), [Source/Tools/RawCopyBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/RawCopyBaker.cpp) |
| <a id="entry-font-format-binding-image-resource-6b1fe7695b"></a><code>font-format.binding.image-resource</code> | Отдельно запечённое изображение | Поставляйте связанное изображение как независимо поддерживаемый ресурс в том же пакете и сохраняйте его относительный путь. | Дескриптор копируется raw-copy, а изображение загружается через SpriteManager и обычный конвейер image-format. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-binding-font-slots-3d6d926638"></a><code>font-format.binding.font-slots</code> | Расширяемые проектом слоты | Привяжите каждый слот FontType до измерения или отрисовки; Engine называет только Default = 0, а встраивающие скрипты могут расширить enum через аннотации codegen. | FontType является индексированной таблицей загруженных шрифтов; незагруженные и выходящие за границы слоты создают исключение. | [Source/Client/FontManager.h](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.h) |
| <a id="entry-font-format-binding-iface-atlas-c418ad6baf"></a><code>font-format.binding.iface-atlas</code> | Атлас спрайтов интерфейса | Привязанные скриптом шрифты размещают обычные и необязательные обведённые текстуры в AtlasType::IfaceSprites. | Обе ветви дескрипторов Game.BindFont передают один тип атласа интерфейса. | [Source/Scripting/ClientGlobalScriptMethods.cpp](https://github.com/cvet/fonline/blob/master/Source/Scripting/ClientGlobalScriptMethods.cpp) |
| <a id="entry-font-format-binding-bind-time-scale-d2935e9b3d"></a><code>font-format.binding.bind-time-scale</code> | Уменьшение при привязке | Передавайте конечный defaultScale в (0, 1]; используйте больший исходный растр и уменьшайте его вместо runtime-увеличения. | При привязке пиксели усредняются по площади, а метрики округляются один раз; значения выше единицы, неположительные и неконечные отклоняются. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-binding-rebind-4b433dbb8a"></a><code>font-format.binding.rebind</code> | Замена слота и сброс кэша | Считайте повторную привязку того же слота заменой; все кэшированные компоновки удаляются до использования перестроенного шрифта. | StoreFont заменяет необязательную запись таблицы, перестраивает атлас и очищает кэш форматов. | [Source/Client/FontManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/FontManager.cpp) |
| <a id="entry-font-format-binding-updater-default-2eff9266bd"></a><code>font-format.binding.updater-default</code> | Запасной шрифт обновлятора | Сохраняйте Fonts/Default.fofnt доступным для встроенного обновлятора, если хост намеренно не заменяет этот контракт ресурса. | Обновлятор привязывает слот Default из точного пути и пропускает уже загруженный слот. | [Source/Client/Updater.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Updater.cpp) |
