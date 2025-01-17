/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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

#include "download_service_stub.h"
#include "ipc_skeleton.h"
#include "message_parcel.h"
#include "download_common.h"
#include "download_service_interface.h"
#include "log.h"

namespace OHOS::Request::Download {
using namespace OHOS::HiviewDFX;

int32_t DownloadServiceStub::OnRemoteRequest(
    uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    DOWNLOAD_HILOGD("OnRemoteRequest started, code = %{public}d", code);
    auto descriptorToken = data.ReadInterfaceToken();
    if (descriptorToken != GetDescriptor()) {
        DOWNLOAD_HILOGE("Remote descriptor not the same as local descriptor.");
        return E_DOWNLOAD_TRANSACT_ERROR;
    }
    switch (code) {
        case CMD_REQUEST:
            return OnRequest(data, reply);
        case CMD_PAUSE:
            return OnPause(data, reply);
        case CMD_QUERY:
            return OnQuery(data, reply);
        case CMD_QUERYMIMETYPE:
            return OnQueryMimeType(data, reply);
        case CMD_REMOVE:
            return OnRemove(data, reply);
            break;
        case CMD_RESUME:
            return OnResume(data, reply);
        case CMD_ON:
            return OnEventOn(data, reply);
            break;
        case CMD_OFF:
            return OnEventOff(data, reply);
            break;
        case CMD_CHECKPERMISSION:
            return OnCheckPermission(data, reply);
            break;
        case CMD_SETSTARTID:
            return OnSetStartId(data, reply);
            break;
        default:
            DOWNLOAD_HILOGE("Default value received, check needed.");
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
    }
    return E_DOWNLOAD_OK;
}

bool DownloadServiceStub::OnRequest(MessageParcel &data, MessageParcel &reply)
{
    DOWNLOAD_HILOGD("Receive request");
    DownloadConfig config;
    int32_t fd  = data.ReadFileDescriptor();
    DOWNLOAD_HILOGI("Get FD from client, fd [%{public}d]", fd);
    config.SetFD(fd);
    config.SetFDError(data.ReadInt32());
    config.SetUrl(data.ReadString());
    config.SetMetered(data.ReadBool());
    config.SetRoaming(data.ReadBool());
    config.SetDescription(data.ReadString());
    config.SetNetworkType(data.ReadUint32());
    config.SetFilePath(data.ReadString());
    config.SetTitle(data.ReadString());
    config.SetDescription(data.ReadString());

    uint32_t headerSize = data.ReadUint32();
    for (uint32_t i = 0; i < headerSize; i++) {
        config.SetHeader(data.ReadString(), data.ReadString());
    }
    config.Dump();
    int32_t result = Request(config);
    if (!reply.WriteInt32(result)) {
        DOWNLOAD_HILOGE("WriteBool failed");
        return false;
    }
    return true;
}

bool DownloadServiceStub::OnPause(MessageParcel &data, MessageParcel &reply)
{
    bool result = Pause(data.ReadUint32());
    if (!reply.WriteBool(result)) {
        DOWNLOAD_HILOGE("WriteBool failed");
        return false;
    }
    return true;
}

bool DownloadServiceStub::OnQuery(MessageParcel &data, MessageParcel &reply)
{
    DownloadInfo info;
    bool result = Query(data.ReadUint32(), info);
    if (result) {
        reply.WriteString(info.GetDescription());
        reply.WriteUint32(info.GetDownloadedBytes());
        reply.WriteUint32(info.GetDownloadId());
        reply.WriteUint32(info.GetFailedReason());
        reply.WriteString(info.GetFileName());
        reply.WriteString(info.GetFilePath());
        reply.WriteUint32(info.GetPausedReason());
        reply.WriteUint32(info.GetStatus());
        reply.WriteString(info.GetTargetURI());
        reply.WriteString(info.GetDownloadTitle());
        info.Dump();
    }
    if (!reply.WriteBool(result)) {
        DOWNLOAD_HILOGE("WriteBool failed");
        return false;
    }
    return true;
}

bool DownloadServiceStub::OnQueryMimeType(MessageParcel &data, MessageParcel &reply)
{
    std::string mime;
    bool result = QueryMimeType(data.ReadInt32(), mime);
    if (result) {
        reply.WriteString(mime);
    }
    if (!reply.WriteBool(result)) {
        DOWNLOAD_HILOGE("WriteBool failed");
        return false;
    }
    return true;
}

bool DownloadServiceStub::OnRemove(MessageParcel &data, MessageParcel &reply)
{
    bool result = Remove(data.ReadInt32());
    if (!reply.WriteBool(result)) {
        DOWNLOAD_HILOGE("WriteBool failed");
        return false;
    }
    return true;
}

bool DownloadServiceStub::OnResume(MessageParcel &data, MessageParcel &reply)
{
    bool result = Resume(data.ReadInt32());
    if (!reply.WriteBool(result)) {
        DOWNLOAD_HILOGE("WriteBool failed");
        return false;
    }
    return true;
}

bool DownloadServiceStub::OnEventOn(MessageParcel &data, MessageParcel &reply)
{
    uint32_t taskId = data.ReadUint32();
    std::string type = data.ReadString();
    DOWNLOAD_HILOGD("DownloadServiceStub::OnEventOn taskId = %{public}d type=%{public}s ", taskId, type.c_str());
    if (type.empty()) {
        DOWNLOAD_HILOGE("DownloadServiceStub::OnEventOn type is null.");
        return false;
    }
    sptr<IRemoteObject> remote = data.ReadRemoteObject();
    if (remote == nullptr) {
        DOWNLOAD_HILOGD("DownloadServiceStub::OnEventOn remote is nullptr");
        if (!reply.WriteInt32(ERR_NONE)) {
            return false;
        }
        return true;
    }
    sptr<DownloadNotifyInterface> listener = iface_cast<DownloadNotifyInterface>(remote);
    if (listener.GetRefPtr() == nullptr) {
        DOWNLOAD_HILOGD("DownloadServiceStub::OnEventOn listener is null");
        return false;
    }
    bool result = On(taskId, type, listener);
    if (!reply.WriteBool(result)) {
        DOWNLOAD_HILOGD("DownloadServiceStub::OnEventOn 4444");
        return false;
    }
    DOWNLOAD_HILOGD("DownloadServiceStub::OnEventOn out");
    return true;
}

bool DownloadServiceStub::OnEventOff(MessageParcel &data, MessageParcel &reply)
{
    DOWNLOAD_HILOGD("DownloadServiceStub::OnEventOff in");
    uint32_t taskId = data.ReadUint32();
    std::string type = data.ReadString();
    DOWNLOAD_HILOGD("DownloadServiceStub::OnEventOff taskId = %{public}d type=%{public}s ", taskId, type.c_str());
    bool result = Off(taskId, type);
    if (!reply.WriteBool(result)) {
        return false;
    }
    DOWNLOAD_HILOGD("DownloadServiceStub::OnEventOff out");
    return true;
}

bool DownloadServiceStub::OnCheckPermission(MessageParcel &data, MessageParcel &reply)
{
    DOWNLOAD_HILOGD("DownloadServiceStub::OnCheckPermission in");

    bool result = CheckPermission();
    if (!reply.WriteBool(result)) {
        return false;
    }
    DOWNLOAD_HILOGD("DownloadServiceStub::OnCheckPermission out");
    return true;
}

bool DownloadServiceStub::OnSetStartId(MessageParcel &data, MessageParcel &reply)
{
    DOWNLOAD_HILOGD("DownloadServiceStub::OnSetStartId in");
    uint32_t startId = data.ReadUint32();
    bool result = SetStartId(startId);
    if (!reply.WriteBool(result)) {
        return false;
    }
    DOWNLOAD_HILOGD("DownloadServiceStub::OnSetStartId out");
    return true;
}
} // namespace OHOS::Request::Download
