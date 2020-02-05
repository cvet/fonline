"use strict";
/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
Object.defineProperty(exports, "__esModule", { value: true });
class Handles {
    constructor(startHandle) {
        this.START_HANDLE = 1000;
        this._handleMap = new Map();
        this._nextHandle = typeof startHandle === 'number' ? startHandle : this.START_HANDLE;
    }
    reset() {
        this._nextHandle = this.START_HANDLE;
        this._handleMap = new Map();
    }
    create(value) {
        var handle = this._nextHandle++;
        this._handleMap.set(handle, value);
        return handle;
    }
    get(handle, dflt) {
        return this._handleMap.get(handle) || dflt;
    }
}
exports.Handles = Handles;
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiaGFuZGxlcy5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbIi4uL3NyYy9oYW5kbGVzLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7QUFBQTs7O2dHQUdnRzs7QUFFaEcsTUFBYSxPQUFPO0lBT25CLFlBQW1CLFdBQW9CO1FBTC9CLGlCQUFZLEdBQUcsSUFBSSxDQUFDO1FBR3BCLGVBQVUsR0FBRyxJQUFJLEdBQUcsRUFBYSxDQUFDO1FBR3pDLElBQUksQ0FBQyxXQUFXLEdBQUcsT0FBTyxXQUFXLEtBQUssUUFBUSxDQUFDLENBQUMsQ0FBQyxXQUFXLENBQUMsQ0FBQyxDQUFDLElBQUksQ0FBQyxZQUFZLENBQUM7SUFDdEYsQ0FBQztJQUVNLEtBQUs7UUFDWCxJQUFJLENBQUMsV0FBVyxHQUFHLElBQUksQ0FBQyxZQUFZLENBQUM7UUFDckMsSUFBSSxDQUFDLFVBQVUsR0FBRyxJQUFJLEdBQUcsRUFBYSxDQUFDO0lBQ3hDLENBQUM7SUFFTSxNQUFNLENBQUMsS0FBUTtRQUNyQixJQUFJLE1BQU0sR0FBRyxJQUFJLENBQUMsV0FBVyxFQUFFLENBQUM7UUFDaEMsSUFBSSxDQUFDLFVBQVUsQ0FBQyxHQUFHLENBQUMsTUFBTSxFQUFFLEtBQUssQ0FBQyxDQUFDO1FBQ25DLE9BQU8sTUFBTSxDQUFDO0lBQ2YsQ0FBQztJQUVNLEdBQUcsQ0FBQyxNQUFjLEVBQUUsSUFBUTtRQUNsQyxPQUFPLElBQUksQ0FBQyxVQUFVLENBQUMsR0FBRyxDQUFDLE1BQU0sQ0FBQyxJQUFJLElBQUksQ0FBQztJQUM1QyxDQUFDO0NBQ0Q7QUF6QkQsMEJBeUJDIiwic291cmNlc0NvbnRlbnQiOlsiLyotLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS1cbiAqICBDb3B5cmlnaHQgKGMpIE1pY3Jvc29mdCBDb3Jwb3JhdGlvbi4gQWxsIHJpZ2h0cyByZXNlcnZlZC5cbiAqICBMaWNlbnNlZCB1bmRlciB0aGUgTUlUIExpY2Vuc2UuIFNlZSBMaWNlbnNlLnR4dCBpbiB0aGUgcHJvamVjdCByb290IGZvciBsaWNlbnNlIGluZm9ybWF0aW9uLlxuICotLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLS0tLSovXG5cbmV4cG9ydCBjbGFzcyBIYW5kbGVzPFQ+IHtcblxuXHRwcml2YXRlIFNUQVJUX0hBTkRMRSA9IDEwMDA7XG5cblx0cHJpdmF0ZSBfbmV4dEhhbmRsZSA6IG51bWJlcjtcblx0cHJpdmF0ZSBfaGFuZGxlTWFwID0gbmV3IE1hcDxudW1iZXIsIFQ+KCk7XG5cblx0cHVibGljIGNvbnN0cnVjdG9yKHN0YXJ0SGFuZGxlPzogbnVtYmVyKSB7XG5cdFx0dGhpcy5fbmV4dEhhbmRsZSA9IHR5cGVvZiBzdGFydEhhbmRsZSA9PT0gJ251bWJlcicgPyBzdGFydEhhbmRsZSA6IHRoaXMuU1RBUlRfSEFORExFO1xuXHR9XG5cblx0cHVibGljIHJlc2V0KCk6IHZvaWQge1xuXHRcdHRoaXMuX25leHRIYW5kbGUgPSB0aGlzLlNUQVJUX0hBTkRMRTtcblx0XHR0aGlzLl9oYW5kbGVNYXAgPSBuZXcgTWFwPG51bWJlciwgVD4oKTtcblx0fVxuXG5cdHB1YmxpYyBjcmVhdGUodmFsdWU6IFQpOiBudW1iZXIge1xuXHRcdHZhciBoYW5kbGUgPSB0aGlzLl9uZXh0SGFuZGxlKys7XG5cdFx0dGhpcy5faGFuZGxlTWFwLnNldChoYW5kbGUsIHZhbHVlKTtcblx0XHRyZXR1cm4gaGFuZGxlO1xuXHR9XG5cblx0cHVibGljIGdldChoYW5kbGU6IG51bWJlciwgZGZsdD86IFQpOiBUIHtcblx0XHRyZXR1cm4gdGhpcy5faGFuZGxlTWFwLmdldChoYW5kbGUpIHx8IGRmbHQ7XG5cdH1cbn1cbiJdfQ==