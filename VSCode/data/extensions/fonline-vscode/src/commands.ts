/*const fs = require('fs');
const vscode = require('vscode');
const path = require('path');
const process = require('process');

class TerminalTree{
	constructor(terminals){
		this.terminals = terminals;
		this.i = -1;
	}
	getChildren(element){
		console.log(element);
		return this.terminals;
	}

	getTreeItem(element){
		console.log(element);
		this.i++;
		let terminal = this.terminals[this.i];
		return {
			label:terminal.label,
			tooltip:`Open ${terminal.label}`,
			command:{command:terminal.command,title:terminal.commandTitle}
		}
	}

	getParent(element){
		console.log(element);
		return 'd';
	}
}


class TerminalManager{
    constructor(context){
        this.context = context;
        this.terminalsConfig = '';
        // Adjust configurations as per the OS
        if(process.platform == 'linux'){
            this.terminalsConfig = `[
    {
        "label":"Login bash",
        "shellPath":"/bin/bash",
        "shellArgs":["-l"]
    },
    {
        "label":"sh",
        "shellPath":"/bin/sh"
    }
]`
        }else if(process.platform == 'win32'){
            this.terminalsConfig = `[
    {
        "label":"Windows Powershell",
        "shellPath":"C://Windows//System32//WindowsPowerShell//v1.0//powershell.exe"
    },
    {
        "label":"CMD",
        "shellPath":"C://Windows//System32//cmd.exe",
        "shellArgs":["/K", "echo Heya!"]
    }
]`
        }else{
            this.terminalsConfig = `[
    {
        "label":"Bash",
        "shellPath":"/bin/bash"
    }
]`
        }
    }

    getTerminals(){
        let terminals;
        console.log("Platform");
        console.log(process.platform);
        try{
            const fileContent = fs.readFileSync(this.context.globalStoragePath+'/terminals.json');
            console.log(fileContent);
            terminals = JSON.parse(fileContent);
            terminals = terminals.map(terminal => {
                terminal.command = 'extension.'+terminal.label.toLowerCase().replace(/ /g,'');
                terminal.commandTitle = terminal.label;
                return terminal;
            })
        }catch(e){
            console.log(e);
            fs.promises.mkdir(this.context.globalStoragePath,{recursive:true})
                .then(() => {
                    return fs.promises.writeFile(this.context.globalStoragePath+'/terminals.json',this.terminalsConfig)
                })
                .catch((e) => {
                    console.log(e);
                })

            terminals = [];
        }

        return terminals;
    }

    loadTerminals(){
        let terminals = this.getTerminals(this.context);
        // vscode.workspace.onDidChangeTextDocument()
        let tree = vscode.window.createTreeView('terminal-manager', {treeDataProvider:  new TerminalTree(terminals)});

        this.context.subscriptions.push(tree);
        // vscode.window.showInformationMessage('Congratulations, your extension "terminal-manager" is now active!');
        for(let terminal of terminals){
            let cmd = vscode.commands.registerCommand(terminal.command, function () {
                let terminalInstance = vscode.window.createTerminal(terminal.commandTitle,terminal.shellPath,terminal.shellArgs);
                terminalInstance.show();
            });
            this.context.subscriptions.push(cmd);
        }

        return Promise.resolve(this.context.subscriptions)
    }

    loadCommands(){
        let editTerminals = vscode.commands.registerCommand('extension.editTerminals',() => {
            let terminalsJsonPath = path.join(this.context.globalStoragePath,'terminals.json');
            vscode.window.showTextDocument(vscode.Uri.file(terminalsJsonPath))
                .catch(console.log);
        })
        this.context.subscriptions.push(editTerminals);

        return Promise.resolve(this.context.subscriptions);
    }

}

module.exports = TerminalManager;*/
