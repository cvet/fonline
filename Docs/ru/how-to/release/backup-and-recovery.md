---
layout: default
title: Резервное копирование и восстановление
locale: ru
document_id: backup-and-recovery
permalink: /Docs/ru/how-to/release/backup-and-recovery.html
---

# Резервное копирование и восстановление

<!-- docs-translation: {"document_id":"backup-and-recovery","locale":"ru","source_path":"Docs/en/how-to/release/backup-and-recovery.md","source_sha256":"ec245f3ae1b0ce75dd3b7116048ca07770e36b6ad3b156ea864dafe5fef26bc8"} -->

Этот runbook определяет переиспользуемую границу резервного копирования, восстановления и disaster recovery для сервера FOnline. Используйте его вместе с [Persistence](../../explanation/persistence/) для механики хранилища, [Release Operations](operations.md) для управления процессом и [Engine Upgrade Guide](../migration/engine-upgrade.md), когда долговечные данные переходят между ревизиями Engine или игры.

## Решение о восстановлении

Сначала определите полный durable set: `Memory` не имеет постоянного набора;
SQLite требует `Storage.sqlite` и активные WAL sidecars; JSON требует полное
дерево storage; Mongo требует provider-native consistent backup. Recovery oplog
не является backup: в pending log добавляется только команда, для которой
backend write сообщил failure, а committed-файл хранит только committed prefix
этого pending log. Поэтому два файла не восстанавливают успешные записи, которые
не были добавлены, или изменения, потерянные при несообщённом сбое питания.

Для переносимого baseline прекратите traffic, корректно остановите сервер до
`Server stopped!`, захватите backend и оба oplog files и называйте результат
только backup candidate, пока он не восстановлен в isolated environment.
Никогда не смешивайте binaries, resources и data несовместимых release units.
Требуйте `Start server complete!`, project-owned semantic probe и новый цикл
write/read, доказывающий, что запись сохранилась. Репетируйте из off-site copy,
измеряйте RPO/RTO, записывайте semantic checks и назначайте corrective action
для каждого пропущенного objective.
Engine не предоставляет команду online backup или checkpoint; provider-native
процедуры SQLite или Mongo должны происходить из проверенного project runbook.

## Установите границу восстановления

Engine владеет facade базы данных, backend-реализациями JSON/SQLite/Mongo/Memory, асинхронной очередью commit, recovery oplog, replay при запуске, panic callback и корректным drain commit. Подключающая игра или её оператор владеет:

- выбранным backend, расположением хранилища, топологией Mongo и параметрами подключения;
- схемами коллекций, миграциями данных и совместимостью данных с кодом;
- провайдером резервного копирования, расписанием, хранением, шифрованием, репликацией и off-site policy;
- recovery point objective (RPO), recovery time objective (RTO), зависимостями сервисов, drain трафика и полномочиями восстановления;
- production credentials, обработкой персональных данных, решениями по инцидентам и одобрением разрушительных действий.

Engine не предоставляет команду snapshot базы данных, online-backup API, контроллер point-in-time recovery, транзакцию миграции, endpoint для drain трафика или orchestrator восстановления. Проект должен предоставить и проверить эти операции, не представляя их встроенным поведением Engine.

Считайте одним восстанавливаемым релизом совместимый комплект: неизменяемый server package, identity эффективной конфигурации и sub-config, ревизии Engine и игры, запечённые ресурсы, постоянные данные, recovery oplog, состояние миграций и secrets или key identifiers, необходимые для чтения комплекта. Никогда не восстанавливайте данные в произвольный binary только потому, что оба запускаются.

## Определите долговечный набор

`Server.DbStorage` выбирает одну из следующих форм подключения:

| Backend | Долговечные данные | Граница согласованности и восстановления |
|---|---|---|
| `Memory` | Нет | Локальное для процесса тестовое состояние. Оно не может удовлетворить цели постоянного backup или recovery. |
| `JSON <directory>` | Один JSON-файл на запись в каталогах коллекций | Каждая вставка или update записывает `<record>.json.tmp` и переименовывает его, но операции над несколькими записями не образуют одну транзакцию. Делайте копию остановленного процесса или filesystem snapshot, согласованность которого доказана для всего каталога. |
| `DbSQLite <directory>` | `<directory>/Storage.sqlite` и активные SQLite WAL sidecars | Engine открывает WAL mode с `synchronous=NORMAL`; отдельные записи выполняются с autocommit. Не копируйте только `Storage.sqlite` во время работы сервера. Используйте согласованный с provider/SQLite online backup либо остановите сервер и сохраните весь каталог хранилища. Engine не предоставляет команду online backup или checkpoint. |
| `Mongo <URI> <database>` | Именованная база Mongo в настроенном deployment | URI и provider определяют write concern, репликацию, snapshot, dump и point-in-time возможности; Engine их не переопределяет. Используйте нативный согласованный метод провайдера и запишите его гарантии. |

Engine также открывает `DataBase.OpLogPath` и файл подтверждённого прогресса, имя которого получается заменой обязательного финального суффикса `.oplog` на `-committed.oplog`. Имена по умолчанию: `DbPendingChanges.oplog` и `DbPendingChanges-committed.oplog`. Пути относительны рабочего каталога сервера, если проект не сделал их абсолютными. При запуске настроенный путь без нужного суффикса отклоняется до открытия обоих файлов.

Оба oplog входят в backup и evidence инцидента. Сохраняйте их вместе со snapshot backend, даже если они пусты. Никогда не редактируйте, не объединяйте, не меняйте порядок, не копируйте частично и не обрезайте их вручную.

## Поймите recovery oplog

Recovery oplog не является резервной копией, replication stream, audit history или point-in-time log.

Обычные записи идут напрямую из in-memory commit queue в backend. Команда добавляется и синхронно записывается в pending oplog только после того, как backend сообщил об ошибке записи при включённом `DataBase.OpLogEnabled`. Затем commit queue удаляет эту команду. После успешного переподключения или следующего запуска Engine:

1. проверяет оба файла и требует, чтобы committed prefix совпадал с pending prefix;
2. повторно применяет только pending-команды после committed prefix;
3. добавляет и синхронно записывает каждую повторённую команду в committed-файл;
4. проверяет точное равенство строк, обрезает committed-файл, затем pending-файл.

Replay намеренно идемпотентен только в узких случаях: удаление отсутствующей записи принимается, идентичная уже существующая вставка принимается, а update, уже содержащийся в сохранённом документе, принимается. Конфликтующая вставка, malformed file, несовпадающий prefix, ошибка replay, append или truncation останавливает восстановление.

При отключённом oplog первая ошибка записи backend запускает database panic. При включённом Engine повторяет попытки согласно `DataBase.ReconnectRetryPeriod`; достижение `DataBase.PanicOpLogSizeThreshold` или ошибка восстановления запускает panic. Panic запрашивает остановку приложения и после `DataBase.PanicShutdownTimeout` принудительно завершает процесс.

Oplog не может восстановить успешные изменения после старой резервной копии и не покрывает потерю питания, о которой backend не успел сообщить. Никогда не объединяйте устаревший snapshot с более поздним oplog и не называйте результат point-in-time recovery.

## Определите контракт резервного копирования

До эксплуатации production запишите одну проверенную политику для каждой среды:

| Обязательное поле | Что записать |
|---|---|
| Scope | Identity backend, полный набор хранилища, оба пути oplog, внешние файлы принадлежащей игре persistence и явные исключения |
| Consistency method | Копия после корректной остановки, filesystem snapshot, согласованный SQLite backup или нативный snapshot/dump Mongo/provider; укажите доказанную границу атомарности |
| Recovery objectives | RPO, RTO, частота backup, допустимый replication lag и максимальный возраст restore |
| Retention | Классы ротации, off-site copies, legal/privacy expiry, полномочия удаления и policy неизменяемых копий |
| Security | Шифрование при передаче и хранении, доступ restore role, key identifier, audit trail и правила redaction |
| Compatibility | Ревизии Engine/игры, `CompatibilityVersion`, identity конфигурации, версия migration/schema и поддерживаемые версии rollback |
| Verification | Hash/inventory checks, сравнение правдоподобных размеров и количества с предыдущими recovery points, проверки целостности backend, семантические проверки игры, дата последнего изолированного restore и владелец evidence |

Каждой резервной копии нужен sidecar manifest вне изменяемого набора данных. Запишите уникальный backup ID, UTC начала и окончания, среду, исходный host/cluster, backend и provider snapshot ID, точные пути и namespaces, размеры и хеши файлов где применимо, commit IDs Engine и игры, digest package/provenance manifest, digest эффективной несекретной конфигурации, размеры и хеши oplog, encryption key ID, consistency method, identity оператора или automation и status проверки. Не помещайте credentials или восстановленные персональные данные в этот manifest.

## Создайте quiesced backup

Резервная копия остановленного процесса является переиспользуемой базовой процедурой, если нет проверенного online method:

1. Подтвердите целевую среду, backup ID, restore destination, свободное место, retention class и полномочия оператора. Отклоните неоднозначный путь хранилища или имя базы данных.
2. Остановите новые сессии и принадлежащие игре mutating jobs средствами инфраструктуры проекта. Engine не предоставляет traffic-drain protocol.
3. Запросите корректную остановку через процедуру [Release Operations](operations.md). Не копируйте данные только потому, что процесс исчез.
4. Потребуйте `Server stopped!`, успешный exit процесса, отсутствие критической ошибки базы данных и предупреждения о негарантированных pending commits. Если чего-то нет, перейдите к capture инцидента ниже.
5. Захватите backend полным способом, соответствующим backend. Захватите оба oplog без изменения.
6. Создайте sidecar manifest, hashes/inventory и provider completion evidence. Сделайте backup неизменяемым по policy проекта до повторного открытия трафика.
7. Восстановите новый backup в изолированной среде и выполните acceptance checks. Успешная копия без проверенного restore остаётся только кандидатом в backup.
8. Запустите production release через обычную readiness gate и сохраняйте предыдущую известную рабочую recovery point, пока новая не удовлетворит policy.

Online backup допустим только тогда, когда метод backend/provider предоставляет документированную consistency boundary, а проект восстановил и проверил именно этот метод под одновременными записями. Liveness процесса, успешный exit инструмента копирования или статус завершённого cloud snapshot не являются достаточным семантическим evidence.

## Зафиксируйте инцидент базы данных

После `Critical database failure`, принудительного exit, повреждённого хранилища или ошибки replay oplog:

1. Уберите трафик и не позволяйте автоматическим restart loops изменять evidence.
2. Сохраните server logs, crash reports, health evidence, identity эффективной несекретной конфигурации, состояние backend и оба oplog как один timestamped incident set.
3. Не запускайте второй сервер с теми же файлами или namespace базы данных. Oplog handles используют exclusive file locking, но это не защищает backend от всех внешних writers.
4. Не обрезайте и не исправляйте production data на месте. Клонируйте evidence и исследуйте клон.
5. Выберите известный рабочий backup с доказанной совместимостью code/data и целостностью. Считайте incident oplog только evidence для replay; не предполагайте, что он заполняет интервал после backup.
6. Перед production write передайте malformed или mismatched oplog, insert conflicts, integrity failures backend и неизвестное состояние миграции владельцу данных проекта.

## Восстановите безопасно

Сначала восстанавливайте в изолированный namespace или host с отключённым исходящим player traffic и внешними side effects:

1. Проверьте identity backup, retention status, signature/hash/inventory, доступ к encryption key, поддержку версии backend и одобрение оператора.
2. Выберите точный совместимый server package, ревизии Engine/игры, запечённые ресурсы, несекретную конфигурацию и migration level из backup. Никогда не смешивайте binaries, resources и data из разных release units.
3. Подготовьте пустое, явно allowlisted назначение. Откажитесь восстанавливать поверх source или единственной известной рабочей копии. Если provider поддерживает remapping namespace/path или dry run, докажите точное назначение до первой записи; никогда не выводите безопасность из похожего имени базы данных или каталога.
4. Восстановите полный набор backend его нативным инструментом. Восстановите оба oplog в записанные расположения `DataBase.OpLogPath`, сохранив names, bytes, ordering и permissions. Сохраните неудачное назначение для исследования, а успешное одноразовое назначение удаляйте только через guard точного имени.
5. Выполните нативные integrity/consistency checks backend до запуска FOnline. Для JSON также отклоните оставшиеся `.tmp` до выяснения происхождения; для SQLite проверьте полную WAL-aware database; для Mongo — результат restore провайдера и ожидаемые database/collections.
6. Запустите один изолированный сервер. Startup подключает backend, проверяет oplog, восстанавливает pending commands, загружает persistent entities и только затем достигает `Start server complete!`. Любое startup/replay exception означает неудачный restore.
7. Выполните принадлежащую проекту semantic probe: аутентифицируйте синтетическую учётную запись, загрузите репрезентативные entities и locations, проверьте critical balances/progress/references, выполните обратимую запись, корректно остановите и перезапустите сервер и докажите сохранение записи.
8. Сравните ожидаемые collection counts/invariants и migration records с backup manifest. Целостность backend сама по себе не доказывает семантику игры.
9. Запишите фактическую длительность restore, получившуюся recovery point, потерю данных относительно RPO, все применённые commands/tools, результаты проверок и approver. Продвигайте восстановленную среду только через staged rollout/readiness procedure.

Не позволяйте автоматической startup migration изменять первую восстановленную копию до сохранения неизменённой baseline. Проверяйте forward migration, restart и любой обещанный rollback на клонах.

## Репетируйте disaster recovery

С cadence, заданной проектом, выполняйте полный drill из off-site или иначе независимой от отказа копии. Drill должен предполагать недоступность primary storage, получать нужные keys через реальный emergency path, восстанавливать infrastructure и data, запускать точный совместимый package, выполнять semantic checks и измерять RPO/RTO.

Со временем включите как минимум следующие failures:

- отсутствующий, просроченный, повреждённый или частично загруженный backup;
- недоступный encryption key или restore credentials;
- устаревший backup плюс непустой oplog;
- malformed или prefix-mismatched oplog;
- пропущенный SQLite WAL sidecar в небезопасной live copy;
- snapshot Mongo со слишком слабыми consistency guarantees;
- успешная migration, после которой rollback не читает новые данные;
- технически здоровый restore с ошибкой game-level invariant.

Drill проходит только тогда, когда evidence определяет backup, release unit, владельца restore, измеренные recovery point/time, backend checks, semantic checks, расхождения и corrective action. Обновляйте runbook и automation в том же change, где обнаружен пробел.

## Маршрутизация отказов

| Симптом | Действие |
|---|---|
| Нет `Server stopped!` или pending commits не гарантированы | Сохраните incident set; не называйте копию quiesced |
| Pending и committed oplog различаются или не разбираются | Остановитесь; сохраните оба точных файла и передайте Engine/runtime и владельцу данных проекта |
| Replay сообщает conflicting insert или не может выполнить truncate | Остановите restart automation; исследуйте клонированные backend и oplog |
| JSON backup содержит необъяснимые `.tmp` | Считайте его прерванным или несогласованным, пока исходное состояние не доказано |
| Live copy SQLite пропустила WAL state | Отклоните её; используйте полную stopped copy или доказанный SQLite-consistent method |
| Гарантии restore Mongo неизвестны | Не продвигайте в production, пока consistency provider и write concern не документированы |
| Восстановленный binary не читает data или migration односторонняя | Не открывайте трафик и выберите совместимый release/backup или одобренное forward recovery |
| Backend checks прошли, но semantic probe нет | Оставьте restore изолированным; передайте владельцу игровой schema/system |
| Backup, log или manifest раскрывает secret | Ограничьте доступ, отзовите или замените credential, сохраните очищенное incident evidence и следуйте [Security and Secrets](security-and-secrets.md) |

## Проверьте runbook

Для каждого поддерживаемого persistent backend автоматизируйте создание backup, изолированный restore, integrity checks, startup сервера, semantic read/write/restart checks и измеряемое recovery evidence на синтетических данных. Проверяйте точную production topology и provider method в проектной lane; unit tests Engine доказывают механику backend и oplog, а не систему резервного копирования оператора.

При изменении persistence или recovery source запускайте `Source/Tests/Test_DataBase.cpp` через Engine unit-test target. Сохраняйте failure-injection coverage для spill в oplog, reconnect/replay, invalid records, conflicts, thresholds и round trips backend. Запускайте package и release-operations lanes при изменении recovery unit или process procedure.

## Проверенные пути исходников

- `Source/Common/Settings.inc`
- `Source/Server/DataBase.h`
- `Source/Server/DataBase.cpp`
- `Source/Server/DataBase-Json.cpp`
- `Source/Server/DataBase-SQLite.cpp`
- `Source/Server/DataBase-Mongo.cpp`
- `Source/Server/DataBase-Memory.cpp`
- `Source/Server/Server.cpp`
- `Source/Tests/Test_DataBase.cpp`
