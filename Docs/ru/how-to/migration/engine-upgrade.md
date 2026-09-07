---
layout: default
title: Обновление Engine во встраивающем проекте
locale: ru
document_id: engine-upgrade-guide
permalink: /Docs/ru/how-to/migration/engine-upgrade.html
---

# Обновление Engine во встраивающем проекте

<!-- docs-translation: {"document_id":"engine-upgrade-guide","locale":"ru","source_path":"Docs/en/how-to/migration/engine-upgrade.md","source_sha256":"6aefd69e78ecc226ea0812b258082ddbe40429be43c8098fcdabe3d5308c4af1"} -->

Это руководство задаёт повторяемую процедуру обновления Engine в игровом
репозитории. Она охватывает интеграцию исходников, сгенерированные контракты,
контент, сохранения, сеть, client runtime и документацию.

## Проверенные исходные пути

- `AGENTS.md`
- `BuildTools/docs_contract_diff.py`
- `Docs/en/contributing/contract-change-management.md`
- `Docs/ru/explanation/runtime/client-updater.md`
- `Docs/en/explanation/persistence/index.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/ru/reference/platforms/support-matrix.md`
- `Docs/en/contributing/documentation/index.md`

## Определите обновление

До изменения submodule или vendored Engine checkout запишите:

- ревизию корня игрового проекта;
- старую точную ревизию Engine;
- планируемую новую точную ревизию Engine;
- upstream branch/repository;
- поддерживаемую build/runtime matrix;
- имена safety branch или stash;
- владельцев проверки compatibility, persistence, release и документации.

Обновление Engine не является одним изменением указателя. Полный входящий
диапазон Engine и проектные изменения для его принятия образуют единую единицу
review.

## Сохраните начальное состояние

1. Получите свежие данные из remotes проекта и Engine.
2. Проверьте локальные изменения в обоих worktrees.
3. Создайте именованную safety branch и при необходимости именованный stash в
   обоих репозиториях.
4. Запишите `git rev-parse HEAD`, Engine gitlink и remote tips.
5. Сохраняйте safety refs, пока обновлённый проект не пройдёт чистую проверку.

Не сбрасывайте, не удаляйте и не перезаписывайте несвязанные локальные
изменения ради видимости чистого обновления.

## Проверьте полный диапазон Engine

Изучите каждый commit и изменённый путь между старой и новой привязками:

```bash
git -C Engine log --oneline <old>..<new>
git -C Engine diff --stat <old>..<new>
git -C Engine diff <old>..<new> -- \
  Source BuildTools ThirdParty Resources Docs Examples
```

Классифицируйте изменения по владельцу и последствиям:

| Изменение | Обязательная проверка |
|---|---|
| CMake option/stage/application/package | Влияние на project configure, target, CI, package и support matrix |
| Project library helper/core-role graph | Project dependency targets, назначение roles, native bridge, platform gates и runtime package |
| Setting/default/config parser | Влияние на `.fomain`, sub-config, secret, resource pack и запуск |
| Script API/metadata/property | Влияние на compile, bake, network, persistence, migration и gameplay |
| Baker/file format/resource runtime | Authored content, forced rebake, cache/output schema и platform |
| Networking/updater/client runtime | Protocol, gameplay compatibility, host/runtime ABI, package и rollout |
| Database/entity serialization | Save migration, backup/restore, rollback и запрет mixed versions |
| Tool/editor | Authoring workflow, round trip, generated files и screenshots/manual |
| Documentation/example | Владение Engine/project, links, commands, pins и translation freshness |

Используйте Last Frontier или TLA только как integration evidence.
Нормативными остаются исходники, тесты, интерфейсы и сгенерированные модели
Engine.

## Сравните сгенерированные контракты

Сгенерируйте новые модели Engine, затем сравните со старой ревизией:

```bash
python Engine/BuildTools/docs_contract_diff.py \
  --root Engine \
  --baseline-git-ref <old-engine-revision> \
  --current-dir Docs/generated \
  --dispositions Docs/contract-change-dispositions.json \
  --write \
  --enforce
```

Для каждого изменения определите:

- является ли оно additive, documentation-only, policy-only или breaking;
- текущую stability promise;
- затронутые project code/content;
- необходимость migration и release note;
- минимальную совместимую ревизию client/server/save;
- остаётся ли rollback возможным после преобразования данных.

Не считайте метку `internal` доказательством отсутствия влияния на проект. Она
означает лишь, что Engine не дал публичного обещания совместимости.

## Согласуйте конфигурацию проекта

Сравните CMake root и `.fomain` проекта с:

- сгенерированным [справочником CMake](../../reference/cmake/index.md);
- [руководством по локальным зависимостям проекта](../../../ProjectDependencies.md);
- сгенерированным [справочником settings](../../../generated/api/settings.md);
- [руководством по конфигурации проекта](../build/project-configuration.md);
- [руководством по безопасности и секретам](../release/security-and-secrets.md);
- изменившимися validation/package interfaces BuildTools.

Удалите устаревшие options и targets, явно добавьте обязательные значения,
проверьте defaults и каждый sub-config для CI, разработки, staging и production.
Повторно проверьте `$ENV`/`$FILE` относительно
`$TARGET_ENV`/`$TARGET_FILE`, command-line masking tokens, side-specific baked
configs, передачу package signing и CI jobs с секретами. Конфиг проекта должен
фиксировать осознанные продуктовые решения, а не случайно наследовать новый
default.

## Пересоберите сгенерированные и запечённые данные

Следуйте [Generated Content Workflow](../build/generated-content.md) в порядке
зависимостей:

1. свежие configure/code generation;
2. native compile;
3. script compile;
4. forced resource bake при изменении contracts или pack inputs;
5. сравнение side-specific metadata;
6. project-generated references и snippet inventory;
7. localization status;
8. site routes, navigation и search;
9. AI evaluation и delivery artifacts.

Сохраняйте старый и новый отчёты о сгенерированных контрактах как evidence
обновления. Не редактируйте generated source или baked output вручную.

## Защитите сохраняемое состояние

Перед проверкой на ценных данных:

1. создайте и проверьте backup;
2. отрепетируйте restore в изолированную database;
3. определите property/prototype/version migration rules;
4. проверьте обновление на представительной копии;
5. проверьте entity counts, ownership, критические fields и пути login/loading;
6. решите, обратима ли migration;
7. запретите старым binaries открывать преобразованные данные, если rollback
   небезопасен.

Правила rename/remove для property и prototype являются runtime-контрактами, а
не удобством очистки. Обновите ссылки в authored content и scripts, сохраняйте
migration rules на поддерживаемый горизонт сохранений и проверяйте
missing/legacy values.

Выполните provider-neutral процедуру из [Backup and Recovery](../release/backup-and-recovery.md).
Абстракция database в Engine не выбирает provider, schedule, retention, schema
rollout, RPO/RTO или полномочия disaster recovery игры; храните эти решения и
evidence в операционной документации проекта.

## Защитите совместимость сети и client

Проверьте три независимые границы:

- gameplay `CompatibilityVersion`;
- поколение updater protocol;
- замороженный client host/runtime ABI.

Не предполагайте, что одна версия покрывает остальные. Разрыв protocol/ABI
может потребовать полного client package и ручной переустановки, даже когда
ресурсы умеют самообновляться. Изменение gameplay compatibility может
запретить смешанные ревизии client/server без изменения updater wire format.

Для online rollout определите:

1. принимаемую когорту старых clients;
2. путь resource/native update;
3. порядок развёртывания server;
4. reconnect behavior;
5. rollback point;
6. пользовательское восстановление для несовместимых frozen hosts;
7. monitoring ошибок update, login, sync и migration.

Точная текущая граница host/runtime и updater описана в
[Client Runtime and Updater](../../explanation/runtime/client-updater.md). Выполняйте
deployment, readiness, graceful stop и rollback по
[Release Operations](../release/operations.md).

## Проверьте принятие обновления

Сначала выполните самые узкие проверки, затем всю заявленную матрицу проекта:

- Engine unit tests для изменённых native domains;
- configure и compile каждым поддерживаемым host compiler;
- `CompileAngelScript`;
- `ForceBakeResources` при изменении data graph;
- focused content/gameplay tests;
- starter/tutorial smoke при изменении integration mechanics;
- visible client scene для rendering, GUI, audio, video, input, maps или assets;
- persistence upgrade/restore rehearsal;
- client/server compatibility и updater route;
- package contents и install/launch;
- synthetic-secret checks по baked configs, package trees, archives, logs и
  signing handoff;
- documentation generators, links, locale freshness, site artifact и AI
  delivery.

Соотносите заявления с [Матрицей поддержки](../../reference/platforms/support-matrix.md).
Cross-build не является device qualification, а headless test не доказывает
работу видимого client.

## Обновите документацию в той же работе

Согласуйте:

- переиспользуемое поведение в `Engine/Docs/`;
- project integration и product policy в документации игрового проекта;
- routing в `AGENTS.md`, если изменились владение или обязательная процедура;
- публичные примеры и точные Engine pins;
- сгенерированные API/format/settings/package references;
- support matrix и platform guides;
- английские исходные страницы и каждый существующий перевод, чей source hash
  изменился;
- active plan, update record и verification report.

Документация проекта может ссылаться на механики Engine, но не должна их
дублировать. Документы Engine не должны использовать приватный игровой
репозиторий как нормативное доказательство.

## Запись о завершении

Запись об обновлении должна содержать:

```text
Project old/new:
Engine old/new:
Incoming Engine commits audited:
Generated contract report:
Required dispositions:
Configuration changes:
Content/resource migrations:
Save migration and restore evidence:
Network/updater/ABI decision:
Validated host/target matrix:
Visible/device checks:
Documentation and translation status:
Known residual risks:
Safety refs retained until:
```

Не называйте обновление завершённым, пока отсутствует обязательное evidence.
Записывайте непроверенную платформу или owner-gated deployment как pending, а
не выводите успех из соседних проверок.
