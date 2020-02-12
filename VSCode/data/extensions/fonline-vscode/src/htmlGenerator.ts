export class HtmlGenerator {
    private _content: string;

    constructor(title: string) {
        this._content = `<!DOCTYPE html>
<html lang="en">
    <head>
        <title>Dashboard</title>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/uikit/3.0.3/css/uikit.min.css" />
        <link href="css/styles.css" rel="stylesheet">
    </head>
    <body>
        <script src="https://cdnjs.cloudflare.com/ajax/libs/uikit/3.1.6/js/uikit.min.js"
            integrity="sha256-v789mr/zBbgR53mfydCI78CSAF+9+nRqu+JRfs1UPg0=" crossorigin="anonymous"></script>
        <script src="https://cdnjs.cloudflare.com/ajax/libs/uikit/3.1.6/js/uikit-icons.min.js"
            integrity="sha256-l+AmZGiFz41J+gms80qC7faslJDberZDhjEsmDmQy8s=" crossorigin="anonymous"></script>
        <div class="uk-position-center">`;
    }

    addSpinner() {
        this._content += `<div uk-spinner="ratio: 3"></div>`;
    }

    addLeadText(text: string) {
        this._content += `<div class="uk-text-lead uk-text-center">${text}<br/></div>`;
    }

    addText(text: string) {
        this._content += `<div class="uk-text-normal uk-text-center">${text}<br/></div>`;
    }

    addStatusMessage(title: string, message: string) {
        this._content += `<dl class="uk-description-list"><dt>${title}</dt><dd>${message}</dd></dl>`;
    }

    finalize(): string {
        this._content += `</div></body></html>`;
        return this._content;
    }
}
