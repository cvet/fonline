"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const os_1 = require("os");
let CommitViewFormatter = class CommitViewFormatter {
    format(item) {
        const sb = [];
        if (item.hash && item.hash.full) {
            sb.push(`sha1 : ${item.hash.full}`);
        }
        if (item.author) {
            sb.push(this.formatAuthor(item));
        }
        if (item.author && item.author.date) {
            const authorDate = item.author.date.toLocaleString();
            sb.push(`Author Date : ${authorDate}`);
        }
        if (item.committer) {
            sb.push(`Committer : ${item.committer.name} <${item.committer.email}>`);
        }
        if (item.committer && item.committer.date) {
            const committerDate = item.committer.date.toLocaleString();
            sb.push(`Commit Date : ${committerDate}`);
        }
        if (item.subject) {
            sb.push(`Subject : ${item.subject}`);
        }
        if (item.body) {
            sb.push(`Body : ${item.body}`);
        }
        if (item.notes) {
            sb.push(`Notes : ${item.notes}`);
        }
        return sb.join(os_1.EOL);
    }
    formatAuthor(logEntry) {
        return `Author : ${logEntry.author.name} <${logEntry.author.email}>`;
    }
};
CommitViewFormatter = __decorate([
    inversify_1.injectable()
], CommitViewFormatter);
exports.CommitViewFormatter = CommitViewFormatter;
//# sourceMappingURL=commitFormatter.js.map