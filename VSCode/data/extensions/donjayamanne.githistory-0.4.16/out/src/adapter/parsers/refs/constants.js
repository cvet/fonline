"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
// These are the prefixes returned by 'git log --decorate=full --format=%D'
// These also include prefixes returned by 'git branch --all'
// These also include prefixes returned by 'git show-ref'
exports.HEAD_REF_PREFIXES = ['HEAD -> refs/heads/', 'refs/heads/'];
exports.REMOTE_REF_PREFIXES = ['remotes/origin/HEAD -> ', 'refs/remotes/', 'remotes/'];
exports.TAG_REF_PREFIXES = ['tag: refs/tags/', 'refs/tags/'];
//# sourceMappingURL=constants.js.map