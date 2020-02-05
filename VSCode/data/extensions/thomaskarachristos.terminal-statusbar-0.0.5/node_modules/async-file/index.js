"use strict";
const fs = require('fs');
const pathutil = require('path');
const rimraf = require('rimraf');
var fs_1 = require('fs');
exports.createReadStream = fs_1.createReadStream;
exports.createWriteStream = fs_1.createWriteStream;
exports.watch = fs_1.watch;
exports.watchFile = fs_1.watchFile;
exports.unwatchFile = fs_1.unwatchFile;
exports.constants = fs_1.constants;
function access(path, mode) { return thunk(fs.access, arguments); }
exports.access = access;
function appendFile(file, data, options) { return thunk(fs.appendFile, arguments); }
exports.appendFile = appendFile;
function chmod(path, mode) { return thunk(fs.chmod, arguments); }
exports.chmod = chmod;
function chown(path, uid, gid) { return thunk(fs.chown, arguments); }
exports.chown = chown;
function close(fd) { return thunk(fs.close, arguments); }
exports.close = close;
function fchmod(fd, mode) { return thunk(fs.fchmod, arguments); }
exports.fchmod = fchmod;
function fchown(fd, uid, gid) { return thunk(fs.fchown, arguments); }
exports.fchown = fchown;
function fstat(fd) { return thunk(fs.fstat, arguments); }
exports.fstat = fstat;
function ftruncate(fd, len) { return thunk(fs.ftruncate, arguments); }
exports.ftruncate = ftruncate;
function futimes(fd, atime, mtime) { return thunk(fs.futimes, arguments); }
exports.futimes = futimes;
function fsync(fd) { return thunk(fs.fsync, arguments); }
exports.fsync = fsync;
function lchmod(path, mode) { return thunk(fs.lchmod, arguments); }
exports.lchmod = lchmod;
function lchown(path, uid, gid) { return thunk(fs.lchown, arguments); }
exports.lchown = lchown;
function link(srcpath, dstpath) { return thunk(fs.link, arguments); }
exports.link = link;
function lstat(path) { return thunk(fs.lstat, arguments); }
exports.lstat = lstat;
function mkdir(path, mode) { return thunk(fs.mkdir, arguments); }
exports.mkdir = mkdir;
function mkdtemp(path) { return thunk(fs.mkdtemp, arguments); }
exports.mkdtemp = mkdtemp;
function open(path, flags, mode) { return thunk(fs.open, arguments); }
exports.open = open;
function read(fd, buffer, offset, length, position) { return thunk(fs.read, arguments, null, function () { return { bytesRead: arguments[1], buffer: arguments[2] }; }); }
exports.read = read;
function readdir(path) { return thunk(fs.readdir, arguments); }
exports.readdir = readdir;
function readFile(file, options) { return thunk(fs.readFile, arguments); }
exports.readFile = readFile;
function readlink(path) { return thunk(fs.readlink, arguments); }
exports.readlink = readlink;
function realpath(path, cache) { return thunk(fs.realpath, arguments); }
exports.realpath = realpath;
function rename(oldPath, newPath) { return thunk(fs.rename, arguments); }
exports.rename = rename;
function rmdir(path) { return thunk(fs.rmdir, arguments); }
exports.rmdir = rmdir;
function stat(path) { return thunk(fs.stat, arguments); }
exports.stat = stat;
function symlink(srcpath, dstpath, type) { return thunk(fs.symlink, arguments); }
exports.symlink = symlink;
function truncate(path, len) { return thunk(fs.truncate, arguments); }
exports.truncate = truncate;
function unlink(path) { return thunk(fs.unlink, arguments); }
exports.unlink = unlink;
function utimes(path, atime, mtime) { return thunk(fs.utimes, arguments); }
exports.utimes = utimes;
function write(fd) { return thunk(fs.write, arguments, null, function () { return { written: arguments[1], buffer: arguments[2] }; }); }
exports.write = write;
function writeFile(file, data, options) { return thunk(fs.writeFile, arguments); }
exports.writeFile = writeFile;
function readTextFile(file, encoding, flags) {
    if (encoding === undefined)
        encoding = 'utf8';
    if (flags === undefined || flags === null)
        flags = 'r';
    return thunk(fs.readFile, [file, { encoding: encoding, flags: flags }]);
}
exports.readTextFile = readTextFile;
function writeTextFile(file, data, encoding, flags, mode) {
    if (encoding === undefined)
        encoding = 'utf8';
    if (flags === undefined || flags === null)
        flags = 'w';
    var options = { encoding: encoding, flags: flags, mode: mode };
    if (flags[0] === 'a')
        return thunk(fs.appendFile, [file, data, options]);
    else
        return thunk(fs.writeFile, [file, data, options]);
}
exports.writeTextFile = writeTextFile;
function createDirectory(path, mode) {
    return new Promise((resolve, reject) => mkdirp(path, mode, err => !err ? resolve() : reject(err)));
}
exports.createDirectory = createDirectory;
exports.mkdirp = createDirectory;
function del(path) {
    return new Promise((resolve, reject) => rimraf(path, err => !err ? resolve() : reject(err)));
}
exports.delete = del;
exports.rimraf = del;
function exists(path) {
    return new Promise((resolve, reject) => fs.lstat(path, (err) => !err ? resolve(true) : err.code === 'ENOENT' ? resolve(false) : reject(err)));
}
exports.exists = exists;
function mkdirp(path, mode, done) {
    path = pathutil.resolve(path);
    fs.mkdir(path, mode, (err) => {
        if (!err)
            done(null);
        else if (err.code === 'ENOENT')
            mkdirp(pathutil.dirname(path), mode, err => !err ? mkdirp(path, mode, done) : done(err));
        else
            fs.stat(path, (err, stat) => err ? done(err) : !stat.isDirectory() ? done(new Error(path + ' is already a file')) : done(null));
    });
}
function thunk(target, args, context, resolver) {
    return new Promise((resolve, reject) => {
        target.apply(context, Array.prototype.slice.call(args).concat([(err, result) => {
                if (err)
                    reject(err);
                else if (resolver)
                    resolver.apply(context, arguments);
                else
                    resolve(result);
            }]));
    });
}
//# sourceMappingURL=index.js.map