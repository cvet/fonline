---
title: Проверка формата текста
document_id: generated-text-format-validation
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-text-format-validation","locale":"ru","source_path":"Docs/en/reference/text-format/validation.md","source_sha256":"1f6b951212a6495dfb96ef5a46904f82440baa56dcba9fc7188e6bad75a05da5"} -->

# Проверка формата текста

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/TextFormatInterface.json`, затем выполните `python BuildTools/docs_text_format.py --write`.

[Индекс](index.md) | [Синтаксис](syntax.md) | [Языки](languages.md) | [Текст прототипов](proto-text.md) | [Runtime](runtime.md) | [Проверка](validation.md) | [Канонический JSON](../../../generated/text-format.json) | [Руководство](../../how-to/content/text-and-localization.md)

| Стабильный ID | Правило | Требование | Причина | Источник |
| --- | --- | --- | --- | --- |
| <a id="entry-text-format-validation-malformed-raw-d5a8fc093b"></a><code>text-format.validation.malformed-raw</code> | Некорректный исходный текст | Исходник без обязательного поля или с незавершённым значением приводит к ошибке text bake, даже если остальные записи разобрались успешно. | LoadFromString сообщает совокупную ошибку, а TextBaker отклоняет исходный файл целиком. | [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp), [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp), [Source/Tests/Test_TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_TextPack.cpp) |
| <a id="entry-text-format-validation-empty-languages-e0d19568f1"></a><code>text-format.validation.empty-languages</code> | Пустой BakeLanguages | TextBaker, ProtoTextBaker и нормализация TextPack отклоняют пустой список Baking.BakeLanguages. | Невозможно детерминированно выбрать базовый язык. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp), [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp), [Source/Common/TextPack.cpp](https://github.com/cvet/fonline/blob/master/Source/Common/TextPack.cpp) |
| <a id="entry-text-format-validation-raw-unsupported-warning-869bf64e2b"></a><code>text-format.validation.raw-unsupported-warning</code> | Предупреждение о неподдерживаемом исходном языке | Файлы неподдерживаемого исходного языка создают предупреждение и не создают выход для этого языка. | Проекты могут хранить несвязанные исходники в более широком дереве входов, не публикуя их молча. | [Source/Tools/TextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/TextBaker.cpp) |
| <a id="entry-text-format-validation-proto-token-count-4cb7b434c3"></a><code>text-format.validation.proto-token-count</code> | Число токенов ключа прототипа | Ключ $Text прототипа, содержащий после $Text больше Language, Key2 и Key3, приводит к ошибке запекания. | У TextPackKey нет дополнительного поля ключа. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp), [Source/Tests/Test_ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ProtoTextBaker.cpp) |
| <a id="entry-text-format-validation-proto-intersection-be92726636"></a><code>text-format.validation.proto-intersection</code> | Пересечение выходов прототипов | Повторяющиеся полные ключи, созданные исходниками разных типов прототипов в одном выходном пакете, приводят к ошибке запекания. | Сгенерированный пакет не должен зависеть от порядка обхода каталогов типов. | [Source/Tools/ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/ProtoTextBaker.cpp), [Source/Tests/Test_ProtoTextBaker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tests/Test_ProtoTextBaker.cpp) |

## Команды проверки

```powershell
python BuildTools\docs_text_format.py --check
python -m unittest BuildTools.tests.test_docs_text_format
cmake --build <build-dir> --config RelWithDebInfo --target RunUnitTests
```

В подключаемом проекте завершите проверку запеканием ресурсов, проектными проверками локализации и видимой проверкой переключения языка и форматированного текста в клиенте.
