/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "audio_renderer_file_sink.h"

#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <string>
#include <unistd.h>

#include "audio_errors.h"
#include "audio_log.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
AudioRendererFileSink::AudioRendererFileSink()
{
}

AudioRendererFileSink::~AudioRendererFileSink()
{
    AudioRendererFileSink::DeInit();
}

AudioRendererFileSink *AudioRendererFileSink::GetInstance()
{
    static AudioRendererFileSink audioRenderer;

    return &audioRenderer;
}

void AudioRendererFileSink::DeInit()
{
    if (filePtr_ != nullptr) {
        fclose(filePtr_);
        filePtr_ = nullptr;
    }
}

void InitAttrs(struct AudioSampleAttributes &attrs)
{
}

int32_t AudioRendererFileSink::Init(const char *filePath)
{
    filePath_.assign(filePath);

    return SUCCESS;
}

int32_t AudioRendererFileSink::RenderFrame(char &data, uint64_t len, uint64_t &writeLen)
{
    if (filePtr_ == nullptr) {
        AUDIO_ERR_LOG("Invalid file ptr");
        return ERROR;
    }

    size_t writeResult = fwrite(static_cast<void*>(&data), 1, len, filePtr_);
    if (writeResult != len) {
        AUDIO_ERR_LOG("Failed to write the file.");
    }

    writeLen = writeResult;

    return SUCCESS;
}

int32_t AudioRendererFileSink::Start(void)
{
    char realPath[PATH_MAX + 1] = {0x00};
    std::string rootPath;
    std::string fileName;

    auto pos = filePath_.rfind("/");
    if (pos!= std::string::npos) {
        rootPath = filePath_.substr(0, pos);
        fileName = filePath_.substr(pos);
    }

    if (filePtr_ == nullptr) {
        if ((filePath_.length() >= PATH_MAX) || (realpath(rootPath.c_str(), realPath) == nullptr)) {
            AUDIO_ERR_LOG("AudioRendererFileSink:: Invalid path  errno = %{public}d", errno);
            return ERROR;
        }

        std::string verifiedPath(realPath);
        filePtr_ = fopen(verifiedPath.append(fileName).c_str(), "wb+");
        CHECK_AND_RETURN_RET_LOG(filePtr_ != nullptr, ERROR, "Failed to open file, errno = %{public}d", errno);
    }

    return SUCCESS;
}

int32_t AudioRendererFileSink::Stop(void)
{
    if (filePtr_ != nullptr) {
        fclose(filePtr_);
        filePtr_ = nullptr;
    }

    return SUCCESS;
}

int32_t AudioRendererFileSink::Pause(void)
{
    return SUCCESS;
}

int32_t AudioRendererFileSink::Resume(void)
{
    return SUCCESS;
}

int32_t AudioRendererFileSink::Reset(void)
{
    return SUCCESS;
}

int32_t AudioRendererFileSink::Flush(void)
{
    return SUCCESS;
}

int32_t AudioRendererFileSink::SetVolume(float left, float right)
{
    return ERR_NOT_SUPPORTED;
}

int32_t AudioRendererFileSink::GetLatency(uint32_t *latency)
{
    return ERR_NOT_SUPPORTED;
}

int32_t AudioRendererFileSink::GetTransactionId(uint64_t *transactionId)
{
    AUDIO_ERR_LOG("AudioRendererFileSink %{public}s", __func__);
    return ERR_NOT_SUPPORTED;
}
} // namespace AudioStandard
} // namespace OHOS

#ifdef __cplusplus
extern "C" {
#endif

using namespace OHOS::AudioStandard;

AudioRendererFileSink *g_fileSinkInstance = AudioRendererFileSink::GetInstance();
int32_t FillinAudioRenderFileSinkWapper(const char *deviceNetworkId, void **wapper)
{
    AudioRendererFileSink *instance = AudioRendererFileSink::GetInstance();
    if (instance != nullptr) {
        *wapper = static_cast<void *>(instance);
    } else {
        *wapper = nullptr;
        return ERROR;
    }

    return SUCCESS;
}

int32_t AudioRendererFileSinkInit(void *wapper, const char *filePath)
{
    AudioRendererFileSink *fileSinkInstance = static_cast<AudioRendererFileSink *>(wapper);
    CHECK_AND_RETURN_RET_LOG(fileSinkInstance != nullptr, ERR_INVALID_HANDLE, "null audioRendererSink");
    return fileSinkInstance->Init(filePath);
}

void AudioRendererFileSinkDeInit(void *wapper)
{
    AudioRendererFileSink *fileSinkInstance = static_cast<AudioRendererFileSink *>(wapper);
    CHECK_AND_RETURN_LOG(fileSinkInstance != nullptr, "null audioRendererSink");
    fileSinkInstance->DeInit();
}

int32_t AudioRendererFileSinkStop(void *wapper)
{
    AudioRendererFileSink *fileSinkInstance = static_cast<AudioRendererFileSink *>(wapper);
    CHECK_AND_RETURN_RET_LOG(fileSinkInstance != nullptr, ERR_INVALID_HANDLE, "null audioRendererSink");
    return fileSinkInstance->Stop();
}

int32_t AudioRendererFileSinkStart(void *wapper)
{
    AudioRendererFileSink *fileSinkInstance = static_cast<AudioRendererFileSink *>(wapper);
    CHECK_AND_RETURN_RET_LOG(fileSinkInstance != nullptr, ERR_INVALID_HANDLE, "null audioRendererSink");
    return fileSinkInstance->Start();
}

int32_t AudioRendererFileSinkPause(void *wapper)
{
    AudioRendererFileSink *fileSinkInstance = static_cast<AudioRendererFileSink *>(wapper);
    CHECK_AND_RETURN_RET_LOG(fileSinkInstance != nullptr, ERR_INVALID_HANDLE, "null audioRendererSink");
    return fileSinkInstance->Pause();
}

int32_t AudioRendererFileSinkResume(void *wapper)
{
    AudioRendererFileSink *fileSinkInstance = static_cast<AudioRendererFileSink *>(wapper);
    CHECK_AND_RETURN_RET_LOG(fileSinkInstance != nullptr, ERR_INVALID_HANDLE, "null audioRendererSink");
    return fileSinkInstance->Resume();
}

int32_t AudioRendererFileSinkRenderFrame(void *wapper, char &data, uint64_t len, uint64_t &writeLen)
{
    AudioRendererFileSink *fileSinkInstance = static_cast<AudioRendererFileSink *>(wapper);
    CHECK_AND_RETURN_RET_LOG(fileSinkInstance != nullptr, ERR_INVALID_HANDLE, "null audioRendererSink");
    return fileSinkInstance->RenderFrame(data, len, writeLen);
}

int32_t AudioRendererFileSinkSetVolume(void *wapper, float left, float right)
{
    AudioRendererFileSink *fileSinkInstance = static_cast<AudioRendererFileSink *>(wapper);
    CHECK_AND_RETURN_RET_LOG(fileSinkInstance != nullptr, ERR_INVALID_HANDLE, "null audioRendererSink");
    return fileSinkInstance->SetVolume(left, right);
}

int32_t AudioRendererFileSinkGetLatency(void *wapper, uint32_t *latency)
{
    AudioRendererFileSink *fileSinkInstance = static_cast<AudioRendererFileSink *>(wapper);
    CHECK_AND_RETURN_RET_LOG(fileSinkInstance != nullptr, ERR_INVALID_HANDLE, "null audioRendererSink");
    return fileSinkInstance->GetLatency(latency);
}

int32_t AudioRendererFileSinkGetTransactionId(uint64_t *transactionId)
{
    return g_fileSinkInstance->GetTransactionId(transactionId);
}
#ifdef __cplusplus
}
#endif
