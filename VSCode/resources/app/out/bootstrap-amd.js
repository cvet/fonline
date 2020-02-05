/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
"use strict";const loader=require("./vs/loader"),bootstrap=require("./bootstrap"),nlsConfig=bootstrap.setupNLS();loader.config({baseUrl:bootstrap.uriFromPath(__dirname),catchError:!0,nodeRequire:require,nodeMain:__filename,"vs/nls":nlsConfig}),(process.env.ELECTRON_RUN_AS_NODE||process.versions.electron)&&loader.define("fs",["original-fs"],(function(o){return o})),nlsConfig.pseudo&&loader(["vs/nls"],(function(o){o.setPseudoTranslation(nlsConfig.pseudo)})),exports.load=function(o,e,n){o&&(process.env.VSCODE_NODE_CACHED_DATA_DIR&&loader.config({nodeCachedData:{path:process.env.VSCODE_NODE_CACHED_DATA_DIR,seed:o}}),loader([o],e=e||function(){},n=n||function(o){console.error(o)}))};
//# sourceMappingURL=https://ticino.blob.core.windows.net/sourcemaps/26076a4de974ead31f97692a0d32f90d735645c0/core/bootstrap-amd.js.map
