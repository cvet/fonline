# Diagnostic Channel Publishers
Provides a set of patches for common Node.js modules to publish instrumentation
data to the [diagnostic-channel](https://github.com/Microsoft/node-diagnostic-channel) channel.

## Currently-supported modules
* [`redis`](https://github.com/NodeRedis/node_redis) v2.x
* [`mysql`](https://github.com/mysqljs/mysql) v2.x
* [`mongodb`](https://github.com/mongodb/node-mongodb-native) v2.x, v3.x
* [`pg`](https://github.com/brianc/node-postgres) v6.x, v7.x
* [`pg-pool`](https://github.com/brianc/node-pg-pool) v1.x, v2.x
* [`bunyan`](https://github.com/trentm/node-bunyan) v1.x
* [`winston`](https://github.com/winstonjs/winston) v2.x, v3.x

## Release notes
### 0.3.3 - August 15, 2019
* Fix patching issue with new [mongodb@3.3.0+](https://github.com/mongodb/node-mongodb-native/releases/tag/v3.3.0) driver

### 0.3.2 - May 13, 2019
* Fix issue with colorized Winston logging levels
* Support new Winston [child loggers](https://github.com/winstonjs/winston/pull/1471) (`winston@3.2.0+`)

### 0.3.1 - April 22, 2019
* Changed semver for mysql patching to `mysql@2.x`

### 0.3.0 - February 19th, 2019
* Added patching for `pg@7.x`, `pg-pool@2.x`
* Added patching for `mysql@2.16.x`
* Added patching for `winston@3.x`
* Added patching for `mongodb@3.x`

### 0.2.0 - August 18th, 2017
* Added patching for `pg`, `pg-pool`, and `winston` modules
* Updated build output to use `dist/` folder instead of `.dist/`
(fixes [#256](https://github.com/Microsoft/ApplicationInsights-node.js/issues/256))
