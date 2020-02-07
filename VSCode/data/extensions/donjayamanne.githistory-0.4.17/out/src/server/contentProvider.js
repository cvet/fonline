"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const querystring = require("query-string");
const vscode_1 = require("vscode");
const types_1 = require("../common/types");
class ContentProvider {
    constructor(serviceContainer) {
        this.serviceContainer = serviceContainer;
    }
    provideTextDocumentContent(uri, _token) {
        const query = querystring.parse(uri.query.toString());
        const port = parseInt(query.port.toString(), 10);
        const internalPort = parseInt(query.internalPort.toString(), 10);
        const id = query.id;
        const branchName = query.branchName ? decodeURIComponent(query.branchName) : '';
        const branchSelection = parseInt(query.branchSelection.toString(), 10);
        const file = decodeURIComponent(query.file.toString());
        const queryArgs = [
            `id=${id}`,
            `branchName=${encodeURIComponent(branchName)}`,
            `file=${encodeURIComponent(file)}`,
            `branchSelection=${branchSelection}`
        ];
        const config = vscode_1.workspace.getConfiguration('gitHistory');
        const settings = {
            id,
            branchName,
            file,
            branchSelection
        };
        this.serviceContainer.getAll(types_1.ILogService)
            .forEach(logger => {
            logger.log(`Server running on http://localhost:${port}/?${queryArgs.join('&')}`);
            logger.log(`Webview port: ${internalPort}`);
        });
        return `<!DOCTYPE html>
                <html>
                    <head>
                        <style type="text/css"> html, body{ height:100%; width:100%; overflow:hidden; padding:0;margin:0; }</style>
                        <meta http-equiv="Content-Security-Policy" content="default-src 'self' http://localhost:* http://127.0.0.1:* 'unsafe-inline' 'unsafe-eval'; img-src *" />
                        <link rel='stylesheet' type='text/css' href='http://localhost:${internalPort}/bundle.css' />
                    <title>Git History</title>
                    <script type="text/javascript">
                        window['configuration'] = ${JSON.stringify(config)};
                        window['settings'] = ${JSON.stringify(settings)};
                        window['locale'] = '${vscode_1.env.language}';
                        window['server_url'] = 'http://localhost:${internalPort}/';

                        // Since CORS is not permitted for redirects and
                        // a redirect from http://localhost:<internalPort> to http://127.0.0.1:<randomPort>
                        // may occur in some cases (proberly due to proxy bypass)
                        // it is necessary to use the "redirected" URL.
                        // This only applies to other methods than "GET" (E.g. POST)
                        // Further info: https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS/Errors/CORSExternalRedirectNotAllowed

                        var request = new XMLHttpRequest();
                        request.open('GET', 'http://localhost:${internalPort}/', true);
                        request.onload = function() {
                            // get the redirected URL
                            window['server_url'] = this.responseURL;
                            console.log("Expected URL: " + this.responseURL + "?${queryArgs.join('&')}");

                            // Load the react app
                            var script = document.createElement('script');
                            script.src = this.responseURL + '/bundle.js';
                            document.head.appendChild(script);
                        };
                        request.send();

                        </script>
                    </head>
                    <body>
                        <div id="root"></div>
                    </body>
                </html>`;
    }
}
exports.ContentProvider = ContentProvider;
//# sourceMappingURL=contentProvider.js.map