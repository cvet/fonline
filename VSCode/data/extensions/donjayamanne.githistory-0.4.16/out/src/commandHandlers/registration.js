"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
// tslint:disable-next-line:no-stateless-class
class CommandHandlerRegister {
    static register(commandName, handlerMethodName, serviceIdentifier) {
        if (!CommandHandlerRegister.Handlers.has(serviceIdentifier)) {
            CommandHandlerRegister.Handlers.set(serviceIdentifier, []);
        }
        const commandList = CommandHandlerRegister.Handlers.get(serviceIdentifier);
        commandList.push({ commandName, handlerMethodName });
        CommandHandlerRegister.Handlers.set(serviceIdentifier, commandList);
    }
    static getHandlers() {
        return CommandHandlerRegister.Handlers.entries();
    }
    dispose() {
        CommandHandlerRegister.Handlers.clear();
    }
}
CommandHandlerRegister.Handlers = new Map();
exports.CommandHandlerRegister = CommandHandlerRegister;
// tslint:disable-next-line:no-any function-name
function command(commandName, serviceIdentifier) {
    // tslint:disable-next-line:no-function-expression
    return function (_target, propertyKey, descriptor) {
        CommandHandlerRegister.register(commandName, propertyKey, serviceIdentifier);
        return descriptor;
    };
}
exports.command = command;
//# sourceMappingURL=registration.js.map