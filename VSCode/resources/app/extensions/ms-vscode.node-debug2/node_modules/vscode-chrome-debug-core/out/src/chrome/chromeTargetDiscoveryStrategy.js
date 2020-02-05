"use strict";
/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const utils = require("../utils");
const executionTimingsReporter_1 = require("../executionTimingsReporter");
const chromeUtils = require("./chromeUtils");
const nls = require("vscode-nls");
const localize = nls.loadMessageBundle(__filename);
class Version {
    constructor(_major, _minor) {
        this._major = _major;
        this._minor = _minor;
    }
    static parse(versionString) {
        const majorAndMinor = versionString.split('.');
        const major = parseInt(majorAndMinor[0], 10);
        const minor = parseInt(majorAndMinor[1], 10);
        return new Version(major, minor);
    }
    static unknownVersion() {
        return new Version(0, 0); // Using 0.0 will make behave isAtLeastVersion as if this was the oldest possible version
    }
    isAtLeastVersion(major, minor) {
        return this._major > major || (this._major === major && this._minor >= minor);
    }
}
exports.Version = Version;
class TargetVersions {
    constructor(protocol, browser) {
        this.protocol = protocol;
        this.browser = browser;
    }
}
exports.TargetVersions = TargetVersions;
class ChromeTargetDiscovery {
    constructor(_logger, _telemetry) {
        this.events = new executionTimingsReporter_1.StepProgressEventsEmitter();
        this.logger = _logger;
        this.telemetry = _telemetry;
    }
    getTarget(address, port, targetFilter, targetUrl) {
        return __awaiter(this, void 0, void 0, function* () {
            const targets = yield this.getAllTargets(address, port, targetFilter, targetUrl);
            if (targets.length > 1) {
                this.logger.log('Warning: Found more than one valid target page. Attaching to the first one. Available pages: ' + JSON.stringify(targets.map(target => target.url)));
            }
            const selectedTarget = targets[0];
            this.logger.verbose(`Attaching to target: ${JSON.stringify(selectedTarget)}`);
            this.logger.verbose(`WebSocket Url: ${selectedTarget.webSocketDebuggerUrl}`);
            return selectedTarget;
        });
    }
    getAllTargets(address, port, targetFilter, targetUrl) {
        return __awaiter(this, void 0, void 0, function* () {
            const targets = yield this._getTargets(address, port);
            /* __GDPR__
               "targetCount" : {
                  "numTargets" : { "classification": "SystemMetaData", "purpose": "FeatureInsight", "isMeasurement": true },
                  "${include}": [ "${DebugCommonProperties}" ]
               }
             */
            this.telemetry.reportEvent('targetCount', { numTargets: targets.length });
            if (!targets.length) {
                return utils.errP(localize(0, null));
            }
            return this._getMatchingTargets(targets, targetFilter, targetUrl);
        });
    }
    _getVersionData(address, port) {
        return __awaiter(this, void 0, void 0, function* () {
            const url = `http://${address}:${port}/json/version`;
            this.logger.log(`Getting browser and debug protocol version via ${url}`);
            const jsonResponse = yield utils.getURL(url, { headers: { Host: 'localhost' } })
                .catch(e => this.logger.log(`There was an error connecting to ${url} : ${e.message}`));
            try {
                if (jsonResponse) {
                    const response = JSON.parse(jsonResponse);
                    const protocolVersionString = response['Protocol-Version'];
                    const browserWithPrefixVersionString = response.Browser;
                    this.logger.log(`Got browser version: ${browserWithPrefixVersionString}`);
                    this.logger.log(`Got debug protocol version: ${protocolVersionString}`);
                    /* __GDPR__
                       "targetDebugProtocolVersion" : {
                           "debugProtocolVersion" : { "classification": "SystemMetaData", "purpose": "FeatureInsight" },
                           "${include}": [ "${DebugCommonProperties}" ]
                       }
                     */
                    const chromePrefix = 'Chrome/';
                    let browserVersion = Version.unknownVersion();
                    if (browserWithPrefixVersionString.startsWith(chromePrefix)) {
                        const browserVersionString = browserWithPrefixVersionString.substr(chromePrefix.length);
                        browserVersion = Version.parse(browserVersionString);
                    }
                    this.telemetry.reportEvent('targetDebugProtocolVersion', { debugProtocolVersion: response['Protcol-Version'] });
                    return new TargetVersions(Version.parse(protocolVersionString), browserVersion);
                }
            }
            catch (e) {
                this.logger.log(`Didn't get a valid response for /json/version call. Error: ${e.message}. Response: ${jsonResponse}`);
            }
            return new TargetVersions(Version.unknownVersion(), Version.unknownVersion());
        });
    }
    _getTargets(address, port) {
        return __awaiter(this, void 0, void 0, function* () {
            // Get the browser and the protocol version
            const version = this._getVersionData(address, port);
            /* __GDPR__FRAGMENT__
               "StepNames" : {
                  "Attach.RequestDebuggerTargetsInformation" : { "classification": "SystemMetaData", "purpose": "FeatureInsight" }
               }
             */
            this.events.emitStepStarted('Attach.RequestDebuggerTargetsInformation');
            const checkDiscoveryEndpoint = (url) => {
                this.logger.log(`Discovering targets via ${url}`);
                return utils.getURL(url, { headers: { Host: 'localhost' } });
            };
            const jsonResponse = yield checkDiscoveryEndpoint(`http://${address}:${port}/json/list`)
                .catch(() => checkDiscoveryEndpoint(`http://${address}:${port}/json`))
                .catch(e => utils.errP(localize(1, null, e.message)));
            /* __GDPR__FRAGMENT__
               "StepNames" : {
                  "Attach.ProcessDebuggerTargetsInformation" : { "classification": "SystemMetaData", "purpose": "FeatureInsight" }
               }
             */
            this.events.emitStepStarted('Attach.ProcessDebuggerTargetsInformation');
            let responseArray;
            try {
                responseArray = JSON.parse(jsonResponse);
            }
            catch (e) {
                try {
                    // If it fails to parse, this is possibly https://github.com/electron/electron/issues/11524.
                    // Workaround, just snip out the title property and try again.
                    // Since we don't know exactly which characters might break JSON.parse or why, we can't give a more targeted fix.
                    responseArray = JSON.parse(removeTitleProperty(jsonResponse));
                }
                catch (e) {
                    return utils.errP(localize(2, null, e.message, jsonResponse));
                }
            }
            if (Array.isArray(responseArray)) {
                return responseArray
                    .map(target => {
                    this._fixRemoteUrl(address, port, target);
                    target.version = version;
                    return target;
                });
            }
            else {
                return utils.errP(localize(3, null, jsonResponse));
            }
        });
    }
    _getMatchingTargets(targets, targetFilter, targetUrl) {
        let filteredTargets = targetFilter ?
            targets.filter(targetFilter) : // Apply the consumer-specific target filter
            targets;
        // If a url was specified, try to filter to that url
        filteredTargets = targetUrl ?
            chromeUtils.getMatchingTargets(filteredTargets, targetUrl) :
            filteredTargets;
        if (!filteredTargets.length) {
            throw new Error(localize(4, null, targetUrl, JSON.stringify(targets.map(target => target.url))));
        }
        // If all possible targets appear to be attached to have some other devtool attached, then fail
        const targetsWithWSURLs = filteredTargets.filter(target => !!target.webSocketDebuggerUrl);
        if (!targetsWithWSURLs.length) {
            throw new Error(localize(5, null, filteredTargets[0].url));
        }
        return targetsWithWSURLs;
    }
    _fixRemoteUrl(remoteAddress, remotePort, target) {
        if (target.webSocketDebuggerUrl) {
            const addressMatch = target.webSocketDebuggerUrl.match(/ws:\/\/([^/]+)\/?/);
            if (addressMatch) {
                const replaceAddress = `${remoteAddress}:${remotePort}`;
                target.webSocketDebuggerUrl = target.webSocketDebuggerUrl.replace(addressMatch[1], replaceAddress);
            }
        }
        return target;
    }
}
exports.ChromeTargetDiscovery = ChromeTargetDiscovery;
function removeTitleProperty(targetsResponse) {
    return targetsResponse.replace(/"title": "[^"]+",?/, '');
}
exports.removeTitleProperty = removeTitleProperty;

//# sourceMappingURL=chromeTargetDiscoveryStrategy.js.map
