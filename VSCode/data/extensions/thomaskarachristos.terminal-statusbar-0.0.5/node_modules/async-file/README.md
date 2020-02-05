# async-file
Adapts the Node.js File System API (fs) for use with TypeScript async/await

This package makes it easier to access the Node.js file system using [TypeScript](http://www.typescriptlang.org/) and [async/await](https://blogs.msdn.microsoft.com/typescript/2015/11/03/what-about-asyncawait/).
It wraps the [Node.js File System API](https://nodejs.org/api/fs.html), replacing callback functions with functions that return a Promise.

Basically it lets you write your code like this...
```js
await fs.unlink('/tmp/hello');
console.log('successfully deleted /tmp/hello');
```
instead of like this...
```js
fs.unlink('/tmp/hello', err => {
  if (err) throw err;
  console.log('successfully deleted /tmp/hello');
});
```


Or like this...
```js
await fs.rename('/tmp/hello', '/tmp/world');
var stats = await fs.stat('/tmp/hello', '/tmp/world');
console.log(`stats: ${JSON.stringify(stats)}`);
```
instead of this...
```js
fs.rename('/tmp/hello', '/tmp/world', (err) => {
  if (err) throw err;
  fs.stat('/tmp/world', (err, stats) => {
    if (err) throw err;
    console.log(`stats: ${JSON.stringify(stats)}`);
  });
});
```

This package is a drop-in replacement for ```fs``` typings in [node.d.ts](https://github.com/DefinitelyTyped/DefinitelyTyped/blob/master/node/node.d.ts)—simply import ```async-file``` instead of fs and call any method within an async function... 

```js
import * as fs from 'async-file';
(async function () {
    var data = await fs.readFile('data.csv', 'utf8');
    await fs.rename('/tmp/hello', '/tmp/world');
    await fs.access('/etc/passd', fs.constants.R_OK | fs.constants.W_OK);
    await fs.appendFile('message.txt', 'data to append');
    await fs.unlink('/tmp/hello');
})();
```

In addition several convenience functions are introduced to simplify accessing text-files, testing for file existance, and creating or deleting files and directories recursively.
Other than the modified async function signatures and added convenience functions, the interface of this wrapper is virtually identical to the native Node.js file system library.


## Getting Started

Make sure you're running Node v4 and TypeScript 1.8 or higher...
```
$ node -v
v4.2.6
$ npm install -g typescript
$ npm install -g tsd
$ tsc -v
Version 1.8.9
```

Install ```async-file``` package and required ```node.d.ts``` dependencies...
```
$ npm install async-file
$ tsd install node
```

Write some code...
```js
import * as fs from 'async-file';
(async function () {
    var list = await fs.readdir('.');
    console.log(list);    
})();
```

Save the above to a file (index.ts), build and run it!
```
$ tsc index.ts typings/node/node.d.ts --target es6 --module commonjs 
$ node index.js
[ 'index.js', 'index.ts', 'node_modules', 'typings' ]
```

## Wrapped Functions
The following is a list of all wrapped functions...

* [```fs.access(path: string, mode?: number|string): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_access_path_mode_callback)
* [```fs.appendFile(file: string|number, data: any, options?: { encoding?: string; mode?: number|string; flag?: string; }): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_appendfile_file_data_options_callback)
* [```fs.chmod(path: string, mode: number|string): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_chmod_path_mode_callback)
* [```fs.chown(path: string, uid: number, gid: number): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_chown_path_uid_gid_callback)
* [```fs.close(fd: number): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_close_fd_callback)
* [```fs.fchmod(fd: number, mode: number|string): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_fchmod_fd_mode_callback)
* [```fs.fchown(fd: number, uid: number, gid: number): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_fchown_fd_uid_gid_callback)
* [```fs.fstat(fd: number): Promise<Stats>```](https://nodejs.org/api/fs.html#fs_fs_fstat_fd_callback)
* [```fs.fsync(fd: number): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_fsync_fd_callback)
* [```fs.ftruncate(fd: number, len?: number): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_ftruncate_fd_len_callback)
* [```fs.futimes(fd: number, atime: Date|number, mtime: Date|number): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_futimes_fd_atime_mtime_callback)
* [```fs.lchmod(path: string, mode: number|string): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_lchmod_path_mode_callback)
* [```fs.lchown(path: string, uid: number, gid: number): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_lchown_path_uid_gid_callback)
* [```fs.link(srcpath: string, dstpath: string): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_link_srcpath_dstpath_callback)
* [```fs.lstat(path: string): Promise<Stats>```](https://nodejs.org/api/fs.html#fs_fs_lstat_path_callback)
* [```fs.mkdir(path: string, mode?: number|string): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_mkdir_path_mode_callback)
* [```fs.mkdtemp(prefix: string): Promise<string>```](https://nodejs.org/api/fs.html#fs_fs_mkdtemp_prefix_callback)
* [```fs.open(path: string, flags: string, mode?: number|string): Promise<number>```](https://nodejs.org/api/fs.html#fs_fs_open_path_flags_mode_callback)
* [```fs.read(fd: number, buffer: Buffer, offset: number, length: number, position: number): Promise<ReadResult>```](https://nodejs.org/api/fs.html#fs_fs_read_fd_buffer_offset_length_position_callback)
* [```fs.readdir(path: string): Promise<string[]>```](https://nodejs.org/api/fs.html#fs_fs_readdir_path_callback)
* [```fs.readFile(file: string|number, options?: {encoding?: string, flag?: string}|string): Promise<any>```](https://nodejs.org/api/fs.html#fs_fs_readfile_file_options_callback)
* [```fs.readlink(path: string): Promise<string>```](https://nodejs.org/api/fs.html#fs_fs_readlink_path_callback)
* [```fs.realpath(path: string, cache?: {[path: string]: string}): Promise<string>```](https://nodejs.org/api/fs.html#fs_fs_realpath_path_cache_callback)
* [```fs.rename(oldPath: string, newPath: string): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_rename_oldpath_newpath_callback)
* [```fs.rmdir(path: string): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_rmdir_path_callback)
* [```fs.stat(path: string): Promise<Stats>```](https://nodejs.org/api/fs.html#fs_fs_stat_path_callback)
* [```fs.symlink(srcpath: string, dstpath: string, type?: string): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_symlink_target_path_type_callback)
* [```fs.truncate(path: string, len?: number): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_truncate_path_len_callback)
* [```fs.unlink(path: string): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_unlink_path_callback)
* [```fs.utimes(path: string, atime: Date|number, mtime: Date|number): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_utimes_path_atime_mtime_callback)
* [```fs.write(fd: number, buffer: Buffer, offset?: number, length?: number, position?: number): Promise<{written: number; buffer: Buffer}>```](https://nodejs.org/api/fs.html#fs_fs_write_fd_data_position_encoding_callback)
* [```fs.write(fd: number, data: any, offset?: number, position?: number, encoding?: string): Promise<{written: number; buffer: Buffer}>```](https://nodejs.org/api/fs.html#fs_fs_write_fd_data_position_encoding_callback)
* [```fs.write(fd: number): Promise<{written: number; buffer: Buffer}>```](https://nodejs.org/api/fs.html#fs_fs_write_fd_data_position_encoding_callback)
* [```fs.writeFile(file: string|number, data: string|any, options?: {encoding?: string, flag?: string, mode?: number|string}): Promise<void>```](https://nodejs.org/api/fs.html#fs_fs_writefile_file_data_options_callback)

## Convenience Functions
In addition to the wrapped functions above, the following convenience functions are provided...

* ```fs.createDirectory(path, mode?: number|string): Promise<void>```
* ```fs.delete(path: string): Promise<void>```
* ```fs.exists(path: string): Promise<boolean>```
* ```fs.readTextFile(file: string|number, encoding?: string, flags?: string): Promise<string>```
* ```fs.writeTextFile(file: string|number, data: string, encoding?: string, mode?: string): Promise<void>```
* ```fs.mkdirp(path: string): Promise<void>```
* ```fs.rimraf(path: string): Promise<void>```

```fs.createDirectory``` creates a directory recursively *(like [mkdirp](https://www.npmjs.com/package/mkdirp))*.

```fs.delete``` deletes any file or directory, performing a deep delete on non-empty directories *(wraps [rimraf](https://www.npmjs.com/package/rimraf))*.

```fs.exists``` implements the recommended solution of opening the file and returning ```true``` when the ```ENOENT``` error results.
 
```fs.readTextFile``` and ```fs.writeTextFile``` are optimized for simple text-file access, dealing exclusively with strings not buffers or streaming.

```fs.mkdirp``` and ```fs.rimraf``` are aliases for ```fs.createDirectory``` and ```fs.delete``` respectively, for those prefering more esoteric nomenclature.

### Convenience Function Examples

Read a series of three text files, one at a time...
```js
var data1 = await fs.readTextFile('data1.csv');
var data2 = await fs.readTextFile('data2.csv');
var data3 = await fs.readTextFile('data3.csv');
```

Append a line into an arbitrary series of text files...
```js
var files = ['data1.log', 'data2.log', 'data3.log'];
for (var file of files)
    await fs.writeTextFile(file, '\nPASSED!\n', null, 'a');
```

Check for the existance of a file...
```js
if (!(await fs.exists('config.json')))
    console.warn('Configuration file not found');
```

Create a directory...
```js
await fs.createDirectory('/tmp/path/to/file');
```

Delete a file or or directory...
```js
await fs.delete('/tmp/path/to/file');
```


## Additional Notes

If access to both the native Node.js file system library and the wrapper is required at the same time *(e.g. to mix callbacks alongside async/await code)*, specify a different name in the import statement of the wrapper...
```js
import * as fs from 'fs';
import * as afs from 'async-file';
await afs.rename('/tmp/hello', '/tmp/world');
fs.unlink('/tmp/hello', err => 
  console.log('/tmp/hello deleted', err));
});
```

By design none of *"sync"* functions are exposed by the wrapper: fs.readFileSync, fs.writeFileSync, etc.

## Related Wrappers
Here are some other TypeScript async/await wrappers you may find useful...

* [**async-parallel**](https://www.npmjs.com/package/async-parallel) simplifies invoking tasks in parallel
* [**web-request**](https://www.npmjs.com/package/web-request) simplifies making web requests
