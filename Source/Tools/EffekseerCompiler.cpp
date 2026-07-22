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

#include "EffekseerCompiler.h"

#if FO_EFFEKSEER_PARTICLES

FO_BEGIN_NAMESPACE

static constexpr int32_t EffekseerBinaryVersion = 1810;
static constexpr float32_t Pi = 3.14159265358979323846f;

struct XmlNode final
{
    string Name {};
    string Text {};
    vector<XmlNode> Children {};
};

class BinaryWriter final
{
public:
    void WriteUInt8(uint8_t value);
    void WriteUInt16(uint16_t value);
    void WriteInt32(int32_t value);
    void WriteFloat(float32_t value);
    void WriteBytes(const_span<uint8_t> value);
    void WriteSized(const BinaryWriter& value);
    void WriteUtf16(string_view value);
    [[nodiscard]] auto MoveData() -> vector<uint8_t>;
    [[nodiscard]] auto GetData() const noexcept -> const vector<uint8_t>&;

private:
    vector<uint8_t> _data {};
};

struct CompilerContext final
{
    string ProjectDirectory {};
    vector<nptr<const XmlNode>> ExportedNodes {};
    map<nptr<const XmlNode>, int32_t> RenderIndices {};
    map<string, int32_t> ColorTextures {};
    map<string, int32_t> NormalTextures {};
    map<string, int32_t> DistortionTextures {};
    map<string, int32_t> Waves {};
    map<string, int32_t> Models {};
    map<string, int32_t> Curves {};
    set<string> Dependencies {};
};

[[nodiscard]] static auto ParseXmlProject(string_view project_path, string_view xml) -> XmlNode;
[[nodiscard]] static auto CreateCompilerContext(string_view project_path, const XmlNode& project) -> CompilerContext;
[[nodiscard]] static auto CompileProject(string_view project_path, const XmlNode& project) -> EffekseerCompilerOutput;

auto CompileEffekseerProject(string_view project_path, const_span<uint8_t> project_data) -> EffekseerCompilerOutput
{
    FO_STACK_TRACE_ENTRY();

    if (project_data.empty()) {
        throw EffekseerCompilerException("Effekseer project is empty", project_path);
    }

    const string_view xml {ptr<const uint8_t> {project_data.data()}.reinterpret_as<const char>().get(), project_data.size()};
    const XmlNode project = ParseXmlProject(project_path, xml);
    return CompileProject(project_path, project);
}

auto GetEffekseerProjectDependencies(string_view project_path, const_span<uint8_t> project_data) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    if (project_data.empty()) {
        throw EffekseerCompilerException("Effekseer project is empty", project_path);
    }

    const string_view xml {ptr<const uint8_t> {project_data.data()}.reinterpret_as<const char>().get(), project_data.size()};
    const XmlNode project = ParseXmlProject(project_path, xml);
    CompilerContext context = CreateCompilerContext(project_path, project);
    return vector<string> {context.Dependencies.begin(), context.Dependencies.end()};
}

[[nodiscard]] static auto IsXmlSpace(char ch) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

[[nodiscard]] static auto TrimXmlText(string_view value) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    size_t begin = 0;
    size_t end = value.size();

    while (begin < end && IsXmlSpace(value[begin])) {
        ++begin;
    }
    while (end > begin && IsXmlSpace(value[end - 1])) {
        --end;
    }

    return string {value.substr(begin, end - begin)};
}

[[nodiscard]] static auto DecodeXmlText(string_view value, string_view project_path) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;
    result.reserve(value.size());

    for (size_t pos = 0; pos < value.size();) {
        if (value[pos] != '&') {
            result.push_back(value[pos++]);
            continue;
        }

        const size_t end = value.find(';', pos + 1);

        if (end == string_view::npos) {
            throw EffekseerCompilerException("Effekseer project contains an unterminated XML entity", project_path);
        }

        const string_view entity = value.substr(pos + 1, end - pos - 1);

        if (entity == "amp") {
            result.push_back('&');
        }
        else if (entity == "lt") {
            result.push_back('<');
        }
        else if (entity == "gt") {
            result.push_back('>');
        }
        else if (entity == "quot") {
            result.push_back('"');
        }
        else if (entity == "apos") {
            result.push_back('\'');
        }
        else {
            throw EffekseerCompilerException("Effekseer project contains an unsupported XML entity", project_path, entity);
        }

        pos = end + 1;
    }

    return result;
}

class XmlParser final
{
public:
    XmlParser(string_view project_path, string_view xml) :
        _projectPath(project_path),
        _xml(xml)
    {
        FO_NO_STACK_TRACE_ENTRY();
    }

    [[nodiscard]] auto Parse() -> XmlNode
    {
        FO_STACK_TRACE_ENTRY();

        if (_xml.starts_with("\xEF\xBB\xBF")) {
            _position = 3;
        }

        SkipSpace();

        if (StartsWith("<?xml")) {
            const size_t declaration_end = _xml.find("?>", _position + 5);

            if (declaration_end == string_view::npos) {
                Fail("Effekseer project has an unterminated XML declaration");
            }

            _position = declaration_end + 2;
        }

        SkipSpace();
        XmlNode root = ParseElement();
        SkipSpace();

        if (_position != _xml.size()) {
            Fail("Effekseer project contains trailing XML data");
        }
        if (root.Name != "EffekseerProject") {
            throw EffekseerCompilerException("Effekseer source is not an EffekseerProject XML document", _projectPath, root.Name);
        }

        return root;
    }

private:
    [[noreturn]] void Fail(string_view message) const
    {
        FO_STACK_TRACE_ENTRY();

        throw EffekseerCompilerException(message, _projectPath, _position);
    }

    void SkipSpace()
    {
        FO_NO_STACK_TRACE_ENTRY();

        while (_position < _xml.size() && IsXmlSpace(_xml[_position])) {
            ++_position;
        }
    }

    [[nodiscard]] auto StartsWith(string_view value) const noexcept -> bool
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _xml.substr(_position).starts_with(value);
    }

    [[nodiscard]] auto ParseName() -> string
    {
        FO_STACK_TRACE_ENTRY();

        const size_t begin = _position;

        while (_position < _xml.size()) {
            const char ch = _xml[_position];
            const bool valid = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';

            if (!valid) {
                break;
            }

            ++_position;
        }

        if (_position == begin) {
            Fail("Effekseer project contains an invalid XML element name");
        }

        return string {_xml.substr(begin, _position - begin)};
    }

    [[nodiscard]] auto ParseElement() -> XmlNode
    {
        FO_STACK_TRACE_ENTRY();

        if (_position >= _xml.size() || _xml[_position] != '<' || StartsWith("</") || StartsWith("<!") || StartsWith("<?")) {
            Fail("Effekseer project contains invalid XML markup");
        }

        ++_position;

        XmlNode node;
        node.Name = ParseName();
        SkipSpace();

        if (StartsWith("/>")) {
            _position += 2;
            return node;
        }
        if (_position >= _xml.size() || _xml[_position] != '>') {
            Fail("Effekseer project XML attributes are unsupported");
        }

        ++_position;

        string text;

        while (true) {
            if (_position >= _xml.size()) {
                Fail("Effekseer project has an unterminated XML element");
            }
            if (StartsWith("</")) {
                _position += 2;
                const string closing_name = ParseName();
                SkipSpace();

                if (_position >= _xml.size() || _xml[_position] != '>') {
                    Fail("Effekseer project has an invalid XML closing element");
                }

                ++_position;

                if (closing_name != node.Name) {
                    throw EffekseerCompilerException("Effekseer project XML element mismatch", _projectPath, node.Name, closing_name);
                }

                node.Text = DecodeXmlText(TrimXmlText(text), _projectPath);
                return node;
            }
            if (_xml[_position] == '<') {
                if (!TrimXmlText(text).empty()) {
                    Fail("Effekseer project XML elements cannot mix text and children");
                }

                text.clear();
                node.Children.emplace_back(ParseElement());
                continue;
            }

            text.push_back(_xml[_position++]);
        }
    }

    string _projectPath;
    string_view _xml;
    size_t _position {};
};

[[nodiscard]] static auto ParseXmlProject(string_view project_path, string_view xml) -> XmlNode
{
    FO_STACK_TRACE_ENTRY();

    XmlParser parser {project_path, xml};
    return parser.Parse();
}

[[nodiscard]] static auto Child(nptr<const XmlNode> node, string_view name) noexcept -> nptr<const XmlNode>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!node) {
        return nullptr;
    }

    const auto it = std::ranges::find(node->Children, name, &XmlNode::Name);
    return it != node->Children.end() ? &*it : nullptr;
}

[[nodiscard]] static auto Find(nptr<const XmlNode> node, string_view path) noexcept -> nptr<const XmlNode>
{
    FO_NO_STACK_TRACE_ENTRY();

    while (node && !path.empty()) {
        const size_t separator = path.find('/');
        const string_view name = path.substr(0, separator);
        node = Child(node, name);

        if (separator == string_view::npos) {
            break;
        }

        path.remove_prefix(separator + 1);
    }

    return node;
}

[[nodiscard]] static auto Text(nptr<const XmlNode> node, string_view path, string_view default_value = {}) -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> value = Find(node, path);
    return value ? string_view {value->Text} : default_value;
}

[[nodiscard]] static auto IntValue(nptr<const XmlNode> node, string_view path, int32_t default_value = 0) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> value = Find(node, path);

    if (!value) {
        return default_value;
    }

    int32_t result {};
    const char* begin = value->Text.data();
    const char* end = begin + value->Text.size();
    const auto [parse_end, error] = std::from_chars(begin, end, result);

    if (error != std::errc {} || parse_end != end) {
        throw EffekseerCompilerException("Effekseer project contains an invalid integer", path, value->Text);
    }

    return result;
}

[[nodiscard]] static auto FloatValue(nptr<const XmlNode> node, string_view path, float32_t default_value = 0.0f) -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> value = Find(node, path);

    if (!value) {
        return default_value;
    }

    float32_t result {};
    const char* begin = value->Text.data();
    const char* end = begin + value->Text.size();
    const auto [parse_end, error] = std::from_chars(begin, end, result, std::chars_format::general);

    if (error != std::errc {} || parse_end != end || !std::isfinite(result)) {
        throw EffekseerCompilerException("Effekseer project contains an invalid float", path, value->Text);
    }

    return result;
}

[[nodiscard]] static auto BoolValue(nptr<const XmlNode> node, string_view path, bool default_value = false) -> bool
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> value = Find(node, path);

    if (!value) {
        return default_value;
    }
    if (value->Text == "True" || value->Text == "true") {
        return true;
    }
    if (value->Text == "False" || value->Text == "false") {
        return false;
    }

    throw EffekseerCompilerException("Effekseer project contains an invalid boolean", path, value->Text);
}

void BinaryWriter::WriteUInt8(uint8_t value)
{
    FO_NO_STACK_TRACE_ENTRY();

    _data.emplace_back(value);
}

void BinaryWriter::WriteUInt16(uint16_t value)
{
    FO_NO_STACK_TRACE_ENTRY();

    _data.emplace_back(numeric_cast<uint8_t>(value & 0xffU));
    _data.emplace_back(numeric_cast<uint8_t>((value >> 8U) & 0xffU));
}

void BinaryWriter::WriteInt32(int32_t value)
{
    FO_NO_STACK_TRACE_ENTRY();

    const uint32_t encoded = std::bit_cast<uint32_t>(value);
    _data.emplace_back(numeric_cast<uint8_t>(encoded & 0xffU));
    _data.emplace_back(numeric_cast<uint8_t>((encoded >> 8U) & 0xffU));
    _data.emplace_back(numeric_cast<uint8_t>((encoded >> 16U) & 0xffU));
    _data.emplace_back(numeric_cast<uint8_t>((encoded >> 24U) & 0xffU));
}

void BinaryWriter::WriteFloat(float32_t value)
{
    FO_NO_STACK_TRACE_ENTRY();

    WriteInt32(std::bit_cast<int32_t>(value));
}

void BinaryWriter::WriteBytes(const_span<uint8_t> value)
{
    FO_NO_STACK_TRACE_ENTRY();

    _data.insert(_data.end(), value.begin(), value.end());
}

void BinaryWriter::WriteSized(const BinaryWriter& value)
{
    FO_NO_STACK_TRACE_ENTRY();

    WriteInt32(numeric_cast<int32_t>(value.GetData().size()));
    WriteBytes(value.GetData());
}

void BinaryWriter::WriteUtf16(string_view value)
{
    FO_STACK_TRACE_ENTRY();

    const wstring wide = strex(value).to_wide_char();
    WriteInt32(numeric_cast<int32_t>(wide.size() + 1));

    for (const wchar_t ch : wide) {
        const uint32_t codepoint = numeric_cast<uint32_t>(ch);

        if (codepoint > 0xffffU) {
            throw EffekseerCompilerException("Effekseer dependency path contains a non-BMP character", value);
        }

        WriteUInt16(numeric_cast<uint16_t>(codepoint));
    }

    WriteUInt16(0);
}

auto BinaryWriter::MoveData() -> vector<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::move(_data);
}

auto BinaryWriter::GetData() const noexcept -> const vector<uint8_t>&
{
    FO_NO_STACK_TRACE_ENTRY();

    return _data;
}

static void WriteVector2(BinaryWriter& writer, nptr<const XmlNode> node, float32_t default_x = 0.0f, float32_t default_y = 0.0f, float32_t multiplier = 1.0f)
{
    FO_NO_STACK_TRACE_ENTRY();

    writer.WriteFloat(FloatValue(node, "X", default_x) * multiplier);
    writer.WriteFloat(FloatValue(node, "Y", default_y) * multiplier);
}

static void WriteVector3(BinaryWriter& writer, nptr<const XmlNode> node, float32_t default_x = 0.0f, float32_t default_y = 0.0f, float32_t default_z = 0.0f, float32_t multiplier = 1.0f)
{
    FO_NO_STACK_TRACE_ENTRY();

    writer.WriteFloat(FloatValue(node, "X", default_x) * multiplier);
    writer.WriteFloat(FloatValue(node, "Y", default_y) * multiplier);
    writer.WriteFloat(FloatValue(node, "Z", default_z) * multiplier);
}

static void WriteRandomFloat(BinaryWriter& writer, nptr<const XmlNode> node, float32_t default_value = 0.0f, float32_t multiplier = 1.0f)
{
    FO_NO_STACK_TRACE_ENTRY();

    writer.WriteFloat(FloatValue(node, "Max", default_value) * multiplier);
    writer.WriteFloat(FloatValue(node, "Min", default_value) * multiplier);
}

static void WriteRandomInt(BinaryWriter& writer, nptr<const XmlNode> node, int32_t default_value = 0)
{
    FO_NO_STACK_TRACE_ENTRY();

    writer.WriteInt32(IntValue(node, "Max", default_value));
    writer.WriteInt32(IntValue(node, "Min", default_value));
}

static void WriteRandomVector2(BinaryWriter& writer, nptr<const XmlNode> node, float32_t default_x = 0.0f, float32_t default_y = 0.0f, float32_t multiplier_x = 1.0f, float32_t multiplier_y = 1.0f)
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> x = Child(node, "X");
    nptr<const XmlNode> y = Child(node, "Y");
    writer.WriteFloat(FloatValue(x, "Max", default_x) * multiplier_x);
    writer.WriteFloat(FloatValue(y, "Max", default_y) * multiplier_y);
    writer.WriteFloat(FloatValue(x, "Min", default_x) * multiplier_x);
    writer.WriteFloat(FloatValue(y, "Min", default_y) * multiplier_y);
}

static void WriteRandomVector3(BinaryWriter& writer, nptr<const XmlNode> node, float32_t default_x = 0.0f, float32_t default_y = 0.0f, float32_t default_z = 0.0f, float32_t multiplier = 1.0f)
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> x = Child(node, "X");
    nptr<const XmlNode> y = Child(node, "Y");
    nptr<const XmlNode> z = Child(node, "Z");
    writer.WriteFloat(FloatValue(x, "Max", default_x) * multiplier);
    writer.WriteFloat(FloatValue(y, "Max", default_y) * multiplier);
    writer.WriteFloat(FloatValue(z, "Max", default_z) * multiplier);
    writer.WriteFloat(FloatValue(x, "Min", default_x) * multiplier);
    writer.WriteFloat(FloatValue(y, "Min", default_y) * multiplier);
    writer.WriteFloat(FloatValue(z, "Min", default_z) * multiplier);
}

[[nodiscard]] static auto EasingCoefficients(int32_t start, int32_t end) -> std::array<float32_t, 3>
{
    FO_NO_STACK_TRACE_ENTRY();

    const float32_t gradient_start = numeric_cast<float32_t>(std::tan((numeric_cast<float64_t>(start) + 45.0) / 180.0 * std::numbers::pi));
    const float32_t gradient_end = numeric_cast<float32_t>(std::tan((numeric_cast<float64_t>(end) + 45.0) / 180.0 * std::numbers::pi));
    const float32_t c = gradient_start;
    const float32_t a = gradient_end - gradient_start - (1.0f - c) * 2.0f;
    const float32_t b = (gradient_end - gradient_start - a * 3.0f) / 2.0f;
    return {a, b, c};
}

static void WriteLegacyEasing(BinaryWriter& writer, int32_t start, int32_t end)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto coefficients = EasingCoefficients(start, end);

    for (const float32_t coefficient : coefficients) {
        writer.WriteFloat(coefficient);
    }
}

static void WriteVector3Easing(BinaryWriter& writer, nptr<const XmlNode> node, float32_t default_x, float32_t default_y, float32_t default_z, float32_t multiplier)
{
    FO_STACK_TRACE_ENTRY();

    BinaryWriter data;

    for (size_t index = 0; index < 4; ++index) {
        data.WriteInt32(-1);
    }

    WriteRandomVector3(data, Find(node, "Start"), default_x, default_y, default_z, multiplier);
    WriteRandomVector3(data, Find(node, "End"), default_x, default_y, default_z, multiplier);

    const bool middle_enabled = BoolValue(node, "IsMiddleEnabled", false);
    data.WriteInt32(middle_enabled ? 1 : 0);

    if (middle_enabled) {
        data.WriteInt32(-1);
        data.WriteInt32(-1);
        WriteRandomVector3(data, Find(node, "Middle"), default_x, default_y, default_z, multiplier);
    }

    const int32_t easing_type = IntValue(node, "Type", 0);
    data.WriteInt32(easing_type);

    if (easing_type == 0) {
        WriteLegacyEasing(data, IntValue(node, "StartSpeed", 0), IntValue(node, "EndSpeed", 0));
    }

    if (BoolValue(node, "IsRandomGroupEnabled", false)) {
        const std::array<int32_t, 3> ids {IntValue(node, "RandomGroupX", 0), IntValue(node, "RandomGroupY", 1), IntValue(node, "RandomGroupZ", 2)};
        map<int32_t, int32_t> channels;
        int32_t next_channel = 0;
        int32_t encoded = 0;

        for (size_t index = 0; index < ids.size(); ++index) {
            if (!channels.contains(ids[index])) {
                channels.emplace(ids[index], next_channel++);
            }

            encoded |= channels.at(ids[index]) << numeric_cast<int32_t>(index * 8);
        }

        data.WriteInt32(encoded);
    }
    else {
        data.WriteInt32(0x00020100);
    }

    const bool individual_enabled = BoolValue(node, "IsIndividualTypeEnabled", false);
    data.WriteInt32(individual_enabled ? 1 : 0);

    if (individual_enabled) {
        data.WriteInt32(IntValue(node, "TypeX", 1));
        data.WriteInt32(IntValue(node, "TypeY", 1));
        data.WriteInt32(IntValue(node, "TypeZ", 1));
    }

    writer.WriteSized(data);
}

static void WriteFloatEasing(BinaryWriter& writer, nptr<const XmlNode> node, float32_t default_value, float32_t multiplier, bool with_size)
{
    FO_STACK_TRACE_ENTRY();

    BinaryWriter data;

    for (size_t index = 0; index < 4; ++index) {
        data.WriteInt32(-1);
    }

    WriteRandomFloat(data, Find(node, "Start"), default_value, multiplier);
    WriteRandomFloat(data, Find(node, "End"), default_value, multiplier);

    const bool middle_enabled = BoolValue(node, "IsMiddleEnabled", false);
    data.WriteInt32(middle_enabled ? 1 : 0);

    if (middle_enabled) {
        data.WriteInt32(-1);
        data.WriteInt32(-1);
        WriteRandomFloat(data, Find(node, "Middle"), default_value, multiplier);
    }

    const int32_t easing_type = IntValue(node, "Type", 0);
    data.WriteInt32(easing_type);

    if (easing_type == 0) {
        WriteLegacyEasing(data, IntValue(node, "StartSpeed", 0), IntValue(node, "EndSpeed", 0));
    }

    data.WriteInt32(0);
    data.WriteInt32(0);

    if (with_size) {
        writer.WriteSized(data);
    }
    else {
        writer.WriteBytes(data.GetData());
    }
}

struct CurveKey final
{
    int32_t Frame {};
    float32_t Value {};
    float32_t LeftX {};
    float32_t LeftY {};
    float32_t RightX {};
    float32_t RightY {};
    int32_t Interpolation {};
};

[[nodiscard]] static auto ReadCurveKeys(nptr<const XmlNode> channel) -> vector<CurveKey>
{
    FO_STACK_TRACE_ENTRY();

    vector<CurveKey> keys;
    nptr<const XmlNode> keys_node = Child(channel, "Keys");

    if (!keys_node) {
        keys_node = channel;
    }
    if (!keys_node) {
        return keys;
    }

    for (const XmlNode& key_node : keys_node->Children) {
        if (!key_node.Name.starts_with("Key")) {
            continue;
        }

        const int32_t frame = IntValue(&key_node, "Frame", 0);
        const float32_t value = FloatValue(&key_node, "Value", 0.0f);
        keys.emplace_back(CurveKey {
            .Frame = frame,
            .Value = value,
            .LeftX = FloatValue(&key_node, "LeftX", numeric_cast<float32_t>(frame)),
            .LeftY = FloatValue(&key_node, "LeftY", value),
            .RightX = FloatValue(&key_node, "RightX", numeric_cast<float32_t>(frame)),
            .RightY = FloatValue(&key_node, "RightY", value),
            .Interpolation = IntValue(&key_node, "InterpolationType", 0),
        });
    }

    std::ranges::sort(keys, [](const CurveKey& left, const CurveKey& right) { return left.Frame != right.Frame ? left.Frame < right.Frame : left.Value < right.Value; });
    return keys;
}

[[nodiscard]] static auto CubicRoot(float32_t value) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return value == 0.0f ? 0.0f : value > 0.0f ? numeric_cast<float32_t>(std::pow(value, 1.0 / 3.0)) : -numeric_cast<float32_t>(std::pow(-value, 1.0 / 3.0));
}

[[nodiscard]] static auto IsCurveRootValid(float32_t value) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !std::isnan(value) && value >= -0.00000001f && value <= 1.000001f;
}

[[nodiscard]] static auto SolveCurveT(float32_t frame, float32_t x0, float32_t x1, float32_t x2, float32_t x3) -> optional<float32_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    const float32_t c3_source = x3 - x0 + 3.0f * (x1 - x2);
    const float32_t c2_source = 3.0f * (x0 - 2.0f * x1 + x2);
    const float32_t c1_source = 3.0f * (x1 - x0);
    const float32_t c0_source = x0 - frame;

    if (c3_source != 0.0f) {
        const float32_t c2 = c2_source / c3_source;
        const float32_t c1 = c1_source / c3_source;
        const float32_t c0 = c0_source / c3_source;
        const float32_t p = c1 / 3.0f - c2 * c2 / 9.0f;
        const float32_t q = (2.0f * c2 * c2 * c2 / 27.0f - c2 / 3.0f * c1 + c0) / 2.0f;
        const float32_t discriminant = q * q + p * p * p;

        if (discriminant > 0.0f) {
            const float32_t root = numeric_cast<float32_t>(std::sqrt(discriminant));
            const float32_t result = CubicRoot(-q + root) + CubicRoot(-q - root) - c2 / 3.0f;
            return IsCurveRootValid(result) ? optional<float32_t> {result} : std::nullopt;
        }
        if (discriminant == 0.0f) {
            for (const float32_t sign : {1.0f, -1.0f}) {
                const float32_t result = sign * 2.0f * CubicRoot(-q) - c2 / 3.0f;

                if (IsCurveRootValid(result)) {
                    return result;
                }
            }

            return std::nullopt;
        }

        const float32_t phi = numeric_cast<float32_t>(std::acos(-q / std::sqrt(-(p * p * p))));
        const float32_t root = numeric_cast<float32_t>(std::sqrt(-p));

        for (size_t index = 0; index < 3; ++index) {
            const float32_t result = 2.0f * root * numeric_cast<float32_t>(std::cos(phi / 3.0 + numeric_cast<float64_t>(index) * 2.0 * std::numbers::pi / 3.0)) - c2 / 3.0f;

            if (IsCurveRootValid(result)) {
                return result;
            }
        }

        return std::nullopt;
    }

    float32_t first = std::numeric_limits<float32_t>::quiet_NaN();
    float32_t second = std::numeric_limits<float32_t>::quiet_NaN();

    if (c2_source != 0.0f) {
        const float32_t discriminant = c1_source * c1_source - 4.0f * c2_source * c0_source;

        if (discriminant > 0.0f) {
            const float32_t root = numeric_cast<float32_t>(std::sqrt(discriminant));
            first = (-c1_source - root) / (2.0f * c2_source);
            second = (-c1_source + root) / (2.0f * c2_source);
        }
        else if (discriminant == 0.0f) {
            first = -c1_source / (2.0f * c2_source);
        }
    }
    else if (c1_source != 0.0f) {
        first = -c0_source / c1_source;
    }
    else if (c0_source == 0.0f) {
        first = 0.0f;
    }
    if (IsCurveRootValid(first)) {
        return first;
    }

    return IsCurveRootValid(second) ? optional<float32_t> {second} : std::nullopt;
}

[[nodiscard]] static auto SampleCurve(const vector<CurveKey>& keys, int32_t frame, int32_t start_edge, int32_t end_edge, float32_t default_value) -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    if (keys.empty()) {
        return default_value;
    }

    const int32_t length = keys.back().Frame - keys.front().Frame;

    if (length == 0) {
        return keys.front().Value;
    }
    if (keys.back().Frame <= frame) {
        if (end_edge == 0) {
            return keys.back().Value;
        }

        frame = end_edge == 1 ? (frame - keys.back().Frame) % length + keys.front().Frame : (length - (frame - keys.back().Frame) % length) + keys.front().Frame;
    }
    if (frame <= keys.front().Frame) {
        if (start_edge == 0) {
            return keys.front().Value;
        }

        frame = start_edge == 1 ? (keys.front().Frame - frame) % length + keys.front().Frame : (length - (keys.front().Frame - frame) % length) + keys.front().Frame;
    }

    size_t left_index = 0;
    size_t right_index = keys.size() - 1;

    while (right_index - left_index > 1) {
        const size_t middle = (left_index + right_index) / 2;

        if (keys[middle].Frame <= frame) {
            left_index = middle;
        }
        else {
            right_index = middle;
        }
    }

    const CurveKey& left = keys[left_index];
    const CurveKey& right = keys[right_index];

    if (left.Interpolation == 1) {
        const float32_t frame_distance = numeric_cast<float32_t>(right.Frame - left.Frame);
        return frame_distance == 0.0f ? left.Value : (right.Value - left.Value) / frame_distance * numeric_cast<float32_t>(frame - left.Frame) + left.Value;
    }
    if (left.Frame == frame) {
        return left.Value;
    }

    float32_t left_right_x = left.RightX;
    float32_t left_right_y = left.RightY;
    float32_t right_left_x = right.LeftX;
    float32_t right_left_y = right.LeftY;
    const float32_t handle_right = std::abs(numeric_cast<float32_t>(left.Frame) - left_right_x);
    const float32_t handle_left = std::abs(numeric_cast<float32_t>(right.Frame) - right_left_x);
    const float32_t segment = numeric_cast<float32_t>(right.Frame - left.Frame);

    if (handle_right + handle_left > segment && handle_right + handle_left != 0.0f) {
        const float32_t factor = segment / (handle_right + handle_left);
        left_right_x = numeric_cast<float32_t>(left.Frame) - factor * (numeric_cast<float32_t>(left.Frame) - left_right_x);
        left_right_y = left.Value - factor * (left.Value - left_right_y);
        right_left_x = numeric_cast<float32_t>(right.Frame) - factor * (numeric_cast<float32_t>(right.Frame) - right_left_x);
        right_left_y = right.Value - factor * (right.Value - right_left_y);
    }

    const optional<float32_t> t = SolveCurveT(numeric_cast<float32_t>(frame), numeric_cast<float32_t>(left.Frame), left_right_x, right_left_x, numeric_cast<float32_t>(right.Frame));

    if (!t) {
        return 0.0f;
    }

    const float32_t c0 = left.Value;
    const float32_t c1 = 3.0f * (left_right_y - left.Value);
    const float32_t c2 = 3.0f * (left.Value - 2.0f * left_right_y + right_left_y);
    const float32_t c3 = right.Value - left.Value + 3.0f * (left_right_y - right_left_y);
    return c0 + *t * c1 + *t * *t * c2 + *t * *t * *t * c3;
}

static void WriteCurveChannel(BinaryWriter& writer, nptr<const XmlNode> channel, float32_t default_value, float32_t multiplier)
{
    FO_STACK_TRACE_ENTRY();

    const int32_t start_edge = IntValue(channel, "StartType", 0);
    const int32_t end_edge = IntValue(channel, "EndType", 0);
    writer.WriteInt32(start_edge);
    writer.WriteInt32(end_edge);
    writer.WriteFloat(FloatValue(channel, "OffsetMax", 0.0f));
    writer.WriteFloat(FloatValue(channel, "OffsetMin", 0.0f));

    const vector<CurveKey> keys = ReadCurveKeys(channel);

    if (keys.empty()) {
        for (size_t index = 0; index < 4; ++index) {
            writer.WriteInt32(0);
        }

        return;
    }

    const int32_t sampling = IntValue(channel, "Sampling", 10);

    if (sampling <= 0) {
        throw EffekseerCompilerException("Effekseer FCurve sampling must be positive", sampling);
    }

    const int32_t length = keys.back().Frame - keys.front().Frame;
    const int32_t count = length % sampling > 0 ? length / sampling + 2 : length / sampling + 1;
    writer.WriteInt32(keys.front().Frame);
    writer.WriteInt32(length);
    writer.WriteInt32(sampling);
    writer.WriteInt32(count);

    for (int32_t frame = keys.front().Frame; frame < keys.back().Frame; frame += sampling) {
        writer.WriteFloat(SampleCurve(keys, frame, start_edge, end_edge, default_value) * multiplier);
    }

    writer.WriteFloat(SampleCurve(keys, keys.back().Frame, start_edge, end_edge, default_value) * multiplier);
}

static void WriteVector3Curve(BinaryWriter& writer, nptr<const XmlNode> node, float32_t default_x, float32_t default_y, float32_t default_z, float32_t multiplier)
{
    FO_NO_STACK_TRACE_ENTRY();

    writer.WriteInt32(IntValue(node, "Timeline", 0));
    nptr<const XmlNode> keys = Find(node, "Keys");
    WriteCurveChannel(writer, Child(keys, "X"), default_x, multiplier);
    WriteCurveChannel(writer, Child(keys, "Y"), default_y, multiplier);
    WriteCurveChannel(writer, Child(keys, "Z"), default_z, multiplier);
}

static void WriteVector2Curve(BinaryWriter& writer, nptr<const XmlNode> node, float32_t default_x, float32_t default_y, float32_t multiplier_x, float32_t multiplier_y)
{
    FO_NO_STACK_TRACE_ENTRY();

    writer.WriteInt32(IntValue(node, "Timeline", 0));
    nptr<const XmlNode> keys = Find(node, "Keys");
    WriteCurveChannel(writer, Child(keys, "X"), default_x, multiplier_x);
    WriteCurveChannel(writer, Child(keys, "Y"), default_y, multiplier_y);
}

static void WriteScalarCurve(BinaryWriter& writer, nptr<const XmlNode> node, float32_t default_value, float32_t multiplier)
{
    FO_NO_STACK_TRACE_ENTRY();

    writer.WriteInt32(IntValue(node, "Timeline", 0));
    nptr<const XmlNode> keys = Find(node, "Keys");
    WriteCurveChannel(writer, Child(keys, "S"), default_value, multiplier);
}

[[nodiscard]] static auto NodeDrawingType(nptr<const XmlNode> node) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return IntValue(Find(node, "DrawingValues"), "Type", 2);
}

[[nodiscard]] static auto NodeIsRendered(nptr<const XmlNode> node) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return BoolValue(node, "IsRendered", true);
}

[[nodiscard]] static auto IsRenderedNode(nptr<const XmlNode> node) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return (NodeIsRendered(node) && NodeDrawingType(node) != 0) || IntValue(Find(node, "SoundValues"), "Type", 0) == 1 || BoolValue(Find(node, "GpuParticles"), "Enabled", false);
}

[[nodiscard]] static auto HasRenderedNode(nptr<const XmlNode> node) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (IsRenderedNode(node)) {
        return true;
    }

    nptr<const XmlNode> children = Child(node, "Children");
    return children && std::ranges::any_of(children->Children, [](const XmlNode& child) { return child.Name == "Node" && HasRenderedNode(&child); });
}

static void CollectExportedNodes(nptr<const XmlNode> parent, vector<nptr<const XmlNode>>& nodes)
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> children = Child(parent, "Children");

    if (!children) {
        return;
    }

    for (const XmlNode& child : children->Children) {
        if (child.Name == "Node" && HasRenderedNode(&child)) {
            nodes.emplace_back(&child);
            CollectExportedNodes(&child, nodes);
        }
    }
}

[[nodiscard]] static auto ChangeDependencyExtension(string_view path, string_view ext) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (path.empty()) {
        return {};
    }

    const string normalized = strex(path).normalize_path_slashes();
    return strex(normalized).change_file_extension(ext);
}

static void AssignResourceIndices(const set<string>& resources, map<string, int32_t>& indices)
{
    FO_NO_STACK_TRACE_ENTRY();

    int32_t index = 0;

    for (const string& resource : resources) {
        indices.emplace(resource, index++);
    }
}

static void CollectResources(CompilerContext& context)
{
    FO_STACK_TRACE_ENTRY();

    set<string> color_textures;
    set<string> normal_textures;
    set<string> distortion_textures;
    set<string> waves;
    set<string> models;
    set<string> curves;

    for (nptr<const XmlNode> node : context.ExportedNodes) {
        nptr<const XmlNode> renderer = Find(node, "RendererCommonValues");
        nptr<const XmlNode> drawing = Find(node, "DrawingValues");
        const int32_t material = IntValue(renderer, "Material", 0);
        const bool rendered = IsRenderedNode(node);

        if (rendered) {
            const string color_path {Text(renderer, "ColorTexture")};
            const string normal_path {Text(renderer, "NormalTexture")};

            if (!color_path.empty()) {
                if (material == 6) {
                    distortion_textures.emplace(color_path);
                }
                else if (material == 0 || material == 7) {
                    color_textures.emplace(color_path);
                }
            }
            if (material == 7 && !normal_path.empty()) {
                normal_textures.emplace(normal_path);
            }

            if (NodeDrawingType(node) == 5) {
                const string model_path = ChangeDependencyExtension(Text(drawing, "Model/Model"), "efkmodel");

                if (!model_path.empty()) {
                    models.emplace(model_path);
                }
            }
            if (IntValue(Find(node, "SoundValues"), "Type", 0) == 1) {
                const string wave_path {Text(Find(node, "SoundValues"), "Sound/Wave")};

                if (!wave_path.empty()) {
                    waves.emplace(wave_path);
                }
            }
        }

        nptr<const XmlNode> generation = Find(node, "GenerationLocationValues");

        if (IntValue(generation, "Type", 0) == 2) {
            const string model_path = ChangeDependencyExtension(Text(generation, "Model/Model"), "efkmodel");

            if (!model_path.empty()) {
                models.emplace(model_path);
            }
        }
        if (IntValue(Find(node, "LocationValues"), "Type", 0) == 4) {
            const string curve_path = ChangeDependencyExtension(Text(Find(node, "LocationValues"), "NurbsCurve/FilePath"), "efkcurve");

            if (!curve_path.empty()) {
                curves.emplace(curve_path);
            }
        }
    }

    AssignResourceIndices(color_textures, context.ColorTextures);
    AssignResourceIndices(normal_textures, context.NormalTextures);
    AssignResourceIndices(distortion_textures, context.DistortionTextures);
    AssignResourceIndices(waves, context.Waves);
    AssignResourceIndices(models, context.Models);
    AssignResourceIndices(curves, context.Curves);

    for (ptr<const map<string, int32_t>> table : {&context.ColorTextures, &context.NormalTextures, &context.DistortionTextures, &context.Waves, &context.Models, &context.Curves}) {
        for (const auto& [path, index] : *table) {
            ignore_unused(index);
            context.Dependencies.emplace(path);
        }
    }
}

static void WriteResourceTable(BinaryWriter& writer, const map<string, int32_t>& resources)
{
    FO_NO_STACK_TRACE_ENTRY();

    writer.WriteInt32(numeric_cast<int32_t>(resources.size()));

    for (const auto& [path, index] : resources) {
        ignore_unused(index);
        writer.WriteUtf16(path);
    }
}

[[nodiscard]] static auto DynamicEquationIndex(nptr<const XmlNode> node, string_view path) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return IntValue(node, path, -1);
}

static void WriteCommonValues(BinaryWriter& writer, nptr<const XmlNode> node)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> common = Find(node, "CommonValues");
    nptr<const XmlNode> generation = Find(common, "Generation");
    nptr<const XmlNode> removal = Find(common, "Removal");
    BinaryWriter data;
    data.WriteInt32(DynamicEquationIndex(common, "MaxGeneration/DynamicEquation/Index"));
    data.WriteInt32(DynamicEquationIndex(common, "Life/DynamicEquationMax/Index"));
    data.WriteInt32(DynamicEquationIndex(common, "Life/DynamicEquationMin/Index"));
    data.WriteInt32(DynamicEquationIndex(generation, "GenerationTime/DynamicEquationMax/Index"));
    data.WriteInt32(DynamicEquationIndex(generation, "GenerationTime/DynamicEquationMin/Index"));
    data.WriteInt32(DynamicEquationIndex(generation, "GenerationTimeOffset/DynamicEquationMax/Index"));
    data.WriteInt32(DynamicEquationIndex(generation, "GenerationTimeOffset/DynamicEquationMin/Index"));
    data.WriteInt32(DynamicEquationIndex(generation, "TriggerCount/DynamicEquationMax/Index"));
    data.WriteInt32(DynamicEquationIndex(generation, "TriggerCount/DynamicEquationMin/Index"));

    data.WriteInt32(BoolValue(common, "MaxGeneration/Infinite", false) ? std::numeric_limits<int32_t>::max() : IntValue(common, "MaxGeneration/Value", 1));
    const int32_t location_effect = IntValue(common, "LocationEffectType", 2);
    data.WriteInt32(location_effect);
    data.WriteInt32(IntValue(common, "RotationEffectType", 2));
    data.WriteInt32(IntValue(common, "ScaleEffectType", 2));
    data.WriteInt32(IntValue(generation, "Timing", 0));

    int32_t removal_flags = 0;
    removal_flags |= BoolValue(removal, "WhenLifeIsExtinct", true) ? 1 << 0 : 0;
    removal_flags |= BoolValue(removal, "WhenParentIsRemoved", false) ? 1 << 1 : 0;
    removal_flags |= BoolValue(removal, "WhenAllChildrenAreRemoved", false) ? 1 << 2 : 0;
    removal_flags |= IntValue(removal, "TriggerToRemove", 0) != 0 ? 1 << 3 : 0;
    data.WriteInt32(removal_flags);
    data.WriteInt32(IntValue(common, "Life/Max", 100));
    data.WriteInt32(IntValue(common, "Life/Min", 100));
    data.WriteFloat(FloatValue(generation, "GenerationTime/Max", 1.0f));
    data.WriteFloat(FloatValue(generation, "GenerationTime/Min", 1.0f));
    data.WriteFloat(FloatValue(generation, "GenerationTimeOffset/Max", 0.0f));
    data.WriteFloat(FloatValue(generation, "GenerationTimeOffset/Min", 0.0f));
    data.WriteInt32(IntValue(generation, "TriggerCount/Max", 1));
    data.WriteInt32(IntValue(generation, "TriggerCount/Min", 1));
    data.WriteUInt16(numeric_cast<uint16_t>(IntValue(generation, "ToStartGeneration", 0)));
    data.WriteUInt16(numeric_cast<uint16_t>(IntValue(generation, "ToStopGeneration", 0)));
    data.WriteUInt16(numeric_cast<uint16_t>(IntValue(removal, "TriggerToRemove", 0)));
    data.WriteUInt16(numeric_cast<uint16_t>(IntValue(generation, "Trigger", 0)));
    writer.WriteSized(data);

    if (location_effect == 4 || location_effect == 5) {
        WriteRandomFloat(writer, Find(common, "SteeringBehaviorParam/MaxFollowSpeed"), 10.0f);
        WriteRandomFloat(writer, Find(common, "SteeringBehaviorParam/SteeringSpeed"), 30.0f);
    }

    writer.WriteInt32(IntValue(common, "LodParameter/MatchingLODs", 15));
    writer.WriteInt32(IntValue(common, "LodParameter/LodBehaviour", 0));
}

static void WriteLocationValues(BinaryWriter& writer, nptr<const XmlNode> node, const CompilerContext& context)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> values = Find(node, "LocationValues");
    const int32_t type = IntValue(values, "Type", 0);
    writer.WriteInt32(type);

    if (type == 0) {
        BinaryWriter data;
        data.WriteInt32(-1);
        WriteVector3(data, Find(values, "Fixed/Location"));
        writer.WriteSized(data);
    }
    else if (type == 1) {
        BinaryWriter data;

        for (size_t index = 0; index < 6; ++index) {
            data.WriteInt32(-1);
        }

        WriteRandomVector3(data, Find(values, "PVA/Location"));
        WriteRandomVector3(data, Find(values, "PVA/Velocity"));
        WriteRandomVector3(data, Find(values, "PVA/Acceleration"));
        writer.WriteSized(data);
    }
    else if (type == 2) {
        WriteVector3Easing(writer, Find(values, "Easing"), 0.0f, 0.0f, 0.0f, 1.0f);
    }
    else if (type == 3) {
        BinaryWriter data;
        WriteVector3Curve(data, Find(values, "LocationFCurve/FCurve"), 0.0f, 0.0f, 0.0f, 1.0f);
        writer.WriteSized(data);
    }
    else if (type == 4) {
        const string path = ChangeDependencyExtension(Text(values, "NurbsCurve/FilePath"), "efkcurve");
        writer.WriteInt32(path.empty() || !context.Curves.contains(path) ? -1 : context.Curves.at(path));
        writer.WriteFloat(FloatValue(values, "NurbsCurve/Scale", 1.0f));
        writer.WriteFloat(FloatValue(values, "NurbsCurve/MoveSpeed", 1.0f));
        writer.WriteInt32(IntValue(values, "NurbsCurve/LoopType", 0));
    }
    else if (type == 5) {
        WriteRandomFloat(writer, Find(values, "ViewOffset/Distance"), 3.0f);
    }
    else {
        throw EffekseerCompilerException("Effekseer project uses an unsupported location type", type);
    }
}

static void WriteLocationAbsValues(BinaryWriter& writer, nptr<const XmlNode> node)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> values = Find(node, "LocationAbsValues");
    writer.WriteInt32(4);

    for (int32_t index = 1; index <= 4; ++index) {
        nptr<const XmlNode> field = Child(values, strex("LocalForceField{}", index));
        const int32_t type = IntValue(field, "Type", 0);
        writer.WriteInt32(type);
        writer.WriteFloat(FloatValue(field, "Power", 1.0f));
        WriteVector3(writer, Find(field, "Position"));
        WriteVector3(writer, Find(field, "Rotation"), 0.0f, 0.0f, 0.0f, Pi / 180.0f);

        if (type == 2) {
            writer.WriteInt32(BoolValue(field, "Force/Gravitation", false) ? 1 : 0);
        }
        else if (type == 4) {
            writer.WriteInt32(IntValue(field, "Vortex/VortexType", 0));
        }
        else if (type == 1) {
            writer.WriteInt32(IntValue(field, "Turbulence/TurbulenceType", 0));
            writer.WriteInt32(IntValue(field, "Turbulence/Seed", 1));
            writer.WriteFloat(FloatValue(field, "Turbulence/FieldScale", 4.0f));
            writer.WriteInt32(IntValue(field, "Turbulence/Octave", 1));
        }
        else if (type == 8) {
            WriteVector3(writer, Find(field, "Gravity/Gravity"));
        }
        else if (type == 9) {
            writer.WriteFloat(FloatValue(field, "AttractiveForce/Control", 1.0f));
            writer.WriteFloat(FloatValue(field, "AttractiveForce/MinRange", 0.0f));
            writer.WriteFloat(FloatValue(field, "AttractiveForce/MaxRange", 0.0f));
        }

        nptr<const XmlNode> falloff = Find(field, "Falloff");
        const int32_t falloff_type = IntValue(falloff, "Type", 0);
        writer.WriteInt32(falloff_type);

        if (falloff_type != 0) {
            writer.WriteFloat(FloatValue(falloff, "Power", 1.0f));
            writer.WriteFloat(FloatValue(falloff, "MaxDistance", 1.0f));
            writer.WriteFloat(FloatValue(falloff, "MinDistance", 0.0f));

            if (falloff_type == 2) {
                writer.WriteFloat(FloatValue(falloff, "Tube/RadiusPower", 1.0f));
                writer.WriteFloat(FloatValue(falloff, "Tube/MaxRadius", 1.0f));
                writer.WriteFloat(FloatValue(falloff, "Tube/MinRadius", 0.0f));
            }
            else if (falloff_type == 3) {
                writer.WriteFloat(FloatValue(falloff, "Cone/AnglePower", 1.0f));
                writer.WriteFloat(FloatValue(falloff, "Cone/MaxAngle", 180.0f) * Pi / 180.0f);
                writer.WriteFloat(FloatValue(falloff, "Cone/MinAngle", 0.0f) * Pi / 180.0f);
            }
        }
    }
}

static void WriteRotationValues(BinaryWriter& writer, nptr<const XmlNode> node)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> values = Find(node, "RotationValues");
    const int32_t type = IntValue(values, "Type", 0);
    writer.WriteInt32(type);

    if (type == 0) {
        BinaryWriter data;
        data.WriteInt32(-1);
        WriteVector3(data, Find(values, "Fixed/Rotation"), 0.0f, 0.0f, 0.0f, Pi / 180.0f);
        writer.WriteSized(data);
    }
    else if (type == 1) {
        BinaryWriter data;

        for (size_t index = 0; index < 6; ++index) {
            data.WriteInt32(-1);
        }

        WriteRandomVector3(data, Find(values, "PVA/Rotation"), 0.0f, 0.0f, 0.0f, Pi / 180.0f);
        WriteRandomVector3(data, Find(values, "PVA/Velocity"), 0.0f, 0.0f, 0.0f, Pi / 180.0f);
        WriteRandomVector3(data, Find(values, "PVA/Acceleration"), 0.0f, 0.0f, 0.0f, Pi / 180.0f);
        writer.WriteSized(data);
    }
    else if (type == 2) {
        WriteVector3Easing(writer, Find(values, "Easing"), 0.0f, 0.0f, 0.0f, Pi / 180.0f);
    }
    else if (type == 3) {
        BinaryWriter data;
        WriteRandomVector3(data, Find(values, "AxisPVA/Axis"), 0.0f, 1.0f, 0.0f);
        WriteRandomFloat(data, Find(values, "AxisPVA/Rotation"), 0.0f, Pi / 180.0f);
        WriteRandomFloat(data, Find(values, "AxisPVA/Velocity"), 0.0f, Pi / 180.0f);
        WriteRandomFloat(data, Find(values, "AxisPVA/Acceleration"), 0.0f, Pi / 180.0f);
        writer.WriteSized(data);
    }
    else if (type == 4) {
        BinaryWriter data;
        WriteRandomVector3(data, Find(values, "AxisEasing/Axis"), 0.0f, 1.0f, 0.0f);
        WriteFloatEasing(data, Find(values, "AxisEasing/Easing"), 0.0f, Pi / 180.0f, true);
        writer.WriteSized(data);
    }
    else if (type == 5) {
        BinaryWriter data;
        WriteVector3Curve(data, Find(values, "RotationFCurve/FCurve"), 0.0f, 0.0f, 0.0f, Pi / 180.0f);
        writer.WriteSized(data);
    }
    else if (type == 6) {
        writer.WriteInt32(0);
    }
    else if (type == 7) {
        BinaryWriter data;
        data.WriteInt32(IntValue(values, "Velocity/Axis", 0));
        writer.WriteSized(data);
    }
    else {
        throw EffekseerCompilerException("Effekseer project uses an unsupported rotation type", type);
    }
}

static void WriteScaleValues(BinaryWriter& writer, nptr<const XmlNode> node)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> values = Find(node, "ScalingValues");
    const int32_t type = IntValue(values, "Type", 0);
    writer.WriteInt32(type);

    if (type == 0) {
        BinaryWriter data;
        data.WriteInt32(-1);
        WriteVector3(data, Find(values, "Fixed/Scale"), 1.0f, 1.0f, 1.0f);
        writer.WriteSized(data);
    }
    else if (type == 1) {
        BinaryWriter data;

        for (size_t index = 0; index < 6; ++index) {
            data.WriteInt32(-1);
        }

        WriteRandomVector3(data, Find(values, "PVA/Scale"), 1.0f, 1.0f, 1.0f);
        WriteRandomVector3(data, Find(values, "PVA/Velocity"));
        WriteRandomVector3(data, Find(values, "PVA/Acceleration"));
        writer.WriteSized(data);
    }
    else if (type == 2) {
        WriteVector3Easing(writer, Find(values, "Easing"), 1.0f, 1.0f, 1.0f, 1.0f);
    }
    else if (type == 3) {
        BinaryWriter data;
        WriteRandomFloat(data, Find(values, "SinglePVA/Scale"), 1.0f);
        WriteRandomFloat(data, Find(values, "SinglePVA/Velocity"), 0.0f);
        WriteRandomFloat(data, Find(values, "SinglePVA/Acceleration"), 0.0f);
        writer.WriteSized(data);
    }
    else if (type == 4) {
        WriteFloatEasing(writer, Find(values, "SingleEasing"), 1.0f, 1.0f, true);
    }
    else if (type == 5) {
        BinaryWriter data;
        WriteVector3Curve(data, Find(values, "FCurve/FCurve"), 1.0f, 1.0f, 1.0f, 1.0f);
        writer.WriteSized(data);
    }
    else if (type == 6) {
        BinaryWriter data;
        WriteScalarCurve(data, Find(values, "SingleFCurve"), 1.0f, 1.0f);
        writer.WriteSized(data);
    }
    else {
        throw EffekseerCompilerException("Effekseer project uses an unsupported scale type", type);
    }
}

static void WriteGenerationLocationValues(BinaryWriter& writer, nptr<const XmlNode> node, const CompilerContext& context)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> values = Find(node, "GenerationLocationValues");
    writer.WriteInt32(BoolValue(values, "EffectsRotation", false) ? 1 : 0);
    const int32_t type = IntValue(values, "Type", 0);
    writer.WriteInt32(type);

    if (type == 0) {
        WriteRandomVector3(writer, Find(values, "Point/Location"));
    }
    else if (type == 1) {
        WriteRandomFloat(writer, Find(values, "Sphere/Radius"));
        WriteRandomFloat(writer, Find(values, "Sphere/RotationX"), 0.0f, Pi / 180.0f);
        WriteRandomFloat(writer, Find(values, "Sphere/RotationY"), 0.0f, Pi / 180.0f);
    }
    else if (type == 2) {
        nptr<const XmlNode> model = Find(values, "Model");
        const int32_t reference_type = IntValue(model, "ModelReference", 0);
        writer.WriteInt32(reference_type);

        if (reference_type == 0) {
            const string model_path = ChangeDependencyExtension(Text(model, "Model"), "efkmodel");
            writer.WriteInt32(!model_path.empty() && context.Models.contains(model_path) ? context.Models.at(model_path) : -1);
        }
        else if (reference_type == 1) {
            throw EffekseerCompilerException("Effekseer procedural models are not supported by the fixed project profile");
        }
        else {
            writer.WriteInt32(IntValue(model, "ExternalModelIndex", 0));
        }

        writer.WriteInt32(IntValue(model, "Type", 0));
        writer.WriteInt32(IntValue(model, "Coordinate", 0));
    }
    else if (type == 3) {
        nptr<const XmlNode> circle = Find(values, "Circle");
        writer.WriteInt32(IntValue(circle, "Division", 8));
        WriteRandomFloat(writer, Find(circle, "Radius"));
        WriteRandomFloat(writer, Find(circle, "AngleStart"), 0.0f, Pi / 180.0f);
        WriteRandomFloat(writer, Find(circle, "AngleEnd"), 360.0f, Pi / 180.0f);
        writer.WriteInt32(IntValue(circle, "Type", 0));
        writer.WriteInt32(IntValue(circle, "AxisDirection", 2));
        WriteRandomFloat(writer, Find(circle, "AngleNoize"), 0.0f, Pi / 180.0f);
    }
    else if (type == 4) {
        nptr<const XmlNode> line = Find(values, "Line");
        writer.WriteInt32(IntValue(line, "Division", 8));
        WriteRandomVector3(writer, Find(line, "PositionStart"));
        WriteRandomVector3(writer, Find(line, "PositionEnd"));
        WriteRandomFloat(writer, Find(line, "PositionNoize"));
        writer.WriteInt32(IntValue(line, "Type", 0));
    }
    else {
        throw EffekseerCompilerException("Effekseer project uses an unsupported generation-location type", type);
    }
}

[[nodiscard]] static auto ResolveDependencyPath(const CompilerContext& context, string_view path) -> string
{
    FO_STACK_TRACE_ENTRY();

    const std::filesystem::path resolved = (std::filesystem::path {fs_make_path(context.ProjectDirectory)} / std::filesystem::path {fs_make_path(strex(path).normalize_path_slashes())}).lexically_normal();
    return fs_path_to_string(resolved);
}

[[nodiscard]] static auto ReadTextureSize(const CompilerContext& context, string_view path) -> optional<std::pair<float32_t, float32_t>>
{
    FO_STACK_TRACE_ENTRY();

    const optional<string> bytes = fs_read_file(ResolveDependencyPath(context, path));

    if (!bytes || bytes->size() < 18) {
        return std::nullopt;
    }

    const ptr<const uint8_t> data = ptr<const char> {bytes->data()}.reinterpret_as<const uint8_t>();
    constexpr std::array<uint8_t, 8> signature {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    uint32_t width = 0;
    uint32_t height = 0;

    if (bytes->size() >= 24 && std::ranges::equal(signature, make_const_span(data, signature.size()))) {
        width = (numeric_cast<uint32_t>(data[16]) << 24U) | (numeric_cast<uint32_t>(data[17]) << 16U) | (numeric_cast<uint32_t>(data[18]) << 8U) | numeric_cast<uint32_t>(data[19]);
        height = (numeric_cast<uint32_t>(data[20]) << 24U) | (numeric_cast<uint32_t>(data[21]) << 16U) | (numeric_cast<uint32_t>(data[22]) << 8U) | numeric_cast<uint32_t>(data[23]);
    }
    else if (bytes->size() >= 20 && data[0] == uint8_t {'D'} && data[1] == uint8_t {'D'} && data[2] == uint8_t {'S'}) {
        width = numeric_cast<uint32_t>(data[16]) | (numeric_cast<uint32_t>(data[17]) << 8U) | (numeric_cast<uint32_t>(data[18]) << 16U) | (numeric_cast<uint32_t>(data[19]) << 24U);
        height = numeric_cast<uint32_t>(data[12]) | (numeric_cast<uint32_t>(data[13]) << 8U) | (numeric_cast<uint32_t>(data[14]) << 16U) | (numeric_cast<uint32_t>(data[15]) << 24U);
    }
    else {
        constexpr string_view tga_footer {"TRUEVISION-XFILE.\0", 18};
        const bool has_tga_footer = bytes->size() >= tga_footer.size() && string_view {bytes->data() + bytes->size() - tga_footer.size(), tga_footer.size()} == tga_footer;

        if (strex(path).get_file_extension() != "tga" && !has_tga_footer) {
            return std::nullopt;
        }

        width = numeric_cast<uint32_t>(data[12]) | (numeric_cast<uint32_t>(data[13]) << 8U);
        height = numeric_cast<uint32_t>(data[14]) | (numeric_cast<uint32_t>(data[15]) << 8U);
    }

    if (width == 0 || height == 0) {
        return std::nullopt;
    }

    return std::make_pair(numeric_cast<float32_t>(width), numeric_cast<float32_t>(height));
}

[[nodiscard]] static auto TextureIndex(const CompilerContext& context, string_view path, const map<string, int32_t>& indices) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = indices.find(string {path});
    return it != indices.end() && ReadTextureSize(context, path) ? it->second : -1;
}

static void WriteBasicUv(BinaryWriter& writer, nptr<const XmlNode> renderer, const CompilerContext& context, string_view texture_path)
{
    FO_STACK_TRACE_ENTRY();

    float32_t width = 128.0f;
    float32_t height = 128.0f;

    if (const auto size = ReadTextureSize(context, texture_path)) {
        width = size->first;
        height = size->second;
    }

    const int32_t type = IntValue(renderer, "UV", 0);
    writer.WriteInt32(type);

    if (type == 1) {
        writer.WriteFloat(FloatValue(renderer, "UVFixed/Start/X", 0.0f) / width);
        writer.WriteFloat(FloatValue(renderer, "UVFixed/Start/Y", 0.0f) / height);
        writer.WriteFloat(FloatValue(renderer, "UVFixed/Size/X", 0.0f) / width);
        writer.WriteFloat(FloatValue(renderer, "UVFixed/Size/Y", 0.0f) / height);
    }
    else if (type == 2) {
        nptr<const XmlNode> animation = Find(renderer, "UVAnimation/AnimationParams");
        writer.WriteFloat(FloatValue(animation, "Start/X", 0.0f) / width);
        writer.WriteFloat(FloatValue(animation, "Start/Y", 0.0f) / height);
        writer.WriteFloat(FloatValue(animation, "Size/X", 0.0f) / width);
        writer.WriteFloat(FloatValue(animation, "Size/Y", 0.0f) / height);
        writer.WriteInt32(BoolValue(animation, "FrameLength/Infinite", false) ? std::numeric_limits<int32_t>::max() / 100 : IntValue(animation, "FrameLength/Value", 1));
        writer.WriteInt32(IntValue(animation, "FrameCountX", 1));
        writer.WriteInt32(IntValue(animation, "FrameCountY", 1));
        writer.WriteInt32(IntValue(animation, "LoopType", 0));
        writer.WriteInt32(IntValue(animation, "StartSheet/Max", 0));
        writer.WriteInt32(IntValue(animation, "StartSheet/Min", 0));
        writer.WriteInt32(IntValue(Find(renderer, "UVAnimation"), "FlipbookInterpolationType", 0));
    }
    else if (type == 3) {
        nptr<const XmlNode> scroll = Find(renderer, "UVScroll");
        WriteRandomVector2(writer, Find(scroll, "Start"), 0.0f, 0.0f, 1.0f / width, 1.0f / height);
        WriteRandomVector2(writer, Find(scroll, "Size"), 0.0f, 0.0f, 1.0f / width, 1.0f / height);
        WriteRandomVector2(writer, Find(scroll, "Speed"), 0.0f, 0.0f, 1.0f / width, 1.0f / height);
    }
    else if (type == 4) {
        WriteVector2Curve(writer, Find(renderer, "UVFCurve/Start"), 0.0f, 0.0f, 1.0f / width, 1.0f / height);
        WriteVector2Curve(writer, Find(renderer, "UVFCurve/Size"), 0.0f, 0.0f, 1.0f / width, 1.0f / height);
    }
    else if (type != 0) {
        throw EffekseerCompilerException("Effekseer project uses an unsupported UV type", type);
    }
}

static void WriteRendererCommonValues(BinaryWriter& writer, nptr<const XmlNode> node, const CompilerContext& context)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> renderer = Find(node, "RendererCommonValues");
    const int32_t material = IntValue(renderer, "Material", 0);
    const string color_path {Text(renderer, "ColorTexture")};
    const string normal_path {Text(renderer, "NormalTexture")};
    writer.WriteInt32(material);

    if (material == 0 || material == 7) {
        writer.WriteFloat(FloatValue(renderer, "EmissiveScaling", 1.0f));
    }
    if (material == 0) {
        writer.WriteInt32(TextureIndex(context, color_path, context.ColorTextures));
        writer.WriteInt32(-1);
    }
    else if (material == 6) {
        writer.WriteInt32(TextureIndex(context, color_path, context.DistortionTextures));
        writer.WriteInt32(-1);
    }
    else if (material == 7) {
        writer.WriteInt32(TextureIndex(context, color_path, context.ColorTextures));
        writer.WriteInt32(TextureIndex(context, normal_path, context.NormalTextures));
    }
    else {
        throw EffekseerCompilerException("Effekseer material files are not supported by the fixed project profile", material);
    }

    for (size_t index = 0; index < 5; ++index) {
        writer.WriteInt32(-1);
    }

    writer.WriteInt32(IntValue(renderer, "AlphaBlend", 1));
    writer.WriteInt32(IntValue(renderer, "Filter", 1));
    writer.WriteInt32(IntValue(renderer, "Wrap", 0));
    writer.WriteInt32(IntValue(renderer, "Filter2", 1));
    writer.WriteInt32(IntValue(renderer, "Wrap2", 0));

    for (size_t index = 0; index < 5; ++index) {
        writer.WriteInt32(1);
        writer.WriteInt32(0);
    }

    writer.WriteInt32(BoolValue(renderer, "ZTest", true) ? 1 : 0);
    writer.WriteInt32(BoolValue(renderer, "ZWrite", false) ? 1 : 0);

    const int32_t fade_in_type = IntValue(renderer, "FadeInType", 0);
    writer.WriteInt32(fade_in_type);

    if (fade_in_type == 1) {
        writer.WriteFloat(FloatValue(renderer, "FadeIn/Frame", 1.0f));
        WriteLegacyEasing(writer, IntValue(renderer, "FadeIn/StartSpeed", 0), IntValue(renderer, "FadeIn/EndSpeed", 0));
    }

    const int32_t fade_out_type = IntValue(renderer, "FadeOutType", 0);
    writer.WriteInt32(fade_out_type);

    if (fade_out_type == 1 || fade_out_type == 2) {
        writer.WriteFloat(FloatValue(renderer, "FadeOut/Frame", 1.0f));
        WriteLegacyEasing(writer, IntValue(renderer, "FadeOut/StartSpeed", 0), IntValue(renderer, "FadeOut/EndSpeed", 0));
    }

    WriteBasicUv(writer, renderer, context, color_path);

    // No advanced renderer values are emitted by the fixed 1.80.5 source profile. Their
    // runtime representation is still present and consists of default UV commands.
    writer.WriteInt32(0);
    writer.WriteInt32(0);
    writer.WriteFloat(0.0f);
    writer.WriteInt32(0);
    writer.WriteInt32(-1);
    writer.WriteInt32(0);
    writer.WriteInt32(0);
    writer.WriteFloat(0.0f);
    writer.WriteInt32(IntValue(renderer, "UVFlipHorizontalProbability", 0));
    writer.WriteInt32(IntValue(renderer, "ColorInheritType", 0));
    writer.WriteFloat(FloatValue(renderer, "DistortionIntensity", 1.0f));
    writer.WriteInt32(0);
    writer.WriteInt32(0);
    writer.WriteInt32(0);
    writer.WriteInt32(0);
    writer.WriteFloat(0.0f);
    writer.WriteFloat(0.0f);
    writer.WriteFloat(0.0f);
}

[[nodiscard]] static auto ByteValue(nptr<const XmlNode> node, string_view path, uint8_t default_value) -> uint8_t
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32_t value = IntValue(node, path, default_value);

    if (value < 0 || value > 255) {
        throw EffekseerCompilerException("Effekseer color channel is outside byte range", path, value);
    }

    return numeric_cast<uint8_t>(value);
}

[[nodiscard]] static auto HsvToRgb(uint8_t hue, uint8_t saturation, uint8_t value) -> std::array<uint8_t, 3>
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32_t h = hue;
    const int32_t s = saturation;
    const int32_t v = value;
    const int32_t section = h / 42 % 6;
    const int32_t fraction = h % 42 * 6;
    const int32_t p = std::clamp((v * (256 - s)) >> 8, 0, 255);
    const int32_t q = std::clamp((v * (256 - ((s * fraction) >> 8))) >> 8, 0, 255);
    const int32_t t = std::clamp((v * (256 - ((s * (252 - fraction)) >> 8))) >> 8, 0, 255);

    switch (section) {
    case 0:
        return {numeric_cast<uint8_t>(v), numeric_cast<uint8_t>(t), numeric_cast<uint8_t>(p)};
    case 1:
        return {numeric_cast<uint8_t>(q), numeric_cast<uint8_t>(v), numeric_cast<uint8_t>(p)};
    case 2:
        return {numeric_cast<uint8_t>(p), numeric_cast<uint8_t>(v), numeric_cast<uint8_t>(t)};
    case 3:
        return {numeric_cast<uint8_t>(p), numeric_cast<uint8_t>(q), numeric_cast<uint8_t>(v)};
    case 4:
        return {numeric_cast<uint8_t>(t), numeric_cast<uint8_t>(p), numeric_cast<uint8_t>(v)};
    default:
        return {numeric_cast<uint8_t>(v), numeric_cast<uint8_t>(p), numeric_cast<uint8_t>(q)};
    }
}

static void WriteColor(BinaryWriter& writer, nptr<const XmlNode> node, uint8_t default_r, uint8_t default_g, uint8_t default_b, uint8_t default_a)
{
    FO_NO_STACK_TRACE_ENTRY();

    uint8_t r = ByteValue(node, "R", default_r);
    uint8_t g = ByteValue(node, "G", default_g);
    uint8_t b = ByteValue(node, "B", default_b);
    const uint8_t a = ByteValue(node, "A", default_a);

    if (IntValue(node, "ColorSpace", 0) == 1) {
        const auto rgb = HsvToRgb(r, g, b);
        r = rgb[0];
        g = rgb[1];
        b = rgb[2];
    }

    writer.WriteUInt8(r);
    writer.WriteUInt8(g);
    writer.WriteUInt8(b);
    writer.WriteUInt8(a);
}

static void WriteRandomColor(BinaryWriter& writer, nptr<const XmlNode> node, uint8_t default_r, uint8_t default_g, uint8_t default_b, uint8_t default_a)
{
    FO_NO_STACK_TRACE_ENTRY();

    writer.WriteUInt8(numeric_cast<uint8_t>(IntValue(node, "ColorSpace", 0)));
    writer.WriteUInt8(0);

    for (const string_view channel : {string_view {"R"}, string_view {"G"}, string_view {"B"}, string_view {"A"}}) {
        const uint8_t default_value = channel == "R" ? default_r : channel == "G" ? default_g : channel == "B" ? default_b : default_a;
        writer.WriteUInt8(ByteValue(Child(node, channel), "Max", default_value));
    }
    for (const string_view channel : {string_view {"R"}, string_view {"G"}, string_view {"B"}, string_view {"A"}}) {
        const uint8_t default_value = channel == "R" ? default_r : channel == "G" ? default_g : channel == "B" ? default_b : default_a;
        writer.WriteUInt8(ByteValue(Child(node, channel), "Min", default_value));
    }
}

static void WriteColorEasing(BinaryWriter& writer, nptr<const XmlNode> node, uint8_t default_r = 255, uint8_t default_g = 255, uint8_t default_b = 255, uint8_t default_a = 255)
{
    FO_NO_STACK_TRACE_ENTRY();

    WriteRandomColor(writer, Find(node, "Start"), default_r, default_g, default_b, default_a);
    WriteRandomColor(writer, Find(node, "End"), default_r, default_g, default_b, default_a);
    WriteLegacyEasing(writer, IntValue(node, "StartSpeed", 0), IntValue(node, "EndSpeed", 0));
}

static void WriteGradient(BinaryWriter& writer, nptr<const XmlNode> node)
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> colors = Find(node, "ColorMarkers");
    nptr<const XmlNode> alphas = Find(node, "AlphaMarkers");
    size_t color_count = 2;

    if (colors) {
        color_count = colors->Children.size();
    }

    writer.WriteInt32(numeric_cast<int32_t>(color_count));

    if (colors) {
        for (const XmlNode& marker : colors->Children) {
            writer.WriteFloat(FloatValue(&marker, "Position", 0.0f));
            writer.WriteFloat(FloatValue(&marker, "ColorR", 1.0f));
            writer.WriteFloat(FloatValue(&marker, "ColorG", 1.0f));
            writer.WriteFloat(FloatValue(&marker, "ColorB", 1.0f));
            writer.WriteFloat(FloatValue(&marker, "Intensity", 1.0f));
        }
    }
    else {
        for (const float32_t position : {0.0f, 1.0f}) {
            writer.WriteFloat(position);
            writer.WriteFloat(1.0f);
            writer.WriteFloat(1.0f);
            writer.WriteFloat(1.0f);
            writer.WriteFloat(1.0f);
        }
    }

    size_t alpha_count = 2;

    if (alphas) {
        alpha_count = alphas->Children.size();
    }

    writer.WriteInt32(numeric_cast<int32_t>(alpha_count));

    if (alphas) {
        for (const XmlNode& marker : alphas->Children) {
            writer.WriteFloat(FloatValue(&marker, "Position", 0.0f));
            writer.WriteFloat(FloatValue(&marker, "Alpha", 1.0f));
        }
    }
    else {
        writer.WriteFloat(0.0f);
        writer.WriteFloat(1.0f);
        writer.WriteFloat(1.0f);
        writer.WriteFloat(1.0f);
    }
}

static void WriteStandardColor(BinaryWriter& writer, nptr<const XmlNode> node, uint8_t default_r = 255, uint8_t default_g = 255, uint8_t default_b = 255, uint8_t default_a = 255)
{
    FO_STACK_TRACE_ENTRY();

    const int32_t type = IntValue(node, "Type", 0);
    writer.WriteInt32(type);

    if (type == 0) {
        WriteColor(writer, Find(node, "Fixed"), default_r, default_g, default_b, default_a);
    }
    else if (type == 1) {
        WriteRandomColor(writer, Find(node, "Random"), default_r, default_g, default_b, default_a);
    }
    else if (type == 2) {
        WriteColorEasing(writer, Find(node, "Easing"), default_r, default_g, default_b, default_a);
    }
    else if (type == 3) {
        nptr<const XmlNode> curve = Find(node, "FCurve/FCurve");
        writer.WriteInt32(IntValue(curve, "Timeline", 0));
        nptr<const XmlNode> keys = Find(curve, "Keys");
        WriteCurveChannel(writer, Child(keys, "R"), numeric_cast<float32_t>(default_r), 1.0f);
        WriteCurveChannel(writer, Child(keys, "G"), numeric_cast<float32_t>(default_g), 1.0f);
        WriteCurveChannel(writer, Child(keys, "B"), numeric_cast<float32_t>(default_b), 1.0f);
        WriteCurveChannel(writer, Child(keys, "A"), numeric_cast<float32_t>(default_a), 1.0f);
    }
    else if (type == 4) {
        WriteGradient(writer, Find(node, "Gradient"));
    }
    else {
        throw EffekseerCompilerException("Effekseer project uses an unsupported standard-color type", type);
    }
}

static void WriteTextureUvType(BinaryWriter& writer, nptr<const XmlNode> drawing)
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> uv = Find(drawing, "TextureUVType");
    const int32_t type = IntValue(uv, "Type", 0);
    writer.WriteInt32(type);

    if (type == 2) {
        writer.WriteFloat(FloatValue(uv, "TileLength", 1.0f));
    }
    else if (type == 1) {
        writer.WriteInt32(IntValue(uv, "TileEdgeHead", 0));
        writer.WriteInt32(IntValue(uv, "TileEdgeTail", 0));
        writer.WriteFloat(FloatValue(uv, "TileLoopingArea/X", 0.0f));
        writer.WriteFloat(FloatValue(uv, "TileLoopingArea/Y", 1.0f));
    }
}

static void WriteSpriteRenderer(BinaryWriter& writer, nptr<const XmlNode> drawing)
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> sprite = Find(drawing, "Sprite");
    writer.WriteInt32(IntValue(sprite, "RenderingOrder", 0));
    writer.WriteInt32(IntValue(sprite, "Billboard", 0));
    WriteStandardColor(writer, Find(drawing, "ColorAll"));

    const int32_t color_type = IntValue(sprite, "Color", 0);
    writer.WriteInt32(color_type);

    if (color_type == 1) {
        WriteColor(writer, Find(sprite, "Color_Fixed_LL"), 255, 255, 255, 255);
        WriteColor(writer, Find(sprite, "Color_Fixed_LR"), 255, 255, 255, 255);
        WriteColor(writer, Find(sprite, "Color_Fixed_UL"), 255, 255, 255, 255);
        WriteColor(writer, Find(sprite, "Color_Fixed_UR"), 255, 255, 255, 255);
    }

    writer.WriteInt32(1);

    if (IntValue(sprite, "Position", 0) == 0) {
        for (const float32_t value : {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f}) {
            writer.WriteFloat(value);
        }
    }
    else {
        WriteVector2(writer, Find(sprite, "Position_Fixed_LL"), -0.5f, -0.5f);
        WriteVector2(writer, Find(sprite, "Position_Fixed_LR"), 0.5f, -0.5f);
        WriteVector2(writer, Find(sprite, "Position_Fixed_UL"), -0.5f, 0.5f);
        WriteVector2(writer, Find(sprite, "Position_Fixed_UR"), 0.5f, 0.5f);
    }
}

static void WriteRibbonRenderer(BinaryWriter& writer, nptr<const XmlNode> drawing)
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> ribbon = Find(drawing, "Ribbon");
    WriteTextureUvType(writer, drawing);
    writer.WriteInt32(IntValue(drawing, "TrailTimeSource", 1));
    writer.WriteInt32(BoolValue(ribbon, "ViewpointDependent", false) ? 1 : 0);

    const int32_t color_all = IntValue(ribbon, "ColorAll", 0);
    writer.WriteInt32(color_all);

    if (color_all == 0) {
        WriteColor(writer, Find(ribbon, "ColorAll_Fixed"), 255, 255, 255, 255);
    }
    else if (color_all == 1) {
        WriteRandomColor(writer, Find(ribbon, "ColorAll_Random"), 255, 255, 255, 255);
    }
    else if (color_all == 2) {
        WriteColorEasing(writer, Find(ribbon, "ColorAll_Easing"));
    }

    const int32_t color = IntValue(ribbon, "Color", 0);
    writer.WriteInt32(color);

    if (color == 1) {
        WriteColor(writer, Find(ribbon, "Color_Fixed_L"), 255, 255, 255, 255);
        WriteColor(writer, Find(ribbon, "Color_Fixed_R"), 255, 255, 255, 255);
    }

    writer.WriteInt32(1);

    if (IntValue(ribbon, "Position", 0) == 0) {
        writer.WriteFloat(-0.5f);
        writer.WriteFloat(0.5f);
    }
    else {
        writer.WriteFloat(FloatValue(ribbon, "Position_Fixed_L", -0.5f));
        writer.WriteFloat(FloatValue(ribbon, "Position_Fixed_R", 0.5f));
    }

    writer.WriteInt32(IntValue(ribbon, "SplineDivision", 1));
}

static void WriteRingShape(BinaryWriter& writer, nptr<const XmlNode> ring)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> shape = Find(ring, "RingShape");
    const int32_t type = IntValue(shape, "Type", 0);
    writer.WriteInt32(type);

    if (type != 1) {
        return;
    }

    nptr<const XmlNode> crescent = Find(shape, "Crescent");
    writer.WriteFloat(FloatValue(crescent, "StartingFade", 0.0f));
    writer.WriteFloat(FloatValue(crescent, "EndingFade", 0.0f));
    const int32_t starting_type = IntValue(crescent, "StartingAngle", 0);
    writer.WriteInt32(starting_type);

    if (starting_type == 0) {
        writer.WriteFloat(FloatValue(crescent, "StartingAngle_Fixed", 0.0f));
    }
    else if (starting_type == 1) {
        WriteRandomFloat(writer, Find(crescent, "StartingAngle_Random"), 0.0f);
    }
    else if (starting_type == 2) {
        WriteFloatEasing(writer, Find(crescent, "StartingAngle_Easing"), 0.0f, 1.0f, true);
    }

    const int32_t ending_type = IntValue(crescent, "EndingAngle", 0);
    writer.WriteInt32(ending_type);

    if (ending_type == 0) {
        writer.WriteFloat(FloatValue(crescent, "EndingAngle_Fixed", 360.0f));
    }
    else if (ending_type == 1) {
        WriteRandomFloat(writer, Find(crescent, "EndingAngle_Random"), 360.0f);
    }
    else if (ending_type == 2) {
        WriteFloatEasing(writer, Find(crescent, "EndingAngle_Easing"), 360.0f, 1.0f, true);
    }
}

static void WriteLegacyFloatEasing(BinaryWriter& writer, nptr<const XmlNode> node, float32_t default_value)
{
    FO_NO_STACK_TRACE_ENTRY();

    writer.WriteFloat(FloatValue(node, "Start/Max", default_value));
    writer.WriteFloat(FloatValue(node, "Start/Min", default_value));
    writer.WriteFloat(FloatValue(node, "End/Max", default_value));
    writer.WriteFloat(FloatValue(node, "End/Min", default_value));
    WriteLegacyEasing(writer, IntValue(node, "StartSpeed", 0), IntValue(node, "EndSpeed", 0));
}

static void WriteRingLocation(BinaryWriter& writer, nptr<const XmlNode> ring, string_view name, float32_t default_x)
{
    FO_STACK_TRACE_ENTRY();

    const int32_t type = IntValue(ring, name, 0);
    writer.WriteInt32(type);

    if (type == 0) {
        const string fixed_path = strex("{}_Fixed/Location", name);
        WriteVector2(writer, Find(ring, fixed_path), default_x, 0.0f);
    }
    else if (type == 1) {
        const string pva_path = strex("{}_PVA", name);
        nptr<const XmlNode> pva = Find(ring, pva_path);
        WriteRandomVector2(writer, Find(pva, "Location"), default_x, 0.0f);
        WriteRandomVector2(writer, Find(pva, "Velocity"));
        WriteRandomVector2(writer, Find(pva, "Acceleration"));
    }
    else if (type == 2) {
        const string easing_path = strex("{}_Easing", name);
        nptr<const XmlNode> easing = Find(ring, easing_path);
        WriteRandomVector2(writer, Find(easing, "Start"));
        WriteRandomVector2(writer, Find(easing, "End"));
        WriteLegacyEasing(writer, IntValue(easing, "StartSpeed", 0), IntValue(easing, "EndSpeed", 0));
    }
}

static void WriteRingColor(BinaryWriter& writer, nptr<const XmlNode> ring, string_view name, uint8_t default_alpha)
{
    FO_STACK_TRACE_ENTRY();

    const int32_t type = IntValue(ring, name, 0);
    writer.WriteInt32(type);

    if (type == 0) {
        WriteColor(writer, Find(ring, strex("{}_Fixed", name)), 255, 255, 255, default_alpha);
    }
    else if (type == 1) {
        WriteRandomColor(writer, Find(ring, strex("{}_Random", name)), 255, 255, 255, default_alpha);
    }
    else if (type == 2) {
        WriteColorEasing(writer, Find(ring, strex("{}_Easing", name)), 255, 255, 255, default_alpha);
    }
}

static void WriteRingRenderer(BinaryWriter& writer, nptr<const XmlNode> drawing)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> ring = Find(drawing, "Ring");
    writer.WriteInt32(IntValue(ring, "RenderingOrder", 0));
    writer.WriteInt32(IntValue(ring, "Billboard", 2));
    WriteRingShape(writer, ring);
    writer.WriteInt32(IntValue(ring, "VertexCount", 16));

    const int32_t viewing_type = IntValue(ring, "ViewingAngle", 0);
    writer.WriteInt32(viewing_type);

    if (viewing_type == 0) {
        writer.WriteFloat(FloatValue(ring, "ViewingAngle_Fixed", 360.0f));
    }
    else if (viewing_type == 1) {
        WriteRandomFloat(writer, Find(ring, "ViewingAngle_Random"), 360.0f);
    }
    else if (viewing_type == 2) {
        WriteLegacyFloatEasing(writer, Find(ring, "ViewingAngle_Easing"), 360.0f);
    }

    WriteRingLocation(writer, ring, "Outer", 2.0f);
    WriteRingLocation(writer, ring, "Inner", 1.0f);

    const int32_t center_type = IntValue(ring, "CenterRatio", 0);
    writer.WriteInt32(center_type);

    if (center_type == 0) {
        writer.WriteFloat(FloatValue(ring, "CenterRatio_Fixed", 0.5f));
    }
    else if (center_type == 1) {
        WriteRandomFloat(writer, Find(ring, "CenterRatio_Random"), 0.5f);
    }
    else if (center_type == 2) {
        WriteFloatEasing(writer, Find(ring, "CenterRatio_Easing"), 0.5f, 1.0f, true);
    }

    WriteRingColor(writer, ring, "OuterColor", 0);
    WriteRingColor(writer, ring, "CenterColor", 255);
    WriteRingColor(writer, ring, "InnerColor", 0);
}

static void WriteModelRenderer(BinaryWriter& writer, nptr<const XmlNode> drawing, const CompilerContext& context)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> model = Find(drawing, "Model");
    const int32_t reference_type = IntValue(model, "ModelReference", 0);
    writer.WriteInt32(reference_type);

    if (reference_type == 0) {
        writer.WriteFloat(1.0f);
        const string path = ChangeDependencyExtension(Text(model, "Model"), "efkmodel");
        writer.WriteInt32(!path.empty() && context.Models.contains(path) ? context.Models.at(path) : -1);
    }
    else if (reference_type == 1) {
        throw EffekseerCompilerException("Effekseer procedural models are not supported by the fixed project profile");
    }
    else {
        writer.WriteInt32(IntValue(model, "ExternalModelIndex", 0));
    }

    writer.WriteInt32(IntValue(model, "Billboard", 2));
    writer.WriteInt32(IntValue(model, "Culling", 0));
    WriteStandardColor(writer, Find(drawing, "ColorAll"));
}

static void WriteTrackRenderer(BinaryWriter& writer, nptr<const XmlNode> drawing)
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> track = Find(drawing, "Track");
    WriteTextureUvType(writer, drawing);
    writer.WriteInt32(IntValue(track, "TrackSizeFor", 0));
    writer.WriteFloat(FloatValue(track, "TrackSizeFor_Fixed", 1.0f));
    writer.WriteInt32(IntValue(track, "TrackSizeMiddle", 0));
    writer.WriteFloat(FloatValue(track, "TrackSizeMiddle_Fixed", 1.0f));
    writer.WriteInt32(IntValue(track, "TrackSizeBack", 0));
    writer.WriteFloat(FloatValue(track, "TrackSizeBack_Fixed", 1.0f));
    writer.WriteInt32(IntValue(track, "SplineDivision", 1));
    writer.WriteInt32(IntValue(drawing, "TrailSmoothing", 1));
    writer.WriteInt32(IntValue(drawing, "TrailTimeSource", 1));

    for (const string_view color : {string_view {"TrailColorLeft"}, string_view {"TrailColorLeftMiddle"}, string_view {"TrailColorCenter"}, string_view {"TrailColorCenterMiddle"}, string_view {"TrailColorRight"}, string_view {"TrailColorRightMiddle"}}) {
        WriteStandardColor(writer, Find(drawing, color));
    }
}

static void WriteRendererValues(BinaryWriter& writer, nptr<const XmlNode> node, const CompilerContext& context, bool exported)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> drawing = Find(node, "DrawingValues");
    const int32_t type = exported ? NodeDrawingType(node) : 0;
    writer.WriteInt32(type);

    switch (type) {
    case 0:
        break;
    case 2:
        WriteSpriteRenderer(writer, drawing);
        break;
    case 3:
        WriteRibbonRenderer(writer, drawing);
        break;
    case 4:
        WriteRingRenderer(writer, drawing);
        break;
    case 5:
        WriteModelRenderer(writer, drawing, context);
        break;
    case 6:
        WriteTrackRenderer(writer, drawing);
        break;
    default:
        throw EffekseerCompilerException("Effekseer project uses an unsupported renderer type", type);
    }
}

static void WriteSoundValues(BinaryWriter& writer, nptr<const XmlNode> node, const CompilerContext& context)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> values = Find(node, "SoundValues");
    const int32_t type = IntValue(values, "Type", 0);
    writer.WriteInt32(type);

    if (type != 1) {
        return;
    }

    nptr<const XmlNode> sound = Find(values, "Sound");
    const string wave {Text(sound, "Wave")};
    writer.WriteInt32(!wave.empty() && context.Waves.contains(wave) ? context.Waves.at(wave) : -1);
    WriteRandomFloat(writer, Find(sound, "Volume"), 1.0f);
    WriteRandomFloat(writer, Find(sound, "Pitch"), 0.0f);
    writer.WriteInt32(IntValue(sound, "PanType", 0));
    WriteRandomFloat(writer, Find(sound, "Pan"), 0.0f);
    writer.WriteFloat(FloatValue(sound, "Distance", 10.0f));
    WriteRandomInt(writer, Find(sound, "Delay"), 0);
}

static void WriteDepthValues(BinaryWriter& writer, nptr<const XmlNode> node)
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> depth = Find(node, "DepthValues");
    writer.WriteFloat(FloatValue(depth, "DepthOffset", 0.0f));
    writer.WriteInt32(BoolValue(depth, "IsScaleChangedDependingOnDepthOffset", false) ? 1 : 0);
    writer.WriteInt32(BoolValue(depth, "IsDepthOffsetChangedDependingOnParticleScale", false) ? 1 : 0);
    writer.WriteFloat(FloatValue(depth, "SuppressionOfScalingByDepth", 1.0f));
    writer.WriteFloat(BoolValue(depth, "DepthClipping/Infinite", true) ? std::numeric_limits<float32_t>::max() : numeric_cast<float32_t>(IntValue(depth, "DepthClipping/Value", 1024)));
    writer.WriteInt32(IntValue(depth, "ZSort", 0));
    writer.WriteInt32(IntValue(depth, "DrawingPriority", 0));
    writer.WriteFloat(1.0f);
}

static void WriteKillRules(BinaryWriter& writer, nptr<const XmlNode> node)
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> rules = Find(node, "KillRulesValues");
    const int32_t type = IntValue(rules, "Type", 0);
    writer.WriteInt32(type);
    writer.WriteInt32(BoolValue(rules, "IsScaleAndRotationApplied", true) ? 1 : 0);

    if (type == 1) {
        WriteVector3(writer, Find(rules, "BoxCenter"));
        WriteVector3(writer, Find(rules, "BoxSize"), 0.5f, 0.5f, 0.5f);
        writer.WriteInt32(BoolValue(rules, "BoxIsKillInside", false) ? 1 : 0);
    }
    else if (type == 2) {
        constexpr std::array<std::array<float32_t, 3>, 6> normals {{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
        const int32_t axis = IntValue(rules, "PlaneAxis", 2);

        if (axis < 0 || axis >= numeric_cast<int32_t>(normals.size())) {
            throw EffekseerCompilerException("Effekseer kill-plane axis is invalid", axis);
        }

        for (const float32_t value : normals[numeric_cast<size_t>(axis)]) {
            writer.WriteFloat(value);
        }

        writer.WriteFloat(FloatValue(rules, "PlaneOffset", 1.0f));
    }
    else if (type == 3) {
        WriteVector3(writer, Find(rules, "SphereCenter"));
        writer.WriteFloat(FloatValue(rules, "SphereRadius", 1.0f));
        writer.WriteInt32(BoolValue(rules, "SphereIsKillInside", false) ? 1 : 0);
    }
}

static void WriteCollisions(BinaryWriter& writer, nptr<const XmlNode> node)
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> values = Find(node, "CollisionsValues");
    writer.WriteInt32(BoolValue(values, "IsGroundCollisionEnabled", false) ? 1 : 0);
    writer.WriteInt32(BoolValue(values, "IsSceneCollisionEnabled", false) ? 1 : 0);
    WriteRandomFloat(writer, Find(values, "Bounce"), 1.0f);
    writer.WriteFloat(FloatValue(values, "Height", 0.0f));
    WriteRandomFloat(writer, Find(values, "Friction"), 0.0f);
    WriteRandomFloat(writer, Find(values, "LifetimeReductionPerCollision"), 0.0f);
    writer.WriteInt32(IntValue(values, "WorldCoordinateSyatem", 0));
}

[[nodiscard]] static auto HasChildColorInheritance(nptr<const XmlNode> node) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<const XmlNode> children = Child(node, "Children");

    if (!children) {
        return false;
    }

    return std::ranges::any_of(children->Children, [](const XmlNode& child) {
        if (child.Name != "Node") {
            return false;
        }

        const int32_t inheritance = IntValue(Find(&child, "RendererCommonValues"), "ColorInheritType", 0);
        return inheritance == 1 || inheritance == 2;
    });
}

[[nodiscard]] static auto OutputNodeType(nptr<const XmlNode> node, bool renderer_exported) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!renderer_exported || NodeDrawingType(node) == 0) {
        return 0;
    }

    return NodeDrawingType(node);
}

static void WriteNode(BinaryWriter& writer, nptr<const XmlNode> node, const CompilerContext& context)
{
    FO_STACK_TRACE_ENTRY();

    const bool renderer_exported = NodeIsRendered(node) || HasChildColorInheritance(node);
    writer.WriteInt32(OutputNodeType(node, renderer_exported));
    BinaryWriter data;
    data.WriteInt32(NodeIsRendered(node) ? 1 : 0);
    const auto render_index = context.RenderIndices.find(node);
    data.WriteInt32(render_index != context.RenderIndices.end() ? render_index->second : -1);
    WriteCommonValues(data, node);
    WriteLocationValues(data, node, context);
    WriteLocationAbsValues(data, node);
    WriteRotationValues(data, node);
    WriteScaleValues(data, node);
    WriteGenerationLocationValues(data, node, context);
    WriteDepthValues(data, node);
    WriteKillRules(data, node);
    WriteCollisions(data, node);
    WriteRendererCommonValues(data, node, context);
    WriteRendererValues(data, node, context, renderer_exported);
    writer.WriteBytes(data.GetData());
    WriteSoundValues(writer, node, context);
    writer.WriteInt32(0); // GPU particles are disabled in the fixed project profile.

    vector<nptr<const XmlNode>> children;
    nptr<const XmlNode> child_container = Child(node, "Children");

    if (child_container) {
        for (const XmlNode& child : child_container->Children) {
            if (child.Name == "Node" && HasRenderedNode(&child)) {
                children.emplace_back(&child);
            }
        }
    }

    writer.WriteInt32(numeric_cast<int32_t>(children.size()));

    for (nptr<const XmlNode> child : children) {
        WriteNode(writer, child, context);
    }
}

static void BuildRenderIndices(CompilerContext& context)
{
    FO_STACK_TRACE_ENTRY();

    vector<std::pair<nptr<const XmlNode>, size_t>> sorted;
    sorted.reserve(context.ExportedNodes.size());

    for (size_t index = 0; index < context.ExportedNodes.size(); ++index) {
        sorted.emplace_back(context.ExportedNodes[index], index);
    }

    std::ranges::stable_sort(sorted, [](const auto& left, const auto& right) {
        const int32_t left_priority = IntValue(Find(left.first, "DepthValues"), "DrawingPriority", 0);
        const int32_t right_priority = IntValue(Find(right.first, "DepthValues"), "DrawingPriority", 0);
        return left_priority != right_priority ? left_priority < right_priority : left.second < right.second;
    });

    for (size_t index = 0; index < sorted.size(); ++index) {
        context.RenderIndices.emplace(sorted[index].first, numeric_cast<int32_t>(index));
    }
}

static void ValidateSupportedFeatures(const CompilerContext& context, string_view project_path)
{
    FO_STACK_TRACE_ENTRY();

    for (nptr<const XmlNode> node : context.ExportedNodes) {
        if (BoolValue(Find(node, "GpuParticles"), "Enabled", false)) {
            throw EffekseerCompilerException("Effekseer GPU particles are not supported by the fixed project profile", project_path, Text(node, "Name"));
        }
        if (Find(node, "AdvancedRendererCommonValuesValues")) {
            throw EffekseerCompilerException("Effekseer advanced renderer values are not supported by the fixed project profile", project_path, Text(node, "Name"));
        }

        nptr<const XmlNode> renderer = Find(node, "RendererCommonValues");

        if (IntValue(renderer, "CustomData1/CustomData", 0) != 0 || IntValue(renderer, "CustomData2/CustomData", 0) != 0) {
            throw EffekseerCompilerException("Effekseer custom renderer data is not supported by the fixed project profile", project_path, Text(node, "Name"));
        }
    }
}

[[nodiscard]] static auto CreateCompilerContext(string_view project_path, const XmlNode& project) -> CompilerContext
{
    FO_STACK_TRACE_ENTRY();

    if (Text(&project, "ToolVersion") != "1.80.5" || IntValue(&project, "Version", -1) != 3) {
        throw EffekseerCompilerException("Effekseer project must be normalized with Editor 1.80.5 and project version 3", project_path, Text(&project, "ToolVersion"), Text(&project, "Version"));
    }

    nptr<const XmlNode> equations = Find(&project, "Dynamic/Equations");

    if (equations && !equations->Children.empty()) {
        throw EffekseerCompilerException("Effekseer dynamic equations are not supported by the fixed project profile", project_path);
    }

    nptr<const XmlNode> procedural_models = Find(&project, "ProceduralModel/ProceduralModels");

    if (procedural_models && !procedural_models->Children.empty()) {
        throw EffekseerCompilerException("Effekseer procedural models are not supported by the fixed project profile", project_path);
    }

    CompilerContext context;
    context.ProjectDirectory = fs_path_to_string(std::filesystem::path {fs_make_path(project_path)}.parent_path());
    nptr<const XmlNode> root = Find(&project, "Root");

    if (!root) {
        throw EffekseerCompilerException("Effekseer project has no Root node", project_path);
    }

    CollectExportedNodes(root, context.ExportedNodes);
    ValidateSupportedFeatures(context, project_path);
    CollectResources(context);
    BuildRenderIndices(context);
    return context;
}

[[nodiscard]] static auto CompileProject(string_view project_path, const XmlNode& project) -> EffekseerCompilerOutput
{
    FO_STACK_TRACE_ENTRY();

    CompilerContext context = CreateCompilerContext(project_path, project);
    nptr<const XmlNode> root = Find(&project, "Root");

    BinaryWriter writer;
    constexpr std::array<uint8_t, 4> magic {'S', 'K', 'F', 'E'};
    writer.WriteBytes(magic);
    writer.WriteInt32(EffekseerBinaryVersion);
    WriteResourceTable(writer, context.ColorTextures);
    WriteResourceTable(writer, context.NormalTextures);
    WriteResourceTable(writer, context.DistortionTextures);
    WriteResourceTable(writer, context.Waves);
    WriteResourceTable(writer, context.Models);
    writer.WriteInt32(0); // Material files are excluded by the fixed project profile.
    WriteResourceTable(writer, context.Curves);
    writer.WriteInt32(0); // Procedural models are excluded by the fixed project profile.

    nptr<const XmlNode> inputs = Find(&project, "Dynamic/Inputs");
    writer.WriteInt32(inputs ? numeric_cast<int32_t>(inputs->Children.size()) : 0);

    if (inputs) {
        for (const XmlNode& input : inputs->Children) {
            writer.WriteFloat(FloatValue(&input, "Input", 0.0f));
        }
    }

    writer.WriteInt32(0);
    writer.WriteInt32(numeric_cast<int32_t>(context.ExportedNodes.size()));
    const int32_t priority_threshold = numeric_cast<int32_t>(std::ranges::count_if(context.ExportedNodes, [](nptr<const XmlNode> node) { return IntValue(Find(node, "DepthValues"), "DrawingPriority", 0) < 0; }));
    writer.WriteInt32(priority_threshold);
    writer.WriteFloat(1.0f);
    writer.WriteInt32(IntValue(Find(&project, "Global"), "RandomSeed", -1));
    nptr<const XmlNode> culling = Find(&project, "Culling");
    const int32_t culling_type = IntValue(culling, "Type", 0);
    writer.WriteInt32(culling_type);

    if (culling_type == 1) {
        writer.WriteFloat(FloatValue(culling, "Sphere/Radius", 0.0f));
        WriteVector3(writer, Find(culling, "Sphere/Location"));
    }

    nptr<const XmlNode> lod = Find(&project, "LOD");
    int32_t enabled_lods = 1;

    if (BoolValue(lod, "Lod1Enabled", false)) {
        enabled_lods |= 1 << 1;

        if (BoolValue(lod, "Lod2Enabled", false)) {
            enabled_lods |= 1 << 2;

            if (BoolValue(lod, "Lod3Enabled", false)) {
                enabled_lods |= 1 << 3;
            }
        }
    }

    writer.WriteFloat((enabled_lods & (1 << 1)) != 0 ? FloatValue(lod, "Distance1", 0.0f) : 0.0f);
    writer.WriteFloat((enabled_lods & (1 << 2)) != 0 ? FloatValue(lod, "Distance2", 0.0f) : 0.0f);
    writer.WriteFloat((enabled_lods & (1 << 3)) != 0 ? FloatValue(lod, "Distance3", 0.0f) : 0.0f);

    writer.WriteInt32(-1);
    vector<nptr<const XmlNode>> root_children;
    nptr<const XmlNode> root_child_container = Child(root, "Children");

    if (root_child_container) {
        for (const XmlNode& child : root_child_container->Children) {
            if (child.Name == "Node" && HasRenderedNode(&child)) {
                root_children.emplace_back(&child);
            }
        }
    }

    writer.WriteInt32(numeric_cast<int32_t>(root_children.size()));

    for (nptr<const XmlNode> child : root_children) {
        WriteNode(writer, child, context);
    }

    EffekseerCompilerOutput output;
    output.Binary = writer.MoveData();
    output.Dependencies.assign(context.Dependencies.begin(), context.Dependencies.end());
    return output;
}

FO_END_NAMESPACE

#endif
