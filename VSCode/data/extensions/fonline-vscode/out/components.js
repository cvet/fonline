"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const vscode = require("vscode");
class ComponentsView {
    constructor(context) {
        const view = vscode.window.createTreeView('componentsView', {
            treeDataProvider: aNodeWithIdTreeDataProvider(),
            showCollapseAll: true
        });
        vscode.commands.registerCommand('componentsView.reveal', () => __awaiter(this, void 0, void 0, function* () {
            const key = yield vscode.window.showInputBox({ placeHolder: 'Type the label of the item to reveal' });
            if (key) {
                yield view.reveal({ key }, { focus: true, select: false, expand: true });
            }
        }));
        vscode.commands.registerCommand('componentsView.changeTitle', () => __awaiter(this, void 0, void 0, function* () {
            const title = yield vscode.window.showInputBox({
                prompt: 'Type the new title for the Test View',
                placeHolder: view.title
            });
            if (title) {
                view.title = title;
            }
        }));
    }
}
exports.ComponentsView = ComponentsView;
const tree = {
    'a': {
        'aa': {
            'aaa': {
                'aaaa': {
                    'aaaaa': {
                        'aaaaaa': {}
                    }
                }
            }
        },
        'ab': {}
    },
    'b': { 'ba': {}, 'bb': {} }
};
let nodes = {};
function aNodeWithIdTreeDataProvider() {
    return {
        getChildren: (element) => {
            return getChildren(element ? element.key : undefined)
                .map(key => getNode(key));
        },
        getTreeItem: (element) => {
            const treeItem = getTreeItem(element.key);
            treeItem.id = element.key;
            return treeItem;
        },
        getParent: ({ key }) => {
            const parentKey = key.substring(0, key.length - 1);
            return parentKey ? new Key(parentKey) : undefined;
        }
    };
}
function getChildren(key) {
    if (!key) {
        return Object.keys(tree);
    }
    let treeElement = getTreeElement(key);
    if (treeElement) {
        return Object.keys(treeElement);
    }
    return [];
}
function getTreeItem(key) {
    const treeElement = getTreeElement(key);
    return {
        label: {
            label: key,
            highlights: key.length > 1 ? [[key.length - 2, key.length - 1]] : void 0
        },
        tooltip: `Tooltip for ${key}`,
        collapsibleState: treeElement && Object.keys(treeElement).length ?
            vscode.TreeItemCollapsibleState.Collapsed :
            vscode.TreeItemCollapsibleState.None
    };
}
function getTreeElement(element) {
    let parent = tree;
    for (let i = 0; i < element.length; i++) {
        parent = parent[element.substring(0, i + 1)];
        if (!parent) {
            return null;
        }
    }
    return parent;
}
function getNode(key) {
    if (!nodes[key]) {
        nodes[key] = new Key(key);
    }
    return nodes[key];
}
class Key {
    constructor(key) {
        this.key = key;
    }
}
//# sourceMappingURL=components.js.map