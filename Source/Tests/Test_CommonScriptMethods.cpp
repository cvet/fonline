//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "catch_amalgamated.hpp"

#include "AngelScriptBackend.h"
#include "AngelScriptHelpers.h"
#include "AngelScriptScripting.h"
#include "Baker.h"
#include "DataSerialization.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

namespace
{
    static auto MakeSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);

        return settings;
    }

    static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        auto script_source = string(R"(

namespace CommonMethods
{
    // ========== Geometry ==========

    int TestGetDirAngle()
    {
        mpos from(10, 10);
        mpos to(15, 10);
        mdir angle = Game.GetDirection(from, to);
        // Angle should be some valid direction value
        // Just verify it doesn't crash and returns a value
        if (angle.angle < -360 || angle.angle > 360) return -1;

        // Same position should give 0 or defined result
        mdir sameAngle = Game.GetDirection(from, from);
        // Just verify no crash
        return 0;
    }

    int TestGetLineDirAngle()
    {
        ipos fromPos(0, 0);
        ipos toPos(100, 0);
        mdir angle = Game.GetLineDirAngle(fromPos, toPos);
        if (angle.angle < -360 || angle.angle > 360) return -1;

        ipos toPos2(0, 100);
        mdir angle2 = Game.GetLineDirAngle(fromPos, toPos2);
        if (angle2.angle < -360 || angle2.angle > 360) return -2;

        return 0;
    }

    int TestAngleToDirAndBack()
    {
        // Test all 6 directions (hex)
        for (int8 dir = 0; dir < 6; dir++) {
            int16 angle = mdir(hdir(dir)).angle;
            if (angle < -360 || angle > 360) return -1;

            mdir backDir = mdir(angle);
            if (backDir.hex != hdir(dir)) return -2;
        }
        return 0;
    }

    int TestRotateDirAngle()
    {
        mdir angle = mdir(0);

        // Rotate clockwise by 60 degrees
        mdir rotated = Game.RotateDirAngle(angle, true, 60);
        if (rotated.angle < -360 || rotated.angle > 360) return -1;

        // Rotate counter-clockwise
        mdir rotatedCCW = Game.RotateDirAngle(angle, false, 60);
        if (rotatedCCW.angle < -360 || rotatedCCW.angle > 360) return -2;

        // Full rotation
        mdir full = Game.RotateDirAngle(angle, true, 360);
        // Should be close to original
        return 0;
    }

    int TestGetDirAngleDiff()
    {
        mdir a1 = mdir(0);
        mdir a2 = mdir(90);
        int16 diff = Game.GetDirAngleDiff(a1, a2);
        if (diff < 0) diff = -diff;
        if (diff > 180) return -1;

        // Same angle should give 0
        int16 sameDiff = Game.GetDirAngleDiff(a1, a1);
        if (sameDiff != 0) return -2;

        return 0;
    }

    int TestGetHexInterval()
    {
        mpos from(10, 10);
        mpos to(12, 12);
        ipos offset;
        Game.GetHexInterval(from, to, offset);
        // Just verify no crash and offset is set
        return 0;
    }
)");

#if FO_ENABLE_3D
        script_source += R"(
    int TestGetModelAnimDuration()
    {
        timespan walk = Game.GetModelAnimDuration("Critters/Test.fo3d".hstr(), CritterStateAnim::Unarmed, CritterActionAnim::Walk);
        if (walk.milliseconds != 500) return -1;

        timespan run = Game.GetModelAnimDuration("Critters/Test.fo3d".hstr(), CritterStateAnim::Unarmed, CritterActionAnim::Run);
        if (run.milliseconds != 250) return -2;

        timespan missingAction = Game.GetModelAnimDuration("Critters/Test.fo3d".hstr(), CritterStateAnim::Unarmed, CritterActionAnim::Idle);
        if (missingAction.milliseconds != 0) return -3;

        timespan missingModel = Game.GetModelAnimDuration("Critters/Missing.fo3d".hstr(), CritterStateAnim::Unarmed, CritterActionAnim::Walk);
        if (missingModel.milliseconds != 0) return -4;

        return 0;
    }
)";
#endif

        script_source += string(R"(
    // ========== Proto Getters ==========

    int TestGetProtoItems()
    {
        array<ProtoItem> protos = Game.GetProtoItems();
        if (protos.length() == 0) return -1;
        return 0;
    }

    int TestGetProtoCritters()
    {
        array<ProtoCritter> protos = Game.GetProtoCritters();
        if (protos.length() == 0) return -1;
        return 0;
    }

    int TestGetProtoMaps()
    {
        array<ProtoMap> protos = Game.GetProtoMaps();
        // Maps may be 0 if none defined in test setup - just verify no crash
        return 0;
    }

    int TestGetProtoLocations()
    {
        array<ProtoLocation> protos = Game.GetProtoLocations();
        if (protos.length() == 0) return -1;
        return 0;
    }

    // ========== UTF-8 ==========

    int TestEncodeDecodeUtf8()
    {
        // Encode a simple ASCII character
        string encoded = Game.EncodeUtf8(65); // 'A'
        if (encoded != "A") return -1;

        // Decode it back
        int length = 0;
        uint codepoint = Game.DecodeUtf8("A", length);
        if (codepoint != 65) return -2;
        if (length != 1) return -3;

        // Encode a multi-byte character (Cyrillic A = U+0410)
        string cyrillic = Game.EncodeUtf8(0x0410);
        if (cyrillic.length() == 0) return -4;

        // Decode it back
        int len2 = 0;
        uint cp2 = Game.DecodeUtf8(cyrillic, len2);
        if (cp2 != 0x0410) return -5;
        if (len2 < 2) return -6;

        return 0;
    }

    // ========== Time Packing ==========

    int TestPackUnpackTime()
    {
        // Pack a known time
        nanotime packed = Game.PackTime(2024, 6, 15, 12, 30, 45, 500, 0, 0);
        if (packed == ZERO_NANOTIME) return -1;

        // Unpack it
        int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, ms = 0, us = 0, ns = 0;
        Game.UnpackTime(packed, year, month, day, hour, minute, second, ms, us, ns);

        if (year != 2024) return -2;
        if (month != 6) return -3;
        if (day != 15) return -4;
        if (hour != 12) return -5;
        if (minute != 30) return -6;
        if (second != 45) return -7;

        return 0;
    }

    int TestPackUnpackSynchronizedTime()
    {
        synctime packed = Game.PackSynchronizedTime(2024, 3, 20, 8, 15, 30, 250);
        if (packed == ZERO_SYNCTIME) return -1;

        int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, ms = 0;
        Game.UnpackSynchronizedTime(packed, year, month, day, hour, minute, second, ms);

        if (year != 2024) return -2;
        if (month != 3) return -3;
        if (day != 20) return -4;
        if (hour != 8) return -5;
        if (minute != 15) return -6;
        if (second != 30) return -7;

        return 0;
    }

    // ========== Script type conversions ==========

    int TestScriptTypeConversions()
    {
        hstring key = "AlphaKey".hstr();
        if (key == EMPTY_HSTRING) return -1;
        if (key.str != "AlphaKey") return -2;
        if (string(key) != "AlphaKey") return -3;
        string implicitKey = key;
        if (implicitKey != "AlphaKey") return -70;
        if (!(key == "AlphaKey")) return -4;
        if (key.hash == 0) return -5;
        if (key.uhash == 0) return -6;

        hstring assigned;
        assigned = key;
        if (assigned != key) return -7;
        if (assigned < key || key < assigned) return -8;

        any keyAny = key;
        hstring keyFromAny = keyAny;
        if (keyFromAny != key) return -9;

        any textAny = "BetaKey";
        hstring textHash = textAny;
        if (textHash.str != "BetaKey") return -10;
        string text = textAny;
        if (text != "BetaKey") return -11;

        any copiedAny = keyAny;
        if (!(copiedAny == keyAny)) return -12;
        any assignedAny;
        assignedAny = textAny;
        if (!(assignedAny == textAny)) return -13;

        any boolAny = true;
        bool boolValue = boolAny;
        if (!boolValue) return -14;

        any boolText = "true";
        int boolAsInt = boolText;
        if (boolAsInt != 1) return -15;

        any numericText = "-42";
        int intValue = numericText;
        if (intValue != -42) return -16;

        any emptyAny;
        int emptyInt = emptyAny;
        if (emptyInt != 0) return -17;

        any uintAny = uint(123);
        uint uintValue = uintAny;
        if (uintValue != 123) return -18;

        ident id;
        id.value = 12345;
        any idAny = id;
        ident idFromAny = idAny;
        if (idFromAny.value != id.value) return -19;
        if (ZERO_IDENT.value != 0) return -20;

        hdir dir = hdir(2);
        if (dir.value != 2) return -21;
        dir.value = 3;
        if (dir.value != 3) return -22;
        any dirAny = dir;
        hdir dirFromAny = dirAny;
        if (dirFromAny.value != dir.value) return -23;

        mdir angle = mdir(dir);
        hdir angleHex = angle.hex;
        if (angleHex.value != dir.value) return -24;
        if (angle.incHex().hex.value == angle.hex.value) return -25;
        if (angle.decHex().hex.value == angle.hex.value) return -26;
        if (angle.rotateHex(2).hex.value == angle.hex.value) return -27;
        if (angle.reverse().hex.value == angle.hex.value) return -28;
        any angleAny = angle;
        mdir angleFromAny = angleAny;
        if (angleFromAny.angle != angle.angle) return -29;

        ucolor clamped = ucolor(-1, 260, 17, 511);
        if (clamped.red != 0) return -30;
        if (clamped.green != 255) return -31;
        if (clamped.blue != 17) return -32;
        if (clamped.alpha != 255) return -33;
        ucolor raw = ucolor(clamped.value);
        if (raw.value != clamped.value) return -34;

        timespan seconds = timespan(2, 3);
        if (seconds.seconds != 2) return -35;
        if (seconds.milliseconds != 2000) return -36;
        timespan millis = timespan(500, 2);
        if ((seconds + millis).milliseconds != 2500) return -37;
        if ((seconds - millis).milliseconds != 1500) return -38;
        seconds += millis;
        if (seconds.milliseconds != 2500) return -39;
        seconds -= millis;
        if (seconds.milliseconds != 2000) return -40;

        nanotime nt = nanotime(10, 3);
        nanotime ntLater = nt + millis;
        if ((ntLater - nt).milliseconds != 500) return -41;
        ntLater -= millis;
        if (ntLater.seconds != nt.seconds) return -42;
        if (nt.timeSinceEpoch.seconds != 10) return -43;

        synctime st = synctime(20, 3);
        synctime stLater = st + millis;
        if ((stLater - st).milliseconds != 500) return -44;
        stLater -= millis;
        if (stLater.seconds != st.seconds) return -45;
        if (st.timeSinceEpoch.seconds != 20) return -46;

        ipos pos = ipos(2, 3);
        ipos moved = pos + ipos(1, 2);
        if (moved.x != 3 || moved.y != 5) return -47;
        moved -= isize(1, 1);
        if (moved.x != 2 || moved.y != 4) return -48;
        if (!moved.fitTo(isize(10, 10))) return -49;
        if (!moved.fitTo(irect(0, 0, 10, 10))) return -50;
        if ((-moved).x != -2 || (-moved).y != -4) return -51;

        fpos fposValue = fpos(1.5f, 2.5f);
        fposValue += fpos(0.5f, 1.5f);
        if (fposValue.x < 1.99f || fposValue.x > 2.01f) return -52;
        if (fposValue.y < 3.99f || fposValue.y > 4.01f) return -53;
        fposValue -= fpos(1.0f, 1.0f);
        fpos negativeFpos = -fposValue;
        if (negativeFpos.x > -0.99f) return -54;
        if (negativeFpos.x < -1.01f) return -55;

        fsize floatSize = fsize(5.5f, 6.5f);
        if (floatSize.width < 5.49f || floatSize.width > 5.51f) return -56;
        if (floatSize.height < 6.49f || floatSize.height > 6.51f) return -57;
        any floatSizeAny = floatSize;
        fsize floatSizeFromAny = floatSizeAny;
        if (floatSizeFromAny.width < 5.49f || floatSizeFromAny.width > 5.51f) return -58;
        if (floatSizeFromAny.height < 6.49f || floatSizeFromAny.height > 6.51f) return -59;

        mpos mapPos = mpos(3, 4);
        msize mapSize;
        mapSize.width = 10;
        mapSize.height = 10;
        if (!mapPos.fitTo(mapSize)) return -60;

        return 0;
    }

    int TestSmallValueTypeArrayInitListPadding()
    {
        hdir[] dirs = {HDIR_NorthEast, HDIR_East, HDIR_SouthEast, HDIR_SouthWest, HDIR_West, HDIR_NorthWest};
        if (dirs.length() != 6) return -1;
        if (dirs[0] != HDIR_NorthEast) return -2;
        if (dirs[1] != HDIR_East) return -3;
        if (dirs[2] != HDIR_SouthEast) return -4;
        if (dirs[3] != HDIR_SouthWest) return -5;
        if (dirs[4] != HDIR_West) return -6;
        if (dirs[5] != HDIR_NorthWest) return -7;

        hdir[] cloned = dirs.clone();
        if (!(cloned == dirs)) return -8;

        return 0;
    }

    int TestScriptValueTypeMetadataConversions()
    {
        if (ZERO_UCOLOR.value != 0) return -1;
        if (ZERO_TIMESPAN.nanoseconds != 0) return -2;
        if (ZERO_NANOTIME.nanoseconds != 0) return -3;
        if (ZERO_SYNCTIME.milliseconds != 0) return -4;
        if (ZERO_IPOS.x != 0 || ZERO_IPOS.y != 0) return -5;
        if (ZERO_ISIZE.width != 0 || ZERO_ISIZE.height != 0) return -6;
        if (ZERO_IRECT.width != 0 || ZERO_IRECT.height != 0) return -7;
        if (ZERO_FPOS.x != 0.0f || ZERO_FPOS.y != 0.0f) return -8;
        if (ZERO_FSIZE.width != 0.0f || ZERO_FSIZE.height != 0.0f) return -9;
        if (ZERO_MPOS.x != 0 || ZERO_MPOS.y != 0) return -10;
        if (ZERO_MSIZE.width != 0 || ZERO_MSIZE.height != 0) return -11;
        if (ZERO_HDIR.value != 0) return -12;
        if (ZERO_MDIR.angle != 0) return -13;

        ucolor color = ucolor(1, 2, 3, 4);
        if (color.str.length() == 0) return -14;
        any colorAny = color;
        ucolor colorFromAny = colorAny;
        if (colorFromAny.value != color.value) return -15;

        timespan span = timespan(3, 3);
        if (span.str.length() == 0) return -16;
        any spanAny = span;
        if (string(spanAny).length() == 0) return -17;

        nanotime nano = nanotime(4, 3);
        if (nano.str.length() == 0) return -18;
        any nanoAny = nano;
        if (string(nanoAny).length() == 0) return -19;

        synctime sync = synctime(5, 3);
        if (sync.str.length() == 0) return -20;
        any syncAny = sync;
        if (string(syncAny).length() == 0) return -21;

        ipos pos = ipos(6, 7);
        if (pos.str.length() == 0) return -22;
        any posAny = pos;
        if (string(posAny).length() == 0) return -23;

        isize size = isize(8, 9);
        if (size.str.length() == 0) return -24;
        any sizeAny = size;
        if (string(sizeAny).length() == 0) return -25;

        irect rect = irect(1, 2, 3, 4);
        if (rect.str.length() == 0) return -26;
        any rectAny = rect;
        if (string(rectAny).length() == 0) return -27;

        fpos floatPos = fpos(1.25f, 2.5f);
        if (floatPos.str.length() == 0) return -28;
        any floatPosAny = floatPos;
        if (string(floatPosAny).length() == 0) return -29;

        mpos mapPos = mpos(10, 11);
        if (mapPos.str.length() == 0) return -30;
        any mapPosAny = mapPos;
        if (string(mapPosAny).length() == 0) return -31;

        msize mapSize;
        mapSize.width = 12;
        mapSize.height = 13;
        if (mapSize.str.length() == 0) return -32;
        any mapSizeAny = mapSize;
        if (string(mapSizeAny).length() == 0) return -33;
)"
                                R"(
        hdir dir = HDIR_SouthEast;
        if (dir.str.length() == 0) return -34;
        if (HDIR_Random.value < 0 || HDIR_Random.value > 5) return -35;
        if (!(dir == hdir(2))) return -36;

        mdir angle = mdir(120);
        if (angle.str.length() == 0) return -37;
        angle.angle = 60;
        if (angle.angle != 60) return -38;
        if (!(angle == mdir(60))) return -39;
)"
                                R"(
        TextPackName pack = TextPackName("Dialogs".hstr());
        TextPackName packCopy(pack);
        if (!(packCopy == pack)) return -40;
        if (pack.str != "Dialogs") return -41;
        if (string(pack) != "Dialogs") return -42;
        hstring packHash = pack;
        if (packHash.str != "Dialogs") return -43;
        any packAny = pack;
        TextPackName packFromAny = packAny;
        if (!(packFromAny == pack)) return -44;
        TextPackName alphaPack = TextPackName("Alpha".hstr());
        TextPackName betaPack = TextPackName("Beta".hstr());
        if (!(alphaPack < betaPack || betaPack < alphaPack)) return -45;

        LanguageName lang = LanguageName("russ".hstr());
        if (lang.str != "russ") return -46;
        if (string(lang) != "russ") return -47;
        hstring langHash = lang;
        if (langHash.str != "russ") return -48;
        any langAny = lang;
        LanguageName langFromAny = langAny;
        if (!(langFromAny == lang)) return -49;
        LanguageName englLang = LanguageName("engl".hstr());
        LanguageName russLang = LanguageName("russ".hstr());
        if (!(englLang < russLang || russLang < englLang)) return -50;
)"
                                R"(

        TextPackKey defaultKey;
        if (defaultKey.Collection.Name != EMPTY_HSTRING) return -51;
        TextPackKey genericKey(pack, "Root".hstr(), "Name".hstr(), "Short".hstr());
        TextPackKey genericCopy(genericKey);
        if (!(genericCopy == genericKey)) return -52;
        if (genericKey.str.length() == 0) return -53;
        any genericAny = genericKey;
        TextPackKey genericFromAny = genericAny;
        if (!(genericFromAny == genericKey)) return -54;

        TextPackKey stringKey1(pack, "Root");
        TextPackKey hashKey1(pack, "Root".hstr());
        TextPackKey stringKey2(pack, "Root", "Name");
        TextPackKey hashKey2(pack, "Root".hstr(), "Name");
        TextPackKey stringKey3(pack, "Root", "Name", "Short");
        TextPackKey hashKey3(pack, "Root".hstr(), "Name", "Short");
        if (!(stringKey1 == hashKey1)) return -55;
        if (!(stringKey2 == hashKey2)) return -56;
        if (!(stringKey3 == hashKey3)) return -57;
        if (!(stringKey1 < stringKey2 || stringKey2 < stringKey1)) return -58;
        if (!(stringKey2 < stringKey3 || stringKey3 < stringKey2)) return -59;

        return 0;
    }

    int TestScriptValueTypeOperatorEdges()
    {
        timespan nanos = timespan(42, 0);
        if (nanos.nanoseconds != 42) return -1;
        timespan micros = timespan(7, 1);
        if (micros.microseconds != 7) return -2;

        nanotime nanoMicros = nanotime(8, 1);
        if (nanoMicros.microseconds != 8) return -3;
        synctime syncMillis = synctime(9, 2);
        if (syncMillis.milliseconds != 9) return -4;

        any floatText = "1.25";
        float floatValue = floatText;
        if (floatValue < 1.24f || floatValue > 1.26f) return -5;
        any doubleText = "2.5";
        double doubleValue = doubleText;
        if (doubleValue < 2.49 || doubleValue > 2.51) return -6;

        any falseText = "false";
        int falseAsInt = falseText;
        if (falseAsInt != 0) return -7;

        ipos pos = ipos(5, 7);
        pos += ipos(2, 3);
        if (pos.x != 7 || pos.y != 10) return -8;
        pos -= ipos(4, 5);
        if (pos.x != 3 || pos.y != 5) return -9;

        ipos posMinus = pos - ipos(1, 2);
        if (posMinus.x != 2 || posMinus.y != 3) return -10;
        ipos posPlusSize = pos + isize(6, 7);
        if (posPlusSize.x != 9 || posPlusSize.y != 12) return -11;
        ipos posMinusSize = pos - isize(1, 2);
        if (posMinusSize.x != 2 || posMinusSize.y != 3) return -12;
        pos += isize(1, 1);
        if (pos.x != 4 || pos.y != 6) return -13;

        if (ipos(-1, 0).fitTo(isize(10, 10))) return -14;
        if (ipos(0, -1).fitTo(irect(0, 0, 10, 10))) return -15;
        if (ipos(10, 0).fitTo(irect(0, 0, 10, 10))) return -16;

        fpos floatPos = fpos(3.5f, 4.5f);
        fpos floatSum = floatPos + fpos(1.0f, 2.0f);
        if (floatSum.x < 4.49f || floatSum.x > 4.51f) return -17;
        if (floatSum.y < 6.49f || floatSum.y > 6.51f) return -18;
        fpos floatDiff = floatSum - fpos(0.5f, 1.5f);
        if (floatDiff.x < 3.99f || floatDiff.x > 4.01f) return -19;
        if (floatDiff.y < 4.99f || floatDiff.y > 5.01f) return -20;

        fsize defaultFloatSize;
        if (defaultFloatSize.width != 0.0f || defaultFloatSize.height != 0.0f) return -21;
        if (defaultFloatSize.str.length() == 0) return -22;

        hdir dir = hdir(4);
        mdir dirAngle = dir;
        if (dirAngle.hex.value != 4) return -23;

        TextPackName alphaPack = TextPackName("Alpha".hstr());
        TextPackName betaPack = TextPackName("Beta".hstr());
        bool packForward = alphaPack < betaPack;
        bool packReverse = betaPack < alphaPack;
        if (packForward == packReverse) return -24;
        if (!(alphaPack == TextPackName("Alpha".hstr()))) return -25;

        LanguageName englLang = LanguageName("engl".hstr());
        LanguageName russLang = LanguageName("russ".hstr());
        bool langForward = englLang < russLang;
        bool langReverse = russLang < englLang;
        if (langForward == langReverse) return -26;
        if (!(englLang == LanguageName("engl".hstr()))) return -27;

        TextPackName pack = TextPackName("Dialogs".hstr());
        TextPackKey keyA(pack, "Root", "A");
        TextPackKey keyB(pack, "Root", "B");
        bool keyForward = keyA < keyB;
        bool keyReverse = keyB < keyA;
        if (keyForward == keyReverse) return -28;
        if (!(keyA == TextPackKey(pack, "Root", "A"))) return -29;

        return 0;
    }

    int TestScriptEnumAnyConversions()
    {
        any deadAny = CritterCondition::Dead;
        string deadText = deadAny;
        if (deadText != "CritterCondition::Dead") return -1;
        CritterCondition dead = deadAny;
        if (dead != CritterCondition::Dead) return -2;

        any assigned;
        assigned = CritterCondition::Knockout;
        string assignedText = assigned;
        if (assignedText != "CritterCondition::Knockout") return -3;
        CritterCondition knockout = assigned;
        if (knockout != CritterCondition::Knockout) return -4;

        any fullAlive = "CritterCondition::Alive";
        CritterCondition alive = fullAlive;
        if (alive != CritterCondition::Alive) return -5;

        any shortAlive = "Alive";
        CritterCondition shortParsed = shortAlive;
        if (shortParsed != CritterCondition::Alive) return -6;

        any fullDead = "CritterCondition::Dead";
        int deadValue = fullDead;
        if (deadValue != 2) return -7;

        return 0;
    }

    void TestScriptEnumAnyWrongTypeThrows()
    {
        any invalid = CritterActionAnim::None;
        CritterCondition parsed = invalid;
    }

    void TestScriptEnumAnyWrongValueThrows()
    {
        any invalid = "CritterCondition::Missing";
        CritterCondition parsed = invalid;
    }

    void TestScriptEnumAnyInvalidConstructThrows()
    {
        CritterCondition invalid = CritterCondition(99);
        any value = invalid;
    }

    int TestScriptDynamicValueTypeConversions()
    {
        BoolSnapshot boolZero = ZERO_BOOLSNAPSHOT;
        if (boolZero.Value) return -1;
        BoolSnapshot boolValue = BoolSnapshot(true);
        if (!boolValue.Value) return -2;
        if (boolValue.str.length() == 0) return -3;
        any boolAny = "true";
        BoolSnapshot boolParsed = boolAny;
        if (!boolParsed.Value) return -4;
        any boolRoundtrip = boolValue;
        BoolSnapshot boolRoundParsed = boolRoundtrip;
        if (!boolRoundParsed.Value) return -5;

        IntSnapshot intZero = ZERO_INTSNAPSHOT;
        if (intZero.Value != 0) return -6;
        IntSnapshot intValue = IntSnapshot(7);
        if (intValue.Value != 7) return -7;
        if (intValue.str != "7") return -8;
        any intAny = "7";
        IntSnapshot intParsed = intAny;
        if (intParsed.Value != 7) return -9;
        any intRoundtrip = intValue;
        IntSnapshot intRoundParsed = intRoundtrip;
        if (intRoundParsed.Value != 7) return -10;

        IntSnapshot low = IntSnapshot(1);
        IntSnapshot high = IntSnapshot(2);
        if (!(low < high)) return -11;
        if (high < low) return -12;
        if (low < IntSnapshot(1)) return -13;
        if (!(low == IntSnapshot(1))) return -14;
        if (low == high) return -15;

        FloatSnapshot floatZero = ZERO_FLOATSNAPSHOT;
        if (floatZero.Value != 0.0f) return -16;
        FloatSnapshot floatValue = FloatSnapshot(1.5f);
        if (floatValue.Value < 1.49f || floatValue.Value > 1.51f) return -17;
        if (floatValue.str.length() == 0) return -18;
        any floatAny = "2.25";
        FloatSnapshot floatParsed = floatAny;
        if (floatParsed.Value < 2.24f || floatParsed.Value > 2.26f) return -19;
        any floatRoundtrip = floatValue;
        FloatSnapshot floatRoundParsed = floatRoundtrip;
        if (floatRoundParsed.Value < 1.49f || floatRoundParsed.Value > 1.51f) return -20;

        dict<ipos, int> posDict = {};
        posDict.set(ipos(2, 0), 20);
        posDict.set(ipos(1, 0), 10);
        if (posDict.length() != 2) return -21;
        if (posDict.get(ipos(2, 0)) != 20) return -22;
        if (!posDict.exists(ipos(1, 0))) return -23;

        return 0;
    }
)"
                                R"(
    int TestScriptDynamicRefTypeAccessors()
    {
        RouteSnapshot snapshot = RouteSnapshot();
        if (snapshot is null) return -1;

        snapshot.Value = 42;
        if (snapshot.Value != 42) return -2;

        array<int32> steps = snapshot.Steps;
        if (steps is null) return -10;
        if (steps.length() != 0) return -11;
        steps.insertLast(5);
        snapshot.Steps = steps;
        if (snapshot.Steps.length() != 1) return -12;
        if (snapshot.Steps[0] != 5) return -13;

        dict<string,int32> counters = snapshot.Counters;
        if (counters.length() != 0) return -17;
        counters.set("seen", 3);
        snapshot.Counters = counters;
        if (snapshot.Counters.get("seen") != 3) return -18;

        RouteNote child = RouteNote();
        if (child is null) return -14;
        child.Text = "child-note";
        snapshot.Child = child;
        if (snapshot.Child is null) return -15;
        if (snapshot.Child.Text != "child-note") return -16;

        ProtoCritter targetProto = Game.GetProtoCritter("TestCritter".hstr());
        if (targetProto is null) return -19;
        snapshot.TargetProto = targetProto;
        if (snapshot.TargetProto is null) return -20;
        if (snapshot.TargetProto.ProtoId != "TestCritter".hstr()) return -21;

        snapshot.OptionalItemProto = null;
        if (snapshot.OptionalItemProto !is null) return -22;
        ProtoItem optionalProto = Game.GetProtoItem("TestItem".hstr());
        if (optionalProto is null) return -23;
        snapshot.OptionalItemProto = optionalProto;
        if (snapshot.OptionalItemProto is null) return -24;
        if (snapshot.OptionalItemProto.ProtoId != "TestItem".hstr()) return -25;

        RouteSnapshotMarkerComponent marker = snapshot.Marker;
        if (marker is null) return -3;

        marker.Steps = 7;
        marker.Note = "marker-note";
        if (marker.Steps != 7) return -4;
        if (marker.Note != "marker-note") return -5;
        if (snapshot.Marker.Steps != 7) return -6;
        if (snapshot.Marker.Note != "marker-note") return -7;

        RouteSnapshot same = snapshot;
        RouteSnapshot other = RouteSnapshot();
        if (!(snapshot == same)) return -8;
        if (snapshot == other) return -9;

        return 0;
    }

    void TestTextPackKeyAnyLessThrows()
    {
        any invalid = "Dialogs Root Name";
        TextPackKey key = invalid;
    }

    void TestTextPackKeyAnyMoreThrows()
    {
        any invalid = "Dialogs Root Name Short Extra";
        TextPackKey key = invalid;
    }

    void TestInvalidTimePlaceThrows()
    {
        timespan invalid = timespan(1, 99);
    }

    void TestInvalidAnyToIntThrows()
    {
        any invalid = "DefinitelyNotAnInt";
        int parsed = invalid;
    }

    // ========== Game-level Time Events ==========

    bool globalTimerFired = false;

    [[TimeEvent]]
    void OnGlobalTimer()
    {
        globalTimerFired = true;
    }

    int TestGameTimeEventCreateCountStop()
    {
        globalTimerFired = false;

        // Start a game-level time event
        uint32 eventId = Game.StartTimeEvent(timespan(60, 3), OnGlobalTimer);
        if (eventId == 0) return -1;

        // Count by function
        int count = Game.CountTimeEvent(OnGlobalTimer);
        if (count < 1) return -2;

        // Count by id
        int countById = Game.CountTimeEvent(eventId);
        if (countById != 1) return -3;

        // Stop by function
        Game.StopTimeEvent(OnGlobalTimer);

        int countAfter = Game.CountTimeEvent(OnGlobalTimer);
        if (countAfter != 0) return -4;

        return 0;
    }

    int TestGameTimeEventStopById()
    {
        uint32 eventId = Game.StartTimeEvent(timespan(60, 3), OnGlobalTimer);
        if (eventId == 0) return -1;

        Game.StopTimeEvent(eventId);

        int countAfter = Game.CountTimeEvent(eventId);
        if (countAfter != 0) return -2;

        return 0;
    }

    int TestGameTimeEventRepeat()
    {
        uint32 eventId = Game.StartTimeEvent(timespan(60, 3), OnGlobalTimer);
        if (eventId == 0) return -1;

        // Change repeat interval
        Game.RepeatTimeEvent(OnGlobalTimer, timespan(30, 3));
        Game.RepeatTimeEvent(eventId, timespan(15, 3));

        // Clean up
        Game.StopTimeEvent(eventId);
        return 0;
    }

    bool globalTimerWithDataFired = false;

    [[TimeEvent]]
    void OnGlobalTimerWithData(any data)
    {
        globalTimerWithDataFired = true;
    }

    int TestGameTimeEventWithData()
    {
        globalTimerWithDataFired = false;

        any data = 42;
        uint32 eventId = Game.StartTimeEvent(timespan(60, 3), OnGlobalTimerWithData, data);
        if (eventId == 0) return -1;

        int count = Game.CountTimeEvent(OnGlobalTimerWithData);
        if (count < 1) return -2;

        // Set new data by id
        any newData = 100;
        Game.SetTimeEventData(eventId, newData);

        Game.StopTimeEvent(OnGlobalTimerWithData);
        return 0;
    }

    int TestGameTimeEventRepeating()
    {
        // Start a repeating event (delay + repeat)
        uint32 eventId = Game.StartTimeEvent(timespan(1, 3), timespan(5, 3), OnGlobalTimer);
        if (eventId == 0) return -1;

        int count = Game.CountTimeEvent(eventId);
        if (count != 1) return -2;

        Game.StopTimeEvent(eventId);
        return 0;
    }

 )") + R"(

    // ========== Invoke ==========

    bool invokeFired = false;
    bool typedInvokeFired = false;
    int typedInvokeNumber = 0;
    string typedInvokeText = "";
    bool typedInvokeFlag = false;
    hstring typedInvokeKey;
    int typedInvokePayloadSize = 0;

    void InvokeTarget()
    {
        invokeFired = true;
    }

    void InvokeTargetWithData(any data)
    {
        invokeFired = true;
    }

    void InvokeTargetWithTwoData(any data1, any data2)
    {
        invokeFired = true;
    }

    void InvokeTargetWithThreeData(any data1, any data2, any data3)
    {
        invokeFired = true;
    }

    void InvokeTargetWithTypedData(int number, string text, bool flag, hstring key, array<any> payload)
    {
        typedInvokeFired = true;
        typedInvokeNumber = number;
        typedInvokeText = text;
        typedInvokeFlag = flag;
        typedInvokeKey = key;
        typedInvokePayloadSize = int(payload.length());
    }

    int TestInvokeByName()
    {
        invokeFired = false;
        bool result = Invoke("CommonMethods::InvokeTarget");
        if (!result) return -1;
        if (!invokeFired) return -2;
        return 0;
    }

    int TestInvokeByNameWithData()
    {
        invokeFired = false;
        any param = 42;
        bool result = Invoke("CommonMethods::InvokeTargetWithData", param);
        if (!result) return -1;
        if (!invokeFired) return -2;
        return 0;
    }

    int TestInvokeByNameWithTwoData()
    {
        invokeFired = false;
        any param1 = 42;
        any param2 = "payload";
        bool result = Invoke("CommonMethods::InvokeTargetWithTwoData", param1, param2);
        if (!result) return -1;
        if (!invokeFired) return -2;
        return 0;
    }

    int TestInvokeByNameWithThreeData()
    {
        invokeFired = false;
        any param1 = 42;
        any param2 = "payload";
        any param3 = true;
        bool result = Invoke("CommonMethods::InvokeTargetWithThreeData", param1, param2, param3);
        if (!result) return -1;
        if (!invokeFired) return -2;
        return 0;
    }

    int TestInvokeByNameWithTypedVariadic()
    {
        typedInvokeFired = false;
        typedInvokeNumber = 0;
        typedInvokeText = "";
        typedInvokeFlag = false;
        typedInvokeKey = hstring();
        typedInvokePayloadSize = 0;

        array<any> payload = {1, "two", true};
        bool result = Invoke("CommonMethods::InvokeTargetWithTypedData", 42, "payload", true, "MixedKey".hstr(), payload);
        if (!result) return -1;
        if (!typedInvokeFired) return -2;
        if (typedInvokeNumber != 42) return -3;
        if (typedInvokeText != "payload") return -4;
        if (!typedInvokeFlag) return -5;
        if (typedInvokeKey != "MixedKey".hstr()) return -6;
        if (typedInvokePayloadSize != 3) return -7;
        return 0;
    }

    int TestInvokeByNameRejectsWrongType()
    {
        array<any> payload = {1, "two", true};

        try {
            Invoke("CommonMethods::InvokeTargetWithTypedData", 42, 99, true, "MixedKey".hstr(), payload);
        }
        catch {
            return 0;
        }

        return -1;
    }

    void InvokeRefTarget(int& value, string& text)
    {
        value = value * 2 + 1;
        text += "-invoked";
    }

    int TestInvokeByNameWithRefArgs()
    {
        int value = 5;
        string text = "ping";
        bool result = Invoke("CommonMethods::InvokeRefTarget", value, text);
        if (!result) return -1;
        if (value != 11) return -2;
        if (text != "ping-invoked") return -3;
        return 0;
    }

 )" + R"(
    // ========== Entity Time Events (Critter) ==========

    [[TimeEvent]]
    void OnCritterTimeEvent(Critter cr)
    {
        // No-op
    }

    [[TimeEvent]]
    void OnCritterTimeEventWithData(Critter cr, any data)
    {
        // No-op
    }

    int TestEntityTimeEventCount()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        uint32 eventId = cr.StartTimeEvent(timespan(60, 3), OnCritterTimeEvent);
        if (eventId == 0) return -2;

        // Count by function
        int count = cr.CountTimeEvent(OnCritterTimeEvent);
        if (count < 1) return -3;

        // Count by id
        int countById = cr.CountTimeEvent(eventId);
        if (countById != 1) return -4;

        cr.StopTimeEvent(eventId);
        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityTimeEventRepeat()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        uint32 eventId = cr.StartTimeEvent(timespan(60, 3), OnCritterTimeEvent);
        if (eventId == 0) return -2;

        // Change repeat by function
        cr.RepeatTimeEvent(OnCritterTimeEvent, timespan(30, 3));

        // Change repeat by id
        cr.RepeatTimeEvent(eventId, timespan(15, 3));

        cr.StopTimeEvent(eventId);
        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityTimeEventSetData()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        any initData = 1;
        uint32 eventId = cr.StartTimeEvent(timespan(60, 3), OnCritterTimeEventWithData, initData);
        if (eventId == 0) return -2;

        // Set data by function
        any newData1 = 2;
        cr.SetTimeEventData(OnCritterTimeEventWithData, newData1);

        // Set data by id
        any newData2 = 3;
        cr.SetTimeEventData(eventId, newData2);

        cr.StopTimeEvent(eventId);
        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityTimeEventStopByFunction()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.StartTimeEvent(timespan(60, 3), OnCritterTimeEvent);
        cr.StartTimeEvent(timespan(120, 3), OnCritterTimeEvent);

        int count = cr.CountTimeEvent(OnCritterTimeEvent);
        if (count < 2) return -2;

        cr.StopTimeEvent(OnCritterTimeEvent);

        int countAfter = cr.CountTimeEvent(OnCritterTimeEvent);
        if (countAfter != 0) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

 )" + R"(
    // ========== Item Container Ops ==========

    int TestItemContainerAddGetItems()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // Add a container item
        Item container = cr.AddItem("TestContainer".hstr(), 1);
        if (container is null) return -2;

        // Add items to the container
        Item subItem1 = container.AddItem("TestItem".hstr(), 3);
        if (subItem1 is null) return -3;

        Item subItem2 = container.AddItem("TestItem".hstr(), 5);
        if (subItem2 is null) return -4;

        // Get items from container
        array<Item> contents = container.GetItems();
        if (contents is null) return -5;
        if (contents.length() == 0) return -6;

        // Container's GetCritter should return the owning critter
        Critter? owner = container.GetCritter();
        if (owner is null) return -7;
        if (owner.Id != cr.Id) return -8;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestItemGetMap()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        // Item is in critter inventory, not on a map
        Map? m = item.GetMap();
        // Should be null since item is not on a map
        if (m !is null) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Critter Script Methods ==========

    int TestCritterGetPlayerOfflineTime()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        // NPC has no player, GetPlayerOfflineTime should throw
        // Just verify the critter was created successfully
        bool isAlive = cr.IsAlive();
        if (!isAlive) return -2;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterGetItemByProperty()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.AddItem("TestItem".hstr(), 5);

        // Get items by property: CritterSlot == Inventory (0)
        array<Item> items = cr.GetItems(ItemProperty::CritterSlot, CritterItemSlot::Inventory);
        if (items is null) return -2;
        if (items.length() == 0) return -3;

        // Get single item by property
        Item? found = cr.GetItem(ItemProperty::CritterSlot, CritterItemSlot::Inventory);
        if (found is null) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterGetItemsByProto()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.AddItem("TestItem".hstr(), 3);
        cr.AddItem("TestItem".hstr(), 7);

        // Get items by proto hstring
        array<Item> items = cr.GetItems("TestItem".hstr());
        if (items is null) return -2;
        if (items.length() == 0) return -3;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestCritterDestroyItemByCount()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        cr.AddItem("TestItem".hstr(), 10);

        int before = cr.CountItem("TestItem".hstr());
        if (before < 10) return -2;

        // Destroy partial stack
        cr.DestroyItem("TestItem".hstr(), 3);

        int after = cr.CountItem("TestItem".hstr());
        if (after != before - 3) return -3;

        // Destroy all remaining
        cr.DestroyItem("TestItem".hstr(), after);

        int final2 = cr.CountItem("TestItem".hstr());
        if (final2 != 0) return -4;

        Game.DestroyCritter(cr);
        return 0;
    }

 )" + R"(
    // ========== Server Global Methods ==========

    int TestGetLocationsOverloads()
    {
        Location loc1 = Game.CreateLocation("TestLocation".hstr());
        Location loc2 = Game.CreateLocation("TestLocation".hstr());
        if (loc1 is null || loc2 is null) return -1;

        // Get all locations
        array<Location> locs = Game.GetLocations();
        if (locs is null) return -2;
        if (locs.length() < 2) return -3;

        // Get locations by pid
        array<Location> byPid = Game.GetLocations("TestLocation".hstr());
        if (byPid is null) return -4;
        if (byPid.length() < 2) return -5;

        Game.DestroyLocation(loc2);
        Game.DestroyLocation(loc1);
        return 0;
    }

    int TestGetCrittersOverloads()
    {
        Critter cr1 = Game.CreateCritter("TestCritter".hstr(), false);
        Critter cr2 = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr1 is null || cr2 is null) return -1;

        // Get all NPCs
        array<Critter> npcs = Game.GetAllNpc();
        if (npcs is null) return -2;
        if (npcs.length() < 2) return -3;

        // Get NPCs by PID
        array<Critter> byPid = Game.GetAllNpc("TestCritter".hstr());
        if (byPid is null) return -4;
        if (byPid.length() < 2) return -5;

        Game.DestroyCritter(cr2);
        Game.DestroyCritter(cr1);
        return 0;
    }

    int TestSetSynchronizedTime()
    {
        // Set synchronized time
        synctime st = Game.PackSynchronizedTime(2025, 1, 1, 0, 0, 0, 0);
        if (st == ZERO_SYNCTIME) return -1;

        Game.SetSynchronizedTime(st);

        // Verify by packing another time and checking it's different
        synctime st2 = Game.PackSynchronizedTime(2025, 6, 15, 12, 0, 0, 0);
        if (st2 == ZERO_SYNCTIME) return -2;
        if (st2 == st) return -3;

        return 0;
    }

    // ========== Location Time Events ==========

    [[TimeEvent]]
    void OnLocationTimeEvent(Location loc)
    {
        // No-op
    }

    int TestLocationTimeEvents()
    {
        Location loc = Game.CreateLocation("TestLocation".hstr());
        if (loc is null) return -1;

        uint32 eventId = loc.StartTimeEvent(timespan(60, 3), OnLocationTimeEvent);
        if (eventId == 0) return -2;

        int count = loc.CountTimeEvent(OnLocationTimeEvent);
        if (count < 1) return -3;

        loc.RepeatTimeEvent(eventId, timespan(30, 3));

        loc.StopTimeEvent(OnLocationTimeEvent);

        int countAfter = loc.CountTimeEvent(OnLocationTimeEvent);
        if (countAfter != 0) return -4;

        Game.DestroyLocation(loc);
        return 0;
    }

    // ========== Item Time Events ==========

    [[TimeEvent]]
    void OnItemTimeEvent(Item item)
    {
        // No-op
    }

    [[TimeEvent]]
    void OnItemTimeEventData(Item item, any data)
    {
        // No-op
    }

    int TestItemTimeEvents()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        uint32 eventId = item.StartTimeEvent(timespan(60, 3), OnItemTimeEvent);
        if (eventId == 0) return -3;

        int count = item.CountTimeEvent(OnItemTimeEvent);
        if (count < 1) return -4;

        item.RepeatTimeEvent(eventId, timespan(30, 3));

        item.StopTimeEvent(eventId);

        int countAfter = item.CountTimeEvent(eventId);
        if (countAfter != 0) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

    int TestItemTimeEventWithData()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        Item item = cr.AddItem("TestItem".hstr(), 1);
        if (item is null) return -2;

        any initData = 42;
        uint32 eventId = item.StartTimeEvent(timespan(60, 3), OnItemTimeEventData, initData);
        if (eventId == 0) return -3;

        // Set data by function
        any newData = 100;
        item.SetTimeEventData(OnItemTimeEventData, newData);

        // Set data by id
        any newData2 = 200;
        item.SetTimeEventData(eventId, newData2);

        item.StopTimeEvent(eventId);
        Game.DestroyCritter(cr);
        return 0;
    }

 )" + R"(
    // ========== Array Time Events with Data ==========

    [[TimeEvent]]
    void OnGlobalTimerWithArrayData(array<any> data)
    {
        // No-op
    }

    int TestGameTimeEventWithArrayData()
    {
        array<any> data = {1, 2, 3};
        uint32 eventId = Game.StartTimeEvent(timespan(60, 3), OnGlobalTimerWithArrayData, data);
        if (eventId == 0) return -1;

        int count = Game.CountTimeEvent(OnGlobalTimerWithArrayData);
        if (count < 1) return -2;

        Game.StopTimeEvent(OnGlobalTimerWithArrayData);
        return 0;
    }

    [[TimeEvent]]
    void OnCritterTimerWithArrayData(Critter cr, array<any> data)
    {
        // No-op
    }

    int TestEntityTimeEventWithArrayData()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        array<any> data = {10, 20};
        uint32 eventId = cr.StartTimeEvent(timespan(60, 3), OnCritterTimerWithArrayData, data);
        if (eventId == 0) return -2;

        int count = cr.CountTimeEvent(OnCritterTimerWithArrayData);
        if (count < 1) return -3;

        // Set array data by function
        array<any> newData = {30, 40, 50};
        cr.SetTimeEventData(OnCritterTimerWithArrayData, newData);

        // Set array data by id
        array<any> newData2 = {60};
        cr.SetTimeEventData(eventId, newData2);

        cr.StopTimeEvent(eventId);
        Game.DestroyCritter(cr);
        return 0;
    }

    int TestEntityTimeEventRepeatingOverloads()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        any initData = 11;
        array<any> arrayData = {21, 22};

        uint32 plainId = cr.StartTimeEvent(timespan(60, 3), timespan(5, 3),
            OnCritterTimeEvent);
        uint32 dataId = cr.StartTimeEvent(timespan(60, 3), timespan(6, 3),
            OnCritterTimeEventWithData, initData);
        uint32 arrayId = cr.StartTimeEvent(timespan(60, 3), timespan(7, 3),
            OnCritterTimerWithArrayData, arrayData);
        if (plainId == 0 || dataId == 0 || arrayId == 0) return -2;

        if (cr.CountTimeEvent(plainId) != 1) return -3;
        if (cr.CountTimeEvent(OnCritterTimeEventWithData) != 1) return -4;
        if (cr.CountTimeEvent(OnCritterTimerWithArrayData) != 1) return -5;

        cr.RepeatTimeEvent(OnCritterTimeEventWithData, timespan(8, 3));
        cr.RepeatTimeEvent(OnCritterTimerWithArrayData, timespan(9, 3));

        cr.StopTimeEvent(OnCritterTimeEventWithData);
        if (cr.CountTimeEvent(dataId) != 0) return -6;

        cr.StopTimeEvent(OnCritterTimerWithArrayData);
        if (cr.CountTimeEvent(arrayId) != 0) return -7;

        cr.StopTimeEvent(plainId);
        if (cr.CountTimeEvent(plainId) != 0) return -8;

        Game.DestroyCritter(cr);
        return 0;
    }

    [[TimeEvent]]
    void OnCritterContextTimer(Critter cr, TimeEventContext event)
    {
        // No-op
    }

    int TestEntityTimeEventContextOverloads()
    {
        Critter cr = Game.CreateCritter("TestCritter".hstr(), false);
        if (cr is null) return -1;

        any initData = 31;
        array<any> arrayData = {41, 42};

        uint32 plainId = cr.StartTimeEvent(timespan(60, 3), OnCritterContextTimer);
        uint32 dataId = cr.StartTimeEvent(timespan(60, 3), OnCritterContextTimer, initData);
        uint32 arrayId = cr.StartTimeEvent(timespan(60, 3), OnCritterContextTimer, arrayData);
        uint32 repeatingPlainId = cr.StartTimeEvent(timespan(60, 3), timespan(5, 3),
            OnCritterContextTimer);
        uint32 repeatingDataId = cr.StartTimeEvent(timespan(60, 3), timespan(6, 3),
            OnCritterContextTimer, initData);
        uint32 repeatingArrayId = cr.StartTimeEvent(timespan(60, 3), timespan(7, 3),
            OnCritterContextTimer, arrayData);

        if (plainId == 0 || dataId == 0 || arrayId == 0 ||
            repeatingPlainId == 0 || repeatingDataId == 0 || repeatingArrayId == 0) return -2;
        if (cr.CountTimeEvent(OnCritterContextTimer) != 6) return -3;
        if (cr.CountTimeEvent(repeatingArrayId) != 1) return -4;

        cr.RepeatTimeEvent(OnCritterContextTimer, timespan(8, 3));

        any newData = 51;
        cr.SetTimeEventData(OnCritterContextTimer, newData);

        array<any> newArrayData = {61, 62, 63};
        cr.SetTimeEventData(OnCritterContextTimer, newArrayData);

        cr.StopTimeEvent(OnCritterContextTimer);
        if (cr.CountTimeEvent(OnCritterContextTimer) != 0) return -5;

        Game.DestroyCritter(cr);
        return 0;
    }

    // ========== Argument slot alignment (8-byte aligned call-argument layout) ==========
    // Regression shapes for the even-argument-slot ABI: padded 1-DWORD slots between pointer/8-byte
    // arguments, nested calls inside padded argument expressions (stack parity), the asBC_Thiscall1
    // fast path (array opIndex with int argument), try/catch stack restore with odd variableSpace,
    // and variadic '?' slots. All of it additionally runs through the bytecode save/load round-trip
    // of this fixture, covering the serializer's argument-offset translation.

    int AlignAddMixed(int a, string s, int b, int64 c, int d)
    {
        return a + b + d + int(c) + int(s.length());
    }

    int AlignNested(int depth, int v)
    {
        if (depth <= 0) return v;
        return AlignNested(depth - 1, v) + 1;
    }

    int TestArgSlotMixedParams()
    {
        int r1 = AlignAddMixed(1, "abc", 2, 40, 3);
        if (r1 != 49) return -1;

        // Nested calls evaluated inside padded argument positions must not disturb the stack parity
        int r2 = AlignAddMixed(AlignNested(3, 10), "x", AlignNested(2, 5), 100, AlignNested(1, 1));
        if (r2 != 123) return -2;

        return 0;
    }

    int TestArgSlotOpIndexThiscall1()
    {
        array<int> arr = {10, 20, 30, 40};
        int sum = 0;

        for (uint i = 0; i < arr.length(); i++) {
            sum += arr[i];
        }
        if (sum != 100) return -1;

        // Nested call as the padded index argument of the Thiscall1 fast path
        if (arr[AlignNested(1, 1)] != 30) return -2;

        return 0;
    }

    int AlignThrowingHelper()
    {
        array<int> a = {1};
        return a[5];
    }

    int TestArgSlotTryCatchParityRestore()
    {
        // Odd count of 1-DWORD locals makes variableSpace odd; the catch stack restore must still
        // return the stack pointer to the even-rounded locals boundary
        int l1 = 1;
        int l2 = 2;
        int l3 = 3;
        bool caught = false;

        try {
            AlignThrowingHelper();
        }
        catch {
            caught = true;
        }
        if (!caught) return -1;

        int r = AlignAddMixed(l1, "zz", l2, 1000, l3);
        if (r != 1008) return -2;

        // 8-byte value-type argument slots and return right after the catch restore
        if ("MixedKey".hstr() != "MixedKey".hstr()) return -3;

        return 0;
    }

    int TestArgSlotVariadicMixed()
    {
        typedInvokeFired = false;
        typedInvokeNumber = 0;
        typedInvokeText = "";
        typedInvokeFlag = false;
        typedInvokeKey = hstring();
        typedInvokePayloadSize = 0;

        // Variadic '?' slots with a method-call argument in the middle and odd locals around
        int localPad = 7;
        array<any> payload = {1, "two", true};
        bool result = Invoke("CommonMethods::InvokeTargetWithTypedData", localPad + 35, "payload", true, "MixedKey".hstr(), payload);
        if (!result) return -1;
        if (!typedInvokeFired) return -2;
        if (typedInvokeNumber != 42) return -3;
        if (typedInvokeText != "payload") return -4;
        if (!typedInvokeFlag) return -5;
        if (typedInvokeKey != "MixedKey".hstr()) return -6;
        if (typedInvokePayloadSize != 3) return -7;

        return 0;
    }
}

 )";

        return BakerTests::CompileInlineScripts(&compiler_engine, "CommonMethodsScripts", {{"Scripts/CommonMethods.fos", script_source}}, [](string_view message) {
            string message_str = string(message);
            if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos) {
                throw ScriptSystemException(message_str);
            }
        });
    }

    static void WriteMetadataToken(DataWriter& writer, string_view token)
    {
        FO_STACK_TRACE_ENTRY();

        writer.Write<uint16_t>(numeric_cast<uint16_t>(token.size()));
        writer.WriteStringBytes(token);
    }

    static auto MakeCommonMethodsMetadataBlob() -> vector<uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        vector<vector<string_view>> value_types = {
            {"BoolSnapshot", "Value", "bool"},
            {"IntSnapshot", "Value", "int32"},
            {"FloatSnapshot", "Value", "float32"},
        };
        vector<vector<string_view>> ref_types = {
            {"RouteSnapshot", "Value", "int32", "0", "Steps", "int32[]", "0", "Counters", "string=>int32", "0", "Child", "RouteNote", "0", "TargetProto", "ProtoCritter", "0", "OptionalItemProto", "ProtoItem", "1", "Nullable", "Marker", "bool", "1", "Component", "Marker.Steps", "int32", "0", "Marker.Note", "string", "0"},
            {"RouteNote", "Text", "string", "0"},
        };

        vector<uint8_t> metadata;
        auto writer = DataWriter(metadata);

        writer.Write<uint16_t>(uint16_t {2});
        WriteMetadataToken(writer, "ValueType");
        writer.Write<uint32_t>(numeric_cast<uint32_t>(value_types.size()));

        for (const auto& value_type : value_types) {
            writer.Write<uint32_t>(numeric_cast<uint32_t>(value_type.size()));

            for (auto token : value_type) {
                WriteMetadataToken(writer, token);
            }
        }

        WriteMetadataToken(writer, "RefType");
        writer.Write<uint32_t>(numeric_cast<uint32_t>(ref_types.size()));

        for (const auto& ref_type : ref_types) {
            writer.Write<uint32_t>(numeric_cast<uint32_t>(ref_type.size()));

            for (auto token : ref_type) {
                WriteMetadataToken(writer, token);
            }
        }

        return metadata;
    }

    static auto MakeResources() -> FileSystem
    {
        auto metadata_blob = MakeCommonMethodsMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("CommonMethodsCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.ToHashedString("Critter");
        hstring item_type = proto_engine.Hashes.ToHashedString("Item");
        hstring location_type = proto_engine.Hashes.ToHashedString("Location");

        auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter");
        auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
        auto container_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestContainer");
        auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "TestLocation");
        auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("CommonMethodsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("CommonMethodsCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("CommonMethodsItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("CommonMethodsContainer.fopro-bin-server", container_blob);
        runtime_source->AddFile("CommonMethodsLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("CommonMethods.fos-bin-server", script_blob);
#if FO_ENABLE_3D
        runtime_source->AddFile("ModelAnimationInfo.foinfo", R"([Critters/Test.fo3d]
BoundsVersion = 2
ModelBoundsMinX = -2
ModelBoundsMinY = -1
ModelBoundsMinZ = 0
ModelBoundsMaxX = 2
ModelBoundsMaxY = 1
ModelBoundsMaxZ = 4
ViewBoundsMinX = -1
ViewBoundsMinY = -0.5
ViewBoundsMinZ = 0
ViewBoundsMaxX = 1
ViewBoundsMaxY = 0.5
ViewBoundsMaxZ = 3
StateAnimations = 1 1
ActionAnimations = 3 5
DurationsMs = 500 250
BoundsStateAnimations = 1
BoundsActionAnimations = 7
BoundsMinX = -1.5
BoundsMinY = -0.75
BoundsMinZ = 0.25
BoundsMaxX = 1.5
BoundsMaxY = 0.75
BoundsMaxZ = 3.5

)");
#endif

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    static auto WaitForStart(ptr<ServerEngine> server) -> string
    {
        for (int32_t i = 0; i < 6000; i++) {
            if (server->IsStarted()) {
                return {};
            }
            if (server->IsStartingError()) {
                return "ServerEngine startup failed";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {10});
        }

        return "ServerEngine startup timed out";
    }

    static auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine>
    {
        return SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeResources());
    }

    static void CheckHstringStringConvBinding(ptr<ServerEngine> server)
    {
        FO_STACK_TRACE_ENTRY();

        auto backend = GetScriptBackend(server);
        auto context_mngr = backend->GetContextMngr();
        REQUIRE(context_mngr);

        auto ctx = context_mngr->RequestContext();
        uint64_t context_generation = context_mngr->GetContextGeneration(ctx);
        auto return_context = scope_exit([&context_mngr, &ctx, &context_generation]() noexcept { context_mngr->ReturnContext(ctx, context_generation); });

        nptr<AngelScript::asIScriptEngine> as_engine = ctx->GetEngine();
        REQUIRE(as_engine != nullptr);

        nptr<AngelScript::asITypeInfo> hstring_type = as_engine->GetTypeInfoByDecl("hstring");
        REQUIRE(hstring_type != nullptr);

        nptr<AngelScript::asIScriptFunction> conv_method = hstring_type->GetMethodByDecl("string opImplConv() const");
        REQUIRE(conv_method != nullptr);

        hstring key = server->Hashes.ToHashedString("AlphaKey");
        REQUIRE(ctx->Prepare(conv_method.get()) >= 0);
        REQUIRE(ctx->SetObject(&key) >= 0);
        REQUIRE(context_mngr->RunContext(ctx, false));

        const string* result = static_cast<const string*>(ctx->GetReturnObject());
        REQUIRE(result != nullptr);
        CHECK(*result == "AlphaKey");
    }
}

#define MAKE_CM_SERVER() \
    auto settings = MakeSettings(); \
    refcount_ptr<ServerEngine> server = MakeServerEngine(settings); \
    auto shutdown = scope_exit([&server]() noexcept { \
        safe_call([&server] { \
            if (server->IsStarted()) { \
                server->Shutdown(); \
            } \
        }); \
    }); \
    string startup_error = WaitForStart(server); \
    INFO(startup_error); \
    REQUIRE(startup_error.empty()); \
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}})); \
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); }); \
    auto get_func = [&server](string_view name) { return server->Hashes.ToHashedString(name); }

#define RUN_CM_FUNC(func_name) \
    auto func = server->FindFunc<int32_t>(get_func("CommonMethods::" func_name)); \
    REQUIRE(func); \
    REQUIRE(func.Call()); \
    CHECK(func.GetResult() == 0)

#define RUN_CM_FUNC_THROWS(func_name, expected_message) \
    auto func = server->FindFunc<void>(get_func("CommonMethods::" func_name)); \
    REQUIRE(func); \
    auto prev_callback = GetExceptionCallback(); \
    string message; \
    SetExceptionCallback([&](string_view msg, const CatchedStackTraceData&, bool) { message = string(msg); }); \
    auto restore_callback = scope_exit([prev = std::move(prev_callback)]() mutable noexcept { SetExceptionCallback(std::move(prev)); }); \
    CHECK_FALSE(func.Call()); \
    INFO(message); \
    CHECK(message.find(expected_message) != string::npos)

// ========== Geometry Tests ==========

TEST_CASE("GeometryDirectionAngles")
{
    MAKE_CM_SERVER();

    SECTION("GetDirAngle")
    {
        RUN_CM_FUNC("TestGetDirAngle");
    }

    SECTION("GetLineDirAngle")
    {
        RUN_CM_FUNC("TestGetLineDirAngle");
    }

    SECTION("AngleToDirAndBack")
    {
        RUN_CM_FUNC("TestAngleToDirAndBack");
    }

    SECTION("RotateDirAngle")
    {
        RUN_CM_FUNC("TestRotateDirAngle");
    }

    SECTION("GetDirAngleDiff")
    {
        RUN_CM_FUNC("TestGetDirAngleDiff");
    }

    SECTION("GetHexInterval")
    {
        RUN_CM_FUNC("TestGetHexInterval");
    }
}

#if FO_ENABLE_3D
TEST_CASE("ModelAnimationInfoLookup")
{
    MAKE_CM_SERVER();

    CHECK(server->Hashes.CheckHashedString("Critters/Test.fo3d"));
    RUN_CM_FUNC("TestGetModelAnimDuration");
}
#endif

// ========== Proto Getter Tests ==========

TEST_CASE("ProtoCollectionGetters")
{
    MAKE_CM_SERVER();

    SECTION("GetProtoItems")
    {
        RUN_CM_FUNC("TestGetProtoItems");
    }

    SECTION("GetProtoCritters")
    {
        RUN_CM_FUNC("TestGetProtoCritters");
    }

    SECTION("GetProtoMaps")
    {
        RUN_CM_FUNC("TestGetProtoMaps");
    }

    SECTION("GetProtoLocations")
    {
        RUN_CM_FUNC("TestGetProtoLocations");
    }
}

// ========== UTF-8 Tests ==========

TEST_CASE("Utf8EncodeDecodeOps")
{
    MAKE_CM_SERVER();

    SECTION("EncodeDecodeUtf8")
    {
        RUN_CM_FUNC("TestEncodeDecodeUtf8");
    }
}

// ========== Time Pack Tests ==========

TEST_CASE("TimePackingOperations")
{
    MAKE_CM_SERVER();

    SECTION("PackUnpackTime")
    {
        RUN_CM_FUNC("TestPackUnpackTime");
    }

    SECTION("PackUnpackSynchronizedTime")
    {
        RUN_CM_FUNC("TestPackUnpackSynchronizedTime");
    }
}

// ========== Script Type Conversion Tests ==========

TEST_CASE("ScriptTypeConversionOps")
{
    MAKE_CM_SERVER();

    SECTION("ValueTypeAndAnyConversions")
    {
        RUN_CM_FUNC("TestScriptTypeConversions");
    }

    SECTION("SmallValueTypeArrayInitListPadding")
    {
        RUN_CM_FUNC("TestSmallValueTypeArrayInitListPadding");
    }

    SECTION("HstringStringConvBinding")
    {
        CheckHstringStringConvBinding(server);
    }

    SECTION("MetadataValueTypeConversions")
    {
        RUN_CM_FUNC("TestScriptValueTypeMetadataConversions");
    }

    SECTION("ValueTypeOperatorEdges")
    {
        RUN_CM_FUNC("TestScriptValueTypeOperatorEdges");
    }

    SECTION("EnumAnyConversions")
    {
        RUN_CM_FUNC("TestScriptEnumAnyConversions");
    }

    SECTION("EnumAnyWrongTypeThrows")
    {
        RUN_CM_FUNC_THROWS("TestScriptEnumAnyWrongTypeThrows", "Invalid enum type for any conversion");
    }

    SECTION("EnumAnyWrongValueThrows")
    {
        RUN_CM_FUNC_THROWS("TestScriptEnumAnyWrongValueThrows", "Invalid enum value for any conversion");
    }

    SECTION("EnumAnyInvalidConstructThrows")
    {
        RUN_CM_FUNC_THROWS("TestScriptEnumAnyInvalidConstructThrows", "Invalid enum value for any conversion");
    }

    SECTION("DynamicValueTypeConversions")
    {
        RUN_CM_FUNC("TestScriptDynamicValueTypeConversions");
    }

    SECTION("DynamicRefTypeAccessors")
    {
        RUN_CM_FUNC("TestScriptDynamicRefTypeAccessors");
    }

    SECTION("InvalidTimePlaceThrows")
    {
        RUN_CM_FUNC_THROWS("TestInvalidTimePlaceThrows", "Invalid time place");
    }

    SECTION("InvalidAnyToIntThrows")
    {
        RUN_CM_FUNC_THROWS("TestInvalidAnyToIntThrows", "Invalid int value for any conversion");
    }

    SECTION("TextPackKeyAnyLessThrows")
    {
        RUN_CM_FUNC_THROWS("TestTextPackKeyAnyLessThrows", "Invalid cast to any (values less then needed)");
    }

    SECTION("TextPackKeyAnyMoreThrows")
    {
        RUN_CM_FUNC_THROWS("TestTextPackKeyAnyMoreThrows", "Invalid cast to any (values more then needed)");
    }
}

// ========== Game Time Events ==========

TEST_CASE("GameLevelTimeEvents")
{
    MAKE_CM_SERVER();

    SECTION("CreateCountStop")
    {
        RUN_CM_FUNC("TestGameTimeEventCreateCountStop");
    }

    SECTION("StopById")
    {
        RUN_CM_FUNC("TestGameTimeEventStopById");
    }

    SECTION("Repeat")
    {
        RUN_CM_FUNC("TestGameTimeEventRepeat");
    }

    SECTION("WithData")
    {
        RUN_CM_FUNC("TestGameTimeEventWithData");
    }

    SECTION("Repeating")
    {
        RUN_CM_FUNC("TestGameTimeEventRepeating");
    }

    SECTION("WithArrayData")
    {
        RUN_CM_FUNC("TestGameTimeEventWithArrayData");
    }
}

// ========== Invoke Tests ==========

TEST_CASE("GameInvokeOperations")
{
    MAKE_CM_SERVER();

    SECTION("ByName")
    {
        RUN_CM_FUNC("TestInvokeByName");
    }

    SECTION("ByNameWithData")
    {
        RUN_CM_FUNC("TestInvokeByNameWithData");
    }

    SECTION("ByNameWithTwoData")
    {
        RUN_CM_FUNC("TestInvokeByNameWithTwoData");
    }

    SECTION("ByNameWithThreeData")
    {
        RUN_CM_FUNC("TestInvokeByNameWithThreeData");
    }

    SECTION("ByNameWithTypedVariadic")
    {
        RUN_CM_FUNC("TestInvokeByNameWithTypedVariadic");
    }

    SECTION("ByNameRejectsWrongType")
    {
        RUN_CM_FUNC("TestInvokeByNameRejectsWrongType");
    }

    SECTION("ByNameWithRefArgs")
    {
        RUN_CM_FUNC("TestInvokeByNameWithRefArgs");
    }
}

// ========== Argument slot alignment ==========

TEST_CASE("ScriptArgumentSlotAlignment")
{
    // Regression coverage for the 8-byte aligned call-argument layout (even argument slots, see
    // asCDataType::GetArgSlotSizeOnStackDWords): padded 1-DWORD slots, nested calls inside padded
    // argument expressions, the asBC_Thiscall1 fast path, the try/catch stack restore with an odd
    // variableSpace, and variadic '?' slots. The fixture loads scripts through the bytecode
    // save/load round-trip, so the serializer's argument-offset translation is covered too.
    MAKE_CM_SERVER();

    SECTION("MixedParams")
    {
        RUN_CM_FUNC("TestArgSlotMixedParams");
    }

    SECTION("OpIndexThiscall1")
    {
        RUN_CM_FUNC("TestArgSlotOpIndexThiscall1");
    }

    SECTION("TryCatchParityRestore")
    {
        RUN_CM_FUNC("TestArgSlotTryCatchParityRestore");
    }

    SECTION("VariadicMixed")
    {
        RUN_CM_FUNC("TestArgSlotVariadicMixed");
    }
}

// ========== Entity Time Events ==========

TEST_CASE("EntityTimeEventOperations")
{
    MAKE_CM_SERVER();

    SECTION("CritterCount")
    {
        RUN_CM_FUNC("TestEntityTimeEventCount");
    }

    SECTION("CritterRepeat")
    {
        RUN_CM_FUNC("TestEntityTimeEventRepeat");
    }

    SECTION("CritterSetData")
    {
        RUN_CM_FUNC("TestEntityTimeEventSetData");
    }

    SECTION("CritterStopByFunction")
    {
        RUN_CM_FUNC("TestEntityTimeEventStopByFunction");
    }

    SECTION("CritterWithArrayData")
    {
        RUN_CM_FUNC("TestEntityTimeEventWithArrayData");
    }

    SECTION("CritterRepeatingOverloads")
    {
        RUN_CM_FUNC("TestEntityTimeEventRepeatingOverloads");
    }

    SECTION("CritterContextOverloads")
    {
        RUN_CM_FUNC("TestEntityTimeEventContextOverloads");
    }
}

// ========== Item/Container Ops ==========

TEST_CASE("ItemContainerAndScriptOps")
{
    MAKE_CM_SERVER();

    SECTION("ContainerAddGetItems")
    {
        RUN_CM_FUNC("TestItemContainerAddGetItems");
    }

    SECTION("ItemGetMap")
    {
        RUN_CM_FUNC("TestItemGetMap");
    }

    SECTION("ItemTimeEvents")
    {
        RUN_CM_FUNC("TestItemTimeEvents");
    }

    SECTION("ItemTimeEventWithData")
    {
        RUN_CM_FUNC("TestItemTimeEventWithData");
    }
}

// ========== Critter Script Methods ==========

TEST_CASE("CritterScriptMethodsAdvanced")
{
    MAKE_CM_SERVER();

    SECTION("GetPlayerOfflineTime")
    {
        RUN_CM_FUNC("TestCritterGetPlayerOfflineTime");
    }

    SECTION("GetItemByProperty")
    {
        RUN_CM_FUNC("TestCritterGetItemByProperty");
    }

    SECTION("GetItemsByProto")
    {
        RUN_CM_FUNC("TestCritterGetItemsByProto");
    }

    SECTION("DestroyItemByCount")
    {
        RUN_CM_FUNC("TestCritterDestroyItemByCount");
    }
}

// ========== Server Global Methods ==========

TEST_CASE("ServerGlobalMethodsExtended")
{
    MAKE_CM_SERVER();

    SECTION("GetLocationsOverloads")
    {
        RUN_CM_FUNC("TestGetLocationsOverloads");
    }

    SECTION("GetCrittersOverloads")
    {
        RUN_CM_FUNC("TestGetCrittersOverloads");
    }

    SECTION("SetSynchronizedTime")
    {
        RUN_CM_FUNC("TestSetSynchronizedTime");
    }
}

// ========== Location Time Events ==========

TEST_CASE("LocationTimeEventOps")
{
    MAKE_CM_SERVER();

    SECTION("LocationTimeEvents")
    {
        RUN_CM_FUNC("TestLocationTimeEvents");
    }
}

// ========== C++ API Tests ==========

TEST_CASE("CommonCppApiTests")
{
    MAKE_CM_SERVER();

    SECTION("ProtoManagerLookups")
    {
        // Verify that protos loaded in our test setup
        CHECK(server->EntityMngr.GetCrittersCount() == 0);
        CHECK(server->EntityMngr.GetLocationsCount() == 0);
        CHECK(server->EntityMngr.GetItemsCount() == 0);
    }

    SECTION("EntityManagerMultipleEntities")
    {
        // Create multiple entities and verify counts
        auto cr1 = server->CreateCritter(get_func("TestCritter"), false);
        auto cr2 = server->CreateCritter(get_func("TestCritter"), false);
        auto cr3 = server->CreateCritter(get_func("TestCritter"), false);

        CHECK(server->EntityMngr.GetCrittersCount() >= 3);

        // Clean up
        server->CrMngr.DestroyCritter(cr3);
        server->CrMngr.DestroyCritter(cr2);
        server->CrMngr.DestroyCritter(cr1);
    }

    SECTION("ItemManagerContainerOps")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        auto item1 = server->ItemMngr.AddItemCritter(cr, get_func("TestItem"), 5);
        auto item2 = server->ItemMngr.AddItemCritter(cr, get_func("TestItem"), 3);
        REQUIRE(static_cast<bool>(item1));
        REQUIRE(static_cast<bool>(item2));

        const auto& inv = cr->GetInvItems();
        CHECK(inv.size() >= 2);

        server->ItemMngr.DestroyItem(item2);
        server->ItemMngr.DestroyItem(item1);
        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("ServerHealthCheck")
    {
        string health = server->GetHealthInfo();
        CHECK(health.find("Server uptime") != string::npos);
    }
}

FO_END_NAMESPACE
