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
    SECTION("Length")
    {
        CHECK(_str().length() == 0);
        CHECK(_str(" hello ").length() == 7);
        CHECK(_str("").empty());
        CHECK_FALSE(_str("fff").empty());
    }

    SECTION("Parse")
    {
        CHECK(_str("  {} World {}", "Hello", "!").str() == "  Hello World !");
        CHECK(_str("{}{}{}", 1, 2, 3).str() == "123");
    }

    SECTION("Trim")
    {
        CHECK(_str().str() == "");
        CHECK(_str("Hello").str() == "Hello");
        CHECK(_str("Hello   ").trim().str() == "Hello");
        CHECK(_str("  Hello").trim().str() == "Hello");
        CHECK(_str("\t\nHel lo\t \r\t").trim().str() == "Hel lo");
    }

    SECTION("GetResult")
    {
        CHECK(static_cast<string_view>(_str(" Hello   ").trim()) == "Hello");
        CHECK(static_cast<string>(_str(" \tHello   ").trim()) == "Hello");
        CHECK(_str("Hello   ").trim().str() == "Hello");
        CHECK(string_view("Hello") == _str("  Hello").trim().c_str());
        CHECK(_str("\t\nHel lo\t \r\t").trim().str() == "Hel lo");
    }

    SECTION("Compare")
    {
        CHECK(_str(" Hello   ").str() == _str(" Hello   ").str());
        CHECK_FALSE(_str(" Hello1   ").str() == _str(" Hello2   ").str());
        CHECK(_str(" Hello   ").compareIgnoreCase(" heLLo   "));
        CHECK_FALSE(_str(" Hello   ").compareIgnoreCase(" hhhhh   "));
        CHECK_FALSE(_str(" Hello   ").compareIgnoreCase("xxx"));
    }

    SECTION("Utf8")
    {
        CHECK(_str(" приВЕт   ").isValidUtf8());
        CHECK(_str(" приВЕт   ").isValidUtf8());
        CHECK(_str(" Привет   ").compareIgnoreCaseUtf8(" приВЕт   "));
        CHECK_FALSE(_str(" Привет   ").compareIgnoreCaseUtf8(" тттввв   "));
        CHECK_FALSE(_str(" Привет   ").compareIgnoreCaseUtf8("еее"));
        CHECK(_str(" Привет   ").lengthUtf8() == 10);
        CHECK(_str("еее").lengthUtf8() == 3);
        CHECK(_str(" Привет   ").compareIgnoreCaseUtf8(" ПРИВЕТ   "));
        CHECK_FALSE(_str(" Привет   ").compareIgnoreCaseUtf8(" ПРЕТТТ   "));
        CHECK(_str(" ПриВЕТ   ").lowerUtf8() == " привет   ");
        CHECK(_str(" ПриВет   ").upperUtf8() == " ПРИВЕТ   ");
    }

    SECTION("StatsWith")
    {
        CHECK(_str(" Hello   ").startsWith(" Hell"));
        CHECK(_str("xHello   ").startsWith('x'));
        CHECK(_str(" Hello1   ").endsWith("1   "));
        CHECK(_str(" Hello1  x").endsWith('x'));
    }

    SECTION("Numbers")
    {
        CHECK_FALSE(_str("").isNumber());
        CHECK_FALSE(_str("   ").isNumber());
        CHECK_FALSE(_str("\t\n\r").isNumber());
        CHECK_FALSE(_str("0x").isNumber());
        CHECK_FALSE(_str("-0x").isNumber());
        CHECK(_str("0x1").isNumber());
        CHECK(_str("123").isNumber());
        CHECK(_str("0x123").isNumber());
        CHECK(_str(" 123").isNumber());
        CHECK(_str("123 \r\n\t").isNumber());
        CHECK_FALSE(_str("123llll").isNumber());
        CHECK_FALSE(_str("123+=").isNumber());
        CHECK(_str("1.0").isNumber());
        CHECK_FALSE(_str("x123").isNumber());
        CHECK(_str("\t0123").isNumber());
        CHECK(_str(" 0x123").isNumber());
        CHECK(_str("1.0f").isNumber());
        CHECK(_str("12.0").isNumber());
        CHECK(_str("12").isNumber());
        CHECK(_str("12.0f").isNumber());
        CHECK(_str("1.0f").isNumber());

        CHECK(_str("TRUE").isExplicitBool());
        CHECK(_str("False").isExplicitBool());
        CHECK_FALSE(_str("1").isExplicitBool());
        CHECK_FALSE(_str("").isExplicitBool());

        CHECK(_str("").toInt() == 0);
        CHECK(_str("00000000012").toInt() == 12);
        CHECK(_str("1").toInt() == 1);
        CHECK(_str("-156").toInt() == -156);
        CHECK(_str("-429496729500").toInt() == std::numeric_limits<int>::min());
        CHECK(_str("4294967295").toInt() == std::numeric_limits<int>::max());
        CHECK(_str("0xFFFF").toInt() == 0xFFFF);
        CHECK(_str("-0xFFFF").toInt() == -0xFFFF);
        CHECK(_str("012345").toInt() == 12345); // Do not recognize octal numbers
        CHECK(_str("-012345").toInt() == -12345);
        CHECK(_str("0xFFFF.88").toInt() == 0);
        CHECK(_str("12.9").toInt() == 12);
        CHECK(_str("12.1111").toInt() == 12);

        CHECK(_str("111").toUInt() == 111);
        CHECK(_str("-100").toUInt() == std::numeric_limits<uint>::min());
        CHECK(_str("1000000000000").toUInt() == std::numeric_limits<uint>::max());
        CHECK(_str("4294967295").toUInt() == 4294967295);

        CHECK(_str("66666666666677").toInt64() == 66666666666677ll);
        CHECK(_str("66666666666677.087468726").toInt64() == 66666666666677ll);
        CHECK(_str("66666666666677.33f").toInt64() == 66666666666677ll);
        CHECK(_str("-666666666666.").toInt64() == -666666666666ll);
        CHECK(_str("-666666666666.f").toInt64() == -666666666666ll);
        CHECK(_str("-666666666666.111111111111111").toInt64() == -666666666666ll);
        CHECK(_str("-9223372036854775808").toInt64() == std::numeric_limits<int64>::min()); // Min 64bit signed -9223372036854775808
        CHECK(_str("-9223372036854775807").toInt64() == (std::numeric_limits<int64>::min() + 1));
        CHECK(_str("-9223372036854775809").toInt64() == std::numeric_limits<int64>::min());
        CHECK(_str("-9223372036854779999").toInt64() == std::numeric_limits<int64>::min());
        CHECK(_str("-9223372036854775809").toInt64() == std::numeric_limits<int64>::min());
        CHECK(_str("-18446744073709551614").toInt64() == std::numeric_limits<int64>::min());
        CHECK(_str("-18446744073709551616").toInt64() == std::numeric_limits<int64>::min());
        CHECK(_str("-18446744073456345654709551616").toInt64() == std::numeric_limits<int64>::min());
        CHECK(_str("9223372036854775807").toInt64() == std::numeric_limits<int64>::max()); // Max 64bit signed 9223372036854775807
        CHECK(_str("9223372036854775806").toInt64() == (std::numeric_limits<int64>::max() - 1));
        CHECK_FALSE(_str("9223372036854775808").toInt64() == std::numeric_limits<int64>::max());
        CHECK_FALSE(_str("18446744073709551615").toInt64() == std::numeric_limits<int64>::max());
        CHECK(_str("18446744073709551615").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max())); // Max 64bit 18446744073709551615
        CHECK(_str("18446744073709551614").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max() - 1));
        CHECK(_str("18446744073709551616").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max()));
        CHECK(_str("184467440737095516546734734716").toInt64() == static_cast<int64>(std::numeric_limits<uint64>::max()));

        CHECK(is_float_equal(_str("455.6573").toFloat(), 455.6573f));
        CHECK(is_float_equal(_str("0xFFFF").toFloat(), static_cast<float>(0xFFFF)));
        CHECK(is_float_equal(_str("-0xFFFF").toFloat(), static_cast<float>(-0xFFFF)));
        CHECK(is_float_equal(_str("0xFFFF.44").toFloat(), static_cast<float>(0)));
        CHECK(is_float_equal(_str("{}", std::numeric_limits<float>::min()).toFloat(), std::numeric_limits<float>::min()));
        CHECK(is_float_equal(_str("{}", std::numeric_limits<float>::max()).toFloat(), std::numeric_limits<float>::max()));
        CHECK(is_float_equal(_str("34567774455.65745678555").toDouble(), 34567774455.65745678555));
        CHECK(is_float_equal(_str("{}", std::numeric_limits<double>::min()).toDouble(), std::numeric_limits<double>::min()));
        CHECK(is_float_equal(_str("{}", std::numeric_limits<double>::max()).toDouble(), std::numeric_limits<double>::max()));
    }

    SECTION("Split")
    {
        CHECK(_str(" One Two    \tThree   ").split(' ') == vector<string>({"One", "Two", "Three"}));
        CHECK(_str(" One Two    \tThree   ").split('\t') == vector<string>({"One Two", "Three"}));
        CHECK(_str(" One Two    \tThree   ").split('X') == vector<string>({"One Two    \tThree"}));
        CHECK(_str(" 111 222  33Three g66 7").splitToInt(' ') == vector<int>({111, 222, 0, 0, 7}));
    }

    SECTION("Substring")
    {
        CHECK(_str("abcdpZ ppplZ ls").substringUntil('Z').str() == "abcdp");
        CHECK(_str("abcdpZ ppplZ ls").substringUntil("lZ").str() == "abcdpZ ppp");
        CHECK(_str("abcdpZ ppplZ ls").substringAfter('Z').str() == " ppplZ ls");
        CHECK(_str("abcdpZ ppplZ ls").substringAfter("lZ").str() == " ls");
    }

    SECTION("Modify")
    {
        CHECK(_str("aaaBBBcccDBEFGh Hello !").erase('B').str() == "aaacccDEFGh Hello !");
        CHECK(_str("aaaBBBcccDBEFaGh HelBlo Ba!").erase('a', 'B').str() == "BBcccDBEFlo Ba!");
        CHECK(_str("aaaBBBcccDBEFaGh HelBlo Ba!").replace('a', 'X').str() == "XXXBBBcccDBEFXGh HelBlo BX!");
        CHECK(_str("aaBDdDBaBBBDBcccDBEFaGh HelDBBlo Ba!").replace('D', 'B', 'X').str() == "aaBDdXaBBBXcccXEFaGh HelXBlo Ba!");
        CHECK(_str("aaBDdDBaHelDBBlocccDBEFaGh HelDBBlo Ba!").replace("HelDBBlo", "X").str() == "aaBDdDBaXcccDBEFaGh X Ba!");
        CHECK(_str("aaaBBBcccDBEFGh Hello !").lower().str() == "aaabbbcccdbefgh hello !");
        CHECK(_str("aaaBBBcccDBEFGh Hello !").upper().str() == "AAABBBCCCDBEFGH HELLO !");
    }

    SECTION("Path")
    {
        CHECK(_str("./cur").formatPath().str() == "cur");
        CHECK(_str("./cur/next").formatPath().str() == "cur/next");
        CHECK(_str("./cur/next/../last").formatPath().str() == "cur/last");
        CHECK(_str("./cur/next/../../last/").formatPath().str() == "last/");
        CHECK(_str("D:\\MyDir\\X\\..\\Y\\.\\.\\Z\\").formatPath().str() == "D:/MyDir/Y/Z/");
        CHECK(_str("./cur/next/../../last/a/FILE1.ZIP").formatPath().extractDir().str() == "last/a");
        CHECK(_str("./cur/next/../../last/FILE1.ZIP").formatPath().extractFileName().str() == "FILE1.ZIP");
        CHECK(_str("./cur/next/../../last/a/FILE1.ZIP").formatPath().extractDir().str() == "last/a");
        CHECK(_str("./cur/next/../../last/FILE1.ZIP").formatPath().changeFileName("NEWfile").str() == "last/NEWfile.zip");
        CHECK(_str("./cur/next/../../last/FILE1.ZIP").formatPath().getFileExtension().str() == "zip");
        CHECK(_str("./cur/next/../../last/FILE1.ZIP").formatPath().eraseFileExtension().str() == "last/FILE1");
        CHECK(_str("./cur/next/../../last/a/").combinePath("x/y/z").str() == "last/a/x/y/z");
        CHECK(_str("D:\\MyDir\\X\\..\\Y\\.\\.\\Z\\").normalizePathSlashes().str() == "D:/MyDir/X/../Y/././Z/");
    }

    SECTION("LineEndings")
    {
        CHECK(_str("Hello\r\nWorld\r\n").normalizeLineEndings().str() == "Hello\nWorld\n");
    }

#if FO_WINDOWS
    SECTION("WinParseWideChar")
    {
        CHECK(_str().parseWideChar(L"Hello").str() == "Hello");
        CHECK(_str("Hello").parseWideChar(L"World").str() == "HelloWorld");
        CHECK(_str().parseWideChar(L"HelloМир").toWideChar() == L"HelloМир");
        CHECK(_str("Мир").parseWideChar(L"Мир").toWideChar() == L"МирМир");
    }
#endif
}
