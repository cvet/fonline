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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "catch.hpp"

#include "StringUtils.h"

TEST_CASE("StringUtils")
{
    SECTION("Storage")
    {
        CHECK(format("hllo").erase('h').trim().str() == "llo");
        CHECK(format("h\rllo").erase('h').trim().str() == "llo");
        CHECK(format("hllo\t\n").erase('h').trim().str() == "llo");
        CHECK(format("h    llo    ").erase('h').trim().str() == "llo");
        CHECK(format("h d     g      llo                            gg  ").erase('h').trim().erase('d').trim().str() == "g      llo                            gg");
    }

    SECTION("Length")
    {
        CHECK(format().length() == 0);
        CHECK(format(" hello ").length() == 7);
        CHECK(format("").empty());
        CHECK_FALSE(format("fff").empty());
    }

    SECTION("Parse")
    {
        CHECK(format("  {} World {}", "Hello", "!").str() == "  Hello World !");
        CHECK(format("{}{}{}", 1, 2, 3).str() == "123");
    }

    SECTION("Trim")
    {
        CHECK(format() == "");
        CHECK(format().str().empty());
        CHECK(*format().c_str() == 0);
        CHECK(format("Hello").str() == "Hello");
        CHECK(format("Hello   ").trim().str() == "Hello");
        CHECK(format("  Hello").trim().str() == "Hello");
        CHECK(format("\t\nHel lo\t \r\t").trim().str() == "Hel lo");
    }

    SECTION("GetResult")
    {
        CHECK(static_cast<string_view>(format(" Hello   ").trim()) == "Hello");
        CHECK(static_cast<string>(format(" \tHello   ").trim()) == "Hello");
        CHECK(format("Hello   ").trim().str() == "Hello");
        CHECK(string_view("Hello") == format("  Hello").trim().c_str());
        CHECK(format("\t\nHel lo\t \r\t").trim().str() == "Hel lo");
    }

    SECTION("Compare")
    {
        CHECK(format(" Hello   ").str() == format(" Hello   ").str());
        CHECK_FALSE(format(" Hello1   ").str() == format(" Hello2   ").str());
        CHECK(format(" Hello   ").compareIgnoreCase(" heLLo   "));
        CHECK_FALSE(format(" Hello   ").compareIgnoreCase(" hhhhh   "));
        CHECK_FALSE(format(" Hello   ").compareIgnoreCase("xxx"));
    }

    SECTION("Utf8")
    {
        CHECK(format("").isValidUtf8());
        CHECK_FALSE(format(string(1, static_cast<char>(200))).isValidUtf8());
        CHECK(format(" приВЕт   ").isValidUtf8());
        CHECK(format(" приВЕт   ").isValidUtf8());
        CHECK(format(" Привет   ").compareIgnoreCaseUtf8(" приВЕт   "));
        CHECK_FALSE(format(" Привет   ").compareIgnoreCaseUtf8(" тттввв   "));
        CHECK_FALSE(format(" Привет   ").compareIgnoreCaseUtf8("еее"));
        CHECK(format(" Привет   ").lengthUtf8() == 10);
        CHECK(format("еее").lengthUtf8() == 3);
        CHECK(format(" Привет   ").compareIgnoreCaseUtf8(" ПРИВЕТ   "));
        CHECK_FALSE(format(" Привет   ").compareIgnoreCaseUtf8(" ПРЕТТТ   "));
        CHECK(format(" ПриВЕТ   ").lowerUtf8() == " привет   ");
        CHECK(format(" ПриВет   ").upperUtf8() == " ПРИВЕТ   ");
    }

    SECTION("StatsWith")
    {
        CHECK(format(" Hello   ").startsWith(" Hell"));
        CHECK(format("xHello   ").startsWith('x'));
        CHECK(format(" Hello1   ").endsWith("1   "));
        CHECK(format(" Hello1  x").endsWith('x'));
    }

    SECTION("Numbers")
    {
        CHECK_FALSE(format("").isNumber());
        CHECK_FALSE(format("   ").isNumber());
        CHECK_FALSE(format("\t\n\r").isNumber());
        CHECK_FALSE(format("0x").isNumber());
        CHECK_FALSE(format("-0x").isNumber());
        CHECK(format("-0x5").isNumber());
        CHECK(format("-0xa").isNumber());
        CHECK_FALSE(format("--1").isNumber());
        CHECK_FALSE(format("-0xy").isNumber());
        CHECK_FALSE(format("0XZ").isNumber());
        CHECK(format("0x1").isNumber());
        CHECK(format("123").isNumber());
        CHECK(format("0x123").isNumber());
        CHECK(format(" 123").isNumber());
        CHECK(format("123 \r\n\t").isNumber());
        CHECK_FALSE(format("123llll").isNumber());
        CHECK_FALSE(format("123+=").isNumber());
        CHECK(format("1.0").isNumber());
        CHECK_FALSE(format("x123").isNumber());
        CHECK(format("\t0123").isNumber());
        CHECK(format(" 0x123").isNumber());
        CHECK(format("1.0f").isNumber());
        CHECK(format("12.0").isNumber());
        CHECK(format("12").isNumber());
        CHECK(format("12.0f").isNumber());
        CHECK(format("1.0f").isNumber());
        CHECK(format(string(StringHelper::MAX_NUMBER_STRING_LENGTH, '5')).isNumber());
        CHECK_FALSE(format(string(StringHelper::MAX_NUMBER_STRING_LENGTH + 1, '5')).isNumber());

        CHECK(format("true").isExplicitBool());
        CHECK(format("TRUE ").isExplicitBool());
        CHECK(format("False  ").isExplicitBool());
        CHECK(format(" \t  False\t").isExplicitBool());
        CHECK_FALSE(format("1").isExplicitBool());
        CHECK_FALSE(format("").isExplicitBool());

        CHECK(format("").toInt() == 0);
        CHECK(format("00000000012").toInt() == 12);
        CHECK(format("1").toInt() == 1);
        CHECK(format("-156").toInt() == -156);
        CHECK(format("-429496729500").toInt() == std::numeric_limits<int>::min());
        CHECK(format("4294967295").toInt() == std::numeric_limits<int>::max());
        CHECK(format("0xFFFF").toInt() == 0xFFFF);
        CHECK(format("-0xFFFF").toInt() == -0xFFFF);
        CHECK(format("012345").toInt() == 12345); // Do not recognize octal numbers
        CHECK(format("-012345").toInt() == -12345);
        CHECK(format("0xFFFF.88").toInt() == 0);
        CHECK(format("12.9").toInt() == 12);
        CHECK(format("12.1111").toInt() == 12);

        CHECK(format("111").toUInt() == 111);
        CHECK(format("-100").toUInt() == std::numeric_limits<uint>::min());
        CHECK(format("1000000000000").toUInt() == std::numeric_limits<uint>::max());
        CHECK(format("4294967295").toUInt() == 4294967295);

        CHECK(format("66666666666677").toInt64() == 66666666666677ll);
        CHECK(format("66666666666677.087468726").toInt64() == 66666666666677ll);
        CHECK(format("66666666666677.33f").toInt64() == 66666666666677ll);
        CHECK(format("-666666666666.").toInt64() == -666666666666ll);
        CHECK(format("-666666666666.f").toInt64() == -666666666666ll);
        CHECK(format("-666666666666.111111111111111").toInt64() == -666666666666ll);
        CHECK(format("-9223372036854775808").toInt64() == std::numeric_limits<int64>::min()); // Min 64bit signed -9223372036854775808
        CHECK(format("-9223372036854775807").toInt64() == (std::numeric_limits<int64>::min() + 1));
        CHECK(format("-9223372036854775809").toInt64() == std::numeric_limits<int64>::min());
        CHECK(format("-9223372036854779999").toInt64() == std::numeric_limits<int64>::min());
        CHECK(format("-9223372036854775809").toInt64() == std::numeric_limits<int64>::min());
        CHECK(format("-18446744073709551614").toInt64() == std::numeric_limits<int64>::min());
        CHECK(format("-18446744073709551616").toInt64() == std::numeric_limits<int64>::min());
        CHECK(format("-18446744073456345654709551616").toInt64() == std::numeric_limits<int64>::min());
        CHECK(format("9223372036854775807").toInt64() == std::numeric_limits<int64>::max()); // Max 64bit signed 9223372036854775807
        CHECK(format("9223372036854775806").toInt64() == (std::numeric_limits<int64>::max() - 1));
        CHECK_FALSE(format("9223372036854775808").toInt64() == std::numeric_limits<int64>::max());
        CHECK_FALSE(format("18446744073709551615").toInt64() == std::numeric_limits<int64>::max());
        CHECK(format("18446744073709551615").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max())); // Max 64bit 18446744073709551615
        CHECK(format("18446744073709551614").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max() - 1));
        CHECK(format("18446744073709551616").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max()));
        CHECK(format("184467440737095516546734734716").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max()));
        CHECK(format(string(StringHelper::MAX_NUMBER_STRING_LENGTH, '5')).toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max()));
        CHECK(format(string(StringHelper::MAX_NUMBER_STRING_LENGTH + 1, '5')).toInt64() == 0);

        CHECK(is_float_equal(format("455.6573").toFloat(), 455.6573f));
        CHECK(is_float_equal(format("0xFFFF").toFloat(), static_cast<float>(0xFFFF)));
        CHECK(is_float_equal(format("-0xFFFF").toFloat(), static_cast<float>(-0xFFFF)));
        CHECK(is_float_equal(format("0xFFFF.44").toFloat(), static_cast<float>(0)));
        CHECK(is_float_equal(format("f").toFloat(), static_cast<float>(0)));
        CHECK(is_float_equal(format("{}", std::numeric_limits<float>::min()).toFloat(), std::numeric_limits<float>::min()));
        CHECK(is_float_equal(format("{}", std::numeric_limits<float>::max()).toFloat(), std::numeric_limits<float>::max()));
        CHECK(is_float_equal(format("34567774455.65745678555").toDouble(), 34567774455.65745678555));
        CHECK(is_float_equal(format("{}", std::numeric_limits<double>::min()).toDouble(), std::numeric_limits<double>::min()));
        CHECK(is_float_equal(format("{}", std::numeric_limits<double>::max()).toDouble(), std::numeric_limits<double>::max()));

        CHECK(format(" true ").toBool() == true);
        CHECK(format(" 1 ").toBool() == true);
        CHECK(format(" 211 ").toBool() == true);
        CHECK(format(" false ").toBool() == false);
        CHECK(format(" 0").toBool() == false);
        CHECK(format(" abc").toBool() == false);
    }

    SECTION("Split")
    {
        CHECK(format(" One Two    \tThree   ").split(' ') == vector<string>({"One", "Two", "Three"}));
        CHECK(format(" One Two    \tThree   ").split('\t') == vector<string>({"One Two", "Three"}));
        CHECK(format(" One Two    \tThree   ").split('X') == vector<string>({"One Two    \tThree"}));
        CHECK(format(" One Two  X \tThree   ").split('X') == vector<string>({"One Two", "Three"}));
        CHECK(format(" 111 222  33Three g66 7").splitToInt(' ') == vector<int>({111, 222, 0, 0, 7}));
        CHECK(format("").splitToInt(' ') == vector<int>({}));
        CHECK(format("             ").splitToInt(' ') == vector<int>({}));
        CHECK(format("1").splitToInt(' ') == vector<int>({1}));
        CHECK(format("1 -2").splitToInt(' ') == vector<int>({1, -2}));
        CHECK(format("\t1   X -2 X 3").splitToInt('X') == vector<int>({1, -2, 3}));
        CHECK(format("\t1 X\t\t  X X-2   X X 3\n").splitToInt('X') == vector<int>({1, -2, 3}));
    }

    SECTION("Substring")
    {
        CHECK(format("abcdpZ ppplZ ls").substringUntil('Z').str() == "abcdp");
        CHECK(format("abcdpZ ppplZ ls").substringUntil('X').str() == "abcdpZ ppplZ ls");
        CHECK(format("abcdpZ ppplZ ls").substringUntil("lZ").str() == "abcdpZ ppp");
        CHECK(format("abcdpZ ppplZ ls").substringUntil("XXX").str() == "abcdpZ ppplZ ls");
        CHECK(format("abcdpZ ppplZ ls").substringAfter('Z').str() == " ppplZ ls");
        CHECK(format("abcdpZ ppplZ ls").substringAfter('X').str().empty());
        CHECK(format("abcdpZ ppplZ ls").substringAfter("lZ").str() == " ls");
        CHECK(format("abcdpZ ppplZ ls").substringAfter("XXX").str().empty());
    }

    SECTION("Modify")
    {
        CHECK(format("aaaBBBcccDBEFGh Hello !").erase('B').str() == "aaacccDEFGh Hello !");
        CHECK(format("aaaBBBcccDBEFaGh HelBlo Ba!").erase('a', 'B').str() == "BBcccDBEFlo Ba!");
        CHECK(format("aaaBBBcccDBEFaGh HelBlo Ba!").erase('X', 'Y').str() == "aaaBBBcccDBEFaGh HelBlo Ba!");
        CHECK(format("aaaBBBcccDBEFaGh HelBlo Ba!").replace('a', 'X').str() == "XXXBBBcccDBEFXGh HelBlo BX!");
        CHECK(format("aaBDdDBaBBBDBcccDBEFaGh HelDBBlo Ba!").replace('D', 'B', 'X').str() == "aaBDdXaBBBXcccXEFaGh HelXBlo Ba!");
        CHECK(format("aaBDdDBaHelDBBlocccDBEFaGh HelDBBlo Ba!").replace("HelDBBlo", "X").str() == "aaBDdDBaXcccDBEFaGh X Ba!");
        CHECK(format("aaaBBBcccDBEFGh Hello !").lower().str() == "aaabbbcccdbefgh hello !");
        CHECK(format("aaaBBBcccDBEFGh Hello !").upper().str() == "AAABBBCCCDBEFGH HELLO !");
    }

    SECTION("Path")
    {
        CHECK(format("./cur").formatPath().str() == "cur");
        CHECK(format("./cur/next").formatPath().str() == "cur/next");
        CHECK(format("./cur/next/../last").formatPath().str() == "cur/last");
        CHECK(format("./cur/next/../../last/").formatPath().str() == "last/");
        CHECK(format("D:\\MyDir\\X\\..\\Y\\.\\.\\Z\\").formatPath().str() == "D:/MyDir/Y/Z/");
        CHECK(format("./cur/next/../../last/a/FILE1.ZIP").formatPath().extractDir().str() == "last/a");
        CHECK(format("./cur/next/../../last/FILE1.ZIP").formatPath().extractFileName().str() == "FILE1.ZIP");
        CHECK(format("./cur/next/../../last/a/FILE1.ZIP").formatPath().extractDir().str() == "last/a");
        CHECK(format("./cur/next/../../last/FILE1.ZIP").formatPath().changeFileName("NEWfile").str() == "last/NEWfile.zip");
        CHECK(format("./cur/next/../../last").formatPath().changeFileName("NEWfile").str() == "last/NEWfile");
        CHECK(format("./cur/next/../../last/FILE1.ZIP").formatPath().getFileExtension().str() == "zip");
        CHECK(format("./cur/next/../../last/FILE1.ZIP").formatPath().eraseFileExtension().str() == "last/FILE1");
        CHECK(format("cur/next/../../last/a/").combinePath("x/y/z").str() == "last/a/x/y/z");
        CHECK(format("../cur/next/../../last/a/").combinePath("x/y/z").str() == "../last/a/x/y/z");
        CHECK(format("../../cur/next/../../last/a/").combinePath("x/y/z").str() == "../../last/a/x/y/z");
        CHECK(format("D:\\MyDir\\X\\..\\Y\\.\\.\\Z\\").normalizePathSlashes().str() == "D:/MyDir/X/../Y/././Z/");
    }

    SECTION("LineEndings")
    {
        CHECK(format("Hello\r\nWorld\r\n").normalizeLineEndings().str() == "Hello\nWorld\n");
    }

#if FO_WINDOWS
    SECTION("WinParseWideChar")
    {
        CHECK(format().parseWideChar(L"Hello").str() == "Hello");
        CHECK(format("Hello").parseWideChar(L"World").str() == "HelloWorld");
        CHECK(format().parseWideChar(L"HelloМир").toWideChar() == L"HelloМир");
        CHECK(format("Мир").parseWideChar(L"Мир").toWideChar() == L"МирМир");
    }
#endif
}
