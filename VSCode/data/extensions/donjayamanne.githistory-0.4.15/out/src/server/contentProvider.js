"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const querystring = require("query-string");
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
        const locale = decodeURIComponent(query.locale.toString());
        const file = decodeURIComponent(query.file.toString());
        return this.generateResultsView(port, internalPort, id, branchName, branchSelection, locale, file);
    }
    generateResultsView(port, internalPort, id, branchName, branchSelection, locale, file) {
        // Fix for issue #669 "Results Panel not Refreshing Automatically" - always include a unique time
        // so that the content returned is different. Otherwise VSCode will not refresh the document since it
        // thinks that there is nothing to be updated.
        // this.provided = true;
        const timeNow = ''; // new Date().getTime();
        const queryArgs = [
            `id=${id}`,
            `branchName=${encodeURIComponent(branchName)}`,
            `file=${encodeURIComponent(file)}`,
            'theme=',
            `branchSelection=${branchSelection}`,
            `locale=${encodeURIComponent(locale)}`
        ];
        // tslint:disable-next-line:no-http-string
        const uri = `http://localhost:${port}/?_&${queryArgs.join('&')}`;
        this.serviceContainer.getAll(types_1.ILogService)
            .forEach(logger => {
            logger.log(`Server running on ${uri}`);
        });
        return `<!DOCTYPE html>
                <html>
                    <head>
                        <style type="text/css"> html, body{ height:100%; width:100%; overflow:hidden; padding:0;margin:0; }</style>
                        <meta http-equiv="Content-Security-Policy" content="default-src 'self' http://localhost:* http://127.0.0.1:* 'unsafe-inline' 'unsafe-eval';" />
                    <title>Git History</title>
                    <script type="text/javascript">
                        function frameLoaded() {
                            console.log('Sending styles through postMessage');
                            var styleText = document.getElementsByTagName("html")[0].style.cssText;
                            // since the nested iframe may become the origin http://127.0.0.1:<randomPort>
                            // it is necessary to use asterisk
                            document.getElementById('myframe').contentWindow.postMessage(styleText,"*");
                        }

                        function start() {
                            var queryArgs = [
                                'id=${id}',
                                'branchName=${encodeURIComponent(branchName)}',
                                'file=${encodeURIComponent(file)}',
                                'branchSelection=${branchSelection}',
                                'locale=${encodeURIComponent(locale)}',
                                'theme=' + document.body.className
                            ];

                            document.getElementById('myframe').src = 'http://localhost:${internalPort}/?_=${timeNow}&' + queryArgs.join('&');
                            document.getElementById('myframe').onload = frameLoaded;

                            // use mutation observer to check if style attribute has changed
                            // this usually occurs when changing the vscode theme
                            var observer = new MutationObserver(frameLoaded);
                            observer.observe(document.getElementsByTagName("html")[0], { attributes: true });
                        }
                        </script>
                    </head>
                    <body onload="start()">
                        <iframe id="myframe" frameborder="0" style="border: 0px solid transparent;height:100%;width:100%;" src="" seamless></iframe>
                    </body>
                </html>`;
    }
}
exports.ContentProvider = ContentProvider;
//# sourceMappingURL=contentProvider.js.map