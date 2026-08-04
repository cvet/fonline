---
title: Командная строка упаковщика
document_id: generated-package-cli
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-package-cli","locale":"ru","source_path":"Docs/en/reference/packages/cli.md","source_sha256":"9d656b9c2182f81234d27673ec7f5a5452972ac93015b0cee582077c3647ab54"} -->

# Командная строка упаковщика

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `BuildTools/PackageInterface.json` или `BuildTools/package.py`, затем выполните `python BuildTools/docs_package.py --write`.

[Индекс](index.md) | [Объявление](declaration.md) | [Матрица](matrix.md) | [Содержимое](payloads.md) | [CLI](cli.md) | [Канонический JSON](../../../generated/package.json)

Обычно CMake вызывает этот внутренний CLI один раз для каждого предложения `BINARY`. Прямые вызовы должны передавать тот же хеш сборки, конфигурацию и контекст входных и выходных данных.

```text
usage: package.py [-h] -maincfg MAINCFG -buildhash BUILDHASH -devname DEVNAME
                  -nicename NICENAME
                  -target {Server,Client,Mapper,Baker,AnimationViewer,ParticleViewer}
                  -platform {Windows,Linux,Android,Web,macOS,iOS} -arch ARCH
                  -pack PACK -config CONFIG -input INPUT
                  [-binary-output-postfix BINARY_OUTPUT_POSTFIX]
                  -output OUTPUT [-zip-compress-level {0,1,2,3,4,5,6,7,8,9}]

FOnline packager

options:
  -h, --help            show this help message and exit
  -maincfg MAINCFG      Main config path
  -buildhash BUILDHASH  build hash
  -devname DEVNAME      Dev game name
  -nicename NICENAME    Representable game name
  -target {Server,Client,Mapper,Baker,AnimationViewer,ParticleViewer}
                        package target type
  -platform {Windows,Linux,Android,Web,macOS,iOS}
                        platform type
  -arch ARCH            architectures to include (divided by +)
  -pack PACK            plus-separated pack tokens from PackageInterface.json
  -config CONFIG        config name
  -input INPUT          input dir (from FO_OUTPUT_PATH)
  -binary-output-postfix BINARY_OUTPUT_POSTFIX
                        suffix appended to binary output dir names
  -output OUTPUT        output dir
  -zip-compress-level {0,1,2,3,4,5,6,7,8,9}
                        override zip compression level
```

| Стабильный ID | Аргумент | Обязательно | Действие | Варианты | По умолчанию | Описание |
| --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-package-cli-argument-maincfg-f67b1f55d3"></a><code>package.cli.argument.maincfg</code> | <code>-maincfg</code> | да | <code>store</code> | - | - | путь к основному конфигурационному файлу |
| <a id="entry-package-cli-argument-buildhash-0f1d295e2a"></a><code>package.cli.argument.buildhash</code> | <code>-buildhash</code> | да | <code>store</code> | - | - | хеш сборки |
| <a id="entry-package-cli-argument-devname-3db5dd1123"></a><code>package.cli.argument.devname</code> | <code>-devname</code> | да | <code>store</code> | - | - | имя игры для разработки |
| <a id="entry-package-cli-argument-nicename-7324a6526f"></a><code>package.cli.argument.nicename</code> | <code>-nicename</code> | да | <code>store</code> | - | - | отображаемое имя игры |
| <a id="entry-package-cli-argument-target-3abbb688eb"></a><code>package.cli.argument.target</code> | <code>-target</code> | да | <code>store</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | - | тип цели упаковки |
| <a id="entry-package-cli-argument-platform-24b87674f7"></a><code>package.cli.argument.platform</code> | <code>-platform</code> | да | <code>store</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code>, <code>macOS</code>, <code>iOS</code> | - | тип платформы |
| <a id="entry-package-cli-argument-arch-77a6c93f85"></a><code>package.cli.argument.arch</code> | <code>-arch</code> | да | <code>store</code> | - | - | включаемые архитектуры (разделяются символом +) |
| <a id="entry-package-cli-argument-pack-97cdb74296"></a><code>package.cli.argument.pack</code> | <code>-pack</code> | да | <code>store</code> | - | - | разделённые знаком + токены наборов из PackageInterface.json |
| <a id="entry-package-cli-argument-config-6b9914184e"></a><code>package.cli.argument.config</code> | <code>-config</code> | да | <code>store</code> | - | - | имя конфигурации |
| <a id="entry-package-cli-argument-input-e00f0e4f00"></a><code>package.cli.argument.input</code> | <code>-input</code> | да | <code>append</code> | - | - | входной каталог (из FO_OUTPUT_PATH) |
| <a id="entry-package-cli-argument-binary-output-postfix-9e412853dd"></a><code>package.cli.argument.binary_output_postfix</code> | <code>-binary-output-postfix</code> | нет | <code>store</code> | - | <code>-</code> | суффикс, добавляемый к именам каталогов выходных бинарных файлов |
| <a id="entry-package-cli-argument-output-2ae5ff278c"></a><code>package.cli.argument.output</code> | <code>-output</code> | да | <code>store</code> | - | - | выходной каталог |
| <a id="entry-package-cli-argument-zip-compress-level-e963a53f02"></a><code>package.cli.argument.zip_compress_level</code> | <code>-zip-compress-level</code> | нет | <code>store</code> | <code>0</code>, <code>1</code>, <code>2</code>, <code>3</code>, <code>4</code>, <code>5</code>, <code>6</code>, <code>7</code>, <code>8</code>, <code>9</code> | - | переопределение уровня сжатия ZIP |
