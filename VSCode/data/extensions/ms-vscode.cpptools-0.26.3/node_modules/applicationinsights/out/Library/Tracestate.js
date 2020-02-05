"use strict";
/**
 * Helper class to manage parsing and strict-validation of tracestate header. W3C tracestate spec
 * is documented at https://www.w3.org/TR/trace-context/#header-value
 * @class Tracestate
 */
var Tracestate = (function () {
    // if true, performs strict tracestate header checking, else just passes it along
    function Tracestate(id) {
        this.fieldmap = [];
        if (!id) {
            return;
        }
        this.fieldmap = this.parseHeader(id);
    }
    Tracestate.prototype.toString = function () {
        var fieldarr = this.fieldmap;
        if (!fieldarr || fieldarr.length == 0) {
            return null;
        }
        return fieldarr.join(", ");
    };
    Tracestate.validateKeyChars = function (key) {
        var keyParts = key.split("@");
        if (keyParts.length == 2) {
            // Parse for tenant@vendor format
            var tenant = keyParts[0].trim();
            var vendor = keyParts[1].trim();
            var tenantValid = Boolean(tenant.match(/^[\ ]?[a-z0-9\*\-\_/]{1,241}$/));
            var vendorValid = Boolean(vendor.match(/^[\ ]?[a-z0-9\*\-\_/]{1,14}$/));
            return tenantValid && vendorValid;
        }
        else if (keyParts.length == 1) {
            // Parse for standard key format
            return Boolean(key.match(/^[\ ]?[a-z0-9\*\-\_/]{1,256}$/));
        }
        return false;
    };
    Tracestate.prototype.parseHeader = function (id) {
        var res = [];
        var keydeduper = {};
        var parts = id.split(",");
        if (parts.length > 32)
            return null;
        for (var _i = 0, parts_1 = parts; _i < parts_1.length; _i++) {
            var rawPart = parts_1[_i];
            var part = rawPart.trim(); // trim out whitespace
            if (part.length === 0) {
                continue; // Discard empty pairs, but keep the rest of this tracestate
            }
            var pair = part.split("=");
            // pair should contain exactly one "="
            if (pair.length !== 2) {
                return null; // invalid pair: discard entire tracestate
            }
            // Validate length and charset of this key
            if (!Tracestate.validateKeyChars(pair[0])) {
                return null;
            }
            // Assert uniqueness of this key
            if (keydeduper[pair[0]]) {
                return null; // duplicate key: discard entire tracestate
            }
            else {
                keydeduper[pair[0]] = true;
            }
            // All checks passed -- add this part
            res.push(part);
        }
        return res;
    };
    Tracestate.strict = true;
    return Tracestate;
}());
module.exports = Tracestate;
//# sourceMappingURL=Tracestate.js.map