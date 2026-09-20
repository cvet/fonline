---
title: Правила нативного биндинга
document_id: generated-native-extension-bindings
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-native-extension-bindings","locale":"ru","source_path":"Docs/en/reference/native-extension/bindings.md","source_sha256":"861ac5093aa606bca0eeea4add7cc3b024c5383cd0deba3e509f35c2f1e6e1bc"} -->

# Правила нативного биндинга

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/NativeExtensionInterface.json`, затем выполните `python BuildTools/docs_native_extension.py --write`.

[Обзор](index.md) | [Роли](roles.md) | [Хуки](hooks.md) | [Биндинги](bindings.md) | [Канонический JSON](../../../generated/native-extension.json) | [Руководство](../../how-to/native-extensions.md)

Эти правила образуют переиспользуемую границу. Настройка проектных зависимостей, нативное состояние, настройки и упаковка остаются во владении проекта.

| Стабильный ID | Правило | Требование | Причина |
| --- | --- | --- | --- |
| <a id="entry-native-extension-binding-registration-order-29d16ead8d"></a><code>native-extension.binding.registration-order</code> | Порядок регистрации | Вызывайте AddEngineSources после AddThirdPartyLibraries и до точки входа стадии RegisterEngineSources. | Проектные файлы должны попасть в списки исходников ролей и FO_SOURCE_META_FILES до настройки кодогенерации и базовых библиотек. |
| <a id="entry-native-extension-binding-namespace-6dffbfe254"></a><code>native-extension.binding.namespace</code> | Пространство имён движка | Объявляйте экспорты метаданных внутри FO_BEGIN_NAMESPACE/FO_END_NAMESPACE, а определения квалифицируйте через FO_NAMESPACE. | Один исходник должен компилироваться как с включённым, так и с выключенным настроенным пространством имён движка. |
| <a id="entry-native-extension-binding-script-export-49a22373ae"></a><code>native-extension.binding.script-export</code> | Граница экспорта в скрипты | Используйте FO_SCRIPT_API с поддерживаемым тегом метаданных ///@; не добавляйте макросы входа трассировки стека в экспортируемые тела. | Кодогенерация разбирает объявление и создаёт границу регистрации между нативным кодом и скриптами. |
| <a id="entry-native-extension-binding-pointer-contract-e9dcb3730c"></a><code>native-extension.binding.pointer-contract</code> | Контракт указателей | Используйте ptr&lt;T&gt;/nptr&lt;T&gt; для заимствований дескрипторов движка, а для владения — словарь владеющих указателей движка. | Кодогенерация отклоняет сырые указатели на дескрипторы, а nullable-семантика должна совпадать в нативных и скриптовых объявлениях. |
| <a id="entry-native-extension-binding-compatibility-54a8201a60"></a><code>native-extension.binding.compatibility</code> | Совместимость | Пересобирайте и перезапекайте каждое проектное изменение нативных метаданных для закреплённой ревизии движка. | Зарегистрированные метаданные и наличие большинства хуков входят в сгенерированное состояние совместимости; бинарная совместимость независимых ревизий не обещается. |
| <a id="entry-native-extension-binding-dependencies-503563d0e1"></a><code>native-extension.binding.dependencies</code> | Зависимости | Объявляйте проектные библиотеки, пути включения, определения компилятора, платформенные условия и содержимое пакета во встраивающем проекте. | AddEngineSources отвечает только за маршрутизацию исходников; движок не выводит автоматически правила проектных зависимостей и распространения. |

## Минимальный экспортируемый метод

```cpp
#include "Common.h"
#include "Server.h"

FO_USING_NAMESPACE();
FO_BEGIN_NAMESPACE
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_ProjectValue(ptr<ServerEngine> server);
FO_END_NAMESPACE

int32_t FO_NAMESPACE Server_Game_ProjectValue(ptr<ServerEngine> server)
{
    ignore_unused(server);
    return 1;
}
```

Экспортируемое тело намеренно не содержит макроса входа трассировки стека. Обычные неэкспортируемые нативные функции сохраняют стандартное соглашение движка о трассировке стека.
