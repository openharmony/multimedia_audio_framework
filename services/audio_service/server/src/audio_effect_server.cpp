/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <iostream>
#include <vector>
#include "functional"
#include "memory"
#include <dlfcn.h>
#include "unistd.h"
#include "audio_log.h"
#include "audio_effect_server.h"

namespace OHOS {
namespace AudioStandard {

#ifdef __aarch64__
    constexpr const char *LD_EFFECT_LIBRARY_PATH[] = {"/system/lib64/"};
#else
    constexpr const char *LD_EFFECT_LIBRARY_PATH[] = {"/system/lib/"};
#endif
static constexpr int32_t SLEEP_TIME_FOR_AUDIO_EFFECT = 1;

bool ResolveLibrary(const std::string &path, std::string &resovledPath)
{
    for (auto *libDir: LD_EFFECT_LIBRARY_PATH) {
        std::string candidatePath = std::string(libDir) + "/" + path;
        if (access(candidatePath.c_str(), R_OK) == 0) {
            resovledPath = std::move(candidatePath);
            return true;
        }
    }

    return false;
}

static bool LoadLibrary(const std::string &relativePath, std::unique_ptr<AudioEffectLibEntry>& libEntry) noexcept
{
    std::string absolutePath;
    // find library in adsolutePath
    if (!ResolveLibrary(relativePath, absolutePath)) {
        AUDIO_ERR_LOG("<log error> find library falied in effect directories: %{public}s",
            relativePath.c_str());
        return false;
    }

    void* handle = dlopen(absolutePath.c_str(), 1);
    if (!handle) {
        AUDIO_ERR_LOG("<log error> dlopen lib %{public}s Fail", relativePath.c_str());
        return false;
    } else {
        AUDIO_INFO_LOG("<log info> dlopen lib %{public}s successful", relativePath.c_str());
    }

    AudioEffectLibrary *audioEffectLibHandle = static_cast<AudioEffectLibrary *>(dlsym(handle,
        AUDIO_EFFECT_LIBRARY_INFO_SYM_AS_STR));
    const char* error = dlerror();
    if (error) {
        AUDIO_ERR_LOG("<log error> dlsym failed: error: %{public}s, %{public}p", error, audioEffectLibHandle);
        dlclose(handle);
        return false;
    } else {
        AUDIO_INFO_LOG("<log info> dlsym lib %{public}s successful, error: %{public}s", relativePath.c_str(), error);
    }

    libEntry->audioEffectLibHandle = audioEffectLibHandle;

    return true;
}

void LoadLibraries(const std::vector<Library> &libs, std::vector<std::unique_ptr<AudioEffectLibEntry>> &libList)
{
    for (Library library: libs) {
        AUDIO_INFO_LOG("<log info> loading %{public}s : %{public}s", library.name.c_str(), library.path.c_str());

        std::unique_ptr<AudioEffectLibEntry> libEntry = std::make_unique<AudioEffectLibEntry>();
        libEntry->libraryName = library.name;

        bool loadLibrarySuccess = LoadLibrary(library.path, libEntry);
        if (!loadLibrarySuccess) {
            AUDIO_ERR_LOG("<log error> loadLibrary fail, please check logs!");
            continue;
        }

        // Register library load success
        libList.emplace_back(std::move(libEntry));
    }
}

AudioEffectLibEntry *FindLibrary(const std::string &name,
    const std::vector<std::unique_ptr<AudioEffectLibEntry>> &libList)
{
    for (const std::unique_ptr<AudioEffectLibEntry> &lib : libList) {
        if (lib->libraryName == name) {
            return lib.get();
        }
    }

    return nullptr;
}

static bool LoadEffect(const Effect &effect, const std::vector<std::unique_ptr<AudioEffectLibEntry>> &libList)
{
    AudioEffectLibEntry *currentLibEntry = FindLibrary(effect.libraryName, libList);
    if (currentLibEntry == nullptr) {
        AUDIO_ERR_LOG("<log error> could not find library %{public}s to load effect %{public}s",
                      effect.libraryName.c_str(), effect.name.c_str());
        return false;
    }
    // check effect
    AudioEffectDescriptor descriptor;
    descriptor.libraryName = effect.libraryName;
    descriptor.effectName = effect.name;

    bool ret = currentLibEntry->audioEffectLibHandle->checkEffect(descriptor);
    if (ret) {
        currentLibEntry->effectName.push_back(effect.name);
    } else {
        AUDIO_ERR_LOG("<log error> the effect %{public}s in lib %{public}s, open check file!",
            effect.name.c_str(), effect.libraryName.c_str());
        return false;
    }

    return true;
}

void CheckEffects(const std::vector<Effect> &effects, std::vector<std::unique_ptr<AudioEffectLibEntry>> &libList,
    std::vector<Effect> &successEffectList)
{
    for (Effect effect: effects) {
        bool ret = LoadEffect(effect, libList);
        if (!ret) {
            AUDIO_ERR_LOG("<log error> LoadEffects have failures, please check log!");
            continue;
        }

        successEffectList.push_back(effect);
    }
}

AudioEffectServer::AudioEffectServer()
{
    AUDIO_INFO_LOG("AudioEffectServer ctor");
}

AudioEffectServer::~AudioEffectServer()
{
}

bool AudioEffectServer::LoadAudioEffects(const std::vector<Library> libraries, const std::vector<Effect> effects,
                                         std::vector<Effect> &successEffectList)
{
    // load library
    sleep(SLEEP_TIME_FOR_AUDIO_EFFECT);
    LoadLibraries(libraries, effectLibEntries);

    // check effects
    CheckEffects(effects, effectLibEntries, successEffectList);
    if (successEffectList.size() > 0) {
        return true;
    } else {
        return false;
    }
}

std::vector<std::unique_ptr<AudioEffectLibEntry>> &AudioEffectServer::GetEffectEntries()
{
    return effectLibEntries;
}

} // namespce AudioStandard
} // namespace OHOS
