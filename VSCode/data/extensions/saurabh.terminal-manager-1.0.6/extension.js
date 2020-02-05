// The module 'vscode' contains the VS Code extensibility API
// Import the module and reference it with the alias vscode in your code below
const fs = require('fs');
const TerminalManager = require('./commands.js')
// this method is called when your extension is activated
// your extension is activated the very first time the command is executed

/**
 * @param {vscode.ExtensionContext} context
 */

// This is supperr hacky way of creating Trees I got no fking clue how to use it in real.

function activate(context) {
	let terminalManager = new TerminalManager(context);

	terminalManager.loadTerminals()
		.then(()=>{
			return terminalManager.loadCommands();
		})
		.then((subscriptions) => {
			fs.watchFile(context.globalStoragePath+'//terminals.json',function(event){
				console.log(event);
				for(let disposable of subscriptions){
					disposable.dispose();
				}
				// tree.dispose();
				terminalManager.loadTerminals();
				terminalManager.loadCommands();
			})
		})
}
exports.activate = activate;

// this method is called when your extension is deactivated
function deactivate() {}

module.exports = {
	activate,
	deactivate
}
