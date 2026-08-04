---
title: Packager Command Line
document_id: generated-package-cli
locale: en
generated: true
---

# Packager Command Line

> Generated reference. Do not edit this page directly. Update `BuildTools/PackageInterface.json` or `BuildTools/package.py`, then run `python BuildTools/docs_package.py --write`.

[Index](index.md) | [Declaration](declaration.md) | [Matrix](matrix.md) | [Payloads](payloads.md) | [CLI](cli.md) | [Canonical JSON](../../../generated/package.json)

CMake normally invokes this internal CLI once for each `BINARY` clause. Direct callers must provide the same build hash, config, input, and output context.

```text
usage: package.py [-h] -maincfg MAINCFG -buildhash BUILDHASH -devname DEVNAME -nicename NICENAME -target {Server,Client,Mapper,Baker,AnimationViewer,ParticleViewer} -platform {Windows,Linux,Android,Web,macOS,iOS} -arch ARCH -pack PACK
                  -config CONFIG -input INPUT [-binary-output-postfix BINARY_OUTPUT_POSTFIX] -output OUTPUT [-zip-compress-level {0,1,2,3,4,5,6,7,8,9}]

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

| Stable ID | Argument | Required | Action | Choices | Default | Description |
| --- | --- | --- | --- | --- | --- | --- |
| <a id="entry-package-cli-argument-maincfg-f67b1f55d3"></a><code>package.cli.argument.maincfg</code> | <code>-maincfg</code> | yes | <code>store</code> | - | - | Main config path |
| <a id="entry-package-cli-argument-buildhash-0f1d295e2a"></a><code>package.cli.argument.buildhash</code> | <code>-buildhash</code> | yes | <code>store</code> | - | - | build hash |
| <a id="entry-package-cli-argument-devname-3db5dd1123"></a><code>package.cli.argument.devname</code> | <code>-devname</code> | yes | <code>store</code> | - | - | Dev game name |
| <a id="entry-package-cli-argument-nicename-7324a6526f"></a><code>package.cli.argument.nicename</code> | <code>-nicename</code> | yes | <code>store</code> | - | - | Representable game name |
| <a id="entry-package-cli-argument-target-3abbb688eb"></a><code>package.cli.argument.target</code> | <code>-target</code> | yes | <code>store</code> | <code>Server</code>, <code>Client</code>, <code>Mapper</code>, <code>Baker</code>, <code>AnimationViewer</code>, <code>ParticleViewer</code> | - | package target type |
| <a id="entry-package-cli-argument-platform-24b87674f7"></a><code>package.cli.argument.platform</code> | <code>-platform</code> | yes | <code>store</code> | <code>Windows</code>, <code>Linux</code>, <code>Android</code>, <code>Web</code>, <code>macOS</code>, <code>iOS</code> | - | platform type |
| <a id="entry-package-cli-argument-arch-77a6c93f85"></a><code>package.cli.argument.arch</code> | <code>-arch</code> | yes | <code>store</code> | - | - | architectures to include (divided by +) |
| <a id="entry-package-cli-argument-pack-97cdb74296"></a><code>package.cli.argument.pack</code> | <code>-pack</code> | yes | <code>store</code> | - | - | plus-separated pack tokens from PackageInterface.json |
| <a id="entry-package-cli-argument-config-6b9914184e"></a><code>package.cli.argument.config</code> | <code>-config</code> | yes | <code>store</code> | - | - | config name |
| <a id="entry-package-cli-argument-input-e00f0e4f00"></a><code>package.cli.argument.input</code> | <code>-input</code> | yes | <code>append</code> | - | - | input dir (from FO_OUTPUT_PATH) |
| <a id="entry-package-cli-argument-binary-output-postfix-9e412853dd"></a><code>package.cli.argument.binary_output_postfix</code> | <code>-binary-output-postfix</code> | no | <code>store</code> | - | <code>-</code> | suffix appended to binary output dir names |
| <a id="entry-package-cli-argument-output-2ae5ff278c"></a><code>package.cli.argument.output</code> | <code>-output</code> | yes | <code>store</code> | - | - | output dir |
| <a id="entry-package-cli-argument-zip-compress-level-e963a53f02"></a><code>package.cli.argument.zip_compress_level</code> | <code>-zip-compress-level</code> | no | <code>store</code> | <code>0</code>, <code>1</code>, <code>2</code>, <code>3</code>, <code>4</code>, <code>5</code>, <code>6</code>, <code>7</code>, <code>8</code>, <code>9</code> | - | override zip compression level |
