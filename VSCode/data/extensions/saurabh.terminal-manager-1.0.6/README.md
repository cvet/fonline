# ![](https://github.com/saurabhdaware/vscode-terminal-manager/raw/master/resources/terminal.png) Terminal Manager

Terminal Manager is a visual studio code extension to switch between various terminals.

[![Current version of Terminal Manager](https://vsmarketplacebadge.apphb.com/version-short/saurabh.terminal-manager.svg)](https://marketplace.visualstudio.com/items?itemName=saurabh.terminal-manager) [![Current version of Terminal Manager](https://vsmarketplacebadge.apphb.com/downloads/saurabh.terminal-manager.svg)](https://marketplace.visualstudio.com/items?itemName=saurabh.terminal-manager) [![Current version of Terminal Manager](https://vsmarketplacebadge.apphb.com/rating-short/saurabh.terminal-manager.svg)](https://marketplace.visualstudio.com/items?itemName=saurabh.terminal-manager)

[![Install extension button](https://res.cloudinary.com/saurabhdaware/image/upload/v1564401766/saurabhdaware.in/otherAssets/iebutton.png)](https://marketplace.visualstudio.com/items?itemName=saurabh.terminal-manager)

## Features

By default vscode lets us define only one link of terminal from settings.

Using this extension you can provide an array of terminals and then you can switch between them from a new Terminal icon in activity bar.

![](https://github.com/saurabhdaware/vscode-terminal-manager/raw/master/screenshots/ss1.png)
![](https://github.com/saurabhdaware/vscode-terminal-manager/raw/master/screenshots/ss2.png)

## Extension Settings

Click the edit icon ![](https://github.com/saurabhdaware/vscode-terminal-manager/raw/master/resources/edit.png) in Terminal Manager activity bar to edit the terminal settings.

Here's what sample terminals.json looks like. (Note : Ubuntu configs will only work if you have wsl installed in your windows)

### In Windows :
```json
[
    {
        "label":"Windows",
        "shellPath":"C://Windows//System32//cmd.exe",
        "shellArgs":[
            "/K",
            "echo Heya!"
        ]
    },
    {
        "label":"Ubuntu",
        "shellPath":"C://Windows//System32//bash.exe"
    }
]
```

### In Linux and OSX:
```json
[
    {
        "label":"Login bash",
        "shellPath":"/bin/bash",
        "shellArgs":["-l"]
    },
    {
        "label": "Restricted Bash",
        "shellPath": "/bin/rbash"
    },
    {
        "label":"sh",
        "shellPath":"/bin/sh"
    }
]
```

## Contribution

- Check for the issues on https://github.com/saurabhdaware/vscode-terminal-manager/issues
- Fork the project
- Finish your changes and make Pull Request to Master branch of https://github.com/saurabhdaware/vscode-terminal-manager

## Local Development

- Fork this project
- `git clone https://github.com/{your username}/vscode-terminal-manager`
- `cd vscode-terminal-manager`
- `npm install`
- Open the project in Visual Studio Code and press `Ctrl + f5` to start Extension host.

## Release Notes

### 1.0.0 - 1.0.4

- Initial Release of Terminal Manager and Minor Updates.

### 1.0.5

- Fixed Path error that was thrown in Ubuntu when edit button was clicked

### 1.0.6

- shellArgs can be added to terminal (Thanks to [#PR5](https://github.com/saurabhdaware/vscode-terminal-manager/pull/5) by [4a-42](https://github.com/4a-42))
- added default terminals.json configs for linux and osx


----

***Dont forget to star my github repository https://github.com/saurabhdaware/vscode-terminal-manager***

***Enjoy 🎉***
