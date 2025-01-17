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

#include "download_manager.h"

#include "system_ability_definition.h"
#include "iservice_registry.h"

#include "data_ability_predicates.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_open_callback.h"
#include "rdb_predicates.h"
#include "rdb_store.h"
#include "result_set.h"

#include "log.h"

namespace OHOS::Request::Download {
std::mutex DownloadManager::instanceLock_;
sptr<DownloadManager> DownloadManager::instance_ = nullptr;

DownloadManager::DownloadManager() : downloadServiceProxy_(nullptr), deathRecipient_(nullptr),
    dataAbilityHelper_(nullptr) {
}

DownloadManager::~DownloadManager()
{
}

sptr<DownloadManager> DownloadManager::GetInstance()
{
    if (instance_ == nullptr) {
        std::lock_guard<std::mutex> autoLock(instanceLock_);
        if (instance_ == nullptr) {
            instance_ = new DownloadManager;
        }
    }
    return instance_;
}

void DownloadManager::SetDataAbilityHelper(std::shared_ptr<OHOS::AppExecFwk::DataAbilityHelper> dataAbilityHelper)
{
    if (dataAbilityHelper_ == nullptr && dataAbilityHelper != nullptr) {
        dataAbilityHelper_ = dataAbilityHelper;
        std::vector<std::string> columns;
        columns.push_back("taskid");
        OHOS::NativeRdb::DataAbilityPredicates predicates;
        predicates.GreaterThan("taskid", "0");
        std::shared_ptr<OHOS::NativeRdb::AbsSharedResultSet> resultSet;
        OHOS::Uri uriDownload("dataability:///com.ohos.download/download/downloadInfo");
        resultSet = dataAbilityHelper_->Query(uriDownload, columns, predicates);
        if (resultSet == nullptr) {
            DOWNLOAD_HILOGE("Failed to get query result");
            return;
        }
        int rowCount = 0;
        resultSet->GetRowCount(rowCount);
        DOWNLOAD_HILOGI("DownloadManager ResultSet rowCount = %{public}d", rowCount);
        if (resultSet->GoToLastRow() == 0) {
            int taskId;
            std::string contactIdKey = "taskid";
            int contactIndex = 0;
            resultSet->GetColumnIndex(contactIdKey, contactIndex);
            resultSet->GetInt(contactIndex, taskId);
            DOWNLOAD_HILOGI("DownloadManager query result id  = %{public}d", taskId);
            if (downloadServiceProxy_ == nullptr) {
                DOWNLOAD_HILOGW("Redo GetDownloadServiceProxy");
                downloadServiceProxy_ = GetDownloadServiceProxy();
            }
            if (downloadServiceProxy_ != nullptr) {
                downloadServiceProxy_->SetStartId(taskId + 1);
            }
        }
        resultSet->Close();
    }
}

DownloadTask* DownloadManager::EnqueueTask(const DownloadConfig &config)
{
    DOWNLOAD_HILOGD("DownloadManager EnqueueTask start.");

    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGW("Redo GetDownloadServiceProxy");
        downloadServiceProxy_ = GetDownloadServiceProxy();
    }
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGE("Pause quit because redoing GetDownloadServiceProxy failed.");
        return nullptr;
    }
    int32_t taskId = downloadServiceProxy_->Request(config);
    if (taskId < 0) {
        DOWNLOAD_HILOGE("taskId invalid");
        return nullptr;
    }
    DOWNLOAD_HILOGD("DownloadManager EnqueueTask succeeded.");
    
    DOWNLOAD_HILOGD("DownloadManager EnqueueTask Save Data.");
    OHOS::Uri uriDownload("dataability:///com.ohos.download/download/downloadInfo");
    OHOS::NativeRdb::ValuesBucket rawContactValues;
    rawContactValues.PutInt("taskId", taskId);
    rawContactValues.PutString("url", config.GetUrl().c_str());
    rawContactValues.PutString("description", config.GetDescription().c_str());
    rawContactValues.PutString("title", config.GetTitle().c_str());
    rawContactValues.PutString("filePath", config.GetFilePath().c_str());
    rawContactValues.PutBool("metered", config.GetMetered());
    rawContactValues.PutBool("roaming", config.GetRoaming());
    rawContactValues.PutInt("network", config.GetNetworkType());

    if (dataAbilityHelper_ != nullptr) {
        int rowId = dataAbilityHelper_->Insert(uriDownload, rawContactValues);
        DOWNLOAD_HILOGI("DownloadManager EnqueueTask rowId = %{public}d", rowId);
    }

    return new DownloadTask(taskId);
}

bool DownloadManager::Pause(uint32_t taskId)
{
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGW("Redo GetDownloadServiceProxy");
        downloadServiceProxy_ = GetDownloadServiceProxy();
    }
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGE("Pause quit because redoing GetDownloadServiceProxy failed.");
        return false;
    }
    DOWNLOAD_HILOGD("DownloadManager Pause succeeded.");
    return downloadServiceProxy_->Pause(taskId);
}

bool DownloadManager::Query(uint32_t taskId, DownloadInfo &info)
{
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGW("Redo GetDownloadServiceProxy");
        downloadServiceProxy_ = GetDownloadServiceProxy();
    }
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGE("Query quit because redoing GetDownloadServiceProxy failed.");
        return false;
    }
    DOWNLOAD_HILOGD("DownloadManager Query succeeded.");
    return downloadServiceProxy_->Query(taskId, info);
}

bool DownloadManager::QueryMimeType(uint32_t taskId, std::string &mimeType)
{
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGW("Redo GetDownloadServiceProxy");
        downloadServiceProxy_ = GetDownloadServiceProxy();
    }
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGE("QueryMimeType quit because redoing GetDownloadServiceProxy failed.");
        return false;
    }
    DOWNLOAD_HILOGD("DownloadManager QueryMimeType succeeded.");
    return downloadServiceProxy_->QueryMimeType(taskId, mimeType);
}

bool DownloadManager::Remove(uint32_t taskId)
{
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGW("Redo GetDownloadServiceProxy");
        downloadServiceProxy_ = GetDownloadServiceProxy();
    }
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGE("Remove quit because redoing GetDownloadServiceProxy failed.");
        return false;
    }
    DOWNLOAD_HILOGD("DownloadManager Remove succeeded.");
    return downloadServiceProxy_->Remove(taskId);
}

bool DownloadManager::Resume(uint32_t taskId)
{
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGW("Redo GetDownloadServiceProxy");
        downloadServiceProxy_ = GetDownloadServiceProxy();
    }
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGE("Resume quit because redoing GetDownloadServiceProxy failed.");
        return false;
    }
    DOWNLOAD_HILOGD("DownloadManager Resume succeeded.");
    return downloadServiceProxy_->Resume(taskId);
}

bool DownloadManager::On(uint32_t taskId, const std::string &type, const sptr<DownloadNotifyInterface> &listener)
{
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGW("Redo GetDownloadServiceProxy");
        downloadServiceProxy_ = GetDownloadServiceProxy();
    }
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGE("Resume quit because redoing GetDownloadServiceProxy failed.");
        return false;
    }
    DOWNLOAD_HILOGD("DownloadManager On succeeded.");
    return downloadServiceProxy_->On(taskId, type, listener);
}

bool DownloadManager::Off(uint32_t taskId, const std::string &type)
{
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGW("Redo GetDownloadServiceProxy");
        downloadServiceProxy_ = GetDownloadServiceProxy();
    }
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGE("Resume quit because redoing GetDownloadServiceProxy failed.");
        return false;
    }
    DOWNLOAD_HILOGD("DownloadManager Off succeeded.");
    return downloadServiceProxy_->Off(taskId, type);
}

bool DownloadManager::CheckPermission()
{
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGW("Redo GetDownloadServiceProxy");
        downloadServiceProxy_ = GetDownloadServiceProxy();
    }
    if (downloadServiceProxy_ == nullptr) {
        DOWNLOAD_HILOGE("Resume quit because redoing GetDownloadServiceProxy failed.");
        return false;
    }
    DOWNLOAD_HILOGD("DownloadManager CheckPermission succeeded.");
    DOWNLOAD_HILOGD("Check Permission enable");
    return downloadServiceProxy_->CheckPermission();
}

sptr<DownloadServiceInterface> DownloadManager::GetDownloadServiceProxy()
{
    sptr<ISystemAbilityManager> systemAbilityManager =
        SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (systemAbilityManager == nullptr) {
        DOWNLOAD_HILOGE("Getting SystemAbilityManager failed.");
        return nullptr;
    }
    auto systemAbility = systemAbilityManager->GetSystemAbility(DOWNLOAD_SERVICE_ID, "");
    if (systemAbility == nullptr) {
        DOWNLOAD_HILOGE("Get SystemAbility failed.");
        return nullptr;
    }
    deathRecipient_ = new DownloadSaDeathRecipient();
    systemAbility->AddDeathRecipient(deathRecipient_);
    sptr<DownloadServiceInterface> serviceProxy = iface_cast<DownloadServiceInterface>(systemAbility);
    if (serviceProxy == nullptr) {
        DOWNLOAD_HILOGE("Get DownloadManagerProxy from SA failed.");
        return nullptr;
    }
    DOWNLOAD_HILOGD("Getting DownloadManagerProxy succeeded.");
    return serviceProxy;
}

void DownloadManager::OnRemoteSaDied(const wptr<IRemoteObject> &remote)
{
    downloadServiceProxy_ = GetDownloadServiceProxy();
}

DownloadSaDeathRecipient::DownloadSaDeathRecipient()
{
}

void DownloadSaDeathRecipient::OnRemoteDied(const wptr<IRemoteObject> &object)
{
    DOWNLOAD_HILOGE("DownloadSaDeathRecipient on remote systemAbility died.");
    DownloadManager::GetInstance()->OnRemoteSaDied(object);
}
} // namespace OHOS::Request::Download
