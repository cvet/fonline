---
layout: default
title: Безопасность исключений и устойчивость инвариантов движка
locale: ru
document_id: exception-safety
permalink: /Docs/ru/contributing/coding-contracts/exception-safety.html
---

# Безопасность исключений и устойчивость инвариантов движка

<!-- docs-translation: {"document_id":"exception-safety","locale":"ru","source_path":"Docs/en/contributing/coding-contracts/exception-safety.md","source_sha256":"0cc37bd85ec70a8d44c5b742ff316d7d57096899d8b265820cdc6426fa6d6ee7"} -->

Этот документ объясняет, как движок сохраняет согласованное состояние при
исключениях. Главное требование: исключение посреди составного изменения
состояния (создание, регистрация, уничтожение и инвалидирование сущности,
межсущностные связи, персистентность) не должно оставлять работающий процесс в
частично измененном состоянии, которое исправляется только перезапуском.

## 1. Нехватка памяти не является восстанавливаемой ошибкой

Пути выделения памяти движка **завершают процесс при исчерпании памяти, а не
бросают исключение**:

- `SafeAlloc::MakeRefCounted` / `MakeRaw` / `MakeUnique` / `MakeRawArr`
  (`Source/Essentials/MemorySystem.h`) используют `nothrow new`, освобождают
  фиксированный резерв, повторяют попытку и вызывают `ReportAndExit`. Так
  создаются все сущности (`Item`, `Critter`, `Map`, `Location`,
  `CustomEntity` и другие).
- `SafeAllocator<T>` обслуживает все контейнерные псевдонимы движка из
  `Source/Essentials/Containers.h`: `vector`, `small_vector`, хешированные и
  упорядоченные наборы и словари, `list`, `deque`, `string` и потоки строк.
  Рост, вставка, резервирование и рехеширование не бросают `std::bad_alloc`:
  процесс детерминированно завершается в точке неудачного выделения.
- Кодек анимации моделей устанавливает в `ModelAnimationData.cpp` адаптер Ozz
  на основе `SafeAllocator<uint8_t>` до создания объектов Ozz. Каждый
  статически связанный модуль устанавливает собственный адаптер, а vendored
  исходники Ozz остаются идентичны закрепленной upstream-версии.
- `SafeAlloc::MallocRaw` / `CallocRaw` / `ReallocRaw` / `FreeRaw` и
  выровненные варианты применяют ту же последовательность report, резерв,
  retry, `ReportAndExit` к C-совместимому выделению. Через них подключены SDL,
  Effekseer, spine-cpp, libpng и curl; низкоуровневые rpmalloc-примитивы
  остаются локальными для `MemorySystem.cpp`.
- `ModelMeshBaker` один раз, до параллельных заданий, передает приватной
  meshoptimizer callbacks на `SafeAllocator<uint8_t>`. Эта зависимость
  используется только baker-ом и не входит в runtime-читатели моделей.

**Следствие: не пишите откат ради возможного выделения памяти.** Если
единственная причина отказа операции состоит в выделении через словарь
движка, операция либо завершается, либо процесс останавливается в точке
отказа. Не добавляйте `scope_exit`/`scope_fail` только из-за гипотетического
роста контейнера, строки или счетчика ссылок между двумя изменениями.

Глобальный бросающий `operator new` по-прежнему доступен для `new T` и
`std::allocator`. Код движка должен предпочитать `SafeAlloc` и контейнеры
движка, если ему нужен контракт terminate-on-OOM.

Этот контракт не делает любую операцию контейнера `noexcept`: конструктор
элемента, преобразование, сравнение, move и swap все еще могут бросать.
Особенно важно заново вывести гарантию при замене `vector` на `small_vector`,
поскольку inline-перемещение меняет адреса. Правила выбора и границы точных
типов принадлежат [Essentials.md](../../reference/native/essentials.md#vector-containers-and-inline-storage).

`std::bad_alloc` остается достижим за пределами словаря памяти движка:
`std::function` за пределами малого буфера, shared state у `std::future`,
`std::promise` и `std::packaged_task`, `std::thread`,
`std::filesystem::path`, файловые потоки и сторонние ABI со своими
контейнерами (`nlohmann::json`, LibreSSL, ogg/vorbis/theora). `BasicCore`,
`StackTrace` и `BaseLogging` намеренно используют стандартные контейнеры выше
`MemorySystem` в порядке включения; OOM-репортер не должен зависеть от
сломавшегося allocator-а. Поэтому interop-граница может обоснованно ловить
`std::bad_alloc`. Проект-встраиватель может вести собственный полный аудит,
но его команды и allowlist не являются нормативным доказательством движка.

Гарантия немедленного завершения относится только к памяти, не к другим
ресурсам ОС. Создание потока может бросить `std::system_error`, открытие файла
или сокета тоже может отказать. Например, `spawn_pool_worker` не является
`noexcept`, а `submit_impl` откатывает только что поставленную задачу, если
создание потока не удалось. Убирать guard допустимо только когда единственная
причина исключения действительно состоит в выделении через `SafeAlloc`.

## 2. Что может быть брошено и где это перехватывается

В работающем сервере могут распространяться:

- `VerificationException` из `FO_VERIFY_AND_THROW(...)` и скриптового
  `verify(...)`;
- исключения движка: `EntitySyncException`, `DataBaseException`,
  `GenericException`, исключения менеджеров и другие;
- исключения native lifecycle-кода вокруг dispatch callback-ов, включая
  повторные проверки после событий;
- `ScriptException` из `ScriptHelpers::CallInitScript`, когда `InitScript`
  сущности не разрешается в функцию нужной сигнатуры. Это отказ движка, а не
  исключение скрипта. Сам `ScriptFunc::Call` является `noexcept`, сообщает об
  исключении скрипта через `ReportExceptionAndContinue` и возвращает `false`.

Игровая работа сервера выполняется заданиями `WorkerPool`.
`WorkerPool::WorkerEntry` ловит `std::exception`, пишет отчет и продолжает,
освобождая `SyncContext`; неизвестное исключение завершает процесс. Поэтому
обычное исключение задания не перезапускает сервер, а уже выполненные побочные
эффекты остаются в мире. Правила ниже нужны именно для их согласованности.

Рассылка скриптовых событий через `Fire(...)` является `noexcept`: исключение
отдельного callback-а превращается в остановку цепочки. Побочные эффекты
обработчика, включая уничтожение и перемещение сущностей, сохраняются, поэтому
движок повторно проверяет состояние после события.

У клиента та же форма восстановления на уровне кадра, но есть дополнительное обязательство renderer-а. `MainEntry` ловит `std::exception` из `ClientEngine::MainLoop`, сообщает о нём и продолжает следующим кадром только после `Application::EndFrame`, который требует отсутствия привязанного render target. Поэтому draw-блок в `ClientEngine::MainLoop` защищён `scope_fail`, вызывающим `SpriteManager::AbortScene`: частичный draw отбрасывается, а stacks scissor и render target полностью снимаются. Любая новая frame-scoped привязка render target обязана обеспечить такую же очистку.

`AbortScene` является `noexcept`, поскольку выполняется во время unwind. Неотказное состояние manager сбрасывается напрямую, а освобождение backend идёт через `safe_call`, чтобы потерянный render context был зарегистрирован, но не заменил исходное исключение. Сам backend operation остаётся throwing на обычном пути. `EndScene` также остаётся обычным вызовом, а не переносится в `scope_success`: его проверки инвариантов должны бросать, пока `scope_fail` ещё активен, а не из неявно `noexcept` destructor-а.

### Нельзя бросать значения вне `std::exception`

Любое исключение native-кода Engine или встраивающего проекта должно наследоваться от `std::exception`. Integer, bare struct или стороннее нестандартное исключение проходит мимо обычных границ отчётности и запрещено.

Поэтому общий `catch (...)` рядом с `catch (const std::exception& ex)` не является recoverable error path. Он означает нарушенный инвариант и должен иметь вид:

```cpp
catch (...) {
    FO_UNKNOWN_EXCEPTION();
}
```

Нельзя журналировать такую ошибку и продолжать, создавать обычный domain error `"Unknown exception"` или преобразовывать её в ожидаемый отказ. Узкие исключения из disposition существуют только для no-throw teardown/unwind boundaries и самой logging/reporting machinery, где ничего не должно выйти наружу или повторно войти в reporter. Они не разрешают исходному коду бросать значение, не наследующее `std::exception`.

## 3. Контракт жизненного цикла сущности (создание / уничтожение)

Жизненный цикл намеренно не является транзакцией с общим откатом. Контракт
закреплен тестами `Source/Tests/Test_EntityLifecycle.cpp` и
`Source/Tests/Test_ServerMapOperations.cpp`.

**Создание** (`CritterManager::CreateCritterOnMap`, `ItemManager::CreateItem`,
`MapManager::CreateLocation`/`CreateMap`, `ServerEngine::CreateCritter` и
`LoadCritter`):

- сущность сначала создается и регистрируется, затем размещается, после чего
  выполняются init-скрипт и входные события;
- событие может законно уничтожить или переместить новую сущность;
- функция создания бросает исключение как сигнал неноминального завершения,
  но не откатывает результат событий. Уничтоженная сущность исчезает,
  перемещенная остается в новом месте, а уничтоженная при загрузке сущность
  остается уничтоженной. Это проверяют `ItemInitEventMayDestroyItem`,
  `CritterInitEventMayDestroyCritter`, `LocationInitEventMayDestroyLocation`,
  `MapAddCritterEventMayMoveCritterAwayThrows`,
  `MapAddCritterInitEventMayMoveCritterAwayThrows` и
  `CritterLoadEventMayDestroyLoadedCritterThrows`.

Общий create-time rollback неверен: в точке исключения нельзя отличить
предусмотренное перемещение выжившей сущности от утечки.

**Уничтожение** (`DestroyCritter`, `DestroyItem`, `DestroyLocation`,
`DestroyMap`, `DestroyCustomEntity`):

- сначала фиксируется `MarkAsDestroying()`, повторный вызов выходит раньше;
- `IsDestroying()` и `IsDestroyed()` являются acquire/release atomic-latch,
  а изменяемое содержимое по-прежнему защищает lock сущности;
- после finish-события окружение отделяется в повторяемом teardown-цикле,
  который ловит и сообщает исключения, пока все зависимости не сняты или
  progress guard не обнаружит отсутствие сходимости;
- snapshot-наборы обходятся через `copy_hold_ref(...)`, а после каждого
  события retained reference снова проверяется через `IsDestroyed()`.

Обработчик finish-события не может отменить уничтожение. Это закреплено
`ItemFinishEventCannotTakeOverItemDestruction` и аналогичными тестами
криттера и локации.

## 3.1 Сходимость цикла уничтожения

Каждый `DestroyX` опустошает коллекции дочерних сущностей и связей в цикле с
`prev_deps`. Таких циклов восемь: `ItemManager::DestroyItem`,
`CritterManager::DestroyCritter` и `DestroyInventory`,
`MapManager::DestroyMapContent`, `DestroyMapInternal` и внутренний цикл
`DestroyLocation`, а также `EntityManager::DestroyInnerEntities` и внутренний
цикл `DestroyCustomEntity`.

Цикл может не сходиться по трем причинам:

1. Re-entrant обработчик события снова добавляет дочернюю сущность к уже
   уничтожаемому владельцу.
2. Шаг отделения бросает на каждой итерации, поэтому коллекция не уменьшается.
3. Ошибка логики оставляет число зависимостей неизменным.

### Механизм выхода

**Основная защита: запрет повторного добавления при уничтожении, эшелонированный
по уровням из раздела 5.** Источник такого добавления обычно скриптовый
обработчик, поэтому правило проверяется на двух глубинах:

- на вершине каждый `FO_SCRIPT_API` add-метод (`Server_Map_AddItem`,
  `Server_Map_AddCritter`, `Server_Critter_AddItem`,
  `Server_Critter_AttachToCritter`, `Server_Item_AddItem`,
  `Server_Location_AddMap`) бросает `ScriptException`;
- внутренние методы изменения (`Entity::AddInnerEntity`,
  `CritterManager::AddItemToCritter`, `Map::AddCritter`, `Map::SetItem`,
  `Item::SetItemToContainer`, `Critter::AttachToCritter`,
  `Location::AddMap`) повторяют проверку через `FO_VERIFY_AND_THROW`.

Запрещено только расширять уничтожаемого владельца. Изменять свойства
уничтожаемой сущности разрешено: ее finish-обработчик законно очищает ее.

**Чтение уничтожаемой карты разрешено под lock.** `_hexField` уничтожается
только деструктором `Map`, а общий `_staticMap->HexField` живет дольше
экземпляра. Во время drain структура grid остается доступной, меняется лишь
содержимое. Конкурентность обеспечивает эксклюзивный lock изменения, а не
проверка `IsDestroying`.

Поэтому query-методы используют `LOCKED, NOT_DESTROYED` без
`NOT_DESTROYING`. Если потребитель не хочет работать с умирающей картой, он
сам проверяет `IsDestroying` или неудачу `Sync::Lock`. Единое правило:
расширение умирающей сущности запрещено; чтение под lock разрешено; полностью
мертвая сущность недоступна; drain является частью уничтожения.

**Предусловия методов объявляет `FO_VALIDATE_ENTITY(<flags>)`.** Это временная
диагностическая инфраструктура lock-системы. Флаги: `LOCKED` требует покрытие
`this` sync-контекстом; `NOT_DESTROYED` делает
`FO_STRONG_ASSERT(!IsDestroyed())`; `NOT_DESTROYING` делает бросающий
`FO_VERIFY_AND_THROW(!IsDestroying())`; `NONE` не добавляет требований.
В `noexcept`-теле `NOT_DESTROYING` недопустим: inline `throw` вызывает C4297
на MSVC `/W4`. Нулевая терпимость к предупреждениям делает это дефектом даже
без `/WX`; review остается основной проверкой.

Политика строгости: mutation-метод по умолчанию получает максимальные
`LOCKED, NOT_DESTROYED, NOT_DESTROYING`, а read/query-метод получает
`LOCKED, NOT_DESTROYED`. Исключения определяются тестами поведения:

1. `noexcept` никогда не принимает `NOT_DESTROYING`; при необходимости он
   использует `FO_VERIFY_AND_RETURN_VALUE`.
2. Accessor-ы, нужные drain-циклу, не принимают `NOT_DESTROYING`: коллекции
   inventory, inner items/entities, visibility и maps живут до деструктора.
3. Методы destroy/transfer cascade должны принимать `IsDestroying`, потому
   что transfer является частью уничтожения.
4. Post-event re-validation, которая сначала делает
   `if (IsDestroyed()) return`, принимает только `LOCKED`; ранний выход и есть
   контракт для retained, но уже уничтоженного объекта.

Положительное основание для `NOT_DESTROYING` одно: операция расширения или
добавления к владельцу (`AddItem`, `AddCritter`, `AddMap`,
`SetItemToContainer` и аналоги). Read/query его не используют: grid и
коллекции живут весь drain, а конкурентный доступ регулируется lock.

**Последний рубеж: завершение процесса при истинной несходимости.** Каждый
teardown-цикл хранит локальный счетчик и требует строгого уменьшения числа
оставшихся зависимостей:

```cpp
for (size_t prev_deps = std::numeric_limits<size_t>::max(); cr->HasItems() || cr->HasInnerEntities() || …;) {
    try { /* tear off one layer */ } catch (const std::exception& ex) { ReportExceptionAndContinue(ex); }

    const size_t remaining_deps = cr->GetInvItems().size() + cr->GetInnerEntitiesCount() + …;
    FO_STRONG_ASSERT(remaining_deps < prev_deps, "Critter destruction made no progress", cr->GetId(), remaining_deps, prev_deps);
    prev_deps = remaining_deps;
}
```

Нормальный drain выходит по условию до следующей проверки, поэтому медленное,
но прогрессирующее уничтожение не считается ошибкой. Непрогрессирующий проход
вызывает `FO_STRONG_ASSERT`: бросающее исключение оставило бы уже финализированную,
но живую сущность в registry. Итого уничтожение либо завершается, либо процесс
детерминированно останавливается на реальной ошибке.

## 4. Инварианты после изменения требуют `FO_STRONG_ASSERT`

Если необратимое изменение уже произошло и ложность проверки означает
повреждение мира, использовать бросающий verify поздно: `WorkerPool` поймал бы
его и продолжил работу с поврежденным состоянием. Такая проверка должна быть
безусловным `FO_STRONG_ASSERT`, вызывающим `ReportExceptionAndExit` во всех
профилях сборки.

Текущие примеры: согласованность typed/global registry в `EntityManager`,
симметрия visibility graph в `Critter`, post-grant инварианты
`EntityLock::Acquire`, данные suspended-контекста перед
`AngelScriptContextManager::ResumeSpecificContext`. Ожидаемые случаи остаются
бросающими проверками: duplicate id при загрузке и штатный
`EntityLockWaitAbortedException` при завершении.

## 5. Уровни ошибок и выбор реакции

Все три уровня активны и в release-сборке:

| Уровень | Когда | Механизм |
|---|---|---|
| **Ожидаемая ошибка** | Предусмотренное неверное значение, недоверенный ввод, отсутствующая или запрещенная цель. | Бросить доменное исключение (`ScriptException`, `DataBaseException` и т. п.) до побочного эффекта. |
| **Неожиданная, но обрабатываемая** | Нарушение инварианта, которое верхний уровень еще может перехватить. | Семейство `FO_VERIFY_*`; оно всегда пишет отчет, а суффикс выбирает дальнейший control flow. |
| **Неожиданная и необрабатываемая** | После изменения продолжение означало бы работу с поврежденным миром. | `FO_STRONG_ASSERT` и детерминированное завершение. |

Вариант `FO_VERIFY_*` выбирается контекстом:

- `FO_VERIFY_AND_THROW` бросает `VerificationException` и допустим только там,
  где исключение законно распространяется к верхнему catch;
- `FO_VERIFY_AND_CONTINUE` сообщает нарушение и продолжает, поэтому подходит
  для `noexcept`-контекста или пропуска плохого элемента цикла;
- `FO_VERIFY_AND_RETURN` сообщает и выходит из `void`-функции;
- `FO_VERIFY_AND_RETURN_VALUE` сообщает и возвращает безопасное значение.

В `noexcept`-области нельзя использовать бросающий вариант. Когда продолжать
опасно, из нее по-прежнему допустим `FO_STRONG_ASSERT`.

Эшелонированная защита намеренно проверяет одну ошибку на нескольких уровнях:
скриптовый add-метод бросает ожидаемый `ScriptException`, внутренний mutation
повторяет инвариант через `FO_VERIFY_AND_THROW`, а несходящийся teardown
останавливается через `FO_STRONG_ASSERT`. Эти проверки дополняют друг друга.

Практические правила:

- неверные аргументы, недоверенный ввод, переполненная или запрещенная цель
  требуют доменного исключения, а не завершения процесса;
- входные данные script/RPC/client-writable property проверяются на границе до
  глубокого `numeric_cast` и низкоуровневого verify; нижняя проверка остается
  backstop-ом;
- восстанавливаемый инвариант до изменения использует
  `FO_VERIFY_AND_THROW`;
- инвариант после необратимого изменения использует `FO_STRONG_ASSERT`;
- lifecycle throw-as-signal не откатывает законный результат событий;
- `scope_fail`/`scope_exit` нужны, когда реальный runtime-отказ иначе рассинхронизирует
  два представления; rollback-body обязан быть `noexcept`;
- лучший вариант часто состоит в порядке validate-first, mutate-last.

Записи `DbStorage.Insert/Update/Delete` только ставят работу в очередь и не
выполняют синхронный backend I/O. Backend-ошибка обрабатывается асинхронно
через recovery op-log, reconnect и panic shutdown с replay после перезапуска,
поэтому write-through rollback для нее не нужен.

## 6. Примитивы

- `scope_exit`, `scope_fail`, `scope_success` из
  `Source/Essentials/BasicCore.h` являются RAII guard-ами; `scope_fail`
  выполняется при unwinding и статически требует `noexcept` callback.
- `safe_call` из `Source/Essentials/CommonHelpers.h` вызывает функцию, поглощая
  исключения, и делает rollback/teardown body небросающим.
- `copy_hold_ref(container)` снимает ref-counted snapshot коллекции сущностей
  для re-entrant обхода.
- Inline progress guard с `prev_deps` требует строго уменьшать число
  зависимостей и обнаруживает отсутствие прогресса без искусственного лимита
  итераций.

## 7. Тесты

Контракты закреплены `Source/Tests/Test_EntityLifecycle.cpp` и
`Source/Tests/Test_ServerMapOperations.cpp`. При изменении lifecycle или
инвариантов запускайте сгенерированную цель unit-тестов проекта-встраивателя и
расширяйте эти suites, не ослабляя assertions.

## 8. Классификация безопасности исключений по функциям (уровни ES)

Каждому определению функции в `Source/**/*.cpp`, кроме `Source/Tests` и
codegen-входов `*.template.cpp`, можно присвоить уровень фактической гарантии:

| Уровень | Гарантия |
|---|---|
| `NoThrow` | Исключение не выходит к вызывающему: это обеспечивает `noexcept`, catch-all или доказуемо небросающее тело. Terminate-on-OOM, `FO_STRONG_ASSERT`, небросающий `Fire` и варианты verify без throw не снижают уровень. |
| `Strong` | При исключении наблюдаемое состояние совпадает с состоянием до вызова: validate-first, read-only или полный rollback. |
| `Basic` | Исключение может выйти после изменения, но все инварианты и объекты остаются корректными. Lifecycle throw-as-signal намеренно относится сюда. |
| `None (<reason>)` | Исключение может оставить конкретный нарушенный инвариант или половинчатое состояние. Это кандидат на исправление, а не приемлемая гарантия. |

Движок задает словарь и правила вывода, но не поставляет канонический полный
per-function baseline или analyzer. Проект-встраиватель может вести собственный
baseline с уровнем, статусом проверки и хешем тела; такой артефакт не является
нормативным доказательством движка.

Порядок вывода: `noexcept`, catch-all или отсутствие реальных throw points дают
`NoThrow`; все throw points до первого изменения либо полный rollback дают
`Strong`; исключение после изменения при сохраненных инвариантах дает `Basic`;
`None` допустим только с названием конкретного нарушенного инварианта.
Классифицируется тело самой функции с учетом поведения callees; lambdas,
declarations, `= default`, `= delete`, header-inline и тесты не входят.

`FO_VALIDATE_ENTITY(...)` при ES-классификации игнорируется: это временная
диагностическая инфраструктура. Его `LOCKED` и `NOT_DESTROYING` не считаются
throw points, а `NOT_DESTROYED` завершает процесс. Ручные проверки тела
считаются всегда.

`noexcept` является семантическим контрактом, а не записью текущего уровня.
Не добавляйте его только потому, что тело сейчас классифицировано `NoThrow`.
Он обоснован для move/swap-контрактов контейнеров, teardown и unwind callbacks,
C/OS ABI callback-ов и документированных небросающих Essentials-примитивов.
Снять случайный `noexcept` при появлении законной проверки нормально; уровень
и project-owned baseline при этом выводятся заново.

Для обоснованного `noexcept` учитывайте три опасности:

1. Достижимый inline throw вызывает MSVC C4297; используйте небросающий вариант
   verify или уберите спецификатор.
2. AngelScript registration отвергает указатель на `noexcept`-функцию как
   `asWRONG_CALLING_CONV`; binding-слой не должен получать такой тип.
3. Класс с несколькими build-selected реализациями должен иметь одинаковую
   спецификацию в declaration и каждой реализации, иначе возникает C2382.
