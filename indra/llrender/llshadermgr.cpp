/**
 * @file llshadermgr.cpp
 * @brief Shader manager implementation.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright © 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "llshadermgr.h"
#include "llrender.h"
#include "llfile.h"
#include "lldir.h"
#include "llsdutil.h"
#include "llsdserialize.h"
#include "llsdjson.h"
#include "hbxxh.h"
#include "fsyspath.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>

#if LL_DARWIN
#include "OpenGL/OpenGL.h"
#endif

// Lots of STL stuff in here, using namespace std to keep things more readable
using std::vector;
using std::pair;
using std::make_pair;
using std::string;

LLShaderMgr * LLShaderMgr::sInstance = NULL;
U32 LLShaderMgr::sAvatarLightResponseAttachCount = 0; // [AvatarLightResponse]

LLShaderMgr::LLShaderMgr()
{
}


LLShaderMgr::~LLShaderMgr()
{
}

// static
LLShaderMgr * LLShaderMgr::instance()
{
    if(NULL == sInstance)
    {
        LL_ERRS("Shaders") << "LLShaderMgr should already have been instantiated by the application!" << LL_ENDL;
    }

    return sInstance;
}

bool LLShaderMgr::computeShadersSupported()
{
#if LL_DARWIN
    return false;
#else
    if (gGLManager.mGLVersion < 4.29f ||
        gGLManager.mGLSLVersionMajor < 4 ||
        (gGLManager.mGLSLVersionMajor == 4 && gGLManager.mGLSLVersionMinor < 30) ||
        !glDispatchCompute || !glMemoryBarrier || !glBindBufferBase ||
        !glBindImageTexture || !glShaderStorageBlockBinding || !glGetIntegeri_v)
        return false;
    static U32 generation = ~U32(0);
    static bool supported = false;
    if (generation == sContextGeneration) return supported;
    generation = sContextGeneration;
    GLint storage = 0, images = 0, invocations = 0, groups = 0;
    glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &storage);
    glGetIntegerv(GL_MAX_IMAGE_UNITS, &images);
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &invocations);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &groups);
    supported = storage >= 1 && images >= 1 && invocations >= 128 && groups >= 1024;
    return supported;
#endif
}

S32 LLShaderMgr::shaderLanguageVersion(GLenum stage)
{
    if (stage == LLGLSLShader::COMPUTE_STAGE) return 430;
    const S32 major = gGLManager.mGLSLVersionMajor;
    const S32 minor = gGLManager.mGLSLVersionMinor;
    if (major >= 4) return minor >= 20 ? 420 : 400;
    if (major == 3) return minor <= 29 ? 150 : 330;
    return stage == GL_GEOMETRY_SHADER || minor >= 50 ? 150 : 140;
}

bool LLShaderMgr::ownsShaderObject(GLuint object) const
{
    for (const auto& entry : mShaderObjects)
        if (entry.second == object) return true;
    if (mShaderBatchJournal && mShaderBatchJournal->issuedObjects.count(object)) return true;
    return false;
}

void LLShaderMgr::clearShaderObjects()
{
    ++sContextGeneration;
    for (const auto& entry : mShaderObjects) glDeleteShader(entry.second);
    mShaderObjects.clear();
    mDiagnosticObjects.clear();
    mVertexShaderObjects.clear();
    mFragmentShaderObjects.clear();
    mShaderFeatureKeys.clear();
}

void LLShaderMgr::prepareWeatherFeatures(LLGLSLShader* shader)
{
    LLShaderFeatures* features = &shader->mFeatures;
    features->hasWeatherMaterial = false;
    shader->mDefines.erase("HAS_WEATHER_MATERIAL");
    if (features->hasSkinning || features->hasObjectSkinning ||
        shader->mDefines.count("HAS_SKIN") || shader->mDefines.count("AVATAR_GEOMETRY") ||
        shader->mDefines.count("IS_HUD") || shader->mDefines.count("MATERIAL_VELOCITY")) return;
    // Compile the supported inventory once. Weather settings only change uniforms.
    if (!weatherMaterialsSupported()) return;
    // Native material inventory. Fullbright, water, HUD beauty and avatar helpers are excluded.
    // diffuseAlphaMaskNoColorF is rigid avatar eyes and has no skinning tag.
    for (const auto& file : shader->mShaderFiles)
    {
        if (file.second == GL_FRAGMENT_SHADER && (
            file.first == "deferred/diffuseF.glsl" ||
            file.first == "deferred/diffuseIndexedF.glsl" ||
            file.first == "deferred/diffuseAlphaMaskF.glsl" ||
            file.first == "deferred/diffuseAlphaMaskIndexedF.glsl" ||
            file.first == "deferred/bumpF.glsl" ||
            file.first == "deferred/treeF.glsl" ||
            file.first == "deferred/materialF.glsl" ||
            file.first == "deferred/materialIndexedF.glsl" ||
            file.first == "deferred/sharedActorFxPbrF.glsl" ||
            file.first == "deferred/pbropaqueF.glsl" ||
            file.first == "deferred/pbropaqueIndexedF.glsl" ||
            file.first == "deferred/terrainF.glsl" ||
            file.first == "deferred/pbrterrainF.glsl" ||
            file.first == "deferred/pbralphaF.glsl" ||
            file.first == "deferred/alphaF.glsl"))
        {
            features->hasWeatherMaterial = true;
        }
    }

    if (features->hasWeatherMaterial) shader->addPermutation("HAS_WEATHER_MATERIAL", "1");
}

template<bool diagnostics>
bool LLShaderMgr::attachShaderFeaturesInternal(LLGLSLShader * shader, std::vector<std::pair<std::string, GLenum>>* files)
{
    llassert_always(shader != NULL);
    LLShaderFeatures *features = & shader->mFeatures;
    auto attach_vertex = [&](const std::string& file)
    {
        if (files) { files->emplace_back(file, GL_VERTEX_SHADER); return true; }
        return shader->attachVertexObjectInternal<diagnostics>(file);
    };
    auto attach_fragment = [&](const std::string& file)
    {
        if (files) { files->emplace_back(file, GL_FRAGMENT_SHADER); return true; }
        return shader->attachFragmentObjectInternal<diagnostics>(file);
    };

    if (features->attachNothing)
    {
        return true;
    }
    //////////////////////////////////////
    // Attach Vertex Shader Features First
    //////////////////////////////////////

    // NOTE order of shader object attaching is VERY IMPORTANT!!!
    if (features->calculatesAtmospherics || features->hasGamma || features->isDeferred)
    {
        if (!attach_vertex("windlight/atmosphericsVarsV.glsl"))
        {
            return false;
        }
    }

    if (features->calculatesLighting || features->calculatesAtmospherics)
    {
        if (!attach_vertex("windlight/atmosphericsHelpersV.glsl"))
        {
            return false;
        }
    }

    if (features->calculatesLighting)
    {
        if (features->isSpecular)
        {
            if (!attach_vertex("lighting/lightFuncSpecularV.glsl"))
            {
                return false;
            }

            if (!features->isAlphaLighting)
            {
                if (!attach_vertex("lighting/sumLightsSpecularV.glsl"))
                {
                    return false;
                }
            }

            if (!attach_vertex("lighting/lightSpecularV.glsl"))
            {
                return false;
            }
        }
        else
        {
            if (!attach_vertex("lighting/lightFuncV.glsl"))
            {
                return false;
            }

            if (!features->isAlphaLighting)
            {
                if (!attach_vertex("lighting/sumLightsV.glsl"))
                {
                    return false;
                }
            }

            if (!attach_vertex("lighting/lightV.glsl"))
            {
                return false;
            }
        }
    }

    // NOTE order of shader object attaching is VERY IMPORTANT!!!
    if (features->calculatesAtmospherics)
    {
        if (!attach_vertex("environment/srgbF.glsl")) // NOTE -- "F" suffix is superfluous here, there is nothing fragment specific in srgbF
        {
            return false;
        }

        if (!attach_vertex("windlight/atmosphericsFuncs.glsl")) {
            return false;
        }

        if (!attach_vertex("windlight/atmosphericsV.glsl"))
        {
            return false;
        }
    }

    if (features->hasSkinning)
    {
        if (!attach_vertex("avatar/avatarSkinV.glsl"))
        {
            return false;
        }
    }

    if (features->hasObjectSkinning)
    {
        if (!files) shader->mRiggedVariant = shader;
        if (!attach_vertex("avatar/objectSkinV.glsl"))
        {
            return false;
        }
    }

    if (!attach_vertex("deferred/textureUtilV.glsl"))
    {
        return false;
    }

    ///////////////////////////////////////
    // Attach Fragment Shader Features Next
    ///////////////////////////////////////

    // NOTE order of shader object attaching is VERY IMPORTANT!!!

    if (!attach_fragment("deferred/globalF.glsl"))
    {
        return false;
    }

    if (features->hasWeatherMaterial && !attach_fragment("deferred/weatherMaterialF.glsl"))
    {
        return false;
    }
    if (features->hasWeatherMaterial && !attach_fragment("deferred/weatherShelterF.glsl"))
    {
        return false;
    }
    if (features->hasActorFx || features->hasActorFxShadow)
    {
        // Beauty and dissolve/shadow coverage form one Actor FX contract. If
        // either primary module failed to load, use identity fallbacks for
        // both so the color and shadow passes cannot disagree about coverage.
        const bool actor_fx_primary_available =
            mFragmentShaderObjects.count("alchemy/actorFxDissolveF.glsl") != 0 &&
            mFragmentShaderObjects.count("alchemy/actorFxF.glsl") != 0;

        if (!files && !actor_fx_primary_available)
        {
            LL_WARNS("Shader") << "Actor FX primary modules unavailable for "
                                << shader->mName << "; attaching coupled identity fallbacks" << LL_ENDL;
        }

        const char* dissolve_module = actor_fx_primary_available
                                          ? "alchemy/actorFxDissolveF.glsl"
                                          : "alchemy/actorFxDissolveFallbackF.glsl";
        if (!attach_fragment(dissolve_module))
        {
            return false;
        }

        if (features->hasActorFx)
        {
            const char* beauty_module = actor_fx_primary_available
                                            ? "alchemy/actorFxF.glsl"
                                            : "alchemy/actorFxFallbackF.glsl";
            if (!attach_fragment(beauty_module))
            {
                return false;
            }

            // [AvatarLightResponse] per-avatar response trim; identity while alrEnabled == 0.
            if (!attach_fragment("alchemy/avatarLightResponseF.glsl"))
            {
                return false;
            }
            if (!files) ++sAvatarLightResponseAttachCount;
        }
    }

    if (features->hasSrgb || features->hasAtmospherics || features->calculatesAtmospherics || features->isDeferred || features->hasWeatherMaterial)
    {
        if (!attach_fragment("environment/srgbF.glsl"))
        {
            return false;
        }
    }

    if(features->calculatesAtmospherics || features->hasGamma || features->isDeferred)
    {
        if (!attach_fragment("windlight/atmosphericsVarsF.glsl"))
        {
            return false;
        }
    }

    if (features->calculatesLighting || features->calculatesAtmospherics)
    {
        if (!attach_fragment("windlight/atmosphericsHelpersF.glsl"))
        {
            return false;
        }
    }

    // we want this BEFORE shadows and AO because those facilities use pos/norm access
    // Classic projector programs supply their frozen utility in mShaderFiles.
    // Use a permutation marker so unrelated programs retain their feature hash.
    if ((features->isDeferred || features->hasReflectionProbes) &&
        shader->mDefines.count("PROJVOL_CLASSIC_UTIL") == 0)
    {
        if (!attach_fragment("deferred/deferredUtil.glsl"))
        {
            return false;
        }
    }

    if (features->hasFullGBuffer)
    {
        if (!attach_fragment("deferred/gbufferUtil.glsl"))
        {
            return false;
        }
    }

    if (features->hasScreenSpaceReflections || features->hasReflectionProbes)
    {
        if (!attach_fragment("deferred/screenSpaceReflUtil.glsl"))
        {
            return false;
        }
    }

    // Classic supplies its pre-Shadow-Sculpt sampler in mShaderFiles too:
    // its rim passes a receiver normal and must never enter the detail path.
    if (features->hasShadows && shader->mDefines.count("PROJVOL_CLASSIC_UTIL") == 0)
    {
        if (!attach_fragment("deferred/shadowUtil.glsl"))
        {
            return false;
        }
    }

    if (features->hasReflectionProbes)
    {
        if (!attach_fragment("deferred/reflectionProbeF.glsl"))
        {
            return false;
        }
    }

    if (features->hasAmbientOcclusion)
    {
        if (!attach_fragment("deferred/aoUtil.glsl"))
        {
            return false;
        }
    }

    if (features->hasGamma || features->isDeferred)
    {
        if (!attach_fragment("windlight/gammaF.glsl"))
        {
            return false;
        }
    }

    if (features->hasAtmospherics || features->isDeferred)
    {
        if (!attach_fragment("windlight/atmosphericsFuncs.glsl")) {
            return false;
        }

        if (!attach_fragment("windlight/atmosphericsF.glsl"))
        {
            return false;
        }

        // [Cine Haze] shared transmittance / atmosphere-colour library.  Pure
        // functions + uniforms (all uniform-gated), attached to exactly the set of
        // programs that can call it: the deferred haze pass and every forward
        // shader that links environment/waterFogF.glsl (hasAtmospherics).
        if (!attach_fragment("windlight/cineHazeF.glsl"))
        {
            return false;
        }
    }

    if (features->isPBRTerrain)
    {
        if (!attach_fragment("deferred/pbrterrainUtilF.glsl"))
        {
            return false;
        }
    }

    if (features->hasTonemap)
    {
        if (!attach_fragment("deferred/tonemapUtilF.glsl"))
        {
            return false;
        }
        // AMD FidelityFX LPM (tonemap type 7) lives in its own object so that
        // RunLPMFilter() (called from tonemapUtilF.glsl) resolves at link time.
        if (!attach_fragment("deferred/LPMUtil.glsl"))
        {
            return false;
        }
    }

    if (features->hasColorGrade)
    {
        if (!attach_fragment("alchemy/colorGradeUtilF.glsl"))
        {
            return false;
        }
    }

    if (features->hasPostEffects)
    {
        if (!attach_fragment("alchemy/postEffectUtilsF.glsl"))
        {
            return false;
        }
    }

    // NOTE order of shader object attaching is VERY IMPORTANT!!!
    if (features->hasAtmospherics)
    {
        if (!attach_fragment("environment/waterFogF.glsl"))
        {
            return false;
        }
    }

    if (features->hasLighting)
    {
        if (features->mIndexedTextureChannels <= 1)
        {
            if (features->hasAlphaMask)
            {
                if (!attach_fragment("lighting/lightAlphaMaskNonIndexedF.glsl"))
                {
                    return false;
                }
            }
            else
            {
                if (!attach_fragment("lighting/lightNonIndexedF.glsl"))
                {
                    return false;
                }
            }
        }
        else
        {
            if (features->hasAlphaMask)
            {
                if (!attach_fragment("lighting/lightAlphaMaskF.glsl"))
                {
                    return false;
                }
            }
            else
            {
                if (!attach_fragment("lighting/lightF.glsl"))
                {
                    return false;
                }
            }
            if (!files) shader->mFeatures.mIndexedTextureChannels = llmax(LLGLSLShader::sIndexedTextureChannels, 1);
        }
    }

    if (features->mIndexedTextureChannels <= 1)
    {
        if (!attach_vertex("objects/nonindexedTextureV.glsl"))
        {
            return false;
        }
    }
    else
    {
        if (!attach_vertex("objects/indexedTextureV.glsl"))
        {
            return false;
        }
    }

    return true;
}

bool LLShaderMgr::attachShaderFeatures(LLGLSLShader* shader)
{
    return attachShaderFeaturesInternal<false>(shader);
}
template bool LLShaderMgr::attachShaderFeaturesInternal<true>(LLGLSLShader*, std::vector<std::pair<std::string, GLenum>>*);
template bool LLShaderMgr::attachShaderFeaturesInternal<false>(LLGLSLShader*, std::vector<std::pair<std::string, GLenum>>*);


//============================================================================
// Load Shader

static std::string get_shader_log(GLuint ret)
{
    std::string res;

    //get log length
    GLint length;
    glGetShaderiv(ret, GL_INFO_LOG_LENGTH, &length);
    if (length > 0)
    {
        //the log could be any size, so allocate appropriately
        GLchar* log = new GLchar[length];
        glGetShaderInfoLog(ret, length, &length, log);
        res = std::string((char *)log);
        delete[] log;
    }
    return res;
}

static std::string get_program_log(GLuint ret)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER;
    std::string res;

    //get log length
    GLint length;
    glGetProgramiv(ret, GL_INFO_LOG_LENGTH, &length);
    if (length > 0)
    {
        //the log could be any size, so allocate appropriately
        GLchar* log = new GLchar[length];
        glGetProgramInfoLog(ret, length, &length, log);
        res = std::string((char*)log);
        delete[] log;
    }
    return res;
}

// get the info log for the given object, be it a shader or program object
// NOTE: ret MUST be a shader OR a program object
static std::string get_object_log(GLuint ret)
{
    if (glIsProgram(ret))
    {
        return get_program_log(ret);
    }
    else
    {
        llassert(glIsShader(ret));
        return get_shader_log(ret);
    }
}

//dump shader source for debugging
void LLShaderMgr::dumpShaderSource(U32 shader_code_count, GLchar** shader_code_text)
{
    char num_str[16]; // U32 = max 10 digits

    LL_SHADER_LOADING_WARNS() << "\n";

    for (U32 i = 0; i < shader_code_count; i++)
    {
        snprintf(num_str, sizeof(num_str), "%4d: ", i+1);
        std::string line_number(num_str);
        LL_CONT << line_number << shader_code_text[i];
    }
    LL_CONT << LL_ENDL;
}

void LLShaderMgr::dumpObjectLog(GLuint ret, bool warns, const std::string& filename)
{
    std::string log;
    log = get_object_log(ret);
    std::string fname = filename;
    if (filename.empty())
    {
        fname = "unknown shader file";
    }

    if (log.length() > 0)
    {
        LL_SHADER_LOADING_WARNS() << "Shader loading from " << fname << LL_ENDL;
        LL_SHADER_LOADING_WARNS() << "\n" << log << LL_ENDL;
    }
 }

bool LLShaderMgr::shaderSourceUsesDefine(const std::string& source, const std::string& name)
{
    auto identifier = [](unsigned char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_';
    };
    if (name.empty()) return false;
    size_t offset = 0;
    while ((offset = source.find(name, offset)) != std::string::npos)
    {
        const size_t end = offset + name.size();
        if ((offset == 0 || !identifier(source[offset - 1])) &&
            (end == source.size() || !identifier(source[end]))) return true;
        offset = end;
    }
    return false;
}

template<bool diagnostics, bool batching>
GLuint LLShaderMgr::loadShaderFileInternal(const std::string& filename, S32 & shader_level, GLenum type, std::map<std::string, std::string>* defines, S32 texture_index_channels, std::vector<LLShaderCompileJob>* pending)
{
    const S32 requested_level = shader_level;
    bool deferred_submitted = false;
    bool deferred_failure = false;
    auto finish_stage = [&](bool success, S32 resolved, GLuint object = 0, const char* outcome = "not_attempted")
    {
        if constexpr (batching)
        {
            LLShaderCompileJob stage;
            stage.object = object;
            stage.resolved = resolved;
            stage.submitted = deferred_submitted;
            stage.forceFailure = deferred_failure;
            stage.outcome = outcome;
            if constexpr (diagnostics) stage.stage = mShaderStage;
            pending->push_back(std::move(stage));
        }
        else if constexpr (diagnostics) finishShaderStage(success, resolved, object, outcome);
    };
    if constexpr (diagnostics) beginShaderStage(filename, type, requested_level);
    if (type == LLGLSLShader::COMPUTE_STAGE && !computeShadersSupported())
    {
        LL_WARNS("ShaderLoading") << "Compute unavailable: GL 4.3, SSBO, image and barrier support required" << LL_ENDL;
        finish_stage(false, shader_level);
        return 0;
    }

    // A failed replacement must not leave a filename alias to an older
    // permutation. Successfully cached objects republish their alias below.
    if (type == GL_VERTEX_SHADER) mVertexShaderObjects.erase(filename);
    if (type == GL_FRAGMENT_SHADER) mFragmentShaderObjects.erase(filename);
    GLenum error = GL_NO_ERROR;

    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        LL_SHADER_LOADING_WARNS() << "GL ERROR entering loadShaderFile(): " << error << " for file: " << filename << LL_ENDL;
    }

    if (filename.empty())
    {
        LL_WARNS("ShaderLoading") << "tried loading empty filename" << LL_ENDL;
        finish_stage(false, shader_level);
        return 0;
    }


    //read in from file
    LLFILE* file = NULL;

    S32 try_gpu_class = shader_level;
    S32 gpu_class;

    std::string open_file_name;

#if 0  // WIP -- try to come up with a way to fallback to an error shader without needing debug stubs all over the place in the shader tree
    if (shader_level == -1)
    {
        // use "error" fallback
        if (type == GL_VERTEX_SHADER)
        {
            open_file_name = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "shaders/errorV.glsl");
        }
        else
        {
            llassert(type == GL_FRAGMENT_SHADER);  // type must be vertex or fragment shader
            open_file_name = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "shaders/errorF.glsl");
        }

        file = LLFile::fopen(open_file_name, LLFILE_MODE("r"));
    }
    else
#endif
    {
        //find the most relevant file
        for (gpu_class = try_gpu_class; gpu_class > 0; gpu_class--)
        {   //search from the current gpu class down to class 1 to find the most relevant shader
            std::stringstream fname;
            fname << getShaderDirPrefix();
            fname << gpu_class << gDirUtilp->getDirDelimiter() << filename;

            open_file_name = fname.str();

            /*
            Would be awesome, if we didn't have shaders that re-use files
            with different environments to say, add skinning, etc
            can't depend on cached version to have evaluate ifdefs identically...
            if we can define a deterministic hash for the shader based on
            all the inputs, maybe we can save some time here.
            if (mShaderObjects.count(filename) > 0)
            {
                return mShaderObjects[filename];
            }

            */

            LL_DEBUGS("ShaderLoading") << "Looking in " << open_file_name << LL_ENDL;
            if constexpr (diagnostics)
            {
                if (injectShaderFailure("missing", filename)) continue;
            }
            file = LLFile::fopen(open_file_name, LLFILE_MODE("r")); /* Flawfinder: ignore */
            if (file)
            {
                LL_DEBUGS("ShaderLoading") << "Loading file: " << open_file_name << " (Want class " << gpu_class << ")" << LL_ENDL;
                break; // done
            }
        }
    }

    if (file == NULL)
    {
        if (gDirUtilp->fileExists(open_file_name))
        {
            LL_WARNS("ShaderLoading") << "GLSL Shader file failed to open: " << open_file_name << LL_ENDL;
        }
        else
        {
            LL_WARNS("ShaderLoading") << "GLSL Shader file not found: " << open_file_name << LL_ENDL;
        }
        finish_stage(false, shader_level);
        return 0;
    }

    // Global maps are distinct from per-program permutations. No GLSL includes
    // are supported here; the load gate rejects real #include directives (the
    // three current occurrences are documentation in comments).
    std::string define_source;
    if (defines == &LLGLSLShader::sGlobalDefines)
    {
        char source_line[1024];
        while (fgets(source_line, sizeof(source_line), file)) define_source += source_line;
        if (ferror(file))
        {
            fclose(file);
            finish_stage(false, shader_level);
            return 0;
        }
        rewind(file);
    }

    //we can't have any lines longer than 1024 characters
    //or any shaders longer than 4096 lines... deal - DaveP
    GLchar buff[1024];
    GLchar *extra_code_text[1024];
    GLchar *shader_code_text[4096 + LL_ARRAY_SIZE(extra_code_text)] = { NULL };
    GLuint extra_code_count = 0, shader_code_count = 0;
    BOOST_STATIC_ASSERT(LL_ARRAY_SIZE(extra_code_text) < LL_ARRAY_SIZE(shader_code_text));


    S32 major_version = gGLManager.mGLSLVersionMajor;
    S32 minor_version = gGLManager.mGLSLVersionMinor;

    if (type == LLGLSLShader::COMPUTE_STAGE)
    {
        shader_code_text[shader_code_count++] = strdup("#version 430\n");
    }
    else if (major_version == 1 && minor_version < 30)
    {
        llassert(false); // GL 3.1 or later required
    }
    else
    {
        if (major_version >= 4)
        {
            //set version to 400 or 420
            if (minor_version >= 20)
            {
                shader_code_text[shader_code_count++] = strdup("#version 420\n");
            }
            else
            {
                shader_code_text[shader_code_count++] = strdup("#version 400\n");
            }
        }
        else if (major_version == 3)
        {
            if (minor_version <= 29)
            {
                // OpenGL 3.2 had GLSL version 1.50.  anything after that the version numbers match.
                // https://www.khronos.org/opengl/wiki/Core_Language_(GLSL)#OpenGL_and_GLSL_versions
                shader_code_text[shader_code_count++] = strdup("#version 150\n");
            }
            else
            {
                shader_code_text[shader_code_count++] = strdup("#version 330\n");
            }
        }
        else
        {
            // OpenGL 3.2 had GLSL version 1.50.  anything after that the version numbers match.
            if (type == GL_GEOMETRY_SHADER || minor_version >= 50)
            {
                //set version to 1.50
                shader_code_text[shader_code_count++] = strdup("#version 150\n");
                //some implementations of GLSL 1.30 require integer precision be explicitly declared
                extra_code_text[extra_code_count++] = strdup("precision mediump int;\n");
                extra_code_text[extra_code_count++] = strdup("precision highp float;\n");
            }
            else
            {
                //set version to 1.40
                shader_code_text[shader_code_count++] = strdup("#version 140\n");
                //some implementations of GLSL 1.30 require integer precision be explicitly declared
                extra_code_text[extra_code_count++] = strdup("precision mediump int;\n");
                extra_code_text[extra_code_count++] = strdup("precision highp float;\n");
            }
        }
    }

    if (type == GL_FRAGMENT_SHADER)
    {
        extra_code_text[extra_code_count++] = strdup("#define FRAGMENT_SHADER 1\n");
    }
    else if (type == LLGLSLShader::COMPUTE_STAGE)
    {
        extra_code_text[extra_code_count++] = strdup("#define COMPUTE_SHADER 1\n");
    }
    else
    {
        extra_code_text[extra_code_count++] = strdup("#define VERTEX_SHADER 1\n");
    }

    // Use alpha float to store bit flags
    // See: C++: addDeferredAttachment(), shader: frag_data[2]
    extra_code_text[extra_code_count++] = strdup("#define GBUFFER_FLAG_SKIP_ATMOS   0.0 \n"); // atmo kill
    extra_code_text[extra_code_count++] = strdup("#define GBUFFER_FLAG_HAS_ATMOS    0.34\n"); // bit 0
    extra_code_text[extra_code_count++] = strdup("#define GBUFFER_FLAG_HAS_PBR      0.67\n"); // bit 1
    extra_code_text[extra_code_count++] = strdup("#define GBUFFER_FLAG_HAS_HDRI      1.0\n");  // bit 2
    // [TronA0] Per-pixel avatar tag in the flag channel. The preamble TEXT is
    // chosen here from the HASH-TOKEN global define GBUFFER_AVATAR_TAG_ENABLED
    // (set by LLViewerShaderMgr::setShaders under exactly the RGBA16-normals
    // predicate, and part of the binary-cache key) so that every compile unit
    // of every program -- utility files included -- sees ONE definition of
    // GBUFFER_AVATAR_TAG. The selector is retained explicitly in program keys;
    // the unused hash token itself is not emitted into GLSL.
    // Encoding 2: .w is a 7-level code at multiples of 1/6. Even codes are the
    // existing flags (0 SKIP_ATMOS, 2 HAS_ATMOS, 4 HAS_PBR, 6 HAS_HDRI -- all
    // inside the 0.1 window of the constants above); odd codes are
    // "avatar + next even flag" (1 = 0.1667, 3 = 0.5, 5 = 0.8333). UNTAG
    // rounds an odd code up to its even neighbour so every existing reader
    // keeps seeing the flag it always saw. With the tag off, today's exact
    // text is emitted (byte-identical preamble by construction).
    const bool avatar_tag = LLGLSLShader::sGlobalDefines.count("GBUFFER_AVATAR_TAG_ENABLED") > 0;
    if (!avatar_tag)
    {
        extra_code_text[extra_code_count++] = strdup("#define GET_GBUFFER_FLAG(data, flag)    (abs(data-flag)< 0.1)\n");
    }
    else
    {
        extra_code_text[extra_code_count++] = strdup("#define GBUFFER_AVATAR_TAG 1\n");
        extra_code_text[extra_code_count++] = strdup("#define GBUFFER_UNTAG(d) ((floor((d)*6.0+0.5) + mod(floor((d)*6.0+0.5), 2.0)) / 6.0)\n");
        extra_code_text[extra_code_count++] = strdup("#define GBUFFER_AVATAR_TAG_OF(d) (mod(floor((d)*6.0+0.5), 2.0) > 0.5)\n");
        // Categorical readers are point sampled (getNormRaw uses texelFetch).
        // The seven legal RGBA16 codes are either the original flag or that
        // flag minus 1/6. Two windows preserve every legal flag test without
        // floor/mod in each lighting branch. Never apply to interpolated IDs.
        extra_code_text[extra_code_count++] = strdup("#define GET_GBUFFER_FLAG(data, flag)    (abs((data)-(flag)) < 0.1 || abs((data)+(1.0/6.0)-(flag)) < 0.1)\n");
    }

    if (defines)
    {
        for (auto iter = defines->begin(); iter != defines->end(); ++iter)
        {
            if (defines == &LLGLSLShader::sGlobalDefines &&
                !shaderSourceUsesDefine(define_source, iter->first)) continue;
            std::string define = "#define " + iter->first + " " + iter->second + "\n";
            extra_code_text[extra_code_count++] = (GLchar *) strdup(define.c_str());
        }
    }

    if( gGLManager.mIsAMD )
    {
        extra_code_text[extra_code_count++] = strdup( "#define IS_AMD_CARD 1\n" );
    }

    if (texture_index_channels > 0 && type == GL_FRAGMENT_SHADER)
    {
        //use specified number of texture channels for indexed texture rendering

        /* prepend shader code that looks like this:

        uniform sampler2D tex0;
        uniform sampler2D tex1;
        uniform sampler2D tex2;
        .
        .
        .
        uniform sampler2D texN;

        flat in int vary_texture_index;

        vec4 ret = vec4(1,0,1,1);

        vec4 diffuseLookup(vec2 texcoord)
        {
            switch (vary_texture_index)
            {
                case 0: ret = texture(tex0, texcoord); break;
                case 1: ret = texture(tex1, texcoord); break;
                case 2: ret = texture(tex2, texcoord); break;
                .
                .
                .
                case N: return texture(texN, texcoord); break;
            }

            return ret;
        }
        */

        extra_code_text[extra_code_count++] = strdup("#define HAS_DIFFUSE_LOOKUP\n");

        //uniform declartion
        for (S32 i = 0; i < texture_index_channels; ++i)
        {
            std::string decl = llformat("uniform sampler2D tex%d;\n", i);
            extra_code_text[extra_code_count++] = strdup(decl.c_str());
        }

        if (texture_index_channels > 1)
        {
            extra_code_text[extra_code_count++] = strdup("flat in int vary_texture_index;\n");
        }

        extra_code_text[extra_code_count++] = strdup("vec4 diffuseLookup(vec2 texcoord)\n");
        extra_code_text[extra_code_count++] = strdup("{\n");


        if (texture_index_channels == 1)
        { //don't use flow control, that's silly
            extra_code_text[extra_code_count++] = strdup("return texture(tex0, texcoord);\n");
            extra_code_text[extra_code_count++] = strdup("}\n");
        }
        else if (major_version > 1 || minor_version >= 30)
        {  //switches are supported in GLSL 1.30 and later
            if (gGLManager.mIsNVIDIA)
            { //switches are unreliable on some NVIDIA drivers
                for (S32 i = 0; i < texture_index_channels; ++i)
                {
                    std::string if_string = llformat("\t%sif (vary_texture_index == %d) { return texture(tex%d, texcoord); }\n", i > 0 ? "else " : "", i, i);
                    extra_code_text[extra_code_count++] = strdup(if_string.c_str());
                }
                extra_code_text[extra_code_count++] = strdup("\treturn vec4(1,0,1,1);\n");
                extra_code_text[extra_code_count++] = strdup("}\n");
            }
            else
            {
                extra_code_text[extra_code_count++] = strdup("\tvec4 ret = vec4(1,0,1,1);\n");
                extra_code_text[extra_code_count++] = strdup("\tswitch (vary_texture_index)\n");
                extra_code_text[extra_code_count++] = strdup("\t{\n");

                //switch body
                for (S32 i = 0; i < texture_index_channels; ++i)
                {
                    std::string case_str = llformat("\t\tcase %d: return texture(tex%d, texcoord);\n", i, i);
                    extra_code_text[extra_code_count++] = strdup(case_str.c_str());
                }

                extra_code_text[extra_code_count++] = strdup("\t}\n");
                extra_code_text[extra_code_count++] = strdup("\treturn ret;\n");
                extra_code_text[extra_code_count++] = strdup("}\n");
            }
        }
        else
        { //should never get here.  Indexed texture rendering requires GLSL 1.30 or later
            // (for passing integers between vertex and fragment shaders)
            LL_ERRS() << "Indexed texture rendering requires GLSL 1.30 or later." << LL_ENDL;
        }
    }

    // Master definition can be found in deferredUtil.glsl
    extra_code_text[extra_code_count++] = strdup("struct GBufferInfo { vec4 albedo; vec4 specular; vec3 normal; vec4 emissive; float gbufferFlag; float envIntensity; };\n");

    //copy file into memory
    enum {
          flag_write_to_out_of_extra_block_area = 0x01
        , flag_extra_block_marker_was_found = 0x02
    };

    unsigned char flags = flag_write_to_out_of_extra_block_area;

    GLuint out_of_extra_block_counter = 0, start_shader_code = shader_code_count, file_lines_count = 0;

#define TOUCH_SHADERS 0

#if TOUCH_SHADERS
    const char* marker = "// touched";
    bool touched = false;
#endif

    while(NULL != fgets((char *)buff, 1024, file)
          && shader_code_count < (LL_ARRAY_SIZE(shader_code_text) - LL_ARRAY_SIZE(extra_code_text)))
    {
        file_lines_count++;

        bool extra_block_area_found = NULL != strstr((const char*)buff, "[EXTRA_CODE_HERE]");

#if TOUCH_SHADERS
        if (NULL != strstr((const char*)buff, marker))
        {
            touched = true;
        }
#endif

        if(extra_block_area_found && !(flag_extra_block_marker_was_found & flags))
        {
            if(!(flag_write_to_out_of_extra_block_area & flags))
            {
                //shift
                for(GLuint to = start_shader_code, from = extra_code_count + start_shader_code;
                    from < shader_code_count; ++to, ++from)
                {
                    shader_code_text[to] = shader_code_text[from];
                }

                shader_code_count -= extra_code_count;
            }

            //copy extra code
            for(GLuint n = 0; n < extra_code_count
                && shader_code_count < (LL_ARRAY_SIZE(shader_code_text) - LL_ARRAY_SIZE(extra_code_text)); ++n)
            {
                shader_code_text[shader_code_count++] = extra_code_text[n];
            }

            extra_code_count = 0;

            flags &= ~flag_write_to_out_of_extra_block_area;
            flags |= flag_extra_block_marker_was_found;
        }
        else
        {
            shader_code_text[shader_code_count] = (GLchar *)strdup((char *)buff);

            if(flag_write_to_out_of_extra_block_area & flags)
            {
                shader_code_text[extra_code_count + start_shader_code + out_of_extra_block_counter]
                    = shader_code_text[shader_code_count];
                out_of_extra_block_counter++;

                if(out_of_extra_block_counter == extra_code_count)
                {
                    shader_code_count += extra_code_count;
                    flags &= ~flag_write_to_out_of_extra_block_area;
                }
            }

            ++shader_code_count;
        }
    } //while

    if(!(flag_extra_block_marker_was_found & flags))
    {
        for(GLuint n = start_shader_code; n < extra_code_count + start_shader_code; ++n)
        {
            shader_code_text[n] = extra_code_text[n - start_shader_code];
        }

        if (file_lines_count < extra_code_count)
        {
            shader_code_count += extra_code_count;
        }

        extra_code_count = 0;
    }

#if TOUCH_SHADERS
    if (!touched)
    {
        fprintf(file, "\n%s\n", marker);
    }
#endif

    fclose(file);

    // Include the assembled bytes: version, stage, permutations, indexed
    // channels, source edits and the resolved class all distinguish objects.
    HBXXH128 object_hash;
    object_hash.update(filename);
    object_hash.update(&type, sizeof(type));
    object_hash.update(&gpu_class, sizeof(gpu_class));
    object_hash.update(&texture_index_channels, sizeof(texture_index_channels));
    for (GLuint i = 0; i < shader_code_count; ++i)
        object_hash.update(std::string(shader_code_text[i]));
    const std::string object_key = object_hash.digest().asString();
    if constexpr (diagnostics)
    {
        HBXXH128 source_hash;
        for (GLuint i = 0; i < shader_code_count; ++i) source_hash.update(std::string(shader_code_text[i]));
        assembledShaderStage(object_key, source_hash.digest().asString(), gpu_class);

    }
    auto cached = mShaderObjects.find(object_key);
    GLuint cached_object = cached != mShaderObjects.end() ? cached->second : 0;
    if constexpr (batching)
    {
        const auto pending_object = mShaderBatchJournal->compiledObjects.find(object_key);
        if (pending_object != mShaderBatchJournal->compiledObjects.end()) cached_object = pending_object->second;
    }
    bool force_compile_failure = false;
    if constexpr (diagnostics) force_compile_failure = injectShaderFailure("compile", filename, requested_level);
    if (cached_object && !force_compile_failure)
    {
        for (GLuint i = 0; i < shader_code_count; ++i) free(shader_code_text[i]);
        if (type == GL_VERTEX_SHADER) mVertexShaderObjects[filename] = cached_object;
        if (type == GL_FRAGMENT_SHADER) mFragmentShaderObjects[filename] = cached_object;
        mLastShaderSourceLevel = gpu_class;
        shader_level = try_gpu_class;
        finish_stage(true, shader_level, cached_object, "reused");
        return cached_object;
    }

    bool compiled = false;
    //create shader object
    GLuint ret = glCreateShader(type);

    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        LL_WARNS("ShaderLoading") << "GL ERROR in glCreateShader: " << error << " for file: " << open_file_name << LL_ENDL;
        if (ret)
        {
            glDeleteShader(ret); //no longer need handle
            ret = 0;
        }
    }

    //load source
    if (ret)
    {
        LL_DEBUGS("ShaderLoading") << "glCreateShader done" << LL_ENDL;
        glShaderSource(ret, shader_code_count, (const GLchar**)shader_code_text, NULL);

        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            LL_WARNS("ShaderLoading") << "GL ERROR in glShaderSource: " << error << " for file: " << open_file_name << LL_ENDL;
            glDeleteShader(ret); //no longer need handle
            ret = 0;
        }
    }

    //compile source
    if (ret)
    {
        LL_DEBUGS("ShaderLoading") << "glShaderSource done" << U32(ret) << LL_ENDL;
        glCompileShader(ret);
        compiled = true;

        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            LL_WARNS("ShaderLoading") << "GL ERROR in glCompileShader: " << error << " for file: " << open_file_name << LL_ENDL;
            glDeleteShader(ret); //no longer need handle
            ret = 0;
        }
    }

    if (ret && error == GL_NO_ERROR)
    {
        //check for errors
        LL_DEBUGS("ShaderLoading") << "glCompileShader done" << U32(ret) << LL_ENDL;
        GLint success = GL_TRUE;
        if constexpr (batching)
        {
            deferred_submitted = true;
            deferred_failure = force_compile_failure;
        }
        else glGetShaderiv(ret, GL_COMPILE_STATUS, &success);
        if constexpr (diagnostics && !batching) if (force_compile_failure) success = GL_FALSE;

        error = glGetError();
        if (error != GL_NO_ERROR || success == GL_FALSE)
        {
            //an error occured, print log
            LL_WARNS("ShaderLoading") << "GLSL Compilation Error:" << LL_ENDL;
            dumpObjectLog(ret, true, open_file_name);
            dumpShaderSource(shader_code_count, shader_code_text);
            glDeleteShader(ret); //no longer need handle
            ret = 0;
        }
    }
    else
    {
        LL_DEBUGS("ShaderLoading") << "loadShaderFile() completed, ret: " << U32(ret) << LL_ENDL;
        ret = 0;
    }
    stop_glerror();

    //free memory
    for (GLuint i = 0; i < shader_code_count; i++)
    {
        free(shader_code_text[i]);
    }

    finish_stage(ret != 0, ret ? try_gpu_class : shader_level, ret, compiled ? (ret ? "compiled" : "failed") : "not_attempted");

    //successfully loaded, save results
    if (ret)
    {
        if constexpr (batching)
        {
            mShaderBatchJournal->compiledObjects[object_key] = ret;
            mShaderBatchJournal->issuedObjects.insert(ret);
        }
        else mShaderObjects[object_key] = ret;
        // Add shader file to map
        if (type == GL_VERTEX_SHADER) {
            mVertexShaderObjects[filename] = ret;
        }
        else if (type == GL_FRAGMENT_SHADER) {
            mFragmentShaderObjects[filename] = ret;
        }
        mLastShaderSourceLevel = gpu_class;
        shader_level = try_gpu_class;
    }
    else
    {
        if (!batching && shader_level > 1)
        {
            shader_level--;
            return loadShaderFileInternal<diagnostics>(filename, shader_level, type, defines, texture_index_channels);
        }
        LL_WARNS("ShaderLoading") << "Failed to load " << filename << LL_ENDL;
    }

    LL_DEBUGS("ShaderLoading") << "loadShaderFile() completed, ret: " << U32(ret) << LL_ENDL;
    return ret;
}

GLuint LLShaderMgr::loadShaderFile(const std::string& filename, S32& level, GLenum type,
                                  std::map<std::string, std::string>* defines, S32 channels)
{
    if (mShaderDiagnosticsEnabled) return loadShaderFileInternal<true>(filename, level, type, defines, channels);
    return loadShaderFileInternal<false>(filename, level, type, defines, channels);
}
template GLuint LLShaderMgr::loadShaderFileInternal<true, false>(const std::string&, S32&, GLenum, std::map<std::string, std::string>*, S32, std::vector<LLShaderCompileJob>*);
template GLuint LLShaderMgr::loadShaderFileInternal<false, false>(const std::string&, S32&, GLenum, std::map<std::string, std::string>*, S32, std::vector<LLShaderCompileJob>*);


void LLShaderMgr::submitProgramLink(GLuint obj)
{
    //check for errors
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_SHADER("glLinkProgram");
        glLinkProgram(obj);
    }
}

template<bool diagnostics>
bool LLShaderMgr::finishProgramLinkInternal(GLuint obj, bool suppress_errors, const std::string& name)
{
    GLint success = GL_TRUE;

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_SHADER("glsl check link status");
        glGetProgramiv(obj, GL_LINK_STATUS, &success);
        if constexpr (diagnostics) if (injectShaderFailure("link", name)) success = GL_FALSE;
        if (!suppress_errors && success == GL_FALSE)
        {
            //an error occured, print log
            LL_SHADER_LOADING_WARNS() << "GLSL Linker Error:" << LL_ENDL;
            dumpObjectLog(obj, true, "linker");
            return success;
        }
    }

    return success;
}

template<bool diagnostics>
bool LLShaderMgr::linkProgramObjectInternal(GLuint obj, bool suppress_errors, const std::string& name)
{
    submitProgramLink(obj);
    return finishProgramLinkInternal<diagnostics>(obj, suppress_errors, name);
}

bool LLShaderMgr::linkProgramObject(GLuint obj, bool suppress_errors)
{
    return linkProgramObjectInternal<false>(obj, suppress_errors, "");
}
template bool LLShaderMgr::linkProgramObjectInternal<true>(GLuint, bool, const std::string&);
template bool LLShaderMgr::linkProgramObjectInternal<false>(GLuint, bool, const std::string&);
template bool LLShaderMgr::finishProgramLinkInternal<true>(GLuint, bool, const std::string&);
template bool LLShaderMgr::finishProgramLinkInternal<false>(GLuint, bool, const std::string&);


bool LLShaderMgr::validateProgramObject(GLuint obj)
{
    //check program validity against current GL
    glValidateProgram(obj);
    GLint success = GL_TRUE;
    glGetProgramiv(obj, GL_VALIDATE_STATUS, &success);
    if (success == GL_FALSE)
    {
        LL_SHADER_LOADING_WARNS() << "GLSL program not valid: " << LL_ENDL;
        dumpObjectLog(obj);
    }
    else
    {
        dumpObjectLog(obj, false);
    }

    return success;
}

bool LLShaderMgr::readShaderSource(const std::string& filename, std::string& source)
{
    namespace fs = std::filesystem;
    const fs::path path = fsyspath(filename);
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec || size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) return false;
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) return false;
    source.resize(static_cast<size_t>(size));
    stream.read(source.data(), static_cast<std::streamsize>(size));
    if (stream.bad() || stream.fail() || stream.gcount() != static_cast<std::streamsize>(size)) return false;
    // Detect truncation, growth and read errors, rather than hashing a prefix.
    if (stream.peek() != std::ifstream::traits_type::eof() || stream.bad()) return false;
    return fs::file_size(path, ec) == size && !ec;
}

// A stage root owns main(); everything else can be linked as a shared module.
// Treat closed comments as whitespace so documentation cannot hide a dependency.
static bool shader_source_has_main(const std::string& source) noexcept
{
    struct MainMatch
    {
        size_t matched = 0;
        bool in_word = false;

        bool feed(char c) noexcept
        {
            const bool word = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                              (c >= '0' && c <= '9') || c == '_';
            const bool space = c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
            if (matched == 4 && (space || c == '('))
            {
                in_word = false;
                return c == '(';
            }
            if (!in_word && c == 'm') matched = 1;
            else if (word && matched > 0 && matched < 4 && c == "main"[matched]) ++matched;
            else matched = 0;
            in_word = word;
            return false;
        }
    } match;

    size_t i = 0;
    while (i < source.size())
    {
        if (source[i] == '/' && i + 1 < source.size())
        {
            if (source[i + 1] == '/')
            {
                match.feed(' ');
                i += 2;
                while (i < source.size() && source[i] != '\r' && source[i] != '\n') ++i;
                continue;
            }
            if (source[i + 1] == '*')
            {
                match.feed(' ');
                i += 2;
                // Preserve the old classifier's treatment of an unterminated
                // block: its bytes remain source (with line comments removed).
                // Track that possibility while skipping, without a second pass.
                MainMatch unterminated;
                bool line_comment = false;
                bool found = false;
                while (i < source.size() && !(source[i] == '*' && i + 1 < source.size() && source[i + 1] == '/'))
                {
                    const char c = source[i++];
                    if (line_comment)
                    {
                        if (c == '\r' || c == '\n') line_comment = false;
                    }
                    else if (c == '/' && i < source.size() && source[i] == '/')
                    {
                        unterminated.feed(' ');
                        line_comment = true;
                        ++i;
                    }
                    else found = unterminated.feed(c) || found;
                }
                if (i == source.size()) return found;
                i += 2;
                continue;
            }
        }
        if (match.feed(source[i++])) return true;
    }
    return false;
}

static bool shader_shared_source_fingerprint(const std::string& prefix, LLUUID& fingerprint)
{
    try
    {
        namespace fs = std::filesystem;
        const fs::path shader_root = fsyspath(prefix).parent_path();
        std::error_code ec;
        if (shader_root.empty() || !fs::is_directory(shader_root, ec) || ec) return false;
        std::vector<fs::path> files;
        // Do not skip permission errors: a partial fingerprint is unsafe.
        for (fs::recursive_directory_iterator it(shader_root, ec), end;
             !ec && it != end; it.increment(ec))
        {
            std::error_code file_ec;
            const bool regular = it->is_regular_file(file_ec);
            if (file_ec) return false;
            if (regular && it->path().extension() == ".glsl") files.push_back(it->path());
        }
        if (ec || files.empty()) return false;
        std::sort(files.begin(), files.end());
        HBXXH128 hash_obj;
        for (const fs::path& path : files)
        {
            const auto relative_path = fs::relative(path, shader_root, ec).generic_u8string();
            const std::string relative(relative_path.begin(), relative_path.end());
            if (ec) return false;
            std::string source;
            if (!LLShaderMgr::readShaderSource(fsyspath(path).string(), source)) return false;
            if (!shader_source_has_main(source))
            {
                const auto length = source.size();
                hash_obj.update(relative);
                hash_obj.update(&length, sizeof(length));
                hash_obj.update(source);
            }
        }
        fingerprint = hash_obj.digest();
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void LLShaderMgr::hashShaderCacheFeatures()
{
    HBXXH128 hash_obj;
    mShaderFeatureKeys.clear();
    // Freeze aliases before program roots can overwrite them. Validate every
    // alias, but hash assembled keys only in programs that attach that module.
    for (const auto* aliases : { &mVertexShaderObjects, &mFragmentShaderObjects })
    {
        for (const auto& alias : *aliases)
        {
            const auto object = std::find_if(mShaderObjects.begin(), mShaderObjects.end(),
                [&alias](const auto& entry) { return entry.second == alias.second; });
            if (object == mShaderObjects.end())
            {
                disableShaderCache();
                return;
            }
            hash_obj.update(alias.first);
            const GLenum stage = aliases == &mVertexShaderObjects ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
            mShaderFeatureKeys[{ alias.first, stage }] = object->first;
        }
    }
    mShaderCacheFeatureHash = hash_obj.digest();
}

GLuint LLShaderMgr::shaderFeatureObject(const std::string& file, GLenum stage) const
{
    // Roots may republish an alias with different permutations after the basic
    // modules are frozen. Attachment must resolve the same object as hashing.
    const auto frozen = mShaderFeatureKeys.find({ file, stage });
    if (frozen != mShaderFeatureKeys.end())
    {
        const auto object = mShaderObjects.find(frozen->second);
        return object != mShaderObjects.end() ? object->second : 0;
    }
    const auto& aliases = stage == GL_VERTEX_SHADER ? mVertexShaderObjects : mFragmentShaderObjects;
    const auto alias = aliases.find(file);
    return alias != aliases.end() ? alias->second : 0;
}

LLUUID LLShaderMgr::shaderFeatureHash(LLGLSLShader* shader)
{
    std::vector<std::pair<std::string, GLenum>> files;
    attachShaderFeaturesInternal<false>(shader, &files);
    HBXXH128 hash_obj;
    for (const auto& file : files)
    {
        const auto key = mShaderFeatureKeys.find(file);
        if (key == mShaderFeatureKeys.end() || !shaderFeatureObject(file.first, file.second)) return LLUUID::null;
        hash_obj.update(file.first);
        hash_obj.update(&file.second, sizeof(file.second));
        hash_obj.update(key->second);
    }
    return hash_obj.digest();
}

void LLShaderMgr::initShaderCache(bool enabled, bool second_instance)
{
    LL_PROFILE_ZONE_SCOPED;
    LL_INFOS("ShaderMgr") << "Initializing shader cache" << LL_ENDL;

    mShaderCacheReadOnly = second_instance;
    if (!mShaderCacheScanAttempted)
    {
        mShaderCacheScanAttempted = true;
        mShaderCacheSourceValid = shader_shared_source_fingerprint(getShaderDirPrefix(), mSharedShaderFingerprint);
        if (!mShaderCacheSourceValid)
        {
            LL_WARNS("ShaderMgr") << "Shader source scan failed; binary cache disabled for this run" << LL_ENDL;
        }
    }
    mShaderCacheEnabled = gGLManager.mGLVersion >= 4.09 && enabled && mShaderCacheSourceValid && !mShaderInjectionActive;
    if (!mShaderCacheEnabled || mShaderCacheVersion.notNull()) return;

    mShaderCacheVersion = LLUUID(SHADER_CACHE_SCHEMA);
    mShaderCacheDir = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "shader_cache");
    if (!second_instance) LLFile::mkdir(mShaderCacheDir);
    const std::string meta_path = gDirUtilp->add(mShaderCacheDir, "shaderdata.llsd");
    llifstream instream(meta_path, std::ifstream::in | std::ifstream::binary);
    LLSD in_data;
    const bool metadata_readable = instream.is_open() &&
        LLSDSerialize::fromBinary(in_data, instream, LLSDSerialize::SIZE_UNLIMITED) > 0 &&
        in_data.isMap() && in_data["shaders"].isMap();
    instream.close();

    // The cache is authoritative even if the viewer never saved its settings.
    if (metadata_readable && in_data["version"].asUUID() == mShaderCacheVersion)
    {
        for (const auto& data_pair : llsd::inMap(in_data["shaders"]))
        {
            ProgramBinaryData binary_info;
            binary_info.mBinaryFormat = data_pair.second["binary_format"].asInteger();
            binary_info.mBinaryLength = data_pair.second["binary_size"].asInteger();
            binary_info.mLastUsedTime = (F32)data_pair.second["last_used"].asReal();
            mShaderBinaryCache.insert_or_assign(LLUUID(data_pair.first), binary_info);
        }
    }
    else if (!second_instance)
    {
        LL_INFOS("ShaderMgr") << "Shader cache schema mismatch or unreadable metadata. Purging." << LL_ENDL;
        clearShaderCache();
    }
}

void LLShaderMgr::clearShaderCache()
{
    std::string shader_cache = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "shader_cache");
    LL_INFOS("ShaderMgr") << "Removing shader cache at " << shader_cache << LL_ENDL;
    const std::string mask = "*";
    gDirUtilp->deleteFilesInDir(shader_cache, mask);
    mShaderBinaryCache.clear();
}

void LLShaderMgr::persistShaderCacheMetadata()
{
    if (!mShaderCacheEnabled || mShaderCacheReadOnly) return;
    if (mShaderCacheVersion.isNull())
    {
        LL_WARNS("ShaderMgr") << "Attempted to save shader cache with no version set" << LL_ENDL;
        return;
    }

    LL_INFOS("ShaderMgr") << "Persisting shader cache metadata to disk" << LL_ENDL;

    LLSD out;
    // Schema lives in metadata; settings may be stale after a crash.
    out["version"] = mShaderCacheVersion;
    out["shaders"] = LLSD::emptyMap();
    LLSD &shaders = out["shaders"];

    static const F32 LRU_TIME = (60.f * 60.f) * 24.f * 7.f; // 7 days
    const F32 current_time = (F32)LLTimer::getTotalSeconds();
    for (auto it = mShaderBinaryCache.begin(); it != mShaderBinaryCache.end();)
    {
        const ProgramBinaryData& shader_metadata = it->second;
        if ((shader_metadata.mLastUsedTime + LRU_TIME) < current_time)
        {
            std::string shader_path = gDirUtilp->add(mShaderCacheDir, it->first.asString() + ".shaderbin");
            LLFile::remove(shader_path);
            it = mShaderBinaryCache.erase(it);
        }
        else
        {
            LLSD data = LLSD::emptyMap();
            data["binary_format"] = LLSD::Integer(shader_metadata.mBinaryFormat);
            data["binary_size"] = LLSD::Integer(shader_metadata.mBinaryLength);
            data["last_used"] = LLSD::Real(shader_metadata.mLastUsedTime);
            shaders[it->first.asString()] = data;
            ++it;
        }
    }

    // A crash can leave binaries saved before their metadata. Reap these too;
    // otherwise they are invisible to the age-based LRU on subsequent launches.
    std::error_code scan_ec;
    namespace fs = std::filesystem;
    for (fs::directory_iterator it(fsyspath(mShaderCacheDir), scan_ec), end;
         !scan_ec && it != end; it.increment(scan_ec))
    {
        const auto& path = it->path();
        if (path.extension() == ".shaderbin" &&
            !shaders.has(path.stem().string()))
        {
            std::error_code remove_ec;
            fs::remove(path, remove_ec);
        }
    }

    std::string meta_out_path = gDirUtilp->add(mShaderCacheDir, "shaderdata.llsd");
    llofstream outstream(meta_out_path, std::ios_base::out | std::ios_base::binary);
    if (!outstream.is_open())
    {
        LL_WARNS("ShaderMgr") << "Failed to open file. Unable to save shader cache to: " << mShaderCacheDir << LL_ENDL;
        return;
    }
    LLSDSerialize::toBinary(out, outstream);
    outstream.close();
}

template<bool diagnostics, bool batching>
bool LLShaderMgr::loadCachedProgramBinaryInternal(LLGLSLShader* shader)
{
    if constexpr (diagnostics)
    {
        // Synthetic rejection enters the same rejection/compile-fallback path,
        // without reading a binary or touching its metadata in an injected run.
        mShaderAttempt["binary_outcome"] = "disabled";
        if (injectShaderFailure("binary", shader->mName))
        {
            mShaderAttempt["binary_outcome"] = "synthetic_rejected";
            return rejectProgramBinary(shader);
        }
    }
    // Fallback Actor FX programs share the primary program feature/hash key.
    // Ignore a cached primary binary while either primary module is missing,
    // otherwise cache loading can defeat the coupled degradation above.
    if (shader && (shader->mFeatures.hasActorFx || shader->mFeatures.hasActorFxShadow) &&
        (mFragmentShaderObjects.count("alchemy/actorFxDissolveF.glsl") == 0 ||
         mFragmentShaderObjects.count("alchemy/actorFxF.glsl") == 0))
    {
        return false;
    }

    // hash() has already checked this program's roots. Never use a partial key,
    // or a key made while the shared-source scan disabled caching.
    if (!mShaderCacheEnabled || !shader->mShaderCacheEligible) return false;

    glProgramParameteri(shader->mProgramObject, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

    auto binary_iter = mShaderBinaryCache.find(shader->mShaderHash);
    if constexpr (diagnostics) mShaderAttempt["binary_outcome"] = "miss";
    if (binary_iter != mShaderBinaryCache.end())
    {
        if constexpr (diagnostics) mShaderAttempt["binary_outcome"] = "read_failure";
        std::string in_path = gDirUtilp->add(mShaderCacheDir, shader->mShaderHash.asString() + ".shaderbin");
        auto& shader_info = binary_iter->second;
        if (shader_info.mBinaryLength > 0)
        {
            std::vector<U8> in_data;
            in_data.resize(shader_info.mBinaryLength);
            std::error_code ec;
            LLFile filep = LLFile(in_path, LLFile::in | LLFile::binary, ec);
            if (!ec && (bool)filep)
            {
                size_t result = filep.read(in_data.data(), in_data.size(), ec);
                filep.close();

                if (result == in_data.size())
                {
                    GLenum error = glGetError(); // Clear current error
                    glProgramBinary(shader->mProgramObject, shader_info.mBinaryFormat, in_data.data(), shader_info.mBinaryLength);
                    if constexpr (diagnostics)
                    {
                        mShaderAttempt["program_binary_ran"] = true;
                        mShaderAttempt["binary_outcome"] = "driver_rejected";
                    }

                    error = glGetError();
                    GLint success = GL_TRUE;
                    if constexpr (batching)
                    {
                        mShaderBatchJournal->binaryErrors[shader->mProgramObject] = error;
                        return true; // Issued binary; validation belongs to the barrier.
                    }
                    glGetProgramiv(shader->mProgramObject, GL_LINK_STATUS, &success);
                    if (error == GL_NO_ERROR && success == GL_TRUE)
                    {
                        if constexpr (diagnostics) mShaderAttempt["binary_outcome"] = "accepted";
                        binary_iter->second.mLastUsedTime = (F32)LLTimer::getTotalSeconds();
                        LL_INFOS() << "Loaded cached binary for shader: " << shader->mName << LL_ENDL;
                        return true;
                    }
                }
            }
        }
        //an error occured, normally we would print log but in this case it means the shader needs recompiling.
        return rejectProgramBinary(shader, in_path);
    }
    return false;
}

bool LLShaderMgr::loadCachedProgramBinary(LLGLSLShader* shader)
{
    return loadCachedProgramBinaryInternal<false>(shader);
}
template bool LLShaderMgr::loadCachedProgramBinaryInternal<true, false>(LLGLSLShader*);
template bool LLShaderMgr::loadCachedProgramBinaryInternal<false, false>(LLGLSLShader*);


bool LLShaderMgr::saveCachedProgramBinary(LLGLSLShader* shader)
{
    // Do not cache a fallback program under the primary key. A later run with
    // working modules must relink the real Actor FX implementation.
    if (shader && (shader->mFeatures.hasActorFx || shader->mFeatures.hasActorFxShadow) &&
        (mFragmentShaderObjects.count("alchemy/actorFxDissolveF.glsl") == 0 ||
         mFragmentShaderObjects.count("alchemy/actorFxF.glsl") == 0))
    {
        return true;
    }

    if (!mShaderCacheEnabled || mShaderCacheReadOnly || !shader->mShaderCacheEligible) return true;

    ProgramBinaryData binary_info = ProgramBinaryData();
    glGetProgramiv(shader->mProgramObject, GL_PROGRAM_BINARY_LENGTH, &binary_info.mBinaryLength);
    if (binary_info.mBinaryLength > 0)
    {
        std::vector<U8> program_binary;
        program_binary.resize(binary_info.mBinaryLength);

        GLenum error = glGetError(); // Clear current error
        glGetProgramBinary(shader->mProgramObject, static_cast<GLsizei>(program_binary.size() * sizeof(U8)), nullptr, &binary_info.mBinaryFormat, program_binary.data());
        error = glGetError();
        if (error == GL_NO_ERROR)
        {
            std::string out_path = gDirUtilp->add(mShaderCacheDir, shader->mShaderHash.asString() + ".shaderbin");
            std::error_code ec;
            LLFile filep = LLFile(out_path, LLFile::out | LLFile::binary, ec);
            if (filep)
            {
                filep.write(program_binary.data(), program_binary.size(), ec);
                filep.close();

                binary_info.mLastUsedTime = (F32)LLTimer::getTotalSeconds();

                mShaderBinaryCache.insert_or_assign(shader->mShaderHash, binary_info);
                return true;
            }
        }
    }
    return false;
}

//virtual
void LLShaderMgr::initAttribsAndUniforms()
{
    //MUST match order of enum in LLVertexBuffer.h
    mReservedAttribs.push_back("position");
    mReservedAttribs.push_back("normal");
    mReservedAttribs.push_back("texcoord0");
    mReservedAttribs.push_back("texcoord1");
    mReservedAttribs.push_back("texcoord2");
    mReservedAttribs.push_back("texcoord3");
    mReservedAttribs.push_back("diffuse_color");
    mReservedAttribs.push_back("emissive");
    mReservedAttribs.push_back("tangent");
    mReservedAttribs.push_back("weight");
    mReservedAttribs.push_back("weight4");
    mReservedAttribs.push_back("clothing");
    mReservedAttribs.push_back("joint");
    mReservedAttribs.push_back("texture_index");

    //matrix state
    mReservedUniforms.push_back("modelview_matrix");
    mReservedUniforms.push_back("projection_matrix");
    mReservedUniforms.push_back("inv_proj");
    mReservedUniforms.push_back("modelview_projection_matrix");
    mReservedUniforms.push_back("weather_material_enabled");
    mReservedUniforms.push_back("weather_material_excluded");
    mReservedUniforms.push_back("weather_surface_cover_matrix");
    mReservedUniforms.push_back("weather_material_view_to_agent");
    mReservedUniforms.push_back("weather_material_origin");
    mReservedUniforms.push_back("weather_material_wet");
    mReservedUniforms.push_back("weather_material_time_range");
    mReservedUniforms.push_back("inv_modelview");
    mReservedUniforms.push_back("identity_matrix");
    mReservedUniforms.push_back("normal_matrix");
    mReservedUniforms.push_back("texture_matrix0");
    mReservedUniforms.push_back("texture_matrix1");
    mReservedUniforms.push_back("texture_matrix2");
    mReservedUniforms.push_back("texture_matrix3");
    mReservedUniforms.push_back("object_plane_s");
    mReservedUniforms.push_back("object_plane_t");

    mReservedUniforms.push_back("texture_base_color_transform"); // (GLTF)
    mReservedUniforms.push_back("texture_normal_transform"); // (GLTF)
    mReservedUniforms.push_back("texture_metallic_roughness_transform"); // (GLTF)
    mReservedUniforms.push_back("texture_occlusion_transform"); // (GLTF)
    mReservedUniforms.push_back("texture_emissive_transform"); // (GLTF)
    mReservedUniforms.push_back("base_color_texcoord"); // (GLTF)
    mReservedUniforms.push_back("emissive_texcoord"); // (GLTF)
    mReservedUniforms.push_back("normal_texcoord"); // (GLTF)
    mReservedUniforms.push_back("metallic_roughness_texcoord"); // (GLTF)
    mReservedUniforms.push_back("occlusion_texcoord"); // (GLTF)

    mReservedUniforms.push_back("terrain_texture_transforms"); // (GLTF)

    llassert(mReservedUniforms.size() == LLShaderMgr::TERRAIN_TEXTURE_TRANSFORMS +1);

    mReservedUniforms.push_back("viewport");

    mReservedUniforms.push_back("local_light_size");
    mReservedUniforms.push_back("local_light_sizes");
    mReservedUniforms.push_back("local_light_spec_exponent");
    mReservedUniforms.push_back("light_position");
    mReservedUniforms.push_back("light_direction");
    mReservedUniforms.push_back("light_attenuation");
    mReservedUniforms.push_back("light_deferred_attenuation");
    mReservedUniforms.push_back("light_diffuse");
    mReservedUniforms.push_back("light_ambient");
    mReservedUniforms.push_back("light_count");
    mReservedUniforms.push_back("light");
    mReservedUniforms.push_back("light_col");
    mReservedUniforms.push_back("far_z");

    llassert(mReservedUniforms.size() == LLShaderMgr::MULTI_LIGHT_FAR_Z+1);

    //NOTE: MUST match order in eGLSLReservedUniforms
    mReservedUniforms.push_back("proj_mat");
    mReservedUniforms.push_back("proj_near");
    mReservedUniforms.push_back("proj_p");
    mReservedUniforms.push_back("proj_n");
    mReservedUniforms.push_back("proj_origin");
    mReservedUniforms.push_back("proj_range");
    mReservedUniforms.push_back("proj_ambiance");
    mReservedUniforms.push_back("proj_shadow_idx");
    mReservedUniforms.push_back("shadow_fade");
    mReservedUniforms.push_back("proj_focus");
    mReservedUniforms.push_back("proj_lod");
    mReservedUniforms.push_back("proj_ambient_lod");

    llassert(mReservedUniforms.size() == LLShaderMgr::PROJECTOR_AMBIENT_LOD+1);

    mReservedUniforms.push_back("color");
    mReservedUniforms.push_back("emissiveColor");
    mReservedUniforms.push_back("metallicFactor");
    mReservedUniforms.push_back("roughnessFactor");
    mReservedUniforms.push_back("mirror_flag");
    mReservedUniforms.push_back("clipPlane");
    mReservedUniforms.push_back("clipSign");

    mReservedUniforms.push_back("diffuseMap");
    mReservedUniforms.push_back("altDiffuseMap");
    mReservedUniforms.push_back("specularMap");
    mReservedUniforms.push_back("metallicRoughnessMap");
    mReservedUniforms.push_back("normalMap");
    mReservedUniforms.push_back("occlusionMap");
    mReservedUniforms.push_back("emissiveMap");
    mReservedUniforms.push_back("bumpMap");
    mReservedUniforms.push_back("bumpMap2");
    mReservedUniforms.push_back("environmentMap");
    mReservedUniforms.push_back("sceneMap");
    mReservedUniforms.push_back("prismGateMap");
    mReservedUniforms.push_back("prismLensMap");
    mReservedUniforms.push_back("sceneDepth");
    mReservedUniforms.push_back("reflectionProbes");
    mReservedUniforms.push_back("irradianceProbes");
    mReservedUniforms.push_back("heroProbes");
    mReservedUniforms.push_back("cloud_noise_texture");
    mReservedUniforms.push_back("cloud_noise_texture_next");
    mReservedUniforms.push_back("lightnorm");
    mReservedUniforms.push_back("sunlight_color");
    mReservedUniforms.push_back("ambient_color");
    mReservedUniforms.push_back("sky_hdr_scale");
    mReservedUniforms.push_back("sky_sunlight_scale");
    mReservedUniforms.push_back("sky_ambient_scale");
    mReservedUniforms.push_back("sky_gi_scale");     // [EnvIntensity] keep in lockstep with LLShaderMgr::SKY_GI_SCALE
    mReservedUniforms.push_back("sky_sun_ev_scale"); // [EnvIntensity] keep in lockstep with LLShaderMgr::SKY_SUN_EV_SCALE
    mReservedUniforms.push_back("classic_mode");
    mReservedUniforms.push_back("blue_horizon");
    mReservedUniforms.push_back("blue_density");
    mReservedUniforms.push_back("haze_horizon");
    mReservedUniforms.push_back("haze_density");
    mReservedUniforms.push_back("cloud_shadow");
    mReservedUniforms.push_back("density_multiplier");
    mReservedUniforms.push_back("distance_multiplier");
    mReservedUniforms.push_back("max_y");
    mReservedUniforms.push_back("glow");
    mReservedUniforms.push_back("cloud_color");
    mReservedUniforms.push_back("cloud_pos_density1");
    mReservedUniforms.push_back("cloud_pos_density2");
    mReservedUniforms.push_back("cloud_scale");
    mReservedUniforms.push_back("gamma");
    mReservedUniforms.push_back("scene_light_strength");

    llassert(mReservedUniforms.size() == LLShaderMgr::SCENE_LIGHT_STRENGTH+1);

    mReservedUniforms.push_back("center");
    mReservedUniforms.push_back("size");
    mReservedUniforms.push_back("falloff");

    mReservedUniforms.push_back("box_center");
    mReservedUniforms.push_back("box_size");


    mReservedUniforms.push_back("minLuminance");
    mReservedUniforms.push_back("maxExtractAlpha");
    mReservedUniforms.push_back("lumWeights");
    mReservedUniforms.push_back("warmthWeights");
    mReservedUniforms.push_back("warmthAmount");
    mReservedUniforms.push_back("glowStrength");
    mReservedUniforms.push_back("glowDelta");
    mReservedUniforms.push_back("glowNoiseMap");

    llassert(mReservedUniforms.size() == LLShaderMgr::GLOW_NOISE_MAP+1);

    mReservedUniforms.push_back("bloom_threshold");
    mReservedUniforms.push_back("bloom_knee");
    mReservedUniforms.push_back("bloom_texel_size");
    mReservedUniforms.push_back("bloom_scatter");
    mReservedUniforms.push_back("bloom_strength");
    mReservedUniforms.push_back("alpha_glow_boost");
    mReservedUniforms.push_back("bloomMap");
    mReservedUniforms.push_back("halationMap");
    mReservedUniforms.push_back("halation_strength");
    mReservedUniforms.push_back("halation_tint");
    mReservedUniforms.push_back("halation_lum_weights");

    llassert(mReservedUniforms.size() == LLShaderMgr::HALATION_LUM_WEIGHTS+1);


    mReservedUniforms.push_back("minimum_alpha");
    mReservedUniforms.push_back("emissive_brightness");

    // Deferred
    mReservedUniforms.push_back("shadow_matrix");
    mReservedUniforms.push_back("env_mat");
    mReservedUniforms.push_back("shadow_clip");
    mReservedUniforms.push_back("sun_wash");
    mReservedUniforms.push_back("shadow_noise");
    mReservedUniforms.push_back("blur_size");
    mReservedUniforms.push_back("ssao_radius");
    mReservedUniforms.push_back("ssao_max_radius");
    mReservedUniforms.push_back("ssao_factor");
    mReservedUniforms.push_back("ssao_factor_inv");
    mReservedUniforms.push_back("ssao_effect_mat");
    mReservedUniforms.push_back("screen_res");
    mReservedUniforms.push_back("near_clip");
    mReservedUniforms.push_back("shadow_offset");
    mReservedUniforms.push_back("shadow_bias");
    mReservedUniforms.push_back("spot_shadow_bias");
    mReservedUniforms.push_back("spot_shadow_offset");
    mReservedUniforms.push_back("sun_dir");
    mReservedUniforms.push_back("moon_dir");
    mReservedUniforms.push_back("shadow_res");
    mReservedUniforms.push_back("proj_shadow_res");
    mReservedUniforms.push_back("depth_cutoff");
    mReservedUniforms.push_back("norm_cutoff");
    mReservedUniforms.push_back("shadow_target_width");

    llassert(mReservedUniforms.size() == LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH + 1);

    mReservedUniforms.push_back("iterationCount");
    mReservedUniforms.push_back("rayStep");
    mReservedUniforms.push_back("distanceBias");
    mReservedUniforms.push_back("depthRejectBias");
    mReservedUniforms.push_back("glossySampleCount");
    mReservedUniforms.push_back("noiseSine");
    mReservedUniforms.push_back("adaptiveStepMultiplier");

    mReservedUniforms.push_back("modelview_delta");
    mReservedUniforms.push_back("inv_modelview_delta");
    mReservedUniforms.push_back("cube_snapshot");

    mReservedUniforms.push_back("tc_scale");
    mReservedUniforms.push_back("rcp_screen_res");
    mReservedUniforms.push_back("rcp_frame_opt");
    mReservedUniforms.push_back("rcp_frame_opt2");

    mReservedUniforms.push_back("focal_distance");
    mReservedUniforms.push_back("blur_constant");
    mReservedUniforms.push_back("tan_pixel_angle");
    mReservedUniforms.push_back("magnification");
    mReservedUniforms.push_back("max_cof");
    mReservedUniforms.push_back("res_scale");
    mReservedUniforms.push_back("dof_width");
    mReservedUniforms.push_back("dof_height");

    mReservedUniforms.push_back("depthMap");
    mReservedUniforms.push_back("shadowMap0");
    mReservedUniforms.push_back("shadowMap1");
    mReservedUniforms.push_back("shadowMap2");
    mReservedUniforms.push_back("shadowMap3");
    mReservedUniforms.push_back("shadowMap4");
    mReservedUniforms.push_back("shadowMap5");
    mReservedUniforms.push_back("shadowMap6"); // [BDMerge NSpot]
    mReservedUniforms.push_back("shadowMap7");
    mReservedUniforms.push_back("shadowMap8");
    mReservedUniforms.push_back("shadowMap9");
    mReservedUniforms.push_back("shadowMap10");
    mReservedUniforms.push_back("shadowMap11");
    mReservedUniforms.push_back("shadowMap12");
    mReservedUniforms.push_back("shadowMap13");
    mReservedUniforms.push_back("shadowMap14");
    mReservedUniforms.push_back("shadowMap15");
    mReservedUniforms.push_back("shadowMap16");
    mReservedUniforms.push_back("shadowMap17");
    mReservedUniforms.push_back("shadowMap18");
    mReservedUniforms.push_back("shadowMap19");

    llassert(mReservedUniforms.size() == LLShaderMgr::DEFERRED_SHADOW19+1);

    mReservedUniforms.push_back("positionMap");
    mReservedUniforms.push_back("diffuseRect");
    mReservedUniforms.push_back("specularRect");
    mReservedUniforms.push_back("emissiveRect");
    mReservedUniforms.push_back("exposureMap");
    mReservedUniforms.push_back("brdfLut");
    mReservedUniforms.push_back("noiseMap");
    mReservedUniforms.push_back("lightFunc");
    mReservedUniforms.push_back("lightMap");
    mReservedUniforms.push_back("projectionMap");
    mReservedUniforms.push_back("norm_mat");

    mReservedUniforms.push_back("specular_color");
    mReservedUniforms.push_back("env_intensity");

    mReservedUniforms.push_back("matrixPalette");
    mReservedUniforms.push_back("translationPalette");

    mReservedUniforms.push_back("screenTex");
    mReservedUniforms.push_back("screenDepth");
    mReservedUniforms.push_back("refTex");
    mReservedUniforms.push_back("exclusionTex");
    mReservedUniforms.push_back("eyeVec");
    mReservedUniforms.push_back("time");
    mReservedUniforms.push_back("waveDir1");
    mReservedUniforms.push_back("waveDir2");
    mReservedUniforms.push_back("lightDir");
    mReservedUniforms.push_back("specular");
    mReservedUniforms.push_back("lightExp");
    mReservedUniforms.push_back("waterFogColor");
    mReservedUniforms.push_back("waterFogColorLinear");
    mReservedUniforms.push_back("waterFogDensity");
    mReservedUniforms.push_back("waterFogKS");
    mReservedUniforms.push_back("refScale");
    mReservedUniforms.push_back("waterHeight");
    mReservedUniforms.push_back("waterPlane");
    mReservedUniforms.push_back("normScale");
    mReservedUniforms.push_back("fresnelScale");
    mReservedUniforms.push_back("fresnelOffset");
    mReservedUniforms.push_back("blurMultiplier");
    mReservedUniforms.push_back("sunAngle");
    mReservedUniforms.push_back("scaledAngle");
    mReservedUniforms.push_back("sunAngle2");

    mReservedUniforms.push_back("camPosLocal");
// [RLVa:KB] - @setsphere
    mReservedUniforms.push_back("rlvEffectMode");
    mReservedUniforms.push_back("rlvEffectParam1");
    mReservedUniforms.push_back("rlvEffectParam2");
    mReservedUniforms.push_back("rlvEffectParam3");
    mReservedUniforms.push_back("rlvEffectParam4");
    mReservedUniforms.push_back("rlvEffectParam5");
// [/RLV:KB]

    mReservedUniforms.push_back("gWindDir");
    mReservedUniforms.push_back("gSinWaveParams");
    mReservedUniforms.push_back("gGravity");

    mReservedUniforms.push_back("detail_0");
    mReservedUniforms.push_back("detail_1");
    mReservedUniforms.push_back("detail_2");
    mReservedUniforms.push_back("detail_3");

    mReservedUniforms.push_back("alpha_ramp");
    mReservedUniforms.push_back("paint_map");

    mReservedUniforms.push_back("detail_0_base_color");
    mReservedUniforms.push_back("detail_1_base_color");
    mReservedUniforms.push_back("detail_2_base_color");
    mReservedUniforms.push_back("detail_3_base_color");
    mReservedUniforms.push_back("detail_0_normal");
    mReservedUniforms.push_back("detail_1_normal");
    mReservedUniforms.push_back("detail_2_normal");
    mReservedUniforms.push_back("detail_3_normal");
    mReservedUniforms.push_back("detail_0_metallic_roughness");
    mReservedUniforms.push_back("detail_1_metallic_roughness");
    mReservedUniforms.push_back("detail_2_metallic_roughness");
    mReservedUniforms.push_back("detail_3_metallic_roughness");
    mReservedUniforms.push_back("detail_0_emissive");
    mReservedUniforms.push_back("detail_1_emissive");
    mReservedUniforms.push_back("detail_2_emissive");
    mReservedUniforms.push_back("detail_3_emissive");

    mReservedUniforms.push_back("baseColorFactors");
    mReservedUniforms.push_back("metallicFactors");
    mReservedUniforms.push_back("roughnessFactors");
    mReservedUniforms.push_back("emissiveColors");
    mReservedUniforms.push_back("minimum_alphas");

    mReservedUniforms.push_back("region_scale");

    mReservedUniforms.push_back("origin");
    mReservedUniforms.push_back("display_gamma");

    mReservedUniforms.push_back("inscatter");
    mReservedUniforms.push_back("sun_size");
    mReservedUniforms.push_back("fog_color");

    mReservedUniforms.push_back("transmittance_texture");
    mReservedUniforms.push_back("scattering_texture");
    mReservedUniforms.push_back("single_mie_scattering_texture");
    mReservedUniforms.push_back("irradiance_texture");
    mReservedUniforms.push_back("blend_factor");
    mReservedUniforms.push_back("moisture_level");
    mReservedUniforms.push_back("droplet_radius");
    mReservedUniforms.push_back("ice_level");
    mReservedUniforms.push_back("rainbow_map");
    mReservedUniforms.push_back("halo_map");
    mReservedUniforms.push_back("moon_brightness");
    mReservedUniforms.push_back("cloud_variance");
    mReservedUniforms.push_back("reflection_probe_ambiance");
    mReservedUniforms.push_back("max_probe_lod");
    mReservedUniforms.push_back("probe_strength");

    mReservedUniforms.push_back("sh_input_r");
    mReservedUniforms.push_back("sh_input_g");
    mReservedUniforms.push_back("sh_input_b");

    mReservedUniforms.push_back("sun_moon_glow_factor");
    mReservedUniforms.push_back("water_edge");
    mReservedUniforms.push_back("sun_up_factor");
    mReservedUniforms.push_back("moonlight_color");

    mReservedUniforms.push_back("debug_normal_draw_length");

    mReservedUniforms.push_back("edgesTex");
    mReservedUniforms.push_back("areaTex");
    mReservedUniforms.push_back("searchTex");
    mReservedUniforms.push_back("blendTex");
    mReservedUniforms.push_back("predicationTex");

    mReservedUniforms.push_back("exposure");
    mReservedUniforms.push_back("tonemap_type");
    mReservedUniforms.push_back("tonemap_mix");
    mReservedUniforms.push_back("tonemap_params");
    mReservedUniforms.push_back("tonemap_slog_gamut");
    mReservedUniforms.push_back("tonemap_slog_purity");
    mReservedUniforms.push_back("tonemap_amd");
    mReservedUniforms.push_back("tonemap_amd_shoulder");
    mReservedUniforms.push_back("tonemap_khronos_params");
    mReservedUniforms.push_back("tonemap_grade");

    // Alchemy Effects Stack
    mReservedUniforms.push_back("uFrameId");
    mReservedUniforms.push_back("uResolution");

    // Chromatic Aberration
    mReservedUniforms.push_back("uCAAmount");
    mReservedUniforms.push_back("uCAFalloff");
    mReservedUniforms.push_back("uCAAngleSinCos");
    mReservedUniforms.push_back("uCAOffsetR");
    mReservedUniforms.push_back("uCAOffsetB");
    mReservedUniforms.push_back("uCAAnisotropy");

    // Lens Flare
    mReservedUniforms.push_back("uLensFlareStrength");
    mReservedUniforms.push_back("uLensFlareSunPos");
    mReservedUniforms.push_back("uLensFlareSunVisibility");
    mReservedUniforms.push_back("uLensFlareStreakLength");
    mReservedUniforms.push_back("uLensFlareStreakFalloff");
    mReservedUniforms.push_back("uLensFlareStreakWidth");
    mReservedUniforms.push_back("uLensFlareStreakIntensity");
    mReservedUniforms.push_back("uLensFlareStreakTint");
    mReservedUniforms.push_back("uLensFlareChromaticSpread");
    mReservedUniforms.push_back("uLensFlareGlowRadius");
    mReservedUniforms.push_back("uLensFlareGlowFalloff");
    mReservedUniforms.push_back("uLensFlareGlow");
    mReservedUniforms.push_back("uLensFlareGhostCount");
    mReservedUniforms.push_back("uLensFlareGhostSpacing");
    mReservedUniforms.push_back("uLensFlareGhost");
    mReservedUniforms.push_back("uLensFlareHaloRadius");
    mReservedUniforms.push_back("uLensFlareHaloWidth");
    mReservedUniforms.push_back("uLensFlareHalo");
    mReservedUniforms.push_back("uLensFlareOcclusionRadius");
    mReservedUniforms.push_back("uLensFlareStarburst");
    mReservedUniforms.push_back("uLensFlareStarburstSpikes");
    mReservedUniforms.push_back("uLensFlareStarburstSharpness");
    mReservedUniforms.push_back("uLensFlareStarburstFalloff");
    mReservedUniforms.push_back("uLensFlareOcclusionTaps");
    mReservedUniforms.push_back("uLensFlareLightColor");

    // Cine rig flare sources (Group 1 — rig source arrays)
    mReservedUniforms.push_back("uCineFlareShaft");
    mReservedUniforms.push_back("uCineFlareHistory");
    mReservedUniforms.push_back("uCineFlareCount");
    mReservedUniforms.push_back("uCineFlareA");
    mReservedUniforms.push_back("uCineFlareColor");
    mReservedUniforms.push_back("uCineFlareThreshold");
    mReservedUniforms.push_back("uCineFlareProbeRadius");

    // Lens flare polished-stack additions (Group 2 — element intensities)
    mReservedUniforms.push_back("uLensFlareGhostChroma");
    mReservedUniforms.push_back("uLensFlareHaloChroma");
    mReservedUniforms.push_back("uLensFlareIris");
    mReservedUniforms.push_back("uLensFlareRing");
    mReservedUniforms.push_back("uLensFlareCircle");
    mReservedUniforms.push_back("uLensFlareArc");
    mReservedUniforms.push_back("uLensFlareWarp");
    mReservedUniforms.push_back("uLensFlareStreakTipAmount");
    mReservedUniforms.push_back("uLensFlareSrcColorAmount");

    // Lens flare polished-stack shape params (Group 3)
    mReservedUniforms.push_back("uLensFlareIrisCount");
    mReservedUniforms.push_back("uLensFlareIrisSides");
    mReservedUniforms.push_back("uLensFlareIrisSize");
    mReservedUniforms.push_back("uLensFlareRingRadius");
    mReservedUniforms.push_back("uLensFlareRingWidth");
    mReservedUniforms.push_back("uLensFlareRingDispersion");
    mReservedUniforms.push_back("uLensFlareRingCount");
    mReservedUniforms.push_back("uLensFlareCircleScale");
    mReservedUniforms.push_back("uLensFlareCircleSpacing");
    mReservedUniforms.push_back("uLensFlareCircleCount");
    mReservedUniforms.push_back("uLensFlareStreakTipTint");

    // Lens flare master (Group 4)
    mReservedUniforms.push_back("uLensFlareMaster");

    // Graduated ND (pre-tonemap exposure region)
    mReservedUniforms.push_back("gradnd_params");
    mReservedUniforms.push_back("gradnd_params2");
    mReservedUniforms.push_back("gradnd_sun");

    // Polarizer (pre-tonemap on-lens filter; shares gradnd_sun)
    mReservedUniforms.push_back("polarizer_params");
    mReservedUniforms.push_back("polarizer_params2");

    // Night Mask (subject-anchored darkness mask; pre-tonemap on-lens filter)
    mReservedUniforms.push_back("night_mask_params");
    mReservedUniforms.push_back("night_mask_params2");
    mReservedUniforms.push_back("night_mask_anchor_view");
    mReservedUniforms.push_back("night_mask_box_basis");
    mReservedUniforms.push_back("night_mask_tint");
    mReservedUniforms.push_back("night_mask_shape");
    mReservedUniforms.push_back("night_mask_inv_proj");

    // Color Correction LUT
    mReservedUniforms.push_back("uColorGradeLut");
    mReservedUniforms.push_back("uColorGradeLutSize");
    mReservedUniforms.push_back("uColorGradeLutStrength");

    // Linear-space grading (pre-tonemap)
    mReservedUniforms.push_back("uWhiteBalanceGain");
    mReservedUniforms.push_back("uLift");
    mReservedUniforms.push_back("uInvGammaCC");
    mReservedUniforms.push_back("uGain");

    // Split toning
    mReservedUniforms.push_back("uShadowRatio");
    mReservedUniforms.push_back("uHighlightRatio");
    mReservedUniforms.push_back("uMidtoneRatio");
    mReservedUniforms.push_back("uMidtoneAmount");
    mReservedUniforms.push_back("uSplitToneMid");
    mReservedUniforms.push_back("uToneAmount");

    // Display-space grading
    mReservedUniforms.push_back("uBWPScale");
    mReservedUniforms.push_back("uBWPBias");
    mReservedUniforms.push_back("uBCScale");
    mReservedUniforms.push_back("uBCBias");
    mReservedUniforms.push_back("uHighlightsScaled");
    mReservedUniforms.push_back("uShadowsScaled");
    mReservedUniforms.push_back("uSaturation");
    mReservedUniforms.push_back("uVibrance");
    mReservedUniforms.push_back("uHueShiftNorm");

    // Per-channel filmic curves
    mReservedUniforms.push_back("uCurveToe");
    mReservedUniforms.push_back("uCurveInvRange");
    mReservedUniforms.push_back("uCurveStrength");

    // Vignette
    mReservedUniforms.push_back("uVignetteAmount");
    mReservedUniforms.push_back("uVignetteRadius");
    mReservedUniforms.push_back("uVignetteSoft");
    mReservedUniforms.push_back("uVignetteShape");
    mReservedUniforms.push_back("uVignetteColor");
    mReservedUniforms.push_back("uVignetteMidColor");
    mReservedUniforms.push_back("uVignetteMidPoint");
    mReservedUniforms.push_back("uVignetteCenter");
    mReservedUniforms.push_back("uVignetteAspect");
    mReservedUniforms.push_back("uVignetteFeather");

    // CVD
    mReservedUniforms.push_back("uCompensateMode");
    mReservedUniforms.push_back("uCompensateAmount");

    // Film Grain
    mReservedUniforms.push_back("uGrainAmount");
    mReservedUniforms.push_back("uGrainStyle");
    mReservedUniforms.push_back("uGrainSize");
    mReservedUniforms.push_back("uGrainRange");
    mReservedUniforms.push_back("uGrainTint");
    mReservedUniforms.push_back("uGrainAnimate");

    // Dithering
    mReservedUniforms.push_back("uDitherAmount");
    mReservedUniforms.push_back("uDitherBits");
    mReservedUniforms.push_back("uDitherAnimate");

    // Previews
    mReservedUniforms.push_back("uPreviewMode");

    // End Alchemy Effects Stack

    // [BDMerge G3.2] volumetric lighting (donor: Black Dragon)
    mReservedUniforms.push_back("godray_res");
    mReservedUniforms.push_back("godray_multiplier");
    mReservedUniforms.push_back("falloff_multiplier");

    // [BDMerge G3.3] per-projector volumetric light cones
    mReservedUniforms.push_back("projvol_g");

    // [BDMerge G3.3 Phase 1] projector-volumetric cinema levers
    mReservedUniforms.push_back("projvol_feather");
    mReservedUniforms.push_back("projvol_shadow_samples");
    mReservedUniforms.push_back("projvol_dither");
    mReservedUniforms.push_back("projvol_frame");
    mReservedUniforms.push_back("projvol_max");

    // [BDMerge G3.3 Phase 3] atmosphere levers
    mReservedUniforms.push_back("projvol_density");
    mReservedUniforms.push_back("projvol_noise_strength");
    mReservedUniforms.push_back("projvol_noise_scale");
    mReservedUniforms.push_back("projvol_noise_speed");
    mReservedUniforms.push_back("projvol_time");
    mReservedUniforms.push_back("projvol_fog_strength");
    mReservedUniforms.push_back("projvol_fog_ground_density");
    mReservedUniforms.push_back("projvol_fog_falloff");
    mReservedUniforms.push_back("projvol_fog_base");
    mReservedUniforms.push_back("projvol_inv_modelview");
    mReservedUniforms.push_back("projvol_bloom_feed");
    mReservedUniforms.push_back("projvol_bloom_anamorphic");

    // [BDMerge G3.3 Batch 1] temporal reprojection (A) + gobo-colored shadows (B)
    mReservedUniforms.push_back("projvol_shadow_tint");
    mReservedUniforms.push_back("projvol_history");
    mReservedUniforms.push_back("projvol_prev_viewproj");
    mReservedUniforms.push_back("projvol_prev_modelview");
    mReservedUniforms.push_back("projvol_temporal_blend");

    // [BDMerge G3.3 Batch B] temporal correctness (R2 contrast reject, R1 beam depth)
    mReservedUniforms.push_back("projvol_temporal_reject");
    mReservedUniforms.push_back("projvol_temporal_beam_depth");

    // [BDMerge G3.3 Rim] surface-coupled rim / wrap glow
    mReservedUniforms.push_back("projvol_rim_strength");
    mReservedUniforms.push_back("projvol_rim_power");
    mReservedUniforms.push_back("projvol_rim_threshold");
    mReservedUniforms.push_back("projvol_rim_wrap");
    mReservedUniforms.push_back("projvol_rim_softness"); // [F4] skin softness
    mReservedUniforms.push_back("projvol_wind"); // [F5] shared dust-wind * cone NoiseSpeed

    // [BDMerge G3.3 Batch A] look-neutral performance (E1 frustum clip, E2 jitter tap)
    mReservedUniforms.push_back("projvol_frustum_clip");
    mReservedUniforms.push_back("projvol_frustum_planes");
    mReservedUniforms.push_back("projvol_shadow_jitter_tap");

    // [BDMerge G3.3 ConservativeShadow] airborne-march shadow sampler A/B gate
    mReservedUniforms.push_back("projvol_conservative_shadow");

    // [BDMerge G3.3 Dust] baked 64^3 dust-volume particulate breakup
    mReservedUniforms.push_back("projvol_dust");
    mReservedUniforms.push_back("projvol_dust_intensity");
    mReservedUniforms.push_back("projvol_dust_scale");
    mReservedUniforms.push_back("projvol_dust_wind");
    mReservedUniforms.push_back("projvol_dust_map");

    // [BDMerge Batch 2] Feature 1: soft shadows
    mReservedUniforms.push_back("soft_shadow_enable");
    mReservedUniforms.push_back("soft_shadow_scale");
    mReservedUniforms.push_back("soft_shadow_max");
    mReservedUniforms.push_back("soft_shadow_fill");
    mReservedUniforms.push_back("soft_shadow_sun");
    // [Vogel A/B] soft-shadow kernel selector + Vogel tap count (lockstep with
    // the SOFT_SHADOW_VOGEL / SOFT_SHADOW_TAPS enum block in llshadermgr.h)
    mReservedUniforms.push_back("soft_shadow_vogel");
    mReservedUniforms.push_back("soft_shadow_taps");
    mReservedUniforms.push_back("spot_shadow_softness");
    // [BDMerge Batch 2] Feature 2: gobo anisotropic/mip filtering
    mReservedUniforms.push_back("gobo_aniso");
    // [Cinematic Gobo] Per-projector procedural cookie and presentation-time UV motion.
    mReservedUniforms.push_back("gobo_time");
    mReservedUniforms.push_back("gobo_pattern");
    mReservedUniforms.push_back("gobo_anim_params");
    // [Gobo v2] colored gel tint + per-pattern variation params.
    mReservedUniforms.push_back("gobo_tint");
    mReservedUniforms.push_back("gobo_pattern_params");

    // [Cine Outline Phase 1] deferred normal/depth outline post pass.
    mReservedUniforms.push_back("outline_color");
    mReservedUniforms.push_back("outline_params");
    mReservedUniforms.push_back("outline_params2");

    // [Pedro Cam Phase 1] fisheye / peephole late post pass.
    mReservedUniforms.push_back("fisheye_params");
    mReservedUniforms.push_back("fisheye_params2");

    // [BDMerge A5.4-1a] velocity / motion-vector buffer (must stay in lockstep
    // with the enum block in llshadermgr.h)
    mReservedUniforms.push_back("current_modelview_matrix");
    mReservedUniforms.push_back("last_modelview_matrix");
    mReservedUniforms.push_back("last_object_matrix");
    mReservedUniforms.push_back("projection_matrix_unjittered");
    mReservedUniforms.push_back("velocityMap");
    mReservedUniforms.push_back("motion_blur_strength");
    // [BDMerge A5.4-1b] previous-frame skinning palettes
    mReservedUniforms.push_back("lastMatrixPalette");

    // [BDMerge Froxel F0] hybrid froxel volumetrics (must stay in lockstep with the
    // FROXEL_* enum block in llshadermgr.h - same position, same order).
    mReservedUniforms.push_back("froxelMedia");
    mReservedUniforms.push_back("froxel_grid");
    mReservedUniforms.push_back("froxel_atlas");
    mReservedUniforms.push_back("froxel_near_far");
    mReservedUniforms.push_back("froxel_tan_half_fov");
    mReservedUniforms.push_back("froxel_inv_modelview");
    mReservedUniforms.push_back("froxel_density");
    mReservedUniforms.push_back("froxel_fog_strength");
    mReservedUniforms.push_back("froxel_fog_ground_density");
    mReservedUniforms.push_back("froxel_fog_falloff");
    mReservedUniforms.push_back("froxel_fog_base");
    mReservedUniforms.push_back("froxel_noise_strength");
    mReservedUniforms.push_back("froxel_noise_scale");
    mReservedUniforms.push_back("froxel_noise_speed");
    mReservedUniforms.push_back("froxel_wind"); // [F5] wind_dir * noise_speed
    mReservedUniforms.push_back("froxel_time");
    // [Local Fog] Flattened eight-volume media arrays.
    mReservedUniforms.push_back("localfog_center");
    mReservedUniforms.push_back("localfog_extents");
    mReservedUniforms.push_back("localfog_inv_rot");
    mReservedUniforms.push_back("localfog_params");
    mReservedUniforms.push_back("localfog_tint");
    mReservedUniforms.push_back("localfog_noise");
    mReservedUniforms.push_back("localfog_count");
    mReservedUniforms.push_back("froxel_debug_mode");
    mReservedUniforms.push_back("froxel_debug_slice");

    // [BDMerge Froxel F1] integrate + apply (lockstep with the FROXEL_INTEGRATED /
    // FROXEL_AMBIENT enums appended to the FROXEL_* block in llshadermgr.h).
    mReservedUniforms.push_back("froxelIntegrated");
    mReservedUniforms.push_back("froxel_ambient");

    // [BDMerge Froxel F2] per-light injection (lockstep with the FROXEL_LIGHT /
    // FROXEL_LIGHT_ENABLE enums appended to the FROXEL_* block in llshadermgr.h).
    mReservedUniforms.push_back("froxelLight");
    mReservedUniforms.push_back("froxel_light_enable");

    // [BDMerge Froxel F3] temporal accumulation (lockstep with the FROXEL_LIGHT_HISTORY
    // / FROXEL_PREV_MODELVIEW / FROXEL_TEMPORAL_BLEND / FROXEL_JITTER / FROXEL_FRAME
    // enums appended to the FROXEL_* block in llshadermgr.h - same position, same order).
    mReservedUniforms.push_back("froxelLightHistory");
    mReservedUniforms.push_back("froxel_prev_modelview");
    mReservedUniforms.push_back("froxel_temporal_blend");
    mReservedUniforms.push_back("froxel_temporal_reject");
    mReservedUniforms.push_back("froxel_jitter");
    mReservedUniforms.push_back("froxel_frame");

    mReservedUniforms.push_back("weather_rain_map");
    mReservedUniforms.push_back("weather_rain_occlusion_map");

    // [Ultimate Diopter] must stay in lockstep with the enum block in llshadermgr.h
    mReservedUniforms.push_back("diopter_shape");
    mReservedUniforms.push_back("diopter_shape2");
    mReservedUniforms.push_back("diopter_shape3");
    mReservedUniforms.push_back("diopter_shape4");
    mReservedUniforms.push_back("diopter_shape5");
    mReservedUniforms.push_back("diopter_shape6");
    mReservedUniforms.push_back("diopter_shape7");
    mReservedUniforms.push_back("diopter_focus");
    mReservedUniforms.push_back("diopter_focus2");
    mReservedUniforms.push_back("diopter_focus3");
    mReservedUniforms.push_back("diopter_focus4");
    mReservedUniforms.push_back("diopter_aperture");
    mReservedUniforms.push_back("diopter_aperture2");
    mReservedUniforms.push_back("diopter_glass");
    mReservedUniforms.push_back("diopter_glass2");
    mReservedUniforms.push_back("diopter_halo");
    mReservedUniforms.push_back("diopter_halo2");
    mReservedUniforms.push_back("diopter_pattern");
    mReservedUniforms.push_back("diopter_comp");
    mReservedUniforms.push_back("diopter_comp2");
    mReservedUniforms.push_back("diopter_ghost");
    mReservedUniforms.push_back("diopter_ghost2");

    // [Ultimate Diopter] warped present depth for ReShade alignment (lockstep
    // with the enum block appended in llshadermgr.h)
    mReservedUniforms.push_back("diopterWarpMap");
    mReservedUniforms.push_back("depth_warp_params");

    // [Ultimate Kaleidoscope] (lockstep with the enum block in llshadermgr.h)
    mReservedUniforms.push_back("kal_pattern");
    mReservedUniforms.push_back("kal_shape");
    mReservedUniforms.push_back("kal_center");
    mReservedUniforms.push_back("kal_source");
    mReservedUniforms.push_back("kal_fx");
    mReservedUniforms.push_back("kal_protect");
    mReservedUniforms.push_back("kal_depth");
    mReservedUniforms.push_back("kal_wave");
    mReservedUniforms.push_back("kal_look");
    mReservedUniforms.push_back("kal_cell");
    mReservedUniforms.push_back("kal_cell2");

    // [Cine Haze] Cinematic Depth Atmosphere (lockstep with the enum block
    // appended in llshadermgr.h)
    mReservedUniforms.push_back("cine_haze_active");
    mReservedUniforms.push_back("cine_haze_params");
    mReservedUniforms.push_back("cine_haze_height");
    mReservedUniforms.push_back("cine_haze_up");
    mReservedUniforms.push_back("cine_haze_color");
    mReservedUniforms.push_back("cine_haze_color_mode");
    mReservedUniforms.push_back("cine_haze_sun");
    mReservedUniforms.push_back("cine_haze_debug");
    mReservedUniforms.push_back("cine_haze_additive");

    // [RigRim] Rig Rim Light (lockstep with the enum block appended in
    // llshadermgr.h)
    mReservedUniforms.push_back("rig_rim_mode");
    mReservedUniforms.push_back("rig_rim_globals");
    mReservedUniforms.push_back("rig_rim_light");
    mReservedUniforms.push_back("rig_rim_lights");
    mReservedUniforms.push_back("rig_rim_shadow"); // [RigRim] Rim shadow

    // [RotoInk] Rotoscope Ink post pass (cineOutlineF.glsl); lockstep with
    // the ROTO_* block at the end of the enum in llshadermgr.h.
    mReservedUniforms.push_back("rotoScene");
    mReservedUniforms.push_back("roto_line");
    mReservedUniforms.push_back("roto_ink");
    mReservedUniforms.push_back("roto_subject");
    mReservedUniforms.push_back("roto_subject2");
    mReservedUniforms.push_back("roto_subject3");
    mReservedUniforms.push_back("roto_sketch");
    mReservedUniforms.push_back("roto_sketch2");
    mReservedUniforms.push_back("roto_motion");
    mReservedUniforms.push_back("roto_motion2");

    // [RotoInk Anim] animated patterns + extended subject isolation;
    // lockstep with the ROTO_MOTION3.. block at the end of the enum.
    mReservedUniforms.push_back("roto_motion3");
    mReservedUniforms.push_back("roto_motion4");
    mReservedUniforms.push_back("roto_motion5");
    mReservedUniforms.push_back("roto_pattern");
    mReservedUniforms.push_back("roto_pattern2");
    mReservedUniforms.push_back("roto_color2");
    mReservedUniforms.push_back("roto_color2b");
    mReservedUniforms.push_back("roto_subject4");
    mReservedUniforms.push_back("roto_targets");
    mReservedUniforms.push_back("roto_targets2");

    // [TronT1] Tron World post pass (tronWorldF.glsl); lockstep with the
    // TRON_* block at the end of the enum in llshadermgr.h. TRON_TRACE* /
    // TRON_RIM* are T2 reservations.
    mReservedUniforms.push_back("tronScene");
    mReservedUniforms.push_back("tron_inv_proj");
    mReservedUniforms.push_back("tron_inv_modelview");
    mReservedUniforms.push_back("tron_cam_rel");
    mReservedUniforms.push_back("tron_master");
    mReservedUniforms.push_back("tron_palette0");
    mReservedUniforms.push_back("tron_palette1");
    mReservedUniforms.push_back("tron_palette2");
    mReservedUniforms.push_back("tron_palette3");
    mReservedUniforms.push_back("tron_grade");
    mReservedUniforms.push_back("tron_grade2");
    mReservedUniforms.push_back("tron_grade3");
    mReservedUniforms.push_back("tron_grid");
    mReservedUniforms.push_back("tron_grid2");
    mReservedUniforms.push_back("tron_grid3");
    mReservedUniforms.push_back("tron_grid4");
    mReservedUniforms.push_back("tron_grid5");
    mReservedUniforms.push_back("tron_water");
    mReservedUniforms.push_back("tron_pulse");
    mReservedUniforms.push_back("tron_pulse2");
    mReservedUniforms.push_back("tron_subject");
    mReservedUniforms.push_back("tron_subject2");
    mReservedUniforms.push_back("tron_targets");
    mReservedUniforms.push_back("tron_targets2");
    mReservedUniforms.push_back("tron_anchors");
    mReservedUniforms.push_back("tron_anchor_count");
    mReservedUniforms.push_back("tron_lattice_r");
    mReservedUniforms.push_back("tron_lattice_k");
    mReservedUniforms.push_back("tron_trace");   // [T2 reserved]
    mReservedUniforms.push_back("tron_trace2");  // [T2 reserved]
    mReservedUniforms.push_back("tron_trace3");  // [T2 reserved]
    mReservedUniforms.push_back("tron_rim");     // [T2 reserved]
    mReservedUniforms.push_back("tron_rim2");    // [T2 reserved]
    mReservedUniforms.push_back("tron_rim3");    // [T2 reserved]

    // [TronT1] Rig Rim "Tron" palette tint (deferredUtil.glsl rigRimTerm());
    // lockstep with RIG_RIM_TRON / RIG_RIM_TRON_COLOR in llshadermgr.h.
    mReservedUniforms.push_back("rig_rim_tron");
    mReservedUniforms.push_back("rig_rim_tron_color");

    // [TronT3] Light-cycle trail ribbons (deferred/tronTrailF.glsl);
    // lockstep with TRON_TRAIL_COLOR..TRON_TRAIL_PARAMS3 in llshadermgr.h.
    mReservedUniforms.push_back("tron_trail_color");
    mReservedUniforms.push_back("tron_trail_params");
    mReservedUniforms.push_back("tron_trail_params2");
    mReservedUniforms.push_back("tron_trail_params3");

    // [EnvIntensity v2] exposure strip uniforms; lockstep with
    // SKY_AMB_SCALE..SHADOW_LIFT_GAIN in llshadermgr.h (same order).
    mReservedUniforms.push_back("sky_amb_scale");       // SKY_AMB_SCALE
    mReservedUniforms.push_back("sky_probe_rad_scale"); // SKY_PROBE_RAD_SCALE
    mReservedUniforms.push_back("sky_sun_tint");        // SKY_SUN_TINT
    mReservedUniforms.push_back("sky_amb_tint");        // SKY_AMB_TINT
    mReservedUniforms.push_back("shadow_lift_gain");    // SHADOW_LIFT_GAIN
    mReservedUniforms.push_back("actorMaskMap");        // [ActorMask] ACTOR_MASK_MAP
    mReservedUniforms.push_back("actor_mask_bits");     // [ActorMask] ACTOR_MASK_BITS

    // [ProbeCost] Append in lockstep with llshadermgr.h.
    mReservedUniforms.push_back("shadow_probe_mode");
    mReservedUniforms.push_back("shadow_reuse_matrix");
    mReservedUniforms.push_back("shadow_reuse_valid");
    mReservedUniforms.push_back("shadow_reuse_res");
    mReservedUniforms.push_back("shadow_reuse_res_h");
    mReservedUniforms.push_back("render_debug_lighting_term");
    mReservedUniforms.push_back("rig_rim_quality");
    mReservedUniforms.push_back("rig_rim_sil");
    mReservedUniforms.push_back("rig_rim_aux_params");
    mReservedUniforms.push_back("rigRimAuxMap");
    mReservedUniforms.push_back("rigRimSilMap");
    mReservedUniforms.push_back("rig_rim_wrap");
    mReservedUniforms.push_back("rig_rim_wrap2");
    mReservedUniforms.push_back("rigRimWrapSrc");
    mReservedUniforms.push_back("rig_rim_smooth_params");
    mReservedUniforms.push_back("rigRimSmoothMap");
    mReservedUniforms.push_back("rigRimBarrierMap");
    mReservedUniforms.push_back("rigRimEdgeMap");
    mReservedUniforms.push_back("rig_rim_smooth_stage");
    mReservedUniforms.push_back("onLensAlbedoMap");
    mReservedUniforms.push_back("lensPyramid0");
    mReservedUniforms.push_back("lensPyramid1");
    mReservedUniforms.push_back("lensPyramid2");
    mReservedUniforms.push_back("lensPyramid3");
    mReservedUniforms.push_back("lensPyramid4");

    mReservedUniforms.push_back("light_shape");
    mReservedUniforms.push_back("light_shapes");
    mReservedUniforms.push_back("area_shadow_map0");
    mReservedUniforms.push_back("area_shadow_map1");
    mReservedUniforms.push_back("area_source");
    mReservedUniforms.push_back("relight_enabled");
    mReservedUniforms.push_back("area_world_atlas");

    // Append cover after all existing samplers: scalar diffuseMap must retain
    // unit zero for tree/grass draws which bind their diffuse texture directly.
    mReservedUniforms.push_back("weather_surface_cover_map");
    mReservedUniforms.push_back("weather_shelter_close_map");
    mReservedUniforms.push_back("weather_shelter_distant_map");
    mReservedUniforms.push_back("weather_surface_cover_min_map");
    mReservedUniforms.push_back("weather_shelter_close_min_map");
    mReservedUniforms.push_back("weather_shelter_distant_min_map");
    mReservedUniforms.push_back("weather_veil_map");

    mReservedUniforms.push_back("env_only_mode");
    mReservedUniforms.push_back("avatar_tag_draw");
    mReservedUniforms.push_back("gbuffer_tag_preferred");
    mReservedUniforms.push_back("local_light_ev_scale");
    mReservedUniforms.push_back("local_light_ev_scales");
    mReservedUniforms.push_back("ssao_irradiance_max_stock");
    mReservedUniforms.push_back("ghostSceneMap");

    llassert(mReservedUniforms.size() == END_RESERVED_UNIFORMS);

    std::set<std::string> dupe_check;

    for (U32 i = 0; i < mReservedUniforms.size(); ++i)
    {
        if (dupe_check.find(mReservedUniforms[i]) != dupe_check.end())
        {
            LL_ERRS() << "Duplicate reserved uniform name found: " << mReservedUniforms[i] << LL_ENDL;
        }
        dupe_check.insert(mReservedUniforms[i]);
    }
}

void LLShaderMgr::configureShaderDiagnostics(bool dump, const std::string& injection, bool second_instance)
{
    // Entries follow the setting; only cache disabling is latched for the process.
    std::vector<std::string> errors;
    mShaderInjections = LLShaderInjection::parse(injection, errors);
    for (const auto& error : errors)
        LL_WARNS("ShaderLoading") << "Invalid shader failure injection: " << error << LL_ENDL;
    if (!injection.empty())
    {
        mShaderInjectionActive = true;
        if (!mShaderInjectionWarned)
        {
            mShaderInjectionWarned = true;
            LL_WARNS("ShaderLoading") << "Shader failure injection active: binary cache reads and writes disabled for this run" << LL_ENDL;
        }
        mShaderCacheEnabled = false;
    }
    if (dump && second_instance && !mShaderDumpSecondWarned)
    {
        mShaderDumpSecondWarned = true;
        LL_WARNS("ShaderLoading") << "Second instance: skipping shader equivalence dump to protect the first instance's file" << LL_ENDL;
    }
    mShaderDumpRequested = dump && !second_instance;
    mShaderDiagnosticsEnabled = mShaderDumpRequested || !injection.empty();
}

void LLShaderMgr::openShaderEquivalence()
{
    if (!mShaderDumpRequested || mShaderDumpOpened) return;
    mShaderDumpOpened = true; // Exactly one truncation attempt, even on I/O failure.
    const auto path = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "shader_equivalence.jsonl");
    mShaderEquivalence.open(fsyspath(path), std::ios::out | std::ios::trunc);
    if (!mShaderEquivalence)
    {
        LL_WARNS("ShaderLoading") << "Cannot open shader equivalence dump: " << path << LL_ENDL;
        return;
    }
    LLSD header = LLSD::emptyMap();
    header["type"] = "run";
    header["vendor"] = gGLManager.mGLVendor;
    header["renderer"] = gGLManager.mGLRenderer;
    header["version"] = gGLManager.mGLVersionString;
    header["epoch"] = (S32)SHADER_CACHE_EPOCH;
    header["cache_enabled"] = mShaderCacheEnabled;
    mShaderEquivalence << LlsdToJson(header) << '\n' << std::flush;
}

bool LLShaderMgr::injectShaderFailure(const std::string& kind, const std::string& target, S32 value) const
{
    for (const auto& item : mShaderInjections)
        if (item.kind == kind && item.target == target && (value < 0 || item.value == value)) return true;
    return false;
}

bool LLShaderMgr::rejectProgramBinary(LLGLSLShader* shader, const std::string& cache_path)
{
    if (mShaderBatchJournal)
    {
        mShaderBatchJournal->failed = true;
        return false; // Replay serially before modifying the rejected disk entry.
    }
    // Used by both real binary rejection and the synthetic rejection. Injection
    // never reads a binary, touches metadata, or removes a cache entry.
    LL_INFOS() << "Failed to load cached binary for shader: " << shader->mName << " falling back to compilation" << LL_ENDL;
    if (!cache_path.empty())
    {
        if (!mShaderCacheReadOnly) LLFile::remove(cache_path);
        mShaderBinaryCache.erase(shader->mShaderHash);
    }
    return false;
}

LLShaderMgr::ShaderAttemptScope::ShaderAttemptScope(LLShaderMgr* mgr,
    std::unique_ptr<LLShaderAttemptContext>& context) : mMgr(mgr), mContext(context.get())
{
    if (!mMgr) return;
    if (!context) context = std::make_unique<LLShaderAttemptContext>();
    mContext = context.get();
    mPreviousAttempt = mMgr->mShaderAttempt;
    mPreviousOwnsPass = mMgr->mShaderAttemptOwnsPass;
    mMgr->mShaderAttempt = mContext->attempt;
    mMgr->mShaderAttemptOwnsPass = mContext->ownsPass;

}

LLShaderMgr::ShaderAttemptScope::~ShaderAttemptScope()
{
    if (!mMgr) return;
    mContext->attempt = mMgr->mShaderAttempt;
    mContext->ownsPass = mMgr->mShaderAttemptOwnsPass;
    mMgr->mShaderAttempt = mPreviousAttempt;
    mMgr->mShaderAttemptOwnsPass = mPreviousOwnsPass;
}

void LLShaderMgr::beginShaderAttempt(LLGLSLShader* shader)
{
    mShaderAttemptOwnsPass = !mShaderPassActive;
    if (mShaderAttemptOwnsPass)
    {
        beginShaderPass("standalone_program");
        mShaderStandalonePass = true;
    }
    ++mShaderAttemptsActive;
    mShaderAttempt = LLSD::emptyMap();
    mShaderAttempt["type"] = "program";
    mShaderAttempt["name"] = shader->mName;
    mShaderAttempt["attempt"] = ++mShaderAttemptNumbers[shader->mName];
    mShaderAttempt["requested_level"] = shader->mShaderLevel;
    mShaderAttempt["stages"] = LLSD::emptyArray();
    mShaderAttempt["attachments"] = LLSD::emptyArray();
    mShaderAttempt["link_outcome"] = "not_attempted";
    mShaderAttempt["binary_outcome"] = "disabled";
    mShaderAttempt["program_binary_ran"] = false;
    mShaderAttempt["cache_key"] = LLSD();
    mShaderAttempt["epoch_neutral_key"] = LLSD();
    snapshotShaderAttempt(shader);
}

void LLShaderMgr::snapshotShaderAttempt(LLGLSLShader* shader, bool capture_key)
{
    mShaderAttempt["shader_group"] = shader->mShaderGroup;
    LLSD features = LLSD::emptyMap();
    features["mIndexedTextureChannels"] = shader->mFeatures.mIndexedTextureChannels;
    features["mIndexedMaterialChannels"] = shader->mFeatures.mIndexedMaterialChannels;
    features["calculatesLighting"] = shader->mFeatures.calculatesLighting;
    features["calculatesAtmospherics"] = shader->mFeatures.calculatesAtmospherics;
    features["hasLighting"] = shader->mFeatures.hasLighting;
    features["isAlphaLighting"] = shader->mFeatures.isAlphaLighting;
    features["isSpecular"] = shader->mFeatures.isSpecular;
    features["hasTransport"] = shader->mFeatures.hasTransport;
    features["hasSkinning"] = shader->mFeatures.hasSkinning;
    features["hasObjectSkinning"] = shader->mFeatures.hasObjectSkinning;
    features["mGLTF"] = shader->mFeatures.mGLTF;
    features["hasAtmospherics"] = shader->mFeatures.hasAtmospherics;
    features["hasGamma"] = shader->mFeatures.hasGamma;
    features["hasShadows"] = shader->mFeatures.hasShadows;
    features["hasAmbientOcclusion"] = shader->mFeatures.hasAmbientOcclusion;
    features["hasSrgb"] = shader->mFeatures.hasSrgb;
    features["isDeferred"] = shader->mFeatures.isDeferred;
    features["hasFullGBuffer"] = shader->mFeatures.hasFullGBuffer;
    features["hasScreenSpaceReflections"] = shader->mFeatures.hasScreenSpaceReflections;
    features["hasAlphaMask"] = shader->mFeatures.hasAlphaMask;
    features["hasReflectionProbes"] = shader->mFeatures.hasReflectionProbes;
    features["attachNothing"] = shader->mFeatures.attachNothing;
    features["hasHeroProbes"] = shader->mFeatures.hasHeroProbes;
    features["isPBRTerrain"] = shader->mFeatures.isPBRTerrain;
    features["hasTonemap"] = shader->mFeatures.hasTonemap;
    features["hasColorGrade"] = shader->mFeatures.hasColorGrade;
    features["hasPostEffects"] = shader->mFeatures.hasPostEffects;
    features["hasActorFx"] = shader->mFeatures.hasActorFx;
    features["hasWeatherMaterial"] = shader->mFeatures.hasWeatherMaterial;
    features["hasActorFxShadow"] = shader->mFeatures.hasActorFxShadow;
    mShaderAttempt["features"] = features;
    LLSD defines = LLSD::emptyMap();
    for (const auto& entry : shader->mDefines) defines[entry.first] = entry.second;
    mShaderAttempt["defines"] = defines;
    LLSD global_defines = LLSD::emptyMap();
    for (const auto& entry : LLGLSLShader::sGlobalDefines) global_defines[entry.first] = entry.second;
    mShaderAttempt["global_defines"] = global_defines;
    if (capture_key)
    {
        mShaderAttempt["cache_key"] = shader->mShaderHash.asString();
        if (mShaderDumpRequested)
        {
            const bool eligible = shader->mShaderCacheEligible;
            mShaderAttempt["epoch_neutral_key"] = shader->hashWithEpoch(0).asString();
            shader->mShaderCacheEligible = eligible;
        }
    }
    mShaderAttempt["eligible"] = shader->mShaderCacheEligible;
    LLSD files = LLSD::emptyArray();
    for (const auto& entry : shader->mShaderFiles)
    {
        LLSD file = LLSD::emptyMap();
        file["file"] = entry.first;
        file["stage"] = (S32)entry.second;
        files.append(file);
    }
    mShaderAttempt["shader_files"] = files;
}

void LLShaderMgr::beginShaderStage(const std::string& file, GLenum type, S32 requested)
{
    mShaderStageOwnsPass = !mShaderPassActive;
    if (mShaderStageOwnsPass) beginShaderPass("standalone_module");
    mShaderStage = LLSD::emptyMap();
    mShaderStage["file"] = file;
    mShaderStage["stage"] = (S32)type;
    mShaderStage["requested_level"] = requested;
    mShaderStage["source_level"] = LLSD();
    mShaderStage["source_hash"] = LLSD();
    mShaderStage["object_key"] = LLSD();
}

void LLShaderMgr::assembledShaderStage(const std::string& key, const std::string& source_hash, S32 source_level)
{
    mShaderStage["source_level"] = source_level;
    mShaderStage["source_hash"] = source_hash;
    mShaderStage["object_key"] = key;
}

void LLShaderMgr::finishShaderStage(bool success, S32 resolved, GLuint object, const char* outcome)
{
    mShaderStage["resolved_level"] = resolved;
    mShaderStage["compile_outcome"] = outcome;
    mShaderStage["success"] = success;
    if (success && object) mDiagnosticObjects[object] = mShaderStage;
    if (mShaderAttempt.isMap()) mShaderAttempt["stages"].append(mShaderStage);
    else
    {
        mShaderStage["type"] = "module_load";
        ++mShaderPassModules;
        writeShaderRecord(mShaderStage);
    }
    if (mShaderStageOwnsPass) finishShaderPass();
}

void LLShaderMgr::traceAttachment(GLuint object)
{
    LLSD attachment = LLSD::emptyMap();
    auto found = mDiagnosticObjects.find(object);
    if (found == mDiagnosticObjects.end())
    {
        // Fail visibly if an attachment escaped the instrumented load path.
        attachment["untracked"] = true;
    }
    else
        for (const char* key : {"file", "stage", "requested_level", "resolved_level", "source_level", "object_key", "source_hash", "compile_outcome"}) attachment[key] = found->second[key];
    attachment["outcome"] = "attached";
    mShaderAttempt["attachments"].append(attachment);
}

void LLShaderMgr::traceFeatureAttachment(const std::string& file, GLenum type, const char* outcome)
{
    // A request is not an attachment. Actual glAttachShader calls are traced
    // only after the call, including the cached vertex path.
    LLSD attachment = LLSD::emptyMap();
    attachment["file"] = file;
    attachment["stage"] = (S32)type;
    attachment["outcome"] = outcome;
    mShaderAttempt["attachments"].append(attachment);
}

void LLShaderMgr::traceLink(bool success)
{
    mShaderAttempt["link_outcome"] = success ? "success" : "failure";
}

void LLShaderMgr::injectShaderOverflow(LLGLSLShader* shader)
{
    for (auto& item : mShaderInjections)
        if (item.kind == "overflow" && item.target == shader->mName)
        {
            shader->mActiveTextureChannels = item.value;
            // Consume each overflow entry once, so the caller's real reduced-
            // sampler retry can recover. Other forms are persistent.
            item.kind.clear();
            break;
        }
}

void LLShaderMgr::finishShaderAttempt(LLGLSLShader* shader, bool success)
{
    snapshotShaderAttempt(shader);
    mShaderAttempt["resolved_level"] = shader->mShaderLevel;
    mShaderAttempt["used_binary"] = shader->mUsingBinaryProgram;
    // An accepted binary never called glLinkProgram in this attempt.
    mShaderAttempt["root_load_outcome"] = shader->mUsingBinaryProgram ? "skipped" :
        (mShaderAttempt["stages"].size() ? "attempted" : "not_attempted");
    mShaderAttempt["success"] = success;
    LLSD attributes = LLSD::emptyMap();
    for (size_t i = 0; i < shader->mAttribute.size(); ++i)
        attributes[mReservedAttribs[i]] = (S32)shader->mAttribute[i];
    mShaderAttempt["attributes"] = attributes;
    mShaderAttempt["attribute_mask"] = LLSD::Real(shader->mAttributeMask);
    LLSD uniforms = LLSD::emptyArray();
    for (size_t i = 0; i < shader->mUniform.size(); ++i)
    {
        LLSD uniform = LLSD::emptyMap();
        uniform["name"] = mReservedUniforms[i];
        uniform["location"] = shader->mUniform[i];
        uniforms.append(uniform);
    }
    mShaderAttempt["uniforms"] = uniforms;
    LLSD uniform_map = LLSD::emptyMap(); // LLSD maps serialize in name order.
    for (const auto& entry : shader->mUniformMap) uniform_map[entry.first.String()] = entry.second;
    mShaderAttempt["uniform_map"] = uniform_map;
    LLSD textures = LLSD::emptyArray();
    for (GLint texture : shader->mTexture) textures.append(texture);
    mShaderAttempt["texture"] = textures;
    mShaderAttempt["active_texture_channels"] = shader->mActiveTextureChannels;
    LLSD blocks = LLSD::emptyMap();
    LLSD samplers = LLSD::emptyMap();
    if (success)
    {
        // Reflection-only queries; no bind or assignment. Include all active
        // blocks/samplers, including names outside the reserved engine tables.
        GLint count = 0;
        GLint max_length = 0;
        glGetProgramiv(shader->mProgramObject, GL_ACTIVE_ATTRIBUTES, &count);
        glGetProgramiv(shader->mProgramObject, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_length);
        std::vector<GLchar> name(llmax(max_length, 1));
        for (GLint i = 0; i < count; ++i)
        {
            GLsizei length = 0;
            GLint size = 0;
            GLenum type = 0;
            glGetActiveAttrib(shader->mProgramObject, i, (GLsizei)name.size(), &length, &size, &type, name.data());
            attributes[std::string(name.data(), length)] = glGetAttribLocation(shader->mProgramObject, name.data());
        }
        mShaderAttempt["attributes"] = attributes;
        glGetProgramiv(shader->mProgramObject, GL_ACTIVE_UNIFORM_BLOCKS, &count);
        glGetProgramiv(shader->mProgramObject, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &max_length);
        name.resize(llmax(max_length, 1));
        for (GLint i = 0; i < count; ++i)
        {
            GLsizei length = 0;
            glGetActiveUniformBlockName(shader->mProgramObject, i, (GLsizei)name.size(), &length, name.data());
            GLint binding = 0;
            glGetActiveUniformBlockiv(shader->mProgramObject, i, GL_UNIFORM_BLOCK_BINDING, &binding);
            blocks[std::string(name.data(), length)] = binding;
        }
        glGetProgramiv(shader->mProgramObject, GL_ACTIVE_UNIFORMS, &count);
        glGetProgramiv(shader->mProgramObject, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_length);
        name.resize(llmax(max_length, 1));
        for (GLint i = 0; i < count; ++i)
        {
            GLsizei length = 0;
            GLint size = 0;
            GLenum type = 0;
            glGetActiveUniform(shader->mProgramObject, i, (GLsizei)name.size(), &length, &size, &type, name.data());
            // Include every GL sampler family, including unreserved samplers
            // still carrying the driver's default unit assignment.
            switch (type)
            {
            case GL_SAMPLER_1D: case GL_SAMPLER_2D: case GL_SAMPLER_3D:
            case GL_SAMPLER_CUBE: case GL_SAMPLER_2D_RECT: case GL_SAMPLER_2D_SHADOW:
            case GL_SAMPLER_1D_SHADOW: case GL_SAMPLER_CUBE_SHADOW:
            case GL_SAMPLER_1D_ARRAY: case GL_SAMPLER_2D_ARRAY:
            case GL_SAMPLER_1D_ARRAY_SHADOW: case GL_SAMPLER_2D_ARRAY_SHADOW:
            case GL_SAMPLER_2D_RECT_SHADOW: case GL_SAMPLER_BUFFER: case GL_INT_SAMPLER_BUFFER:
            case GL_UNSIGNED_INT_SAMPLER_BUFFER: case GL_SAMPLER_2D_MULTISAMPLE:
            case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
            case GL_INT_SAMPLER_1D: case GL_INT_SAMPLER_2D: case GL_INT_SAMPLER_3D:
            case GL_INT_SAMPLER_CUBE: case GL_INT_SAMPLER_2D_RECT:
            case GL_INT_SAMPLER_1D_ARRAY: case GL_INT_SAMPLER_2D_ARRAY:
            case GL_INT_SAMPLER_2D_MULTISAMPLE: case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
            case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
            case GL_UNSIGNED_INT_SAMPLER_1D: case GL_UNSIGNED_INT_SAMPLER_2D:
            case GL_UNSIGNED_INT_SAMPLER_3D: case GL_UNSIGNED_INT_SAMPLER_CUBE:
            case GL_UNSIGNED_INT_SAMPLER_2D_RECT: case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
            case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY: case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
            case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
            case GL_SAMPLER_CUBE_MAP_ARRAY: case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
                {
                    std::string base(name.data(), length);
                    if (base.size() > 3 && base.substr(base.size() - 3) == "[0]") base.resize(base.size() - 3);
                    for (GLint j = 0; j < size; ++j)
                    {
                        std::string element = size > 1 ? base + "[" + std::to_string(j) + "]" : base;
                        GLint location = glGetUniformLocation(shader->mProgramObject, element.c_str());
                        GLint unit = -1;
                        if (location >= 0) glGetUniformiv(shader->mProgramObject, location, &unit);
                        samplers[element] = unit;
                    }
                }
                break;
            default: break;
            }
        }
    }
    mShaderAttempt["ubo_bindings"] = blocks;
    mShaderAttempt["samplers"] = samplers;
    ++mShaderPassPrograms;
    writeShaderRecord(mShaderAttempt);
    mShaderAttempt = LLSD();
    // Overlapping standalone attempts share one journal pass. Its initiating
    // job can finish first; close only after every participating job finishes.
    llassert_always(mShaderAttemptsActive > 0);
    if (--mShaderAttemptsActive == 0 && mShaderStandalonePass)
    {
        finishShaderPass();
        mShaderStandalonePass = false;
    }
}

void LLShaderMgr::writeShaderRecord(LLSD record)
{
    if (!mShaderDumpRequested) return;
    if (!record.has("pass")) record["pass"] = mShaderPassNumber;
    if (mShaderBatchJournal)
    {
        mShaderBatchJournal->records.push_back(std::move(record));
        return;
    }
    if (mShaderEquivalence.is_open())
    {
        mShaderEquivalence << LlsdToJson(record) << '\n' << std::flush;
        if (!mShaderEquivalence)
            LL_WARNS("ShaderLoading") << "Shader equivalence dump write failed" << LL_ENDL;
    }
}

void LLShaderMgr::beginShaderPass(const std::string& name)
{
    openShaderEquivalence(); // First shader operation: truncate once, before any work.
    mShaderPassActive = true;
    ++mShaderPassNumber;
    mShaderPassPrograms = mShaderPassModules = 0;
    LLSD record = LLSD::emptyMap();
    record["type"] = "pass_begin";
    record["name"] = name;
    writeShaderRecord(record);
}

void LLShaderMgr::finishShaderPass()
{
    LLSD record = LLSD::emptyMap();
    record["type"] = "pass_complete";
    record["program_attempts"] = mShaderPassPrograms;
    record["module_attempts"] = mShaderPassModules;
    writeShaderRecord(record);
    mShaderPassActive = false;
}

template GLuint LLShaderMgr::loadShaderFileInternal<true, true>(const std::string&, S32&, GLenum, std::map<std::string, std::string>*, S32, std::vector<LLShaderCompileJob>*);
template GLuint LLShaderMgr::loadShaderFileInternal<false, true>(const std::string&, S32&, GLenum, std::map<std::string, std::string>*, S32, std::vector<LLShaderCompileJob>*);

void LLShaderMgr::beginShaderBatch()
{
    llassert_always(!mShaderBatchJournal);
    llassert_always(mShaderAttemptsActive == 0);
    // Keep cache metadata even when diagnostics are disabled: speculative saves
    // must not turn a serial replay into a binary-cache hit.
    mShaderBatchJournal = std::make_unique<ShaderBatchJournal>();
    auto& saved = *mShaderBatchJournal;
    saved.binaries = mShaderBinaryCache;
    saved.avatarAttachments = sAvatarLightResponseAttachCount;
    if (!mShaderDiagnosticsEnabled) return;
    openShaderEquivalence();
    saved.attempts = mShaderAttemptNumbers;
    saved.objects = mDiagnosticObjects;
    saved.injections = mShaderInjections;
    saved.programs = mShaderPassPrograms;
    saved.modules = mShaderPassModules;
    saved.pass = mShaderPassNumber;
    saved.passActive = mShaderPassActive;
    saved.stage = mShaderStage;
}

void LLShaderMgr::endShaderBatch(bool success)
{
    auto saved = std::move(mShaderBatchJournal);
    llassert_always(saved);
    if (!success)
    {
        mShaderBinaryCache = std::move(saved->binaries);
        sAvatarLightResponseAttachCount = saved->avatarAttachments;
        if (!mShaderDiagnosticsEnabled) return;
        llassert_always(mShaderAttemptsActive == 0);
        mShaderAttemptNumbers = std::move(saved->attempts);
        mDiagnosticObjects = std::move(saved->objects);
        mShaderInjections = std::move(saved->injections);
        mShaderPassPrograms = saved->programs;
        mShaderPassModules = saved->modules;
        mShaderPassNumber = saved->pass;
        mShaderPassActive = saved->passActive;
        mShaderStage = saved->stage;
        mShaderStageOwnsPass = false;
    }
    else for (auto& record : saved->records) writeShaderRecord(std::move(record));
}

template<bool diagnostics>
bool LLShaderMgr::finishBatchBinary(LLGLSLShader* shader)
{
    GLint success = GL_FALSE;
    glGetProgramiv(shader->mProgramObject, GL_LINK_STATUS, &success);
    if (mShaderBatchJournal->binaryErrors.at(shader->mProgramObject) != GL_NO_ERROR || success != GL_TRUE)
    {
        mShaderBatchJournal->failed = true;
        return false;
    }
    if constexpr (diagnostics) mShaderAttempt["binary_outcome"] = "accepted";
    mShaderBinaryCache.at(shader->mShaderHash).mLastUsedTime = (F32)LLTimer::getTotalSeconds();
    LL_INFOS() << "Loaded cached binary for shader: " << shader->mName << LL_ENDL;
    return true;
}
template bool LLShaderMgr::finishBatchBinary<true>(LLGLSLShader*);
template bool LLShaderMgr::finishBatchBinary<false>(LLGLSLShader*);
template bool LLShaderMgr::loadCachedProgramBinaryInternal<true, true>(LLGLSLShader*);
template bool LLShaderMgr::loadCachedProgramBinaryInternal<false, true>(LLGLSLShader*);
