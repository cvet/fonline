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
const commandManager_1 = require("../application/types/commandManager");
const disposableRegistry_1 = require("../application/types/disposableRegistry");
const types_1 = require("../ioc/types");
const registration_1 = require("./registration");
let CommandHandlerManager = class CommandHandlerManager {
    constructor(disposableRegistry, commandManager, serviceContainer) {
        this.disposableRegistry = disposableRegistry;
        this.commandManager = commandManager;
        this.serviceContainer = serviceContainer;
    }
    registerHandlers() {
        for (const item of registration_1.CommandHandlerRegister.getHandlers()) {
            const serviceIdentifier = item[0];
            const handlers = item[1];
            const target = this.serviceContainer.get(serviceIdentifier);
            handlers.forEach(handlerInfo => this.registerCommand(handlerInfo.commandName, handlerInfo.handlerMethodName, target));
        }
    }
    registerCommand(commandName, handlerMethodName, target) {
        // tslint:disable-next-line:no-any
        const handler = target[handlerMethodName];
        // tslint:disable-next-line:no-any
        const disposable = this.commandManager.registerCommand(commandName, (...args) => {
            handler.apply(target, args);
        });
        this.disposableRegistry.register(disposable);
    }
};
CommandHandlerManager = __decorate([
    inversify_1.injectable(),
    __param(0, inversify_1.inject(disposableRegistry_1.IDisposableRegistry)),
    __param(1, inversify_1.inject(commandManager_1.ICommandManager)),
    __param(2, inversify_1.inject(types_1.IServiceContainer)),
    __metadata("design:paramtypes", [Object, Object, Object])
], CommandHandlerManager);
exports.CommandHandlerManager = CommandHandlerManager;
//# sourceMappingURL=handlerManager.js.map