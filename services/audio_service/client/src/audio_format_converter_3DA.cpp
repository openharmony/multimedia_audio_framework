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
#include "audio_format_converter_3DA.h"
#include <cstdint>
#include <string>
#include <iostream>

namespace OHOS {
namespace AudioStandard {
static constexpr int32_t AUDIO_VIVID_SAMPLES = 1024;
static constexpr int32_t AVS3METADATA_SIZE = 19824;
static constexpr int32_t INVALID_FORMAT = -1;
static constexpr AudioChannelLayout DEFAULT_LAYOUT = CH_LAYOUT_5POINT1POINT2;

#if (defined(__aarch64__) || defined(__x86_64__))
    constexpr const char *LD_EFFECT_LIBRARY_PATH[] = {"/system/lib64/"};
#else
    constexpr const char *LD_EFFECT_LIBRARY_PATH[] = {"/system/lib/"};
#endif

std::map<uint8_t, int8_t> format2bps = {{SAMPLE_U8, sizeof(uint8_t)},
                                        {SAMPLE_S16LE, sizeof(int16_t)},
                                        {SAMPLE_S24LE, sizeof(int16_t) + sizeof(int8_t)},
                                        {SAMPLE_S32LE, sizeof(int32_t)}};

static int8_t GetBps(uint8_t format)
{
    return format2bps.count(format) > 0 ? format2bps[format] : INVALID_FORMAT;
}

static int32_t GetBits(uint64_t x)
{
    return x == 0 ? 0 : (x & 1) + GetBits(x >> 1);
}

static bool LoadFromXML(Library &lib, AudioChannelLayout &layout)
{
    AudioConverterParser parser;
    ConverterConfig result;
    layout = DEFAULT_LAYOUT;
    if (parser.LoadConfig(result) != 0) {
        return false;
    }
    lib = result.library;
    layout = result.outChannelLayout;
    AUDIO_INFO_LOG("<log info> lib %{public}s %{public}s successful from xml", lib.path.c_str(), lib.name.c_str());
    return true;
}

int32_t AudioFormatConverter3DA::GetPcmLength(int32_t channels, int8_t bps)
{
    if (encoding_ == ENCODING_AUDIOVIVID) {
        return channels * AUDIO_VIVID_SAMPLES * bps;
    }
    AUDIO_INFO_LOG("encodingType is not supported."); // never run
    return 0;
}

int32_t AudioFormatConverter3DA::GetMetaLength()
{
    if (encoding_ == ENCODING_AUDIOVIVID) {
        return AVS3METADATA_SIZE;
    }
    AUDIO_INFO_LOG("encodingType is not supported."); // never run
    return 0;
}

int32_t AudioFormatConverter3DA::Init(const AudioStreamParams info)
{
    inChannel_ = info.channels;
    outChannel_ = info.channels;

    encoding_ = info.encoding;

    bps_ = GetBps(info.format);
    if (bps_ < 0) {
        AUDIO_ERR_LOG("converter: Unsupported sample format");
        return INVALID_FORMAT;
    }

    Library library;
    loadSuccess_ = false;
    if (LoadFromXML(library, outChannelLayout_)) {
        if (externalLoader_.AddAlgoHandle(library)) {
            outChannel_ = GetBits(outChannelLayout_);
            externalLoader_.SetIOBufferConfig(true, info.format, inChannel_, info.channelLayout);
            externalLoader_.SetIOBufferConfig(false, info.format, outChannel_, outChannelLayout_);
            if (externalLoader_.Init()) {
                loadSuccess_ = true;
            }
        }
    }
    if (!loadSuccess_) {
        outChannel_ = info.channels;
        outChannelLayout_ = CH_LAYOUT_UNKNOWN; // can not convert
    }
    return 0;
}

void AudioFormatConverter3DA::ConverterChannels(uint8_t &channels, uint64_t &channelLayout)
{
    channels = outChannel_;
    channelLayout = outChannelLayout_;
}

bool AudioFormatConverter3DA::GetInputBufferSize(size_t &bufferSize)
{
    bufferSize = GetPcmLength(inChannel_, bps_);
    return bufferSize > 0;
}

bool AudioFormatConverter3DA::CheckInputValid(const BufferDesc pcmBuffer, const BufferDesc metaBuffer)
{
    if (pcmBuffer.buffer == nullptr || metaBuffer.buffer == nullptr) {
        AUDIO_ERR_LOG("pcm or metadata buffer is nullptr");
        return false;
    }
    if (pcmBuffer.bufLength - GetPcmLength(inChannel_, bps_) != 0) {
        AUDIO_ERR_LOG("pcm bufLength invalid, pcmBufferSize = %{public}zu, excepted %{public}d",
            pcmBuffer.bufLength, GetPcmLength(inChannel_, bps_));
        return false;
    }
    if (metaBuffer.bufLength - GetMetaLength() != 0) {
        AUDIO_ERR_LOG("metadata bufLength invalid, metadataBufferSize = %{public}zu, excepted %{public}d",
            metaBuffer.bufLength, GetMetaLength());
        return false;
    }
    return true;
}

bool AudioFormatConverter3DA::AllocateMem()
{
    outPcmBuf_ = std::make_unique<uint8_t[]>(GetPcmLength(outChannel_, bps_));
    return outPcmBuf_ != nullptr;
}

void AudioFormatConverter3DA::GetOutputBufferStream(uint8_t *&buffer, uint32_t &bufferLen)
{
    buffer = outPcmBuf_.get();
    bufferLen = GetPcmLength(outChannel_, bps_);
}

int32_t AudioFormatConverter3DA::Process(const BufferDesc pcmBuffer, const BufferDesc metaBuffer)
{
    int32_t ret = 0;
    if (!loadSuccess_) {
        size_t n = GetPcmLength(outChannel_, bps_);
        for (size_t i = 0; i < pcmBuffer.bufLength && i < n; i++)
            outPcmBuf_[i] = pcmBuffer.buffer[i];
    } else {
        AudioBuffer inBuffer = {
            .frameLength = AUDIO_VIVID_SAMPLES,
            .raw = pcmBuffer.buffer,
            .metaData = metaBuffer.buffer};
        AudioBuffer outBuffer = {
            .frameLength = AUDIO_VIVID_SAMPLES,
            .raw = outPcmBuf_.get(),
            .metaData = metaBuffer.buffer};
        ret = externalLoader_.ApplyAlgo(inBuffer, outBuffer);
    }
    return ret;
}

bool AudioFormatConverter3DA::Flush()
{
    return loadSuccess_ ? externalLoader_.FlushAlgo() : true;
}

static bool ResolveLibrary(const std::string &path, std::string &resovledPath)
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

LibLoader::~LibLoader()
{
    if (libEntry_ != nullptr && libEntry_->audioEffectLibHandle != nullptr) {
        libEntry_->audioEffectLibHandle->releaseEffect(handle_);
    }
    if (libHandle_ != nullptr) {
        dlclose(libHandle_);
        libHandle_ = nullptr;
    }
}

bool LibLoader::LoadLibrary(const std::string &relativePath) noexcept
{
    std::string absolutePath;
    // find library in adsolutePath
    bool ret = ResolveLibrary(relativePath, absolutePath);
    CHECK_AND_RETURN_RET_LOG(ret, false, "<log error> find library falied in effect directories: %{public}s",
        relativePath.c_str());

    libHandle_ = dlopen(absolutePath.c_str(), 1);
    CHECK_AND_RETURN_RET_LOG(libHandle_, false, "<log error> dlopen lib %{public}s Fail", relativePath.c_str());
    AUDIO_INFO_LOG("<log info> dlopen lib %{public}s successful", relativePath.c_str());

    AudioEffectLibrary *audioEffectLibHandle = static_cast<AudioEffectLibrary *>(dlsym(libHandle_,
        AUDIO_EFFECT_LIBRARY_INFO_SYM_AS_STR));
    const char* error = dlerror();
    if (error) {
        AUDIO_ERR_LOG("<log error> dlsym failed: error: %{public}s, %{public}p", error, audioEffectLibHandle);
        dlclose(libHandle_);
        return false;
    }
    AUDIO_INFO_LOG("<log info> dlsym lib %{public}s successful, error: %{public}s", relativePath.c_str(), error);

    libEntry_->audioEffectLibHandle = audioEffectLibHandle;

    return true;
}

void LibLoader::SetIOBufferConfig(bool isInput, uint8_t format, uint32_t channels, uint64_t channelLayout)
{
    if (isInput) {
        ioBufferConfig_.inputCfg.channels = channels;
        ioBufferConfig_.inputCfg.format = format;
        ioBufferConfig_.inputCfg.channelLayout = channelLayout;
        ioBufferConfig_.inputCfg.encoding = ENCODING_AUDIOVIVID;
    } else {
        ioBufferConfig_.outputCfg.channels = channels;
        ioBufferConfig_.outputCfg.format = format;
        ioBufferConfig_.outputCfg.channelLayout = channelLayout;
        ioBufferConfig_.outputCfg.encoding = ENCODING_AUDIOVIVID;
    }
}

bool LibLoader::AddAlgoHandle(Library library)
{
    AudioEffectDescriptor descriptor = {.libraryName = library.name, .effectName = library.name};
    libEntry_ = std::make_unique<AudioEffectLibEntry>();
    libEntry_->libraryName = library.name;
    bool loadLibrarySuccess = LoadLibrary(library.path);
    CHECK_AND_RETURN_RET_LOG(loadLibrarySuccess, false,
        "<log error> loadLibrary fail, please check logs!");
    int32_t ret = libEntry_->audioEffectLibHandle->createEffect(descriptor, &handle_);
    CHECK_AND_RETURN_RET_LOG(ret == 0, false, "%{public}s create fail", library.name.c_str());
    return true;
}

bool LibLoader::Init()
{
    int32_t ret = 0;
    int32_t replyData = 0;
    AudioEffectTransInfo cmdInfo = {sizeof(AudioEffectConfig), &ioBufferConfig_};
    AudioEffectTransInfo replyInfo = {sizeof(int32_t), &replyData};
    ret = (*handle_)->command(handle_, EFFECT_CMD_INIT, &cmdInfo, &replyInfo);
    CHECK_AND_RETURN_RET_LOG(ret == 0, false,
        "[%{public}s] lib EFFECT_CMD_INIT fail", libEntry_->libraryName.c_str());
    ret = (*handle_)->command(handle_, EFFECT_CMD_ENABLE, &cmdInfo, &replyInfo);
    CHECK_AND_RETURN_RET_LOG(ret == 0, false,
        "[%{public}s] lib EFFECT_CMD_ENABLE fail", libEntry_->libraryName.c_str());
    ret = (*handle_)->command(handle_, EFFECT_CMD_SET_CONFIG, &cmdInfo, &replyInfo);
    CHECK_AND_RETURN_RET_LOG(ret == 0, false,
        "[%{public}s] lib EFFECT_CMD_SET_CONFIG fail", libEntry_->libraryName.c_str());
    return true;
}

int32_t LibLoader::ApplyAlgo(AudioBuffer &inputBuffer, AudioBuffer &outputBuffer)
{
    int32_t ret = (*handle_)->process(handle_, &inputBuffer, &outputBuffer);
    CHECK_AND_RETURN_RET_LOG(ret == 0, ret, "converter algo lib process fail");
    return ret;
}

bool LibLoader::FlushAlgo()
{
    int32_t ret = 0;
    int32_t replyData = 0;
    AudioEffectTransInfo cmdInfo = {sizeof(AudioEffectConfig), &ioBufferConfig_};
    AudioEffectTransInfo replyInfo = {sizeof(int32_t), &replyData};
    ret = (*handle_)->command(handle_, EFFECT_CMD_ENABLE, &cmdInfo, &replyInfo);
    CHECK_AND_RETURN_RET_LOG(ret == 0, false,
        "[%{public}s] lib EFFECT_CMD_ENABLE fail", libEntry_->libraryName.c_str());
    return true;
}
} // namespace AudioStandard
} // namespace OHOS