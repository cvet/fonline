"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const child_process_1 = require("child_process");
const vscode = require("vscode");
const GNOME = "gnome";
const GNOME_CLASSIC = "gnome-classic";
const KDE_PLASMA = "kde-plasma";
const platforms = {
    mac: "darwin",
    win: "win32",
    linux: "linux"
};
const dictionary = {
    EXTENSION_NAME: "open-native-terminal",
    SETTINGS_NAME: "use-default-terminal",
    WRONG_PATH: "Oops, the path is wrong, please try again",
    EMPTY_PATH: "Oops, the path is empty, please try again"
};
const getSettings = (name) => {
    const settings = vscode.workspace.getConfiguration(dictionary.EXTENSION_NAME);
    if (!settings)
        return;
    return settings.get(name);
};
const runMacOSTerminal = (path) => {
    const defaultTerminal = getSettings(dictionary.SETTINGS_NAME);
    child_process_1.exec(`open -a ${defaultTerminal || "Terminal.app"} "${path}"`);
};
exports.launchTerminal = (path) => {
    switch (process.platform) {
        case platforms.mac:
            runMacOSTerminal(path);
        case platforms.win:
            child_process_1.exec(`start cmd @cmd /k cd ${path}`);
        default:
            runLinuxTerminal(path);
    }
};
const runLinuxTerminal = (path) => {
    const defaultTerminal = getSettings(dictionary.SETTINGS_NAME);
    if (defaultTerminal) {
        child_process_1.exec(`cd ${path} && ${defaultTerminal}`);
        return;
    }
    switch (true) {
        case process.env.DESKTOP_SESSION === GNOME ||
            process.env.DESKTOP_SESSION === GNOME_CLASSIC:
            child_process_1.exec(`cd ${path} && gnome-terminal`);
            break;
        case process.env.DESKTOP_SESSION === KDE_PLASMA:
            child_process_1.exec(`cd ${path} && konsole`);
            break;
        default:
            child_process_1.exec(`cd ${path} && x-terminal-emulator`);
    }
};
//# sourceMappingURL=runSystemTerminal.js.map