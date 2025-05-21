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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "StringUtils.h"

FO_BEGIN_NAMESPACE();

TEST_CASE("StringUtils")
{
    SECTION("Storage")
    {
        CHECK(strex("hllo").erase('h').trim().str() == "llo");
        CHECK(strex("h\rllo").erase('h').trim().str() == "llo");
        CHECK(strex("hllo\t\n").erase('h').trim().str() == "llo");
        CHECK(strex("h    llo    ").erase('h').trim().str() == "llo");
        CHECK(strex("h d     g      llo                            gg  ").erase('h').trim().erase('d').trim().str() == "g      llo                            gg");
    }

    SECTION("Length")
    {
        CHECK(strex().length() == 0);
        CHECK(strex(" hello ").length() == 7);
        CHECK(strex("").empty());
        CHECK_FALSE(strex("fff").empty());
    }

    SECTION("Parse")
    {
        CHECK(strex("  {} World {}", "Hello", "!").str() == "  Hello World !");
        CHECK(strex("{}{}{}", 1, 2, 3).str() == "123");
    }

    SECTION("Trim")
    {
        CHECK(strex() == "");
        CHECK(strex().str().empty());
        CHECK(*strex().c_str() == 0);
        CHECK(strex("Hello").str() == "Hello");
        CHECK(strex("Hello   ").trim().str() == "Hello");
        CHECK(strex("  Hello").trim().str() == "Hello");
        CHECK(strex("\t\nHel lo\t \r\t").trim().str() == "Hel lo");
    }

    SECTION("GetResult")
    {
        CHECK(static_cast<string_view>(strex(" Hello   ").trim()) == "Hello");
        CHECK(static_cast<string>(strex(" \tHello   ").trim()) == "Hello");
        CHECK(strex("Hello   ").trim().str() == "Hello");
        CHECK(string_view("Hello") == strex("  Hello").trim().c_str());
        CHECK(strex("\t\nHel lo\t \r\t").trim().str() == "Hel lo");
    }

    SECTION("Compare")
    {
        CHECK(strex(" Hello   ").str() == strex(" Hello   ").str());
        CHECK_FALSE(strex(" Hello1   ").str() == strex(" Hello2   ").str());
        CHECK(strex(" Hello   ").compareIgnoreCase(" heLLo   "));
        CHECK_FALSE(strex(" Hello   ").compareIgnoreCase(" hhhhh   "));
        CHECK_FALSE(strex(" Hello   ").compareIgnoreCase("xxx"));
    }

    SECTION("Utf8")
    {
        CHECK(strex("").isValidUtf8());
        CHECK_FALSE(strex(string(1, static_cast<char>(200))).isValidUtf8());
        CHECK(strex(" приВЕт   ").isValidUtf8());
        CHECK(strex(" приВЕт   ").isValidUtf8());
        CHECK(strex(" Привет   ").compareIgnoreCaseUtf8(" приВЕт   "));
        CHECK_FALSE(strex(" Привет   ").compareIgnoreCaseUtf8(" тттввв   "));
        CHECK_FALSE(strex(" Привет   ").compareIgnoreCaseUtf8("еее"));
        CHECK(strex(" Привет   ").lengthUtf8() == 10);
        CHECK(strex("еее").lengthUtf8() == 3);
        CHECK(strex(" Привет   ").compareIgnoreCaseUtf8(" ПРИВЕТ   "));
        CHECK_FALSE(strex(" Привет   ").compareIgnoreCaseUtf8(" ПРЕТТТ   "));
        CHECK(strex(" ПриВЕТ   ").lowerUtf8() == " привет   ");
        CHECK(strex(" ПриВет   ").upperUtf8() == " ПРИВЕТ   ");
    }

    SECTION("StatsWith")
    {
        CHECK(strex(" Hello   ").startsWith(" Hell"));
        CHECK(strex("xHello   ").startsWith('x'));
        CHECK(strex(" Hello1   ").endsWith("1   "));
        CHECK(strex(" Hello1  x").endsWith('x'));
    }

    SECTION("Numbers")
    {
        CHECK_FALSE(strex("").isNumber());
        CHECK_FALSE(strex("   ").isNumber());
        CHECK_FALSE(strex("\t\n\r").isNumber());
        CHECK_FALSE(strex("0x").isNumber());
        CHECK_FALSE(strex("-0x").isNumber());
        CHECK(strex("-0x5").isNumber());
        CHECK(strex("-0xa").isNumber());
        CHECK_FALSE(strex("--1").isNumber());
        CHECK_FALSE(strex("-0xy").isNumber());
        CHECK_FALSE(strex("0XZ").isNumber());
        CHECK(strex("0x1").isNumber());
        CHECK(strex("123").isNumber());
        CHECK(strex("0x123").isNumber());
        CHECK(strex(" 123").isNumber());
        CHECK(strex("123 \r\n\t").isNumber());
        CHECK_FALSE(strex("123llll").isNumber());
        CHECK_FALSE(strex("123+=").isNumber());
        CHECK(strex("1.0").isNumber());
        CHECK_FALSE(strex("x123").isNumber());
        CHECK(strex("\t0123").isNumber());
        CHECK(strex(" 0x123").isNumber());
        CHECK(strex("1.0f").isNumber());
        CHECK(strex("12.0").isNumber());
        CHECK(strex("12").isNumber());
        CHECK(strex("12.0f").isNumber());
        CHECK(strex("1.0f").isNumber());
        CHECK(strex(string(strex::MAX_NUMBER_STRING_LENGTH, '5')).isNumber());
        CHECK_FALSE(strex(string(strex::MAX_NUMBER_STRING_LENGTH + 1, '5')).isNumber());

        CHECK(strex("true").isExplicitBool());
        CHECK(strex("TRUE ").isExplicitBool());
        CHECK(strex("False  ").isExplicitBool());
        CHECK(strex(" \t  False\t").isExplicitBool());
        CHECK_FALSE(strex("1").isExplicitBool());
        CHECK_FALSE(strex("").isExplicitBool());

        CHECK(strex("").toInt() == 0);
        CHECK(strex("00000000012").toInt() == 12);
        CHECK(strex("1").toInt() == 1);
        CHECK(strex("-156").toInt() == -156);
        CHECK(strex("-429496729500").toInt() == std::numeric_limits<int>::min());
        CHECK(strex("4294967295").toInt() == std::numeric_limits<int>::max());
        CHECK(strex("0xFFFF").toInt() == 0xFFFF);
        CHECK(strex("-0xFFFF").toInt() == -0xFFFF);
        CHECK(strex("012345").toInt() == 12345); // Do not recognize octal numbers
        CHECK(strex("-012345").toInt() == -12345);
        CHECK(strex("0xFFFF.88").toInt() == 0);
        CHECK(strex("12.9").toInt() == 12);
        CHECK(strex("12.1111").toInt() == 12);

        CHECK(strex("111").toUInt() == 111);
        CHECK(strex("-100").toUInt() == std::numeric_limits<uint>::min());
        CHECK(strex("1000000000000").toUInt() == std::numeric_limits<uint>::max());
        CHECK(strex("4294967295").toUInt() == 4294967295);

        CHECK(strex("66666666666677").toInt64() == 66666666666677ll);
        CHECK(strex("66666666666677.087468726").toInt64() == 66666666666677ll);
        CHECK(strex("66666666666677.33f").toInt64() == 66666666666677ll);
        CHECK(strex("-666666666666.").toInt64() == -666666666666ll);
        CHECK(strex("-666666666666.f").toInt64() == -666666666666ll);
        CHECK(strex("-666666666666.111111111111111").toInt64() == -666666666666ll);
        CHECK(strex("-9223372036854775808").toInt64() == std::numeric_limits<int64>::min()); // Min 64bit signed -9223372036854775808
        CHECK(strex("-9223372036854775807").toInt64() == (std::numeric_limits<int64>::min() + 1));
        CHECK(strex("-9223372036854775809").toInt64() == std::numeric_limits<int64>::min());
        CHECK(strex("-9223372036854779999").toInt64() == std::numeric_limits<int64>::min());
        CHECK(strex("-9223372036854775809").toInt64() == std::numeric_limits<int64>::min());
        CHECK(strex("-18446744073709551614").toInt64() == std::numeric_limits<int64>::min());
        CHECK(strex("-18446744073709551616").toInt64() == std::numeric_limits<int64>::min());
        CHECK(strex("-18446744073456345654709551616").toInt64() == std::numeric_limits<int64>::min());
        CHECK(strex("9223372036854775807").toInt64() == std::numeric_limits<int64>::max()); // Max 64bit signed 9223372036854775807
        CHECK(strex("9223372036854775806").toInt64() == (std::numeric_limits<int64>::max() - 1));
        CHECK_FALSE(strex("9223372036854775808").toInt64() == std::numeric_limits<int64>::max());
        CHECK_FALSE(strex("18446744073709551615").toInt64() == std::numeric_limits<int64>::max());
        CHECK(strex("18446744073709551615").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max())); // Max 64bit 18446744073709551615
        CHECK(strex("18446744073709551614").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max() - 1));
        CHECK(strex("18446744073709551616").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max()));
        CHECK(strex("184467440737095516546734734716").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max()));
        CHECK(strex(string(strex::MAX_NUMBER_STRING_LENGTH, '5')).toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max()));
        CHECK(strex(string(strex::MAX_NUMBER_STRING_LENGTH + 1, '5')).toInt64() == 0);

        CHECK(is_float_equal(strex("455.6573").toFloat(), 455.6573f));
        CHECK(is_float_equal(strex("0xFFFF").toFloat(), static_cast<float>(0xFFFF)));
        CHECK(is_float_equal(strex("-0xFFFF").toFloat(), static_cast<float>(-0xFFFF)));
        CHECK(is_float_equal(strex("0xFFFF.44").toFloat(), static_cast<float>(0)));
        CHECK(is_float_equal(strex("f").toFloat(), static_cast<float>(0)));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<float>::min()).toFloat(), std::numeric_limits<float>::min()));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<float>::max()).toFloat(), std::numeric_limits<float>::max()));
        CHECK(is_float_equal(strex("34567774455.65745678555").toDouble(), 34567774455.65745678555));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<double>::min()).toDouble(), std::numeric_limits<double>::min()));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<double>::max()).toDouble(), std::numeric_limits<double>::max()));

        CHECK(strex(" true ").toBool() == true);
        CHECK(strex(" 1 ").toBool() == true);
        CHECK(strex(" 211 ").toBool() == true);
        CHECK(strex(" false ").toBool() == false);
        CHECK(strex(" 0").toBool() == false);
        CHECK(strex(" abc").toBool() == false);
    }

    SECTION("Split")
    {
        CHECK(strex(" One Two    \tThree   ").split(' ') == vector<string>({"One", "Two", "Three"}));
        CHECK(strex(" One Two    \tThree   ").split('\t') == vector<string>({"One Two", "Three"}));
        CHECK(strex(" One Two    \tThree   ").split('X') == vector<string>({"One Two    \tThree"}));
        CHECK(strex(" One Two  X \tThree   ").split('X') == vector<string>({"One Two", "Three"}));
        CHECK(strex(" 111 222  33Three g66 7").splitToInt(' ') == vector<int>({111, 222, 0, 0, 7}));
        CHECK(strex("").splitToInt(' ') == vector<int>({}));
        CHECK(strex("             ").splitToInt(' ') == vector<int>({}));
        CHECK(strex("1").splitToInt(' ') == vector<int>({1}));
        CHECK(strex("1 -2").splitToInt(' ') == vector<int>({1, -2}));
        CHECK(strex("\t1   X -2 X 3").splitToInt('X') == vector<int>({1, -2, 3}));
        CHECK(strex("\t1 X\t\t  X X-2   X X 3\n").splitToInt('X') == vector<int>({1, -2, 3}));
    }

    SECTION("Substring")
    {
        CHECK(strex("abcdpZ ppplZ ls").substringUntil('Z').str() == "abcdp");
        CHECK(strex("abcdpZ ppplZ ls").substringUntil('X').str() == "abcdpZ ppplZ ls");
        CHECK(strex("abcdpZ ppplZ ls").substringUntil("lZ").str() == "abcdpZ ppp");
        CHECK(strex("abcdpZ ppplZ ls").substringUntil("XXX").str() == "abcdpZ ppplZ ls");
        CHECK(strex("abcdpZ ppplZ ls").substringAfter('Z').str() == " ppplZ ls");
        CHECK(strex("abcdpZ ppplZ ls").substringAfter('X').str().empty());
        CHECK(strex("abcdpZ ppplZ ls").substringAfter("lZ").str() == " ls");
        CHECK(strex("abcdpZ ppplZ ls").substringAfter("XXX").str().empty());
    }

    SECTION("Modify")
    {
        CHECK(strex("aaaBBBcccDBEFGh Hello !").erase('B').str() == "aaacccDEFGh Hello !");
        CHECK(strex("aaaBBBcccDBEFaGh HelBlo Ba!").erase('a', 'B').str() == "BBcccDBEFlo Ba!");
        CHECK(strex("aaaBBBcccDBEFaGh HelBlo Ba!").erase('X', 'Y').str() == "aaaBBBcccDBEFaGh HelBlo Ba!");
        CHECK(strex("aaaBBBcccDBEFaGh HelBlo Ba!").replace('a', 'X').str() == "XXXBBBcccDBEFXGh HelBlo BX!");
        CHECK(strex("aaBDdDBaBBBDBcccDBEFaGh HelDBBlo Ba!").replace('D', 'B', 'X').str() == "aaBDdXaBBBXcccXEFaGh HelXBlo Ba!");
        CHECK(strex("aaBDdDBaHelDBBlocccDBEFaGh HelDBBlo Ba!").replace("HelDBBlo", "X").str() == "aaBDdDBaXcccDBEFaGh X Ba!");
        CHECK(strex("aaaBBBcccDBEFGh Hello !").lower().str() == "aaabbbcccdbefgh hello !");
        CHECK(strex("aaaBBBcccDBEFGh Hello !").upper().str() == "AAABBBCCCDBEFGH HELLO !");
    }

    SECTION("Path")
    {
        CHECK(strex("./cur").formatPath().str() == "cur");
        CHECK(strex("./cur/next").formatPath().str() == "cur/next");
        CHECK(strex("./cur/next/../last").formatPath().str() == "cur/last");
        CHECK(strex("./cur/next/../../last/").formatPath().str() == "last/");
        CHECK(strex("D:\\MyDir\\X\\..\\Y\\.\\.\\Z\\").formatPath().str() == "D:/MyDir/Y/Z/");
        CHECK(strex("./cur/next/../../last/a/FILE1.ZIP").formatPath().extractDir().str() == "last/a");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").formatPath().extractFileName().str() == "FILE1.ZIP");
        CHECK(strex("./cur/next/../../last/a/FILE1.ZIP").formatPath().extractDir().str() == "last/a");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").formatPath().changeFileName("NEWfile").str() == "last/NEWfile.zip");
        CHECK(strex("./cur/next/../../last").formatPath().changeFileName("NEWfile").str() == "NEWfile");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").formatPath().getFileExtension().str() == "zip");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").formatPath().eraseFileExtension().str() == "last/FILE1");
        CHECK(strex("cur/next/../../last/a/").combinePath("x/y/z").str() == "last/a/x/y/z");
        CHECK(strex("../cur/next/../../last/a/").combinePath("x/y/z").str() == "../last/a/x/y/z");
        CHECK(strex("../../cur/next/../../last/a/").combinePath("x/y/z").str() == "../../last/a/x/y/z");
        CHECK(strex("D:\\MyDir\\X\\..\\Y\\.\\.\\Z\\").normalizePathSlashes().str() == "D:/MyDir/X/../Y/././Z/");
    }

    SECTION("LineEndings")
    {
        CHECK(strex("Hello\r\nWorld\r\n").normalizeLineEndings().str() == "Hello\nWorld\n");
    }

#if FO_WINDOWS
    SECTION("WinParseWideChar")
    {
        CHECK(strex().parseWideChar(L"Hello").str() == "Hello");
        CHECK(strex("Hello").parseWideChar(L"World").str() == "HelloWorld");
        CHECK(strex().parseWideChar(L"HelloМир").toWideChar() == L"HelloМир");
        CHECK(strex("Мир").parseWideChar(L"Мир").toWideChar() == L"МирМир");
    }
#endif
}

FO_END_NAMESPACE();
