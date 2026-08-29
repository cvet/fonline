---
layout: default
title: Анализ потокобезопасности
locale: ru
document_id: thread-safety-analysis
permalink: /Docs/ru/contributing/coding-contracts/thread-safety-analysis.html
---

# Анализ потокобезопасности

<!-- docs-translation: {"document_id":"thread-safety-analysis","locale":"ru","source_path":"Docs/en/contributing/coding-contracts/thread-safety-analysis.md","source_sha256":"b92dd0d64ee820868b29fc958040c34b8003dcfd043fd70da65a842de955dc29"} -->

Движок аннотирует обычные mutex с помощью
[Clang Thread Safety Analysis](https://clang.llvm.org/docs/ThreadSafetyAnalysis.html)
(TSA), чтобы неправильная работа с блокировками, например обращение к
защищённому состоянию без mutex, отсутствие требуемой capability или возврат с
удерживаемой блокировкой, становилась **ошибкой компиляции** на каждом toolchain
Clang. Анализ статический и не имеет runtime-стоимости; он дополняет, но не
заменяет runtime-проверки и тесты конкурентности.

> TSA является дополнительной защитой для **обычных mutex с лексической
> областью действия**. Он намеренно не моделирует кооперативные и динамически
> получаемые схемы блокировок; см. раздел «Исключения».

> Об exception safety при получении блокировки, в частности почему
> постусловие выданного `EntityLock` использует `FO_STRONG_ASSERT`, см.
> [Exception Safety](exception-safety.md).

## Toolchain и обязательность

- В `BuildTools/cmake/stages/Init.cmake` анализ включён для каждого compiler id
  Clang: native `clang`, `clang-cl`, AppleClang, Emscripten и Android NDK.
  Используются `-Wthread-safety -Werror=thread-safety`, а для cl-style драйвера
  `clang-cl` флаги передаются через `/clang:`. MSVC и GCC TSA не реализуют,
  поэтому авторитетным gate является **сборка Clang**.
- Макросы `FO_TSA_*` разворачиваются в `__attribute__((...))` только при
  `__clang__`; для остальных компиляторов это no-op, поэтому аннотированный код
  без изменений собирается MSVC/GCC.
- Сторонние библиотеки подавляются через `DisableLibWarnings` (`-w` и
  `-Wno-error=` для legacy-C ошибок по умолчанию в Clang 20+), поэтому TSA
  анализирует только собственный код движка и подключающего проекта.

## Зачем нужны обёртки mutex из `fo::`

Проект использует платформенную STL, а libc++ отключена. Ни MS STL, ни
libstdc++ не аннотируют `std::mutex` / `std::shared_mutex` как capabilities.
Поэтому `FO_TSA_GUARDED_BY(std_mutex_member)` выдал бы сообщение о требуемой
capability и ничего не проверил. Защищённое состояние должно использовать
аннотированные примитивы движка из `Source/Essentials/Threading.h`.

Это **прямые замены std-аналогов** с теми же именами в snake_case и теми же
методами, поэтому в месте блокировки достаточно заменить `std::` на `fo::`:

| Тип | Оборачивает / повторяет | Назначение |
|------|-------------------------|------------|
| `fo::mutex` | `std::mutex` | состояние только с exclusive-доступом |
| `fo::shared_mutex` | `std::shared_mutex` | состояние с reader/writer-доступом |
| `fo::atomic_mutex` | park/wake поверх атомарного состояния | короткие critical sections из `noexcept` code, где ошибку получения OS mutex невозможно сообщить |
| `fo::scoped_lock<T>` | `std::scoped_lock` / `std::lock_guard` | exclusive RAII guard для `mutex` или `shared_mutex`; CTAD: `scoped_lock lk {m}` |
| `fo::shared_lock<T>` | `std::shared_lock` | shared reader guard для `shared_mutex` |
| `fo::unique_lock<T>` | `std::unique_lock` | exclusive guard с ручными `lock()`/`unlock()`, пригодный для `std::condition_variable_any` |

`std::scoped_lock`, `std::unique_lock` и `std::shared_lock` непрозрачны для
анализатора в платформенной STL, поэтому в проверяемых местах нужны guard из
`fo::`. Код движка находится внутри `FO_BEGIN_NAMESPACE`, где имена применяются
без квалификатора. Подключайте `Threading.h`; он расположен низко в Essentials,
сразу над `HashedString`, и доступен даже низкоуровневым заголовкам.

Condition variables используют `std::condition_variable_any`, принимающий
любой Lockable, включая `fo::unique_lock`. Передавайте guard прямо в `wait`:

```cpp
unique_lock lock {_dataLocker};
_workSignal.wait(lock, [this]() FO_TSA_REQUIRES(_dataLocker) { return _ready; });
```

## Аннотирование mutex

1. Используйте `mutex` / `shared_mutex` и объявляйте его **до** защищаемых полей.
2. Добавьте `FO_TSA_GUARDED_BY(_locker)` ко всем защищённым полям данных.
3. В местах блокировки замените `std::scoped_lock`/`std::lock_guard` на
   `scoped_lock`, `std::shared_lock` на `shared_lock`, обычный exclusive
   `std::unique_lock` на `scoped_lock`, а `std::unique_lock` с condition variable
   или ручным relock на `unique_lock`; сам cv замените на
   `std::condition_variable_any`. Всегда используйте brace initialization:
   `scoped_lock lock {mutex};`, `shared_lock lock {mutex};`.
4. На private helper, предполагающий уже удерживаемую блокировку, ставьте
   `FO_TSA_REQUIRES(_locker)` для exclusive или
   `FO_TSA_REQUIRES_SHARED(_locker)` для read-доступа.
5. У hand-written RAII guard пометьте класс `FO_TSA_SCOPED_CAPABILITY`,
   конструктор `FO_TSA_ACQUIRE(mutex)`, деструктор `FO_TSA_RELEASE()`; move
   constructor, если он есть, требует `FO_TSA_NO_ANALYSIS`.

> Не помещайте атрибут между `()` и trailing `-> type`. Для аннотированных
> методов используйте ведущий return type, например
> `bool try_lock() FO_TSA_TRY_ACQUIRE(true)`.
>
> `fo::thread`, то есть handle задачи пула и результат
> `threading::run_thread`, также объявлен в `Threading.h` в namespace `fo`.

## Словарь макросов

`FO_TSA_CAPABILITY(name)`, `FO_TSA_SCOPED_CAPABILITY`, `FO_TSA_GUARDED_BY(x)`,
`FO_TSA_PT_GUARDED_BY(x)`, `FO_TSA_ACQUIRED_BEFORE(...)`,
`FO_TSA_ACQUIRED_AFTER(...)`, `FO_TSA_REQUIRES(...)`,
`FO_TSA_REQUIRES_SHARED(...)`, `FO_TSA_ACQUIRE(...)`,
`FO_TSA_ACQUIRE_SHARED(...)`, `FO_TSA_RELEASE(...)`,
`FO_TSA_RELEASE_SHARED(...)`, `FO_TSA_RELEASE_GENERIC(...)`,
`FO_TSA_TRY_ACQUIRE(...)`, `FO_TSA_TRY_ACQUIRE_SHARED(...)`,
`FO_TSA_EXCLUDES(...)`, `FO_TSA_ASSERT_CAPABILITY(x)`,
`FO_TSA_ASSERT_SHARED_CAPABILITY(x)`, `FO_TSA_RETURN_CAPABILITY(x)`,
`FO_TSA_NO_ANALYSIS`.

## Исключения: что TSA не покрывает

- **`std::recursive_mutex`**: повторное получение нельзя смоделировать.
  Recursive locks остаются сырыми `std::recursive_mutex` без `GUARDED_BY` и с
  комментарием `// recursive: not modelable by TSA`.
- **Однопоточные init/teardown-проходы**, которые обходят защищённое состояние,
  повторно входя в locking-код и поэтому не могут удерживать lock: помечайте
  функцию и каждую внутреннюю lambda отдельно как `FO_TSA_NO_ANALYSIS`, добавляя
  комментарий, почему выполнение однопоточное. Используйте редко и никогда для
  сокрытия настоящей гонки.
- **Кооперативные или динамически получаемые наборы блокировок**, где набор
  зависит от данных и получается нелексически, выразить нельзя. Не аннотируйте
  их и документируйте исключение в документе владеющей подсистемы.

Подключающие проекты описывают собственный перечень защищённых полей и
проектные исключения в своей документации по потокам. Эта страница определяет
только переиспользуемый механизм.
