#include "GraphicLoader.h"
#include "3dAnimation.h"
#include "Crypt.h"
#include "GraphicStructures.h"
#include "Log.h"
#include "ResourceManager.h"
#include "SpriteManager.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"
#include "Version_Include.h"

GraphicLoader::GraphicLoader(SpriteManager& spr_mngr) : sprMngr(spr_mngr)
{
}

GraphicLoader::~GraphicLoader()
{
    for (auto* model : loadedModels)
        delete model;
    for (auto* anim : loadedAnimations)
        delete anim;
    for (auto* tex : loadedMeshTextures)
        delete tex;
    for (auto* effect : loadedEffects)
        delete effect;
}

AnyFrames* GraphicLoader::CreateAnyFrames(uint frames, uint ticks)
{
    AnyFrames* anim = (AnyFrames*)anyFramesPool.Get();
    memzero(anim, sizeof(AnyFrames));
    anim->CntFrm = MIN(frames, MAX_FRAMES);
    anim->Ticks = (ticks ? ticks : frames * 100);
    anim->HaveDirs = false;
    return anim;
}

void GraphicLoader::CreateAnyFramesDirAnims(AnyFrames* anim)
{
    anim->HaveDirs = true;
    for (int dir = 0; dir < DIRS_COUNT - 1; dir++)
        anim->Dirs[dir] = CreateAnyFrames(anim->CntFrm, anim->Ticks);
}

void GraphicLoader::DestroyAnyFrames(AnyFrames* anim)
{
    if (!anim || anim == DummyAnim)
        return;

    for (int dir = 1; dir < anim->DirCount(); dir++)
        anyFramesPool.Put(anim->GetDir(dir));
    anyFramesPool.Put(anim);
}

Bone* GraphicLoader::LoadModel(const string& fname)
{
    // Find already loaded
    for (size_t i = 0, j = loadedModelNames.size(); i < j; i++)
        if (_str(loadedModelNames[i]).compareIgnoreCase(fname))
            return loadedModels[i];

    // Add to already processed
    for (size_t i = 0, j = processedFiles.size(); i < j; i++)
        if (_str(processedFiles[i]).compareIgnoreCase(fname))
            return nullptr;
    processedFiles.push_back(fname);

    // Load file data
    File file;
    if (!file.LoadFile(fname))
    {
        WriteLog("3d file '{}' not found.\n", fname);
        return nullptr;
    }

    // Load bones
    Bone* root_bone = new Bone();
    root_bone->Load(file);
    root_bone->FixAfterLoad(root_bone);

    // Load animations
    uint anim_sets_count = file.GetBEUInt();
    for (uint i = 0; i < anim_sets_count; i++)
    {
        AnimSet* anim_set = new AnimSet();
        anim_set->Load(file);
        loadedAnimations.push_back(anim_set);
    }

    // Add to collection
    loadedModels.push_back(root_bone);
    loadedModelNames.push_back(fname);
    return root_bone;
}

void GraphicLoader::DestroyModel(Bone* root_bone)
{
    for (size_t i = 0, j = loadedModels.size(); i < j; i++)
    {
        if (loadedModels[i] == root_bone)
        {
            processedFiles.erase(std::find(processedFiles.begin(), processedFiles.end(), loadedModelNames[i]));
            loadedModels.erase(loadedModels.begin() + i);
            loadedModelNames.erase(loadedModelNames.begin() + i);
            break;
        }
    }
    delete root_bone;
}

AnimSet* GraphicLoader::LoadAnimation(const string& anim_fname, const string& anim_name)
{
    // Find in already loaded
    bool take_first = anim_name == "Base";
    for (size_t i = 0; i < loadedAnimations.size(); i++)
    {
        AnimSet* anim = loadedAnimations[i];
        if (_str(anim->GetFileName()).compareIgnoreCase(anim_fname) &&
            (take_first || _str(anim->GetName()).compareIgnoreCase(anim_name)))
            return anim;
    }

    // Check maybe file already processed and nothing founded
    for (size_t i = 0; i < processedFiles.size(); i++)
        if (_str(processedFiles[i]).compareIgnoreCase(anim_fname))
            return nullptr;

    // File not processed, load and recheck animations
    if (LoadModel(anim_fname))
        return LoadAnimation(anim_fname, anim_name);

    return nullptr;
}

MeshTexture* GraphicLoader::LoadTexture(const string& texture_name, const string& model_path)
{
    if (texture_name.empty())
        return nullptr;

    // Try find already loaded texture
    for (auto it = loadedMeshTextures.begin(), end = loadedMeshTextures.end(); it != end; ++it)
    {
        MeshTexture* texture = *it;
        if (_str(texture->Name).compareIgnoreCase(texture_name))
            return texture && texture->Id ? texture : nullptr;
    }

    // Allocate structure
    MeshTexture* mesh_tex = new MeshTexture();
    mesh_tex->Name = texture_name;
    mesh_tex->Id = 0;
    loadedMeshTextures.push_back(mesh_tex);

    // First try load from textures folder
    sprMngr.PushAtlasType(RES_ATLAS_TEXTURES);
    AnyFrames* anim = sprMngr.LoadAnimation(_str(model_path).extractDir() + texture_name);
    sprMngr.PopAtlasType();
    if (!anim)
        return nullptr;

    SpriteInfo* si = sprMngr.GetSpriteInfo(anim->Ind[0]);
    mesh_tex->Id = si->Atlas->TextureOwner->Id;
    memcpy(mesh_tex->SizeData, si->Atlas->TextureOwner->SizeData, sizeof(mesh_tex->SizeData));
    mesh_tex->AtlasOffsetData[0] = si->SprRect[0];
    mesh_tex->AtlasOffsetData[1] = si->SprRect[1];
    mesh_tex->AtlasOffsetData[2] = si->SprRect[2] - si->SprRect[0];
    mesh_tex->AtlasOffsetData[3] = si->SprRect[3] - si->SprRect[1];
    DestroyAnyFrames(anim);
    return mesh_tex;
}

void GraphicLoader::DestroyTextures()
{
    for (auto it = loadedMeshTextures.begin(), end = loadedMeshTextures.end(); it != end; ++it)
        delete *it;
    loadedMeshTextures.clear();
}

Effect* GraphicLoader::LoadEffect(const string& effect_name, bool use_in_2d, const string& defines /* = "" */,
    const string& model_path /* = "" */, EffectDefault* defaults /* = nullptr */, uint defaults_count /* = 0 */)
{
    // Erase extension
    string fname = _str(effect_name).eraseFileExtension();

    // Reset defaults to nullptr if it's count is zero
    if (defaults_count == 0)
        defaults = nullptr;

    // Try find already loaded effect
    string loaded_fname = fname;
    for (auto it = loadedEffects.begin(), end = loadedEffects.end(); it != end; ++it)
    {
        Effect* effect = *it;
        if (_str(effect->Name).compareIgnoreCase(loaded_fname) && effect->Defines == defines &&
            effect->Defaults == defaults)
            return effect;
    }

    // Add extension
    fname += ".glsl";

    // Load text file
    File file;
    string path = _str(model_path).extractDir() + fname;
    if (!file.LoadFile(path))
    {
        WriteLog("Effect file '{}' not found.\n", path);
        return nullptr;
    }

    // Parse effect commands
    vector<StrVec> commands;
    while (true)
    {
        string line = file.GetNonEmptyLine();
        if (line.empty())
            break;

        if (_str(line).startsWith("Effect "))
        {
            StrVec tokens = _str(line.substr(_str("Effect ").length())).split(' ');
            if (!tokens.empty())
                commands.push_back(tokens);
        }
        else if (_str(line).startsWith("#version"))
        {
            break;
        }
    }

    // Find passes count
    bool fail = false;
    uint passes = 1;
    for (size_t i = 0; i < commands.size(); i++)
        if (commands[i].size() >= 2 && commands[i][0] == "Passes")
            passes = ConvertParamValue(commands[i][1], fail);

    // New effect
    auto effect = std::make_unique<Effect>();
    effect->Name = loaded_fname;
    effect->Defines = defines;

    // Load passes
    for (uint pass = 0; pass < passes; pass++)
        if (!LoadEffectPass(effect.get(), fname, file, pass, use_in_2d, defines, defaults, defaults_count))
            return nullptr;

    // Process commands
    for (size_t i = 0; i < commands.size(); i++)
    {
        static auto get_gl_blend_func = [](const string& s) {
            if (s == "GL_ZERO")
                return GL_ZERO;
            if (s == "GL_ONE")
                return GL_ONE;
            if (s == "GL_SRC_COLOR")
                return GL_SRC_COLOR;
            if (s == "GL_ONE_MINUS_SRC_COLOR")
                return GL_ONE_MINUS_SRC_COLOR;
            if (s == "GL_DST_COLOR")
                return GL_DST_COLOR;
            if (s == "GL_ONE_MINUS_DST_COLOR")
                return GL_ONE_MINUS_DST_COLOR;
            if (s == "GL_SRC_ALPHA")
                return GL_SRC_ALPHA;
            if (s == "GL_ONE_MINUS_SRC_ALPHA")
                return GL_ONE_MINUS_SRC_ALPHA;
            if (s == "GL_DST_ALPHA")
                return GL_DST_ALPHA;
            if (s == "GL_ONE_MINUS_DST_ALPHA")
                return GL_ONE_MINUS_DST_ALPHA;
            if (s == "GL_CONSTANT_COLOR")
                return GL_CONSTANT_COLOR;
            if (s == "GL_ONE_MINUS_CONSTANT_COLOR")
                return GL_ONE_MINUS_CONSTANT_COLOR;
            if (s == "GL_CONSTANT_ALPHA")
                return GL_CONSTANT_ALPHA;
            if (s == "GL_ONE_MINUS_CONSTANT_ALPHA")
                return GL_ONE_MINUS_CONSTANT_ALPHA;
            if (s == "GL_SRC_ALPHA_SATURATE")
                return GL_SRC_ALPHA_SATURATE;
            return -1;
        };
        static auto get_gl_blend_equation = [](const string& s) {
            if (s == "GL_FUNC_ADD")
                return GL_FUNC_ADD;
            if (s == "GL_FUNC_SUBTRACT")
                return GL_FUNC_SUBTRACT;
            if (s == "GL_FUNC_REVERSE_SUBTRACT")
                return GL_FUNC_REVERSE_SUBTRACT;
            if (s == "GL_MAX")
                return GL_MAX;
            if (s == "GL_MIN")
                return GL_MIN;
            return -1;
        };

        StrVec& tokens = commands[i];
        if (tokens[0] == "Pass" && tokens.size() >= 3)
        {
            uint pass = ConvertParamValue(tokens[1], fail);
            if (pass < passes)
            {
                EffectPass& effect_pass = effect->Passes[pass];
                if (tokens[2] == "BlendFunc" && tokens.size() >= 5)
                {
                    effect_pass.IsNeedProcess = effect_pass.IsChangeStates = true;
                    effect_pass.BlendFuncParam1 = get_gl_blend_func(tokens[3]);
                    effect_pass.BlendFuncParam2 = get_gl_blend_func(tokens[4]);
                    if (effect_pass.BlendFuncParam1 == -1 || effect_pass.BlendFuncParam2 == -1)
                        fail = true;
                }
                else if (tokens[2] == "BlendEquation" && tokens.size() >= 4)
                {
                    effect_pass.IsNeedProcess = effect_pass.IsChangeStates = true;
                    effect_pass.BlendEquation = get_gl_blend_equation(tokens[3]);
                    if (effect_pass.BlendEquation == -1)
                        fail = true;
                }
                else if (tokens[2] == "Shadow")
                {
                    effect_pass.IsShadow = true;
                }
                else
                {
                    fail = true;
                }
            }
            else
            {
                fail = true;
            }
        }
    }
    if (fail)
    {
        WriteLog("Invalid commands in effect '{}'.\n", fname);
        return nullptr;
    }

    // Assign identifier and return
    effect->Id = ++effectId;
    loadedEffects.push_back(effect.release());
    return loadedEffects.back();
}

bool GraphicLoader::LoadEffectPass(Effect* effect, const string& fname, File& file, uint pass, bool use_in_2d,
    const string& defines, EffectDefault* defaults, uint defaults_count)
{
    EffectPass effect_pass;
    memzero(&effect_pass, sizeof(effect_pass));
    GLuint program = 0;

    // Make effect binary file name
    string binary_cache_name;
    if (GL_HAS(get_program_binary))
    {
        binary_cache_name = _str("Shader/{}", fname).eraseFileExtension();
        if (!defines.empty())
        {
            binary_cache_name += _str("_" + defines)
                                     .replace('\t', ' ')
                                     . // Tabs to spaces
                                 replace(' ', ' ', ' ')
                                     . // Multiple spaces to single
                                 replace('\r', '\n', '_')
                                     . // EOL's to '_'
                                 replace('\r', '_')
                                     . // EOL's to '_'
                                 replace('\n', '_'); // EOL's to '_'
        }
        binary_cache_name += _str("_{}_{}.glslb", sizeof(void*) * 8, pass);
    }

    // Load from binary
    File file_binary;
    if (GL_HAS(get_program_binary))
    {
        UCharVec data;
        if (Crypt.GetCache(binary_cache_name, data) && data.size() > sizeof(uint64))
        {
            uint64 write_time = *(uint64*)&data[data.size() - sizeof(uint64)];
            if (write_time >= file.GetWriteTime())
                file_binary.LoadStream(&data[0], (uint)data.size() - sizeof(uint64));
        }
    }
    if (file_binary.IsLoaded())
    {
        bool loaded = false;
        uint version = file_binary.GetBEUInt();
        if (version == (uint)FO_VERSION)
        {
            GLenum format = file_binary.GetBEUInt();
            UNUSED_VARIABLE(format); // OGL ES
            GLsizei length = file_binary.GetBEUInt();
            if (file_binary.GetFsize() >= length + sizeof(uint) * 3)
            {
                GL(program = glCreateProgram());
                glProgramBinary(program, format, file_binary.GetCurBuf(), length);
                glGetError(); // Skip error from glProgramBinary, if it has
                GLint linked;
                GL(glGetProgramiv(program, GL_LINK_STATUS, &linked));
                if (linked)
                {
                    loaded = true;
                }
                else
                {
                    WriteLog("Failed to link binary shader program '{}', effect '{}'.\n", binary_cache_name, fname);
                    GL(glDeleteProgram(program));
                }
            }
            else
            {
                WriteLog("Binary shader program '{}' truncated, effect '{}'.\n", binary_cache_name, fname);
            }
        }
        if (!loaded)
            file_binary.UnloadFile();
    }

    // Load from text
    if (!file_binary.IsLoaded())
    {
        // Get version
        string file_content = _str(file.GetCStr()).normalizeLineEndings();
        string version;
        size_t ver_begin = file_content.find("#version");
        if (ver_begin != string::npos)
        {
            size_t ver_end = file_content.find('\n', ver_begin);
            if (ver_end != string::npos)
            {
                version = file_content.substr(ver_begin, ver_end - ver_begin);
                file_content = file_content.substr(ver_end + 1);
            }
        }
#ifdef FO_OPENGL_ES
        version = "precision lowp float;\n";
#endif

        // Internal definitions
        string internal_defines = _str("#define PASS{}\n#define MAX_BONES {}\n", pass, MaxBones);

        // Create shaders
        GLuint vs, fs;
        GL(vs = glCreateShader(GL_VERTEX_SHADER));
        GL(fs = glCreateShader(GL_FRAGMENT_SHADER));
        string buf = _str(
            "{}{}{}{}{}{}{}", version, "\n", "#define VERTEX_SHADER\n", internal_defines, defines, "\n", file_content);
        const GLchar* vs_str = &buf[0];
        GL(glShaderSource(vs, 1, &vs_str, nullptr));
        buf = _str("{}{}{}{}{}{}{}", version, "\n", "#define FRAGMENT_SHADER\n", internal_defines, defines, "\n",
            file_content);
        const GLchar* fs_str = &buf[0];
        GL(glShaderSource(fs, 1, &fs_str, nullptr));

        // Info parser
        struct ShaderInfo
        {
            static void Log(const char* shader_name, GLint shader)
            {
                int len = 0;
                GL(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len));
                if (len > 0)
                {
                    GLchar* str = new GLchar[len];
                    int chars = 0;
                    glGetShaderInfoLog(shader, len, &chars, str);
                    WriteLog("{} output:\n{}", shader_name, str);
                    delete[] str;
                }
            }
            static void LogProgram(GLint program)
            {
                int len = 0;
                GL(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len));
                if (len > 0)
                {
                    GLchar* str = new GLchar[len];
                    int chars = 0;
                    glGetProgramInfoLog(program, len, &chars, str);
                    WriteLog("Program info output:\n{}", str);
                    delete[] str;
                }
            }
        };

        // Compile vs
        GLint compiled;
        GL(glCompileShader(vs));
        GL(glGetShaderiv(vs, GL_COMPILE_STATUS, &compiled));
        if (!compiled)
        {
            WriteLog("Vertex shader not compiled, effect '{}'.\n", fname);
            ShaderInfo::Log("Vertex shader", vs);
            GL(glDeleteShader(vs));
            GL(glDeleteShader(fs));
            return false;
        }

        // Compile fs
        GL(glCompileShader(fs));
        GL(glGetShaderiv(fs, GL_COMPILE_STATUS, &compiled));
        if (!compiled)
        {
            WriteLog("Fragment shader not compiled, effect '{}'.\n", fname);
            ShaderInfo::Log("Fragment shader", fs);
            GL(glDeleteShader(vs));
            GL(glDeleteShader(fs));
            return false;
        }

        // Make program
        GL(program = glCreateProgram());
        GL(glAttachShader(program, vs));
        GL(glAttachShader(program, fs));

        if (use_in_2d)
        {
            GL(glBindAttribLocation(program, 0, "InPosition"));
            GL(glBindAttribLocation(program, 1, "InColor"));
            GL(glBindAttribLocation(program, 2, "InTexCoord"));
            GL(glBindAttribLocation(program, 3, "InTexEggCoord"));
        }
        else
        {
            GL(glBindAttribLocation(program, 0, "InPosition"));
            GL(glBindAttribLocation(program, 1, "InNormal"));
            GL(glBindAttribLocation(program, 2, "InTexCoord"));
            GL(glBindAttribLocation(program, 3, "InTexCoordBase"));
            GL(glBindAttribLocation(program, 4, "InTangent"));
            GL(glBindAttribLocation(program, 5, "InBitangent"));
            GL(glBindAttribLocation(program, 6, "InBlendWeights"));
            GL(glBindAttribLocation(program, 7, "InBlendIndices"));
        }

        if (GL_HAS(get_program_binary))
            GL(glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE));

        GL(glLinkProgram(program));
        GLint linked;
        GL(glGetProgramiv(program, GL_LINK_STATUS, &linked));
        if (!linked)
        {
            WriteLog("Failed to link shader program, effect '{}'.\n", fname);
            ShaderInfo::LogProgram(program);
            GL(glDetachShader(program, vs));
            GL(glDetachShader(program, fs));
            GL(glDeleteShader(vs));
            GL(glDeleteShader(fs));
            GL(glDeleteProgram(program));
            return false;
        }

        // Save in binary
        if (GL_HAS(get_program_binary))
        {
            GLsizei buf_size;
            GL(glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &buf_size));
            GLsizei length = 0;
            GLenum format = 0;
            UCharVec buf;
            buf.resize(buf_size);
            GL(glGetProgramBinary(program, buf_size, &length, &format, &buf[0]));

            file_binary.SetBEUInt((uint)FO_VERSION);
            file_binary.SetBEUInt(format);
            file_binary.SetBEUInt(length);
            file_binary.SetData(&buf[0], length);

            uint64 write_time = file.GetWriteTime() + 1;
            file_binary.SetData(&write_time, sizeof(write_time));

            Crypt.SetCache(binary_cache_name, file_binary.GetOutBuf(), file_binary.GetOutBufLen());
        }
    }

    // Bind data
    GL(effect_pass.ProjectionMatrix = glGetUniformLocation(program, "ProjectionMatrix"));
    GL(effect_pass.ZoomFactor = glGetUniformLocation(program, "ZoomFactor"));
    GL(effect_pass.ColorMap = glGetUniformLocation(program, "ColorMap"));
    GL(effect_pass.ColorMapSize = glGetUniformLocation(program, "ColorMapSize"));
    GL(effect_pass.ColorMapSamples = glGetUniformLocation(program, "ColorMapSamples"));
    GL(effect_pass.EggMap = glGetUniformLocation(program, "EggMap"));
    GL(effect_pass.EggMapSize = glGetUniformLocation(program, "EggMapSize"));
    GL(effect_pass.SpriteBorder = glGetUniformLocation(program, "SpriteBorder"));
    GL(effect_pass.GroundPosition = glGetUniformLocation(program, "GroundPosition"));
    GL(effect_pass.LightColor = glGetUniformLocation(program, "LightColor"));
    GL(effect_pass.WorldMatrices = glGetUniformLocation(program, "WorldMatrices"));

    GL(effect_pass.Time = glGetUniformLocation(program, "Time"));
    effect_pass.TimeCurrent = 0.0f;
    effect_pass.TimeLastTick = Timer::AccurateTick();
    GL(effect_pass.TimeGame = glGetUniformLocation(program, "TimeGame"));
    effect_pass.TimeGameCurrent = 0.0f;
    effect_pass.TimeGameLastTick = Timer::AccurateTick();
    effect_pass.IsTime = (effect_pass.Time != -1 || effect_pass.TimeGame != -1);
    GL(effect_pass.Random1 = glGetUniformLocation(program, "Random1Effect"));
    GL(effect_pass.Random2 = glGetUniformLocation(program, "Random2Effect"));
    GL(effect_pass.Random3 = glGetUniformLocation(program, "Random3Effect"));
    GL(effect_pass.Random4 = glGetUniformLocation(program, "Random4Effect"));
    effect_pass.IsRandom = (effect_pass.Random1 != -1 || effect_pass.Random2 != -1 || effect_pass.Random3 != -1 ||
        effect_pass.Random4 != -1);
    effect_pass.IsTextures = false;
    for (int i = 0; i < EFFECT_TEXTURES; i++)
    {
        GL(effect_pass.Textures[i] = glGetUniformLocation(program, _str("Texture{}", i).c_str()));
        if (effect_pass.Textures[i] != -1)
        {
            effect_pass.IsTextures = true;
            GL(effect_pass.TexturesSize[i] = glGetUniformLocation(program, _str("Texture%dSize", i).c_str()));
            GL(effect_pass.TexturesAtlasOffset[i] =
                    glGetUniformLocation(program, _str("Texture{}AtlasOffset", i).c_str()));
        }
    }
    effect_pass.IsScriptValues = false;
    for (int i = 0; i < EFFECT_SCRIPT_VALUES; i++)
    {
        GL(effect_pass.ScriptValues[i] = glGetUniformLocation(program, _str("EffectValue{}", i).c_str()));
        if (effect_pass.ScriptValues[i] != -1)
            effect_pass.IsScriptValues = true;
    }
    GL(effect_pass.AnimPosProc = glGetUniformLocation(program, "AnimPosProc"));
    GL(effect_pass.AnimPosTime = glGetUniformLocation(program, "AnimPosTime"));
    effect_pass.IsAnimPos = (effect_pass.AnimPosProc != -1 || effect_pass.AnimPosTime != -1);
    effect_pass.IsNeedProcess = (effect_pass.IsTime || effect_pass.IsRandom || effect_pass.IsTextures ||
        effect_pass.IsScriptValues || effect_pass.IsAnimPos || effect_pass.IsChangeStates);

    // Set defaults
    if (defaults)
    {
        GL(glUseProgram(program));
        for (uint d = 0; d < defaults_count; d++)
        {
            EffectDefault& def = defaults[d];

            GLint location = -1;
            GL(location = glGetUniformLocation(program, def.Name));
            if (!IS_EFFECT_VALUE(location))
                continue;

            switch (def.Type)
            {
            case EffectDefault::String:
                break;
            case EffectDefault::Float:
                GL(glUniform1fv(location, def.Size / sizeof(float), (GLfloat*)def.Data));
                break;
            case EffectDefault::Int:
                GL(glUniform1iv(location, def.Size / sizeof(int), (GLint*)def.Data));
                break;
            default:
                break;
            }
        }
        GL(glUseProgram(0));
    }

    effect_pass.Program = program;
    effect->Passes.push_back(effect_pass);
    return true;
}

void GraphicLoader::EffectProcessVariables(EffectPass& effect_pass, bool start, float anim_proc /* = 0.0f */,
    float anim_time /* = 0.0f */, MeshTexture** textures /* = NULL */)
{
    // Process effect
    if (start)
    {
        if (effect_pass.IsTime)
        {
            double tick = Timer::AccurateTick();
            if (IS_EFFECT_VALUE(effect_pass.Time))
            {
                effect_pass.TimeCurrent += (float)(tick - effect_pass.TimeLastTick) / 1000.0f;
                effect_pass.TimeLastTick = tick;
                if (effect_pass.TimeCurrent >= 120.0f)
                    effect_pass.TimeCurrent = fmod(effect_pass.TimeCurrent, 120.0f);

                SET_EFFECT_VALUE(effect, effect_pass.Time, effect_pass.TimeCurrent);
            }
            if (IS_EFFECT_VALUE(effect_pass.TimeGame))
            {
                if (!Timer::IsGamePaused())
                {
                    effect_pass.TimeGameCurrent += (float)(tick - effect_pass.TimeGameLastTick) / 1000.0f;
                    effect_pass.TimeGameLastTick = tick;
                    if (effect_pass.TimeGameCurrent >= 120.0f)
                        effect_pass.TimeGameCurrent = fmod(effect_pass.TimeGameCurrent, 120.0f);
                }
                else
                {
                    effect_pass.TimeGameLastTick = tick;
                }

                SET_EFFECT_VALUE(effect, effect_pass.TimeGame, effect_pass.TimeGameCurrent);
            }
        }

        if (effect_pass.IsRandom)
        {
            if (IS_EFFECT_VALUE(effect_pass.Random1))
                SET_EFFECT_VALUE(effect, effect_pass.Random1, (float)((double)Random(0, 2000000000) / 2000000000.0));
            if (IS_EFFECT_VALUE(effect_pass.Random2))
                SET_EFFECT_VALUE(effect, effect_pass.Random2, (float)((double)Random(0, 2000000000) / 2000000000.0));
            if (IS_EFFECT_VALUE(effect_pass.Random3))
                SET_EFFECT_VALUE(effect, effect_pass.Random3, (float)((double)Random(0, 2000000000) / 2000000000.0));
            if (IS_EFFECT_VALUE(effect_pass.Random4))
                SET_EFFECT_VALUE(effect, effect_pass.Random4, (float)((double)Random(0, 2000000000) / 2000000000.0));
        }

        if (effect_pass.IsTextures)
        {
            for (int i = 0; i < EFFECT_TEXTURES; i++)
            {
                if (IS_EFFECT_VALUE(effect_pass.Textures[i]))
                {
                    GLuint id = (textures && textures[i] ? textures[i]->Id : 0);
                    GL(glActiveTexture(GL_TEXTURE2 + i));
                    GL(glBindTexture(GL_TEXTURE_2D, id));
                    GL(glActiveTexture(GL_TEXTURE0));
                    GL(glUniform1i(effect_pass.Textures[i], 2 + i));
                    if (effect_pass.TexturesSize[i] != -1 && textures && textures[i])
                        GL(glUniform4fv(effect_pass.TexturesSize[i], 1, textures[i]->SizeData));
                    if (effect_pass.TexturesAtlasOffset[i] != -1 && textures && textures[i])
                        GL(glUniform4fv(effect_pass.TexturesAtlasOffset[i], 1, textures[i]->AtlasOffsetData));
                }
            }
        }

        if (effect_pass.IsScriptValues)
        {
            for (int i = 0; i < EFFECT_SCRIPT_VALUES; i++)
                if (IS_EFFECT_VALUE(effect_pass.ScriptValues[i]))
                    SET_EFFECT_VALUE(effect, effect_pass.ScriptValues[i], GameOpt.EffectValues[i]);
        }

        if (effect_pass.IsAnimPos)
        {
            if (IS_EFFECT_VALUE(effect_pass.AnimPosProc))
                SET_EFFECT_VALUE(effect, effect_pass.AnimPosProc, anim_proc);
            if (IS_EFFECT_VALUE(effect_pass.AnimPosTime))
                SET_EFFECT_VALUE(effect, effect_pass.AnimPosTime, anim_time);
        }

        if (effect_pass.IsChangeStates)
        {
            if (effect_pass.BlendFuncParam1)
                GL(glBlendFunc(effect_pass.BlendFuncParam1, effect_pass.BlendFuncParam2));
            if (effect_pass.BlendEquation)
                GL(glBlendEquation(effect_pass.BlendEquation));
        }
    }
    // Finish processing
    else
    {
        if (effect_pass.IsChangeStates)
        {
            if (effect_pass.BlendFuncParam1)
                GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            if (effect_pass.BlendEquation)
                GL(glBlendEquation(GL_FUNC_ADD));
        }
    }
}

#define LOAD_EFFECT(effect_handle, effect_name, use_in_2d, defines) \
    if (!(effect_handle = effect_handle##Default = LoadEffect(effect_name, use_in_2d, defines, "Effects/"))) \
    effect_errors++

bool GraphicLoader::LoadMinimalEffects()
{
    uint effect_errors = 0;
    LOAD_EFFECT(Effects.Font, "Font_Default", true, "");
    LOAD_EFFECT(Effects.FlushRenderTarget, "Flush_RenderTarget", true, "");
    if (effect_errors > 0)
    {
        WriteLog("Minimal effects not loaded.\n");
        return false;
    }
    return true;
}

bool GraphicLoader::LoadDefaultEffects()
{
    // Default effects
    uint effect_errors = 0;
    LOAD_EFFECT(Effects.Generic, "2D_Default", true, "");
    LOAD_EFFECT(Effects.Critter, "2D_Default", true, "");
    LOAD_EFFECT(Effects.Roof, "2D_Default", true, "");
    LOAD_EFFECT(Effects.Rain, "2D_WithoutEgg", true, "");
    LOAD_EFFECT(Effects.Iface, "Interface_Default", true, "");
    LOAD_EFFECT(Effects.Primitive, "Primitive_Default", true, "");
    LOAD_EFFECT(Effects.Light, "Primitive_Light", true, "");
    LOAD_EFFECT(Effects.Fog, "Primitive_Fog", true, "");
    LOAD_EFFECT(Effects.Font, "Font_Default", true, "");
    LOAD_EFFECT(Effects.Tile, "2D_WithoutEgg", true, "");
    LOAD_EFFECT(Effects.FlushRenderTarget, "Flush_RenderTarget", true, "");
    LOAD_EFFECT(Effects.FlushPrimitive, "Flush_Primitive", true, "");
    LOAD_EFFECT(Effects.FlushMap, "Flush_Map", true, "");
    LOAD_EFFECT(Effects.FlushLight, "Flush_Light", true, "");
    LOAD_EFFECT(Effects.FlushFog, "Flush_Fog", true, "");
    if (effect_errors > 0)
    {
        WriteLog("Default effects not loaded.\n");
        return false;
    }

    LOAD_EFFECT(Effects.Contour, "Contour_Default", true, "");
    return true;
}

bool GraphicLoader::Load3dEffects()
{
    uint effect_errors = 0;
    LOAD_EFFECT(Effects.Skinned3d, "3D_Skinned", false, "");
    if (effect_errors > 0)
    {
        WriteLog("3D effects not loaded.\n");
        return false;
    }
    return true;
}

#undef LOAD_EFFECT
