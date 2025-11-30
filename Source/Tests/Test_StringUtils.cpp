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

#include "Common.h"

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
        CHECK(strex("\t\nHel lo\t \r\t").trim("\t\r ").str() == "\nHel lo");
        CHECK(strex("\t\nHel lo\t \r\t").ltrim("\t\n").str() == "Hel lo\t \r\t");
        CHECK(strex("\t\nHel lo\t \r\t").rtrim("\t\n").str() == "\t\nHel lo\t \r");
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
        CHECK(strex(" Hello   ").compare_ignore_case(" heLLo   "));
        CHECK_FALSE(strex(" Hello   ").compare_ignore_case(" hhhhh   "));
        CHECK_FALSE(strex(" Hello   ").compare_ignore_case("xxx"));
    }

    SECTION("Utf8")
    {
        CHECK(strex("").is_valid_utf8());
        CHECK_FALSE(strex(string(1, static_cast<char>(200))).is_valid_utf8());
        CHECK(strex(" приВЕт   ").is_valid_utf8());
        CHECK(strex(" приВЕт   ").is_valid_utf8());
        CHECK(strex(" Привет   ").compare_ignore_case_utf8(" приВЕт   "));
        CHECK_FALSE(strex(" Привет   ").compare_ignore_case_utf8(" тттввв   "));
        CHECK_FALSE(strex(" Привет   ").compare_ignore_case_utf8("еее"));
        CHECK(strex(" Привет   ").length_utf8() == 10);
        CHECK(strex("еее").length_utf8() == 3);
        CHECK(strex(" Привет   ").compare_ignore_case_utf8(" ПРИВЕТ   "));
        CHECK_FALSE(strex(" Привет   ").compare_ignore_case_utf8(" ПРЕТТТ   "));
        CHECK(strex(" ПриВЕТ   ").lower_utf8() == " привет   ");
        CHECK(strex(" ПриВет   ").upper_utf8() == " ПРИВЕТ   ");
    }

    SECTION("StartsWith")
    {
        CHECK(strex(" Hello   ").starts_with(" Hell"));
        CHECK(strex("xHello   ").starts_with('x'));
        CHECK(strex(" Hello1   ").ends_with("1   "));
        CHECK(strex(" Hello1  x").ends_with('x'));
    }

    SECTION("Numbers")
    {
        CHECK_FALSE(strex("").is_number());
        CHECK_FALSE(strex("   ").is_number());
        CHECK_FALSE(strex("\t\n\r").is_number());
        CHECK_FALSE(strex("0x").is_number());
        CHECK_FALSE(strex("-0x").is_number());
        CHECK(strex("-0x5").is_number());
        CHECK(strex("-0xa").is_number());
        CHECK_FALSE(strex("--1").is_number());
        CHECK_FALSE(strex("-0xy").is_number());
        CHECK_FALSE(strex("0XZ").is_number());
        CHECK(strex("0x1").is_number());
        CHECK(strex("123").is_number());
        CHECK(strex("0x123").is_number());
        CHECK(strex(" 123").is_number());
        CHECK(strex("123 \r\n\t").is_number());
        CHECK_FALSE(strex("123llll").is_number());
        CHECK_FALSE(strex("123+=").is_number());
        CHECK(strex("1.0").is_number());
        CHECK_FALSE(strex("x123").is_number());
        CHECK(strex("\t0123").is_number());
        CHECK(strex(" 0x123").is_number());
        CHECK(strex("1.0f").is_number());
        CHECK(strex("12.0").is_number());
        CHECK(strex("12").is_number());
        CHECK(strex("12.0f").is_number());
        CHECK(strex("1.0f").is_number());
        CHECK(strex(string(strex::MAX_NUMBER_STRING_LENGTH, '5')).is_number());
        CHECK_FALSE(strex(string(strex::MAX_NUMBER_STRING_LENGTH + 1, '5')).is_number());

        CHECK(strex("true").is_explicit_bool());
        CHECK(strex("TRUE ").is_explicit_bool());
        CHECK(strex("False  ").is_explicit_bool());
        CHECK(strex(" \t  False\t").is_explicit_bool());
        CHECK_FALSE(strex("1").is_explicit_bool());
        CHECK_FALSE(strex("").is_explicit_bool());

        CHECK(strex("").to_int32() == 0);
        CHECK(strex("00000000012").to_int32() == 12);
        CHECK(strex("1").to_int32() == 1);
        CHECK(strex("-156").to_int32() == -156);
        CHECK(strex("-429496729500").to_int32() == std::numeric_limits<int32>::min());
        CHECK(strex("4294967295").to_int32() == std::numeric_limits<int32>::max());
        CHECK(strex("0xFFFF").to_int32() == 0xFFFF);
        CHECK(strex("-0xFFFF").to_int32() == -0xFFFF);
        CHECK(strex("012345").to_int32() == 12345); // Do not recognize octal numbers
        CHECK(strex("-012345").to_int32() == -12345);
        CHECK(strex("0xFFFF.88").to_int32() == 0);
        CHECK(strex("12.9").to_int32() == 12);
        CHECK(strex("12.1111").to_int32() == 12);

        CHECK(strex("111").to_uint32() == 111);
        CHECK(strex("-100").to_uint32() == std::numeric_limits<uint32>::min());
        CHECK(strex("1000000000000").to_uint32() == std::numeric_limits<uint32>::max());
        CHECK(strex("4294967295").to_uint32() == 4294967295);

        CHECK(strex("66666666666677").to_int64() == 66666666666677ll);
        CHECK(strex("66666666666677.087468726").to_int64() == 66666666666677ll);
        CHECK(strex("66666666666677.33f").to_int64() == 66666666666677ll);
        CHECK(strex("-666666666666.").to_int64() == -666666666666ll);
        CHECK(strex("-666666666666.f").to_int64() == -666666666666ll);
        CHECK(strex("-666666666666.111111111111111").to_int64() == -666666666666ll);
        CHECK(strex("-9223372036854775808").to_int64() == std::numeric_limits<int64>::min()); // Min 64bit signed -9223372036854775808
        CHECK(strex("-9223372036854775807").to_int64() == (std::numeric_limits<int64>::min() + 1));
        CHECK(strex("-9223372036854775809").to_int64() == std::numeric_limits<int64>::min());
        CHECK(strex("-9223372036854779999").to_int64() == std::numeric_limits<int64>::min());
        CHECK(strex("-9223372036854775809").to_int64() == std::numeric_limits<int64>::min());
        CHECK(strex("-18446744073709551614").to_int64() == std::numeric_limits<int64>::min());
        CHECK(strex("-18446744073709551616").to_int64() == std::numeric_limits<int64>::min());
        CHECK(strex("-18446744073456345654709551616").to_int64() == std::numeric_limits<int64>::min());
        CHECK(strex("9223372036854775807").to_int64() == std::numeric_limits<int64>::max()); // Max 64bit signed 9223372036854775807
        CHECK(strex("9223372036854775806").to_int64() == (std::numeric_limits<int64>::max() - 1));
        CHECK_FALSE(strex("9223372036854775808").to_int64() == std::numeric_limits<int64>::max());
        CHECK_FALSE(strex("18446744073709551615").to_int64() == std::numeric_limits<int64>::max());
        CHECK(strex("18446744073709551615").to_int64() == std::bit_cast<int64>(std::numeric_limits<uint64>::max())); // Max 64bit 18446744073709551615
        CHECK(strex("18446744073709551614").to_int64() == std::bit_cast<int64>(std::numeric_limits<uint64>::max() - 1));
        CHECK(strex("18446744073709551616").to_int64() == std::bit_cast<int64>(std::numeric_limits<uint64>::max()));
        CHECK(strex("184467440737095516546734734716").to_int64() == std::bit_cast<int64>(std::numeric_limits<uint64>::max()));
        CHECK(strex(string(strex::MAX_NUMBER_STRING_LENGTH, '5')).to_int64() == std::bit_cast<int64>(std::numeric_limits<uint64>::max()));
        CHECK(strex(string(strex::MAX_NUMBER_STRING_LENGTH + 1, '5')).to_int64() == 0);

        CHECK(is_float_equal(strex("455.6573").to_float32(), 455.6573f));
        CHECK(is_float_equal(strex("0xFFFF").to_float32(), numeric_cast<float32>(0xFFFF)));
        CHECK(is_float_equal(strex("-0xFFFF").to_float32(), numeric_cast<float32>(-0xFFFF)));
        CHECK(is_float_equal(strex("0xFFFF.44").to_float32(), numeric_cast<float32>(0)));
        CHECK(is_float_equal(strex("f").to_float32(), numeric_cast<float32>(0)));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<float32>::min()).to_float32(), std::numeric_limits<float32>::min()));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<float32>::max()).to_float32(), std::numeric_limits<float32>::max()));
        CHECK(is_float_equal(strex("34567774455.65745678555").to_float64(), 34567774455.65745678555));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<float64>::min()).to_float64(), std::numeric_limits<float64>::min()));
        CHECK(is_float_equal(strex("{}", std::numeric_limits<float64>::max()).to_float64(), std::numeric_limits<float64>::max()));

        CHECK(strex(" true ").to_bool() == true);
        CHECK(strex(" 1 ").to_bool() == true);
        CHECK(strex(" 211 ").to_bool() == true);
        CHECK(strex(" false ").to_bool() == false);
        CHECK(strex(" 0").to_bool() == false);
        CHECK(strex(" abc").to_bool() == false);
    }

    SECTION("Split")
    {
        CHECK(strex(" One Two    \tThree   ").split(' ') == vector<string>({"One", "Two", "Three"}));
        CHECK(strex(" One Two    \tThree   ").split('\t') == vector<string>({"One Two", "Three"}));
        CHECK(strex(" One Two    \tThree   ").split('X') == vector<string>({"One Two    \tThree"}));
        CHECK(strex(" One Two  X \tThree   ").split('X') == vector<string>({"One Two", "Three"}));
        CHECK(strex(",One,Two").split(',') == vector<string>({"One", "Two"}));
        CHECK(strex("One,Two,").split(',') == vector<string>({"One", "Two"}));
        CHECK(strex(" 111 222  33Three g66 7").split_to_int32(' ') == vector<int32>({111, 222, 0, 0, 7}));
        CHECK(strex("").split_to_int32(' ') == vector<int32>({}));
        CHECK(strex("             ").split_to_int32(' ') == vector<int32>({}));
        CHECK(strex("1").split_to_int32(' ') == vector<int32>({1}));
        CHECK(strex("1 -2").split_to_int32(' ') == vector<int32>({1, -2}));
        CHECK(strex("\t1   X -2 X 3").split_to_int32('X') == vector<int32>({1, -2, 3}));
        CHECK(strex("\t1 X\t\t  X X-2   X X 3\n").split_to_int32('X') == vector<int32>({1, -2, 3}));
    }

    SECTION("Substring")
    {
        CHECK(strex("abcdpZ ppplZ ls").substring_until('Z').str() == "abcdp");
        CHECK(strex("abcdpZ ppplZ ls").substring_until('X').str() == "abcdpZ ppplZ ls");
        CHECK(strex("abcdpZ ppplZ ls").substring_until("lZ").str() == "abcdpZ ppp");
        CHECK(strex("abcdpZ ppplZ ls").substring_until("XXX").str() == "abcdpZ ppplZ ls");
        CHECK(strex("abcdpZ ppplZ ls").substring_after('Z').str() == " ppplZ ls");
        CHECK(strex("abcdpZ ppplZ ls").substring_after('X').str().empty());
        CHECK(strex("abcdpZ ppplZ ls").substring_after("lZ").str() == " ls");
        CHECK(strex("abcdpZ ppplZ ls").substring_after("XXX").str().empty());
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
        CHECK(strex("./cur").format_path().str() == "cur");
        CHECK(strex("./cur/next").format_path().str() == "cur/next");
        CHECK(strex("./cur/next/../last").format_path().str() == "cur/last");
        CHECK(strex("./cur/next/../../last/").format_path().str() == "last/");
        CHECK(strex("D:\\MyDir\\X\\..\\Y\\.\\.\\Z\\").format_path().str() == "D:/MyDir/Y/Z/");
        CHECK(strex("./cur/next/../../last/a/FILE1.ZIP").format_path().extract_dir().str() == "last/a");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").format_path().extract_file_name().str() == "FILE1.ZIP");
        CHECK(strex("./cur/next/../../last/a/FILE1.ZIP").format_path().extract_dir().str() == "last/a");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").format_path().change_file_name("NEWfile").str() == "last/NEWfile.zip");
        CHECK(strex("./cur/next/../../last").format_path().change_file_name("NEWfile").str() == "NEWfile");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").format_path().get_file_extension().str() == "zip");
        CHECK(strex("./cur/next/../../last/FILE1.ZIP").format_path().erase_file_extension().str() == "last/FILE1");
        CHECK(strex("cur/next/../../last/a/").combine_path("x/y/z").str() == "last/a/x/y/z");
        CHECK(strex("../cur/next/../../last/a/").combine_path("x/y/z").str() == "../last/a/x/y/z");
        CHECK(strex("../../cur/next/../../last/a/").combine_path("x/y/z").str() == "../../last/a/x/y/z");
        CHECK(strex("D:\\MyDir\\X\\..\\Y\\.\\.\\Z\\").normalize_path_slashes().str() == "D:/MyDir/X/../Y/././Z/");
    }

    SECTION("LineEndings")
    {
        CHECK(strex("Hello\r\nWorld\r\n").normalize_line_endings().str() == "Hello\nWorld\n");
    }

#if FO_WINDOWS
    SECTION("WinParseWideChar")
    {
        CHECK(strex().parse_wide_char(L"Hello").str() == "Hello");
        CHECK(strex("Hello").parse_wide_char(L"World").str() == "HelloWorld");
        CHECK(strex().parse_wide_char(L"HelloМир").to_wide_char() == L"HelloМир");
        CHECK(strex("Мир").parse_wide_char(L"Мир").to_wide_char() == L"МирМир");
    }
#endif
}

FO_END_NAMESPACE();
