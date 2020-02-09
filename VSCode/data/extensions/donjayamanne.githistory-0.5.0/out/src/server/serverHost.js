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
const bodyParser = require("body-parser");
const cors = require("cors");
const events_1 = require("events");
const express = require("express");
const http = require("http");
const inversify_1 = require("inversify");
const path = require("path");
const vscode_1 = require("vscode");
const commandManager_1 = require("../application/types/commandManager");
const types_1 = require("../ioc/types");
const types_2 = require("../types");
const apiController_1 = require("./apiController");
let ServerHost = class ServerHost extends events_1.EventEmitter {
    constructor(gitServiceFactory, serviceContainer) {
        super();
        this.gitServiceFactory = gitServiceFactory;
        this.serviceContainer = serviceContainer;
    }
    dispose() {
        this.app = undefined;
        this.port = undefined;
        if (this.httpServer) {
            this.httpServer.close();
            this.httpServer = undefined;
        }
        if (this.apiController) {
            this.apiController.dispose();
        }
    }
    start() {
        return __awaiter(this, void 0, void 0, function* () {
            if (this.startPromise) {
                return this.startPromise;
            }
            this.app = express();
            // tslint:disable-next-line:no-any
            this.httpServer = http.createServer(this.app);
            const rootDirectory = path.join(__dirname, '..', '..', 'browser');
            this.app.use(bodyParser.urlencoded({ extended: false }));
            this.app.use(bodyParser.json({ limit: '2mb' }));
            this.app.use(express.static(rootDirectory));
            this.app.use(express.static(path.join(__dirname, '..', '..', '..', 'resources'), { extensions: ['.svg', 'svg', 'json', '.json'] }));
            this.app.use(cors());
            this.app.get('/', (req, res) => {
                this.rootRequestHandler(req, res);
            });
            return this.startPromise = new Promise((resolve, reject) => {
                const commandManager = this.serviceContainer.get(commandManager_1.ICommandManager);
                this.apiController = new apiController_1.ApiController(this.app, this.gitServiceFactory, this.serviceContainer, commandManager);
                this.httpServer.listen(0, () => {
                    this.port = this.httpServer.address().port;
                    resolve({ port: this.port });
                });
                this.httpServer.on('error', error => {
                    if (!this.port) {
                        reject(error);
                    }
                });
            });
        });
    }
    rootRequestHandler(req, res) {
        if (Object.keys(req.query).length <= 0) {
            // This request is used to fetch the redirected url
            res.status(200).send('The Git History server is running...');
            return;
        }
        // DEVELOPER HINT: 
        // The below is only necessary when opening from an external browser
        const locale = 'en_US';
        const settings = {
            id: req.query.id,
            branchName: req.query.branchName,
            file: req.query.file,
            branchSelection: req.query.branchSelection
        };
        const config = vscode_1.workspace.getConfiguration('gitHistory');
        res.render(path.join(__dirname, '..', '..', 'browser', 'index.ejs'), { locale, config, settings });
    }
};
ServerHost = __decorate([
    __param(0, inversify_1.inject(types_2.IGitServiceFactory)),
    __param(1, inversify_1.inject(types_1.IServiceContainer)),
    __metadata("design:paramtypes", [Object, Object])
], ServerHost);
exports.ServerHost = ServerHost;
//# sourceMappingURL=serverHost.js.map