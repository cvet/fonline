"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const path = require("path");
const resourcesPath = path.join(__dirname, '..', '..', '..', 'resources');
exports.FolderIcon = {
    dark: path.join(resourcesPath, 'darkTheme', 'folder.svg'),
    light: path.join(resourcesPath, 'lightTheme', 'folder.svg')
};
exports.AddedIcon = {
    light: path.join(resourcesPath, 'icons', 'light', 'status-added.svg'),
    dark: path.join(resourcesPath, 'icons', 'dark', 'status-added.svg')
};
exports.RemovedIcon = {
    light: path.join(resourcesPath, 'icons', 'light', 'status-deleted.svg'),
    dark: path.join(resourcesPath, 'icons', 'dark', 'status-deleted.svg')
};
exports.ModifiedIcon = {
    light: path.join(resourcesPath, 'icons', 'light', 'status-modified.svg'),
    dark: path.join(resourcesPath, 'icons', 'dark', 'status-modified.svg')
};
exports.FileIcon = {
    dark: path.join(resourcesPath, 'darkTheme', 'document.svg'),
    light: path.join(resourcesPath, 'lightTheme', 'document.svg')
};
exports.RenameIcon = {
    light: path.join(resourcesPath, 'icons', 'light', 'status-renamed.svg'),
    dark: path.join(resourcesPath, 'icons', 'dark', 'status-renamed.svg')
};
//# sourceMappingURL=nodeIcons.js.map