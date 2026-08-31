---
layout: default
title: Базовый слой Essentials
locale: ru
document_id: native-essentials
permalink: /Docs/ru/reference/native/essentials.html
---

<!-- docs-translation: {"document_id":"native-essentials","locale":"ru","source_path":"Docs/en/reference/native/essentials.md","source_sha256":"6181ecce8047626d8406a62aeb4d8ed336bcf3c569800683a24d6552b6edb775"} -->

# Базовый слой Essentials

> Документация движка. Эта страница описывает низкоуровневый слой `Source/Essentials/`: требования к платформе и компилятору, вспомогательные средства жизненного цикла процесса, журналирование, память, строки, сериализацию, файловую систему, сокеты и базовые типы, используемые всеми вышележащими слоями движка.

## Назначение

Используйте эту страницу при изменении кода ниже `Source/Common/` и когда нужно определить, относится ли новая утилита к переиспользуемому фундаменту движка, а не к клиенту, серверу, инструментам или конкретной игре.

Контракт обработки исключений в модели памяти описан в разделе [Безопасность исключений](../../contributing/coding-contracts/exception-safety.md): `SafeAlloc` / `SafeAllocator` завершают процесс при нехватке памяти, поэтому `std::bad_alloc` не является восстанавливаемой ошибкой, а уровни `throw` / `FO_VERIFY_*` / `FO_STRONG_ASSERT` строятся поверх `ExceptionHandling.h`.

Слой Essentials должен сохранять минимум зависимостей. Большая часть движка подключает его через `Source/Essentials/Essentials.h`, поэтому изменение здесь способно затронуть каждое приложение.

## Решение между слоями

Essentials следует строгому dependency DAG: зависимость должна находиться раньше
в umbrella block, а обратную зависимость следует передать вверх через параметры
или более высокий владеющий слой. Регистрируйте новые implementation files в
`FO_ESSENTIALS_SOURCE`; владеющей целью является `EssentialsLib`, а consumers
должны линковаться в correct dependency point, не обходя слой.

Точный umbrella order: `BasicCore`, `GlobalData`, `StackTrace`, `BaseLogging`,
`FatalError`, `FunctionObjects`, `SmartPointers`, `MemorySystem`, `StringObject`,
`Containers`, `StringUtils`, `Platform`,
`ExceptionHandling`, `Threading`, `SafeArithmetics`, `DataSerialization`,
`HashedString`, `StrongType`, `TimeRelated`, `ExtendedTypes`, `Compressor`,
`WorkThread`, `Logging`, `DiskFileSystem`, `CommonHelpers` и `NetSockets`.
Не меняйте этот список местами для исправления cycle. Передавайте reverse
dependency pressure вверх через parameter или разделяйте ответственность в
более высоком слое-владельце. Каждый новый Essentials `.h` / `.cpp` должен
войти в проверяемый список `FO_ESSENTIALS_SOURCE`, из которого `CoreLibs.cmake`
создаёт `EssentialsLib`; inventory путей исходников сам по себе не является
build wiring.
В umbrella order `Essentials.h` участвуют только headers; никогда не добавляйте
туда `.cpp`. Регистрируйте и headers, и implementation files в
`FO_ESSENTIALS_SOURCE`, затем проверяйте, что ими владеет `EssentialsLib`, а его
consumers линкуются в correct dependency point.

Когда то же изменение затрагивает script-visible metadata, разделяйте владение.
Engine владеет reusable metadata/codegen machinery; embedding project
предоставляет project configuration, дополнительные metadata sources, common
headers и script/content inputs; generated files являются build artifacts.
Сравнивайте все восемнадцать canonical generated models и требуйте проверенную
точную domain-bound disposition для каждого gated compatibility break; unit
test Essentials не обходит contract-change gate.

## Проверенные исходные пути

- `Source/Essentials/Essentials.h`
- `Source/Essentials/Essentials.cpp`
- `Source/Essentials/BasicCore.h`
- `Source/Essentials/BasicCore.cpp`
- `Source/Essentials/GlobalData.h`
- `Source/Essentials/GlobalData.cpp`
- `Source/Essentials/StackTrace.h`
- `Source/Essentials/StackTrace.cpp`
- `Source/Essentials/BaseLogging.h`
- `Source/Essentials/BaseLogging.cpp`
- `Source/Essentials/FatalError.h`
- `Source/Essentials/FatalError.cpp`
- `Source/Essentials/FunctionObjects.h`
- `Source/Essentials/FunctionObjects.cpp`
- `Source/Essentials/SmartPointers.h`
- `Source/Essentials/SmartPointers.cpp`
- `Source/Essentials/MemorySystem.h`
- `Source/Essentials/MemorySystem.cpp`
- `Source/Essentials/StringObject.h`
- `Source/Essentials/StringObject.cpp`
- `Source/Essentials/Containers.h`
- `Source/Essentials/Containers.cpp`
- `ThirdParty/small_vector/README.md`
- `ThirdParty/small_vector/source/include/gch/small_vector.hpp`
- `Source/Essentials/StringUtils.h`
- `Source/Essentials/StringUtils.cpp`
- `Source/Essentials/Platform.h`
- `Source/Essentials/Platform.cpp`
- `Source/Essentials/ExceptionHandling.h`
- `Source/Essentials/ExceptionHandling.cpp`
- `Source/Essentials/Threading.h`
- `Source/Essentials/Threading.cpp`
- `Source/Essentials/SafeArithmetics.h`
- `Source/Essentials/SafeArithmetics.cpp`
- `Source/Essentials/DataSerialization.h`
- `Source/Essentials/DataSerialization.cpp`
- `Source/Essentials/HashedString.h`
- `Source/Essentials/HashedString.cpp`
- `Source/Essentials/StrongType.h`
- `Source/Essentials/StrongType.cpp`
- `Source/Essentials/TimeRelated.h`
- `Source/Essentials/TimeRelated.cpp`
- `Source/Essentials/ExtendedTypes.h`
- `Source/Essentials/ExtendedTypes.cpp`
- `Source/Essentials/Compressor.h`
- `Source/Essentials/Compressor.cpp`
- `Source/Essentials/WorkThread.h`
- `Source/Essentials/WorkThread.cpp`
- `Source/Essentials/Logging.h`
- `Source/Essentials/Logging.cpp`
- `Source/Essentials/DiskFileSystem.h`
- `Source/Essentials/DiskFileSystem.cpp`
- `Source/Essentials/CommonHelpers.h`
- `Source/Essentials/CommonHelpers.cpp`
- `Source/Essentials/NetSockets.h`
- `Source/Essentials/NetSockets.cpp`
- `Source/Essentials/UcsTables.inc`
- `Source/Essentials/WinApiUndef.inc`
- `BuildTools/natvis/essentials.natvis`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/tests/test_essentials_layering.py`
- связанные тесты в `Source/Tests/`

## Модель подключений и зависимостей

`Source/Essentials/Essentials.h` является общим umbrella-заголовком. Его точный include order одновременно задаёт dependency order фундаментального слоя:

`BasicCore` → `GlobalData` → `StackTrace` → `BaseLogging` → `FatalError` → `FunctionObjects` → `SmartPointers` → `MemorySystem` → `StringObject` → `Containers` → `StringUtils` → `Platform` → `ExceptionHandling` → `Threading` → `SafeArithmetics` → `DataSerialization` → `HashedString` → `StrongType` → `TimeRelated` → `ExtendedTypes` → `Compressor` → `WorkThread` → `Logging` → `DiskFileSystem` → `CommonHelpers` → `NetSockets`.

Этот список намеренно точный, а не тематический. `Essentials.h` задаёт строгий DAG зависимостей: каждый заголовок Essentials и соответствующий `.cpp` может подключать и вызывать только modules, расположенные в umbrella-блоке выше него. Объявление API в раннем header с определением в более позднем `.cpp` всё равно создаёт обратную link dependency. `BuildTools/tests/test_essentials_layering.py` проверяет прямые includes и ownership внешних namespace-level definitions. Не меняйте порядок ради сокрытия цикла; передайте данные параметром или разделите ответственность на правильной границе слоёв.

Новые API Essentials не должны зависеть от `Source/Common/`, `Source/Client/`, `Source/Server/`, `Source/Tools/` или заголовков встраиваемого проекта.

## Карта подсистем

### Граница платформы и компилятора

`BasicCore.h` проверяет выбранный макрос ОС (`FO_WINDOWS`, `FO_LINUX`, `FO_MAC`, `FO_ANDROID`, `FO_IOS` или `FO_WEB`) и требует C++20. Здесь же часто используемые стандартные типы вводятся в namespace движка и объявляются базовые макросы, включая `FO_EXPORT_FUNC`, `FO_KEEP_DATA_SYMBOL` и helpers для namespace. Средства подавления предупреждений также находятся здесь: `FO_DISABLE_WARNINGS_PUSH/POP` отключает все предупреждения при обёртывании third-party headers, а пары `FO_GCC_IGNORE_WARNINGS_PUSH/POP`, `FO_CLANG_IGNORE_WARNINGS_PUSH/POP` и `FO_MSVC_IGNORE_WARNINGS_PUSH/POP` подавляют одно именованное предупреждение только в соответствующем компиляторе. Это позволяет изолировать false positive одного toolchain, не заставляя остальные отвергать неизвестный номер `-W` или warning. Сначала исправляйте причину предупреждения; per-compiler helper допустим только для документированного false positive компилятора.

`Platform.h` / `.cpp` владеет небольшим набором host-specific helpers: информационным журналированием, именами потоков, поиском пути executable и пользовательского каталога данных, форматированием process id, fork там, где он доступен, использованием памяти процессом, CPU snapshots и загрузкой динамических модулей. `Platform::GetUserDataBase()` намеренно использует только окружение, без shell и SDL: Windows берёт `%LOCALAPPDATA%` с fallback на `%APPDATA%`, macOS/iOS использует `$HOME/Library/Application Support`, Linux/Android/прочие платформы используют `$XDG_DATA_HOME` с fallback на `$HOME/.local/share`. Вышележащий слой добавляет имя приложения и решает, является ли отсутствие пути фатальным. `Platform::GetCpuUsageSnapshot()` возвращает накопительные системные счётчики по ядрам и CPU time текущего процесса; вызывающий код сравнивает два snapshot для вычисления процентов и хранит sampling/cache state вне Platform. `Platform` находится выше `ExceptionHandling` и использует более ранний `FO_BASIC_STRONG_ASSERT` для terminating host-API invariants, не импортируя поздние exception macros. Platform-specific поведение приложения, окна и рендеринга находится в `Source/Frontend/`.

Windows builds сохраняют compile baseline `_WIN32_WINNT=0x0601`. Единый registry Windows build platforms владеет архитектурой CMake, toolset и канонической packaging-архитектурой обычных вариантов, `-clang` и `-win7`. Пара Win7 фиксирует MSVC 14.44, а `FO_BINARY_OUTPUT_POSTFIX` остаётся независимым от платформы. В package DSL конкретная запись `BINARY` может выбрать собственный postfix, например `BINARY Client Windows win32-win7 Raw+Zip+Wix POSTFIX Win7`, не затрагивая соседние binaries. Проверки совместимости находятся вне application targets.

### Диагностика и обработка сбоев

`BaseLogging.*` и `Logging.*` образуют фундамент журналирования. `WriteLogMessage()` объединяет последовательные дубликаты с одинаковыми `LogType` и текстом: повторения пропускаются, а перед следующей отличающейся строкой выводится сводка вида `...and 25 more same messages`. `LogToFile()` открывает файл без exclusive lock в рамках поведения платформы: `std::ofstream` MSVC использует deny-none, а POSIX не вводит обязательную блокировку при открытии. Благодаря этому два модуля движка в одном процессе, например runtime host EXE и загруженная им runtime DLL со своей копией engine global data, могут одновременно держать один файл открытым. Перед каждой записью `WriteSync` перемещается в конец файла, поэтому один handle не перезапишет данные, добавленные другим; параметр `append` по-прежнему выбирает truncate по умолчанию либо append при первоначальном открытии.

`WriteLog` / `WriteBaseLog` безопасно деградируют, если global data ещё не созданы: сначала переходят к base log, затем к `std::cout`.

`FatalError.*` является ранним native-only fatal layer. Он следует за `StackTrace` и `BaseLogging`, приостанавливает asynchronous writes, пишет одно синхронное сообщение с native trace и передаёт `BasicCore::ExitApp(false)` только механическое завершение. Ему принадлежат `ReportFatalAndExit`, `ReportStrongAssertAndExit` и `FO_BASIC_STRONG_ASSERT`; слой не создаёт exception objects и не зависит от более позднего `ExceptionHandling`. Сам `ExitApp(false)` остаётся status-only, поскольку его используют и контролируемые command failures, и fatal invariant failures.

`StackTrace.*` собирает и форматирует native/script stacks, а `ExceptionHandling.*` владеет более поздними helpers отчётов об exception objects. Debugger-сценарии описаны в разделе [Native- и AngelScript-отладка](../../troubleshooting/debugging.md).

<a id="memory-pointers-and-lifetime-utilities"></a>
### Память, указатели и время жизни

`MemorySystem.*` владеет резервными блоками памяти, отчётами о failed allocation и `SafeAllocator`. `SmartPointers.*` содержит wrappers, явно выражающие владение, nullability и назначение raw reference; словарь `ptr` / `nptr` и правила миграции приведены в разделе [Умные указатели](../../contributing/coding-contracts/smart-pointers.md). В этом слое должны находиться только общие средства владения; время жизни entity и holder semantics принадлежат [модели сущностей](../../explanation/entity-and-property-model/).

#### Словарь callable

`FunctionObjects.*` заменяет `std::function` двумя wrappers движка.
`function<Signature>` является alias move-only типа
`move_only_function<Signature>` и используется по умолчанию.
`copyable_function<Signature>` нужен только когда копирование stored target
действительно входит в ownership contract, например при snapshot callback для
нескольких owners. Оба хранят небольшой nothrow-movable target inline, а крупный
выделяют через fail-fast path, поэтому создание не вводит recoverable
`std::bad_alloc`. Если migration обнаружил копирование, сначала проверьте, не
должен ли owner выполнить move. Единственный оставшийся `std::function` — hook
script provider в `StackTrace.h`, расположенный до `FunctionObjects` в порядке
зависимостей.

#### Словарь выделения памяти

Код движка выделяет память только через две поверхности:

- **Псевдонимы контейнеров `fo`** из `Containers.h`: `string`, `wstring`, `vector`, `map`, `unordered_map`, `set`, `list`, `deque`, `stringstream`, `small_vector` и связанные типы. `string` и `wstring` используют engine `basic_string` из `StringObject.h`; остальные allocator-aware aliases используют `SafeAllocator`. Используйте их вместо вариантов из `std::`.
- **`SafeAlloc`**: `MakeUnique` / `MakeShared` / `MakeRefCounted` / `MakeRawArr` / `MakeUniqueArr` для типизированных объектов и raw-уровень `MallocRaw` / `CallocRaw` / `ReallocRaw` / `FreeRaw`, а также `MallocAlignedRaw` / `FreeAlignedRaw` для C ABI.

Raw-уровень нужен из-за C-образных allocator hooks third-party библиотек: они требуют `realloc`, нетипизированный блок байтов или оба варианта, что невозможно выразить C++ allocator. Он сохраняет ту же политику нехватки памяти, что и `SafeAllocator`: сообщить об ошибке, освободить резервный пул, повторить попытку и детерминированно завершить процесс. Поэтому подключение библиотеки через этот путь не выводит её из общего контракта. Запрос нулевого размера передаётся нижнему allocator, а не трактуется как ошибка.

Примитивы `rpmalloc` намеренно не экспортируются из `MemorySystem.h`. Они возвращают null при сбое и создали бы вторую доступную точку входа, обходящую контракт, поэтому остаются file-local statics в `MemorySystem.cpp`. Операции над блоками `MemCopy` / `MemMove` / `MemFill` / `MemCompare` / `MemReadUnaligned` / `MemWriteUnaligned` не выделяют память и остаются публичными.

Vendored rpmalloc сохраняет upstream spans размером 256 MiB на 64-bit targets.
На 32-bit targets один span уменьшен до `LARGE_PAGE_SIZE` (16 MiB). Старые
Windows allocation APIs резервируют `size + alignment`, поэтому aligned span
256 MiB может потребовать contiguous hole размером 512 MiB в 2 GiB x86 address
space и сорвать уже первое небольшое allocation. Span 16 MiB на x86 сохраняет
все встроенные page classes и устраняет startup-зависимость от одной огромной
непрерывной reservation.

При обходе этого словаря возникают три разных последствия, и их тяжесть различается:

| | Фактическое поведение |
|---|---|
| **Отдельная куча** | Глобальные `operator new` / `delete` заменены на rpmalloc, поэтому любой `new` и `std::allocator` уже попадает в engine heap. Но rpmalloc собирается с `ENABLE_OVERRIDE=0`, C `malloc` / `free` не перехватываются, и использующие их библиотеки остаются в CRT heap, вне rpmalloc, статистики `AllocatorGetInUseBytes()` и Tracy allocation tracking. |
| **Неверная политика нехватки памяти** | `std::allocator` бросает `std::bad_alloc` вместо terminate-on-OOM модели из раздела [Безопасность исключений](../../contributing/coding-contracts/exception-safety.md), пункт 1. |
| **Выравнивание** | `SafeAllocator` направляет over-aligned element types через aligned-перегрузки `operator new` / `delete`. Проверка over-alignment должна оставаться member-функцией: `alignof(T)` требует полного `T`, но allocator обязан работать с неполным типом, поскольку `std::vector<T>` может быть объявлен до определения `T`. |

Известные допустимые ограничения: `std::future` / `std::promise` / `std::packaged_task`, `std::thread`, `std::filesystem::path` и файловые streams не принимают allocator. Они попадают в engine heap через global `new`, но бросают исключение при исчерпании памяти. Единственный `std::function` в `StackTrace.h` также расположен до callable module движка. Отдельно `BasicCore`, `StackTrace` и `BaseLogging` расположены до `MemorySystem` в порядке `Essentials.h` и поэтому намеренно используют контейнеры `std::`: `MemorySystem.cpp` вызывает `GetStackTrace()` из `ReportBadAlloc`, и reporting path не должен зависеть от allocator, который только что отказал.

<a id="third-party-allocators"></a>
#### Allocators внешних библиотек

| Библиотека | Направляется в | Место |
|---|---|---|
| ImGui | `SafeAllocator` | `Common/ImGuiExt/ImGuiStuff.cpp` |
| AngelScript | `SafeAllocator` | `Scripting/AngelScript/AngelScriptScripting.cpp` |
| zlib | `SafeAllocator` | `Essentials/Compressor.cpp` |
| ozz-animation | aligned-уровень `SafeAlloc` | `Common/ModelAnimationData.cpp` |
| meshoptimizer | `SafeAllocator` | `Tools/ModelMeshBaker.cpp` |
| ufbx | `SafeAllocator` | compile-time `UFBX_EXTERNAL_MALLOC` и `extern "C" ufbx_malloc/realloc/free` в `Tools/ModelMeshBaker.cpp` |
| SDL | `SafeAlloc::*Raw` | `Frontend/Application.cpp` |
| Effekseer | `SafeAlloc::*Raw` + aligned | `Client/EffekseerExtension.cpp`, объявление в его header; оба владельца, client runtime и `Tools/ParticleBaker.cpp`, устанавливают callbacks через одно определение |
| libpng | `SafeAlloc::*Raw` | `Tools/ImageBaker.cpp` через `png_create_read_struct_2` |
| libbson / mongo-c | `SafeAlloc::*Raw` + aligned | общий `Server/DataBase.cpp`; каждая BSON-backed factory для JSON, SQLite и Mongo устанавливает process-global vtable до создания backend |
| SQLite | `SafeAlloc::*Raw` | `Server/DataBase-SQLite.cpp` через `sqlite3_config(SQLITE_CONFIG_MALLOC)` до `sqlite3_initialize()` |

Форму bson vtable нужно изучить до её копирования в другую интеграцию. Она предоставляет `aligned_alloc`, но освобождает полученные блоки через обычный member `free`, не запоминая alignment. Это корректно, только пока оба пути используют одну release-функцию. В rpmalloc это так: `rpaligned_alloc` и `rpmalloc` завершаются в `rpfree`. То же верно на POSIX без rpmalloc, где блоки `posix_memalign` по определению освобождаются через `free()`. Ломается только Windows без rpmalloc, то есть sanitizer configurations, в которых `expr_RpmallocEnabled` отключает allocator ради interposition sanitizer: aligned-путь там использует `_aligned_malloc` / `_aligned_free`.

Поэтому `BsonAlignedAlloc` ровно в этом случае переходит к обычному `SafeAlloc::MallocRaw`. Так делает и default vtable bson под MSVC по той же явно указанной причине: `_aligned_alloc_impl` в libbson `memory.c` намеренно не вызывает `_aligned_malloc`. Все aligned-запросы mongoc используют `BSON_ALIGNOF` обычной C-структуры, для которой fundamental alignment `malloc` достаточен. Vtable является process-global, поэтому каждая BSON-backed factory устанавливает одинаковые callbacks до того, как backend сможет выделить память; поздняя замена могла бы сопоставить старый allocation новому free callback. Удаление `aligned_alloc` из vtable не является решением: bson подставит внутренний fallback, отбрасывающий требуемое alignment на всех платформах, а не только на проблемной.

Hook SQLite требует callback `xSize` и передаёт функциям free/realloc/size только указатель, поэтому каждый блок несёт 8-байтовый заголовок размера. Конфигурация должна быть установлена до `sqlite3_initialize`, из-за чего библиотека собирается с `SQLITE_OMIT_AUTOINIT`, а каждый вызывающий код проходит через один экспортированный initializer.

Не подключены по документированным причинам: **LibreSSL** экспортирует `CRYPTO_set_mem_functions`, но его реализация представляет собой неработающий `return 0;`, поскольку custom allocators были удалены upstream. Вызов создавал бы ложное впечатление покрытия. **ogg / vorbis / theora** не предоставляют allocator hook.

При добавлении или обновлении vendored-библиотеки проверьте наличие allocator hook, подключите его либо запишите причину отказа. Читайте реализацию hook, а не только declaration: несколько интеграций в этой таблице первоначально были неверно поняты по call site или имени symbol.

<a id="vector-containers-and-inline-storage"></a>
#### Векторные контейнеры и inline storage

`Containers.h` предоставляет два sequence aliases с `SafeAllocator<T>`:

- `vector<T>` является обычной динамически выделяемой последовательностью и остаётся default для неограниченных данных, persistent collections с амортизируемым allocation, move-heavy pipelines и точных интерфейсов движка.
- `small_vector<T, InlineCapacity>` хранит до `InlineCapacity` элементов внутри объекта и переходит на storage с `SafeAllocator` при превышении лимита. Alias движка требует явную capacity; `GCH_SMALL_VECTOR_DEFAULT_SIZE` настраивает vendored implementation, но не задаёт политику выбора capacity.

Используйте `small_vector` только тогда, когда измерения или жёсткий protocol limit доказывают, что часто создаваемая коллекция обычно мала. Выбирайте capacity по наблюдаемой типичной cardinality, сохраняйте корректность редких больших случаев через heap spill и учитывайте inline bytes в каждом экземпляре. Scratch list на один вызов может быть хорошим кандидатом; несколько inline buffers в каждой ячейке плотной карты способны потребить больше памяти, чем сэкономит отсутствие первой allocation. Метод `inlined()` показывает текущий storage mode и полезен в focused tests и profiling instrumentation.

У представления есть несколько важных последствий для корректности:

1. Перемещение inline `small_vector` переносит элементы во внутренний buffer destination-объекта. Указатели, references и iterators в source не следуют за перемещением так, как это обычно происходит при передаче heap allocation обычным `vector`. Проверьте каждый адрес, живущий дольше move.
2. Inline moves и swaps выполняют операции над элементами и являются `noexcept` только условно. Заново определите гарантию exception safety затронутой функции, не наследуйте предположения от `vector`.
3. Member `small_vector<T, N>` инстанцирует уничтожение inline elements на границе содержащего class. Тип `T` должен быть полным в этой точке; это не drop-in замена member `vector`, элемент которого только forward-declared.
4. В vendored implementation member с вложенным element type, имеющим default member initializers, способен сделать default-inserting операции ill-formed, пока внешний class неполон, особенно под Clang. Для сокращения используйте `erase(begin() + new_size, end())`; в остальных случаях вынесите element type из внешнего class либо явно задайте требования к конструированию.
5. Heap spill сохраняет terminate-on-OOM policy движка, потому что alias использует `SafeAllocator`. Конструирование, преобразование и move элементов всё ещё могут бросать исключения; см. [Безопасность исключений](../../contributing/coding-contracts/exception-safety.md).

Не заменяйте `vector` на `small_vector` через границу точного типа только потому, что набор операций выглядит одинаковым:

- script export/codegen signatures и регистрация `ScriptSystem` используют точные написания и type identities `vector<T>` / `readonly_vector<T>`;
- property writes, serialized backing stores, `DataReader` / `DataWriter`, `NetBuffer` и `CScriptArray` на отдельных границах имеют точный контракт `vector`;
- внутренний helper, принимающий span, может обслуживать оба представления без раскрытия concrete container type, и это предпочтительная граница, когда допустимы оба;
- `FO_ENTITY_PROPERTY` не может непосредственно принять `small_vector<T, N>`, потому что запятая одновременно разделяет аргументы макроса.

`vector_collection` допускает оба aliases движка для generic readers. Производящие helpers `vec_filter`, `vec_transform` и `vec_sorted` сохраняют различие `vector` / `small_vector` и inline capacity через `rebind_vector_t`; диапазоны других видов материализуются как `vector`. `to_vector` намеренно всегда создаёт `vector`, а `copy_hold_ref` предоставляет непрозрачный ref-held snapshot вместо concrete sequence contract. Generic formatter принимает оба aliases для обычных числовых элементов, но специальные случаи строк и bool сейчас совпадают только с точными типами `vector<string>` и `vector<bool>`. Не предполагайте такое же форматирование `small_vector<string, N>` или `small_vector<bool, N>` без расширения и тестирования formatter.

При внедрении запишите измеренное распределение и число объектов, проверьте lifetime адресов и места move/swap, подтвердите complete-type и exact-interface constraints, добавьте focused coverage для inline operation, spill и result type generic helpers. Затем запустите полный набор native unit tests, проектные audits exception safety и smart pointers, если они существуют, а также репрезентативные bake, gameplay и profiling paths изменённой подсистемы.

### Сериализация, значения, строки и хеши

`StringObject.*` владеет реализацией engine `basic_string`. API следует
`std::basic_string`, а `FO_STRING_INLINE_CAPACITY` выбирает compiled small-string
buffer для `string` и `wstring`; это build-wide ABI choice, а не per-call
optimization. На трёх границах standard library нужны явные adapters: текст для
standard stream копируется через `make_stream_string`,
`std::filesystem::path` строится через `fs_make_path`, а `getline` вызывается
без квалификатора, чтобы ADL выбрал overload движка. Рост строки следует тому же
детерминированному terminate-on-OOM contract, что и остальные engine storage.

`DataSerialization.*` содержит binary read/write helpers, используемые сетью, persistence, ресурсами и тестами. `DataReader::Read<T>()` и `DataWriter::Write<T>()` копируют standard-layout values через byte copies, поэтому serialized streams не зависят от выравнивания buffer. Zero-copy overload `ReadPtr<T>(size)` предназначен только для raw byte/string views (`uint8_t`, `char` или `void`); типизированные значения, которым нужно alignment, должны использовать `Read<T>()` или `ReadPtr(destination, size)`.

`StringUtils.*`, `HashedString.*`, `StrongType.*`, `ExtendedTypes.*`, `SafeArithmetics.*` и `TimeRelated.*` предоставляют небольшие переиспользуемые значения, которые вышележащие слои считают примитивами. `iround` отвергает non-finite и выходящие за диапазон int64 floating-point values до округления, чтобы ни одно значение с неопределённым для `std::llround` поведением не достигло функции. `HashStorage::SetResolveHashFailureHandler` позволяет вышележащему слою наблюдать неудачное разрешение hash как в throwing, так и во flagged no-throw lookup path, не обучая Essentials конкретной recovery policy.

### Файловая система, сжатие, сокеты и рабочие потоки

`DiskFileSystem.*` является низкоуровневой абстракцией диска. Небольшой policy helper `fs_make_writable_path(user_writable_path, relative)` используется вышележащими слоями для writable overlay установленного клиента: пустой root или absolute input возвращает input без изменений, а relative path помещается под writable root. Вышележащее смонтированное представление ресурсов находится в `Source/Common/FileSystem.*` и описано в разделе [Конфигурация и источники данных](../settings/configuration-and-data-sources.md). `Compressor.*` владеет generic compression round trips, `NetSockets.*` содержит raw socket helpers ниже высокоуровневой модели network commands/connections из раздела [Сеть](../../explanation/authority-and-networking/), а `WorkThread.*` предоставляет простую инфраструктуру фоновых workers.

Когда job `WorkThread` бросает исключение, поток сначала вызывает свой local exception handler, чтобы обновить worker-owned policy, например очистить очередь jobs. Затем original exception передаётся global non-fatal exception reporter уже вне worker lock.

## Интеграция сборки

`BuildTools/cmake/stages/EngineSources.cmake` перечисляет в `FO_ESSENTIALS_SOURCE` каждую authored пару `.h` / `.cpp` из Essentials, два файла `.inc` и debugger visualization. Затем `BuildTools/cmake/stages/CoreLibs.cmake` создаёт из этого списка `EssentialsLib`. Библиотека входит в core dependency chain приложений, tools, tests и consumers generated code. При добавлении файла Essentials поместите его в правильную точку зависимостей `Essentials.h`, включите в `FO_ESSENTIALS_SOURCE` и добавьте focused coverage, где это возможно.

## Какие тесты проверять

Прямое покрытие слоя Essentials находится в следующих тестах:

- `Source/Tests/Test_BaseLogging.cpp`
- `Source/Tests/Test_BasicCore.cpp`
- `Source/Tests/Test_CommonHelpers.cpp`
- `Source/Tests/Test_Compressor.cpp`
- `Source/Tests/Test_Containers.cpp`
- `Source/Tests/Test_DataSerialization.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_ExceptionHandling.cpp`
- `Source/Tests/Test_ExtendedTypes.cpp`
- `Source/Tests/Test_FunctionObjects.cpp`
- `Source/Tests/Test_GenericUtils.cpp`
- `Source/Tests/Test_GlobalData.cpp`
- `Source/Tests/Test_HashedString.cpp`
- `Source/Tests/Test_Logging.cpp`
- `Source/Tests/Test_MemorySystem.cpp`
- `Source/Tests/Test_NetSockets.cpp`
- `Source/Tests/Test_Platform.cpp`
- `Source/Tests/Test_SafeArithmetics.cpp`
- `Source/Tests/Test_SmartPointers.cpp`
- `Source/Tests/Test_StackTrace.cpp`
- `Source/Tests/Test_StringObject.cpp`
- `Source/Tests/Test_StringUtils.cpp`
- `Source/Tests/Test_StrongType.cpp`
- `Source/Tests/Test_TimeRelated.cpp`
- `Source/Tests/Test_WorkThread.cpp`

`Test_Containers.cpp` фиксирует alias движка, allocator, переход inline-to-heap, move, swap и форматирование. `Test_CommonHelpers.cpp` фиксирует сохранение вида контейнера через `rebind_vector_t` и производящие helpers `vec_*`. При изменении vector aliases или generic sequence helpers поддерживайте оба focused suites в актуальном состоянии.

Полная карта suites и target wiring находится в разделе [Тестирование](../../contributing/testing/).

## Маршрутизация изменений

- Ограничения компилятора/ОС, namespace, базовые aliases и низкоуровневые макросы: `Source/Essentials/BasicCore.*`.
- Регистрация global create/delete callbacks: `Source/Essentials/GlobalData.*`.
- Stack traces, журналирование и отчёты об исключениях: `Source/Essentials/StackTrace.*`, `BaseLogging.*`, `Logging.*`, `ExceptionHandling.*` и [Native- и AngelScript-отладка](../../troubleshooting/debugging.md).
- Общие средства памяти и указателей: `Source/Essentials/MemorySystem.*`, `SmartPointers.*` и [Умные указатели](../../contributing/coding-contracts/smart-pointers.md).
- Владение callable и inline targets: `Source/Essentials/FunctionObjects.*`.
- Строки движка и build-wide inline-capacity contract: `Source/Essentials/StringObject.*`; aliases и stream interop: `Containers.h`.
- Байты файлов и низкоуровневая сборка writable path на диске: `Source/Essentials/DiskFileSystem.*`; смонтированные ресурсы движка и overlays установленного клиента: [Конфигурация и источники данных](../settings/configuration-and-data-sources.md).
- Socket primitives: `Source/Essentials/NetSockets.*`; protocol, command и network runtime: [Сеть](../../explanation/authority-and-networking/).

## Контрольный список проверки

1. Убедитесь, что изменение не вводит зависимость Essentials от вышележащего слоя движка.
2. При добавлении или удалении файлов Essentials обновите `BuildTools/cmake/stages/EngineSources.cmake`.
3. Запустите минимальный подходящий Essentials test, затем более широкий target `RunUnitTests`, если изменение пересекает границы утилит.
4. Для диагностики также проверьте актуальность раздела [Native- и AngelScript-отладка](../../troubleshooting/debugging.md).
5. Для файловой системы, сокетов или threading проверьте хотя бы одного вышележащего consumer, если низкоуровневый контракт изменился.
6. При внедрении `small_vector` подтвердите capacity и object-count измерениями, проверьте move/address lifetime и exact-type boundaries, затем повторно запустите gates exception safety и pointer ownership.
