"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
var __metadata = (this && this.__metadata) || function (k, v) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(k, v);
};
var __param = (this && this.__param) || function (paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
};
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const child_process_1 = require("child_process");
const iconv = require("iconv-lite");
const inversify_1 = require("inversify");
const vscode_1 = require("vscode");
const stopWatch_1 = require("../../common/stopWatch");
const types_1 = require("../../common/types");
const DEFAULT_ENCODING = 'utf8';
const isWindows = /^win/.test(process.platform);
let GitCommandExecutor = class GitCommandExecutor {
    constructor(loggers) {
        this.loggers = loggers;
        this.gitExtension = vscode_1.extensions.getExtension('vscode.git').exports;
        const gitApi = this.gitExtension.getAPI(1);
        this.gitExecutablePath = gitApi.git.path;
    }
    // tslint:disable-next-line:no-any
    exec(options, ...args) {
        return __awaiter(this, void 0, void 0, function* () {
            let gitPath = this.gitExecutablePath;
            gitPath = isWindows ? gitPath.replace(/\\/g, '/') : gitPath;
            const childProcOptions = typeof options === 'string' ? { cwd: options, encoding: DEFAULT_ENCODING } : options;
            if (typeof childProcOptions.encoding !== 'string' || childProcOptions.encoding.length === 0) {
                childProcOptions.encoding = DEFAULT_ENCODING;
            }
            const binaryOuput = childProcOptions.encoding === 'binary';
            const destination = binaryOuput ? args.shift() : undefined;
            const gitPathCommand = childProcOptions.shell && gitPath.indexOf(' ') > 0 ? `"${gitPath}"` : gitPath;
            const stopWatch = new stopWatch_1.StopWatch();
            const gitShow = child_process_1.spawn(gitPathCommand, args, childProcOptions);
            if (binaryOuput) {
                gitShow.stdout.pipe(destination);
            }
            const disposables = [];
            const on = (ee, name, fn) => {
                ee.on(name, fn);
                disposables.push({ dispose: () => ee.removeListener(name, fn) });
            };
            const buffers = [];
            if (!binaryOuput) {
                on(gitShow.stdout, 'data', (data) => buffers.push(data));
            }
            const errBuffers = [];
            on(gitShow.stderr, 'data', (data) => errBuffers.push(data));
            // tslint:disable-next-line:no-any
            return new Promise((resolve, reject) => {
                gitShow.once('close', () => {
                    if (errBuffers.length > 0) {
                        let stdErr = decode(errBuffers, childProcOptions.encoding);
                        stdErr = stdErr.startsWith('error: ') ? stdErr.substring('error: '.length) : stdErr;
                        this.loggers.forEach(logger => {
                            logger.log('git', ...args, ` (completed in ${stopWatch.elapsedTime / 1000}s)`);
                            logger.error(stdErr);
                        });
                        reject(stdErr);
                    }
                    else {
                        const stdOut = binaryOuput ? undefined : decode(buffers, childProcOptions.encoding);
                        this.loggers.forEach(logger => {
                            logger.log('git', ...args, ` (completed in ${stopWatch.elapsedTime / 1000}s)`);
                            logger.trace(binaryOuput ? '<binary>' : stdOut);
                        });
                        resolve(stdOut);
                    }
                    disposables.forEach(disposable => disposable.dispose());
                });
                gitShow.once('error', ex => {
                    reject(ex);
                    this.loggers.forEach(logger => {
                        logger.log('git', ...args, ` (completed in ${stopWatch.elapsedTime / 1000}s)`);
                        logger.error(ex);
                    });
                    disposables.forEach(disposable => disposable.dispose());
                });
            });
        });
    }
};
GitCommandExecutor = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.multiInject(types_1.ILogService)),
    __metadata("design:paramtypes", [Array])
], GitCommandExecutor);
exports.GitCommandExecutor = GitCommandExecutor;
function decode(buffers, encoding) {
    return iconv.decode(Buffer.concat(buffers), encoding);
}
//# sourceMappingURL=gitCommandExec.js.map