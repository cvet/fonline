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
Object.defineProperty(exports, "__esModule", { value: true });
const inversify_1 = require("inversify");
const querystring = require("query-string");
const vscode_1 = require("vscode");
const vscode_2 = require("vscode");
const types_1 = require("../application/types");
const types_2 = require("../ioc/types");
const contentProvider_1 = require("./contentProvider");
let HtmlViewer = class HtmlViewer {
    constructor(serviceContainer) {
        this.disposable = [];
        this.onPreviewHtml = (uri, column, title) => {
            this.createHtmlView(vscode_1.Uri.parse(uri), column, title);
        };
        this.htmlView = new Map();
        const commandManager = serviceContainer.get(types_1.ICommandManager);
        this.disposable.push(commandManager.registerCommand('previewHtml', this.onPreviewHtml));
        this.contentProvider = new contentProvider_1.ContentProvider(serviceContainer);
    }
    dispose() {
        this.disposable.forEach(disposable => disposable.dispose());
    }
    createHtmlView(uri, column, title) {
        if (this.htmlView.has(uri.toString())) {
            // skip recreating a webview, when already exist
            // and reveal it in tab view
            const webviewPanel = this.htmlView.get(uri.toString());
            if (webviewPanel) {
                webviewPanel.reveal();
            }
            return;
        }
        const query = querystring.parse(uri.query.toString());
        const port = parseInt(query.port.toString(), 10);
        const internalPort = parseInt(query.internalPort.toString(), 10);
        // tslint:disable-next-line:no-any
        const htmlContent = this.contentProvider.provideTextDocumentContent(uri, undefined);
        const webviewPanel = vscode_2.window.createWebviewPanel('gitLog', title, column, {
            enableScripts: true,
            retainContextWhenHidden: true,
            portMapping: [
                { webviewPort: internalPort, extensionHostPort: port }
            ]
        });
        this.htmlView.set(uri.toString(), webviewPanel);
        webviewPanel.onDidDispose(() => {
            if (this.htmlView.has(uri.toString())) {
                this.htmlView.delete(uri.toString());
            }
        });
        webviewPanel.webview.html = htmlContent;
        //return webviewPanel.webview;
    }
};
HtmlViewer = __decorate([
    __param(0, inversify_1.inject(types_2.IServiceContainer)),
    __metadata("design:paramtypes", [Object])
], HtmlViewer);
exports.HtmlViewer = HtmlViewer;
//# sourceMappingURL=htmlViewer.js.map