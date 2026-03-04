"use strict";
var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

// src/web-extension.ts
var web_extension_exports = {};
__export(web_extension_exports, {
  activate: () => activate,
  deactivate: () => deactivate
});
module.exports = __toCommonJS(web_extension_exports);

// src/activateFosDebug.ts
var vscode = __toESM(require("vscode"));

// src/attachDiscovery.ts
var DISCOVERY_PROBE = "fos-debug-discover-v1";
function getNodeDgramModule() {
  const localRequire = typeof require === "function" ? require : void 0;
  if (localRequire) {
    try {
      return localRequire("dgram");
    } catch {
    }
  }
  const globalRequire = globalThis?.require;
  if (typeof globalRequire === "function") {
    try {
      return globalRequire("dgram");
    } catch {
    }
  }
  const moduleRequire = globalThis?.module?.require;
  if (typeof moduleRequire === "function") {
    try {
      return moduleRequire("dgram");
    } catch {
    }
  }
  try {
    const dynamicRequire = new Function("return require")();
    return dynamicRequire("dgram");
  } catch {
    return void 0;
  }
}
async function discoverTargets(port, timeoutMs) {
  const dgram = getNodeDgramModule();
  if (!dgram) {
    throw new Error("Attach discovery requires Node.js dgram runtime. Use attach.endpoint (e.g. tcp://127.0.0.1:43042) or run the extension in desktop extension host.");
  }
  return await new Promise((resolve, reject) => {
    const socket = dgram.createSocket("udp4");
    const discovered = /* @__PURE__ */ new Map();
    let settled = false;
    const finish = (targets, error) => {
      if (settled) {
        return;
      }
      settled = true;
      try {
        socket.close();
      } catch {
      }
      if (error) {
        reject(error);
      } else {
        resolve(targets ?? []);
      }
    };
    socket.on("error", (error) => finish(void 0, error));
    socket.on("message", (msg) => {
      let parsed;
      try {
        parsed = JSON.parse(msg.toString("utf8"));
      } catch {
        return;
      }
      if (parsed.type !== "discovery" || !parsed.processId || !parsed.endpoint) {
        return;
      }
      discovered.set(parsed.endpoint, {
        processId: parsed.processId,
        endpoint: parsed.endpoint,
        protocolVersion: parsed.protocolVersion,
        targetName: parsed.targetName
      });
    });
    socket.bind(0, () => {
      try {
        socket.setBroadcast(true);
        socket.send(Buffer.from(DISCOVERY_PROBE, "utf8"), port, "255.255.255.255");
      } catch (error) {
        finish(void 0, error);
        return;
      }
      setTimeout(() => finish([...discovered.values()]), timeoutMs);
    });
  });
}

// src/activateFosDebug.ts
function activateFosDebug(context, factory) {
  context.subscriptions.push(
    vscode.commands.registerCommand("extension.fos-debug.toggleFormatting", (variable) => {
      const ds = vscode.debug.activeDebugSession;
      if (ds) {
        ds.customRequest("toggleFormatting");
      }
    })
  );
  context.subscriptions.push(vscode.commands.registerCommand("extension.fos-debug.getProgramName", (config) => {
    return vscode.window.showInputBox({
      placeHolder: "Please enter the name of a markdown file in the workspace folder",
      value: "readme.md"
    });
  }));
  const provider = new FosConfigurationProvider();
  context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider("fos", provider));
  context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider("fos", {
    provideDebugConfigurations(folder) {
      return [
        {
          name: "Dynamic Launch",
          request: "launch",
          type: "fos",
          program: "${file}"
        },
        {
          name: "Another Dynamic Launch",
          request: "launch",
          type: "fos",
          program: "${file}"
        },
        {
          name: "FOS Launch",
          request: "launch",
          type: "fos",
          program: "${file}"
        }
      ];
    }
  }, vscode.DebugConfigurationProviderTriggerKind.Dynamic));
  if (factory) {
    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory("fos", factory));
    if ("dispose" in factory) {
    }
  }
  context.subscriptions.push(vscode.languages.registerEvaluatableExpressionProvider("fos", {
    provideEvaluatableExpression(document, position) {
      const VARIABLE_REGEXP = /\$[a-z][a-z0-9]*/ig;
      const line = document.lineAt(position.line).text;
      let m;
      while (m = VARIABLE_REGEXP.exec(line)) {
        const varRange = new vscode.Range(position.line, m.index, position.line, m.index + m[0].length);
        if (varRange.contains(position)) {
          return new vscode.EvaluatableExpression(varRange);
        }
      }
      return void 0;
    }
  }));
  context.subscriptions.push(vscode.languages.registerInlineValuesProvider("fos", {
    provideInlineValues(document, viewport, context2) {
      const allValues = [];
      for (let l = viewport.start.line; l <= context2.stoppedLocation.end.line; l++) {
        const line = document.lineAt(l);
        var regExp = /\$([a-z][a-z0-9]*)/ig;
        do {
          var m = regExp.exec(line.text);
          if (m) {
            const varName = m[1];
            const varRange = new vscode.Range(l, m.index, l, m.index + varName.length);
            allValues.push(new vscode.InlineValueVariableLookup(varRange, varName, false));
          }
        } while (m);
      }
      return allValues;
    }
  }));
}
var FosConfigurationProvider = class {
  /**
   * Massage a debug configuration just before a debug session is being launched,
   * e.g. add all missing attributes to the debug configuration.
   */
  async resolveDebugConfiguration(folder, config, token) {
    if (!config.type && !config.request && !config.name) {
      const editor = vscode.window.activeTextEditor;
      if (editor && editor.document.languageId === "fos") {
        config.type = "fos";
        config.name = "Launch";
        config.request = "launch";
        config.program = "${file}";
        config.stopOnEntry = true;
      }
    }
    if (config.request === "launch" && !config.program) {
      return vscode.window.showInformationMessage("Cannot find a program to debug").then((_) => {
        return void 0;
      });
    }
    if (config.request === "attach" && !config.endpoint) {
      const discoveryPort = typeof config.discoveryPort === "number" && config.discoveryPort > 0 ? config.discoveryPort : 43001;
      const discoveryTimeoutMs = typeof config.discoveryTimeoutMs === "number" && config.discoveryTimeoutMs > 0 ? config.discoveryTimeoutMs : 800;
      let targets;
      try {
        targets = await discoverTargets(discoveryPort, discoveryTimeoutMs);
      } catch (error) {
        const message = error.message;
        const endpoint = await vscode.window.showInputBox({
          title: "Choose endpoint to attach:",
          prompt: `Attach discovery unavailable (${message}). Enter endpoint manually (e.g. tcp://127.0.0.1:43042).`,
          placeHolder: "tcp://127.0.0.1:43042",
          ignoreFocusOut: true
        });
        if (!endpoint || endpoint.trim().length === 0) {
          return void 0;
        }
        config.endpoint = endpoint.trim();
        return config;
      }
      if (targets.length === 0) {
        await vscode.window.showWarningMessage(`No debugger targets discovered on UDP port ${discoveryPort}.`);
        return void 0;
      }
      targets.sort((left, right) => left.endpoint.localeCompare(right.endpoint));
      const pickItems = targets.map((target) => {
        const role = target.targetName ? target.targetName.charAt(0).toUpperCase() + target.targetName.slice(1) : "Runtime";
        return {
          label: `${role}: ${target.endpoint}`,
          target
        };
      });
      pickItems.push({
        label: "Cancel",
        cancel: true
      });
      const picked = await vscode.window.showQuickPick(pickItems, {
        title: "Choose endpoint to attach:",
        placeHolder: "Choose endpoint to attach:",
        ignoreFocusOut: true
      });
      if (!picked || picked.cancel || !picked.target) {
        return void 0;
      }
      config.endpoint = picked.target.endpoint;
      config.processId = picked.target.processId;
    }
    return config;
  }
};
var workspaceFileAccessor = {
  isWindows: typeof process !== "undefined" && process.platform === "win32",
  async readFile(path) {
    let uri;
    try {
      uri = pathToUri(path);
    } catch (e) {
      return new TextEncoder().encode(`cannot read '${path}'`);
    }
    return await vscode.workspace.fs.readFile(uri);
  },
  async writeFile(path, contents) {
    await vscode.workspace.fs.writeFile(pathToUri(path), contents);
  }
};
function pathToUri(path) {
  try {
    return vscode.Uri.file(path);
  } catch (e) {
    return vscode.Uri.parse(path);
  }
}

// src/web-extension.ts
function activate(context) {
  activateFosDebug(context);
}
function deactivate() {
}
//# sourceMappingURL=web-extension.js.map
