﻿/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "download_service_manager.h"
#include "network_adapter.h"
#include "net_conn_constants.h"
#include "unistd.h"
#include "log.h"

static constexpr uint32_t THREAD_POOL_NUM = 4;
static constexpr uint32_t TASK_SLEEP_INTERVAL = 1;
static constexpr uint32_t MAX_RETRY_TIMES = 3;

using namespace OHOS::NetManagerStandard;
namespace OHOS::Request::Download {
std::mutex DownloadServiceManager::instanceLock_;
DownloadServiceManager *DownloadServiceManager::instance_ = nullptr;
DownloadServiceManager::DownloadServiceManager()
    : initialized_(false), interval_(TASK_SLEEP_INTERVAL), threadNum_(THREAD_POOL_NUM), timeoutRetry_(MAX_RETRY_TIMES),
    taskId_(0)
{
}

DownloadServiceManager::~DownloadServiceManager()
{
    Destroy();
}

DownloadServiceManager *DownloadServiceManager::GetInstance()
{
    if (instance_ == nullptr) {
        std::lock_guard<std::mutex> lock(instanceLock_);
        if (instance_ == nullptr) {
            instance_ = new (std::nothrow) DownloadServiceManager;
        }
    }
    return instance_;
}

bool DownloadServiceManager::Create(uint32_t threadNum)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (initialized_) {
        return true;
    }

    threadNum_ = threadNum;
    for (uint32_t i = 0; i < threadNum; i++) {
        threadList_.push_back(std::make_shared<DownloadThread>([this]() {
            return ProcessTask();
        }, interval_));
        threadList_[i]->Start();
    }

    std::thread th = std::thread([this]() {
        constexpr int RETRY_MAX_TIMES = 100;
        int retryCount = 0;
        constexpr int RETRY_TIME_INTERVAL_MILLISECOND = 1 * 1000 * 1000; // retry after 1 second
        do {
            if (this->MonitorNetwork() == NET_CONN_SUCCESS) {
                break;
            }
            retryCount++;
            usleep(RETRY_TIME_INTERVAL_MILLISECOND);
        } while (retryCount < RETRY_MAX_TIMES);
        DOWNLOAD_HILOGD("RegisterNetConnCallback retryCount= %{public}d", retryCount);
    });
    th.detach();

    initialized_ = true;
    return initialized_;
}

void DownloadServiceManager::Destroy()
{
    std::for_each(threadList_.begin(), threadList_.end(), [](auto t) { t->Stop(); });
    threadList_.clear();
    initialized_ = false;
}

uint32_t DownloadServiceManager::AddTask(const DownloadConfig& config)
{
    if (!initialized_) {
        return -1;
    }
    uint32_t taskId = GetCurrentTaskId();
    if (taskMap_.find(taskId) != taskMap_.end()) {
        DOWNLOAD_HILOGD("Invalid case: duplicate taskId");
        return -1;
    }
    auto task = std::make_shared<DownloadServiceTask>(taskId, config);
    if (task == nullptr) {
        DOWNLOAD_HILOGD("No mem to add task");
        return -1;
    }
    // move new task into pending queue
    task->SetRetryTime(timeoutRetry_);
    taskMap_[taskId] = task;
    MoveTaskToQueue(taskId, task);
    return taskId;
}

void DownloadServiceManager::InstallCallback(uint32_t taskId, DownloadTaskCallback eventCb)
{
    if (!initialized_) {
        return;
    }
    std::map<uint32_t, std::shared_ptr<DownloadServiceTask>>::iterator it = taskMap_.find(taskId);
    if (it != taskMap_.end()) {
        it->second->InstallCallback(eventCb);
    }
}

bool DownloadServiceManager::ProcessTask()
{
    if (!initialized_) {
        return false;
    }
    uint32_t taskId;
    auto pickupTask = [this, &taskId]() -> std::shared_ptr<DownloadServiceTask> {
        // pick up one task from pending queue
        std::lock_guard<std::recursive_mutex> autoLock(mutex_);
        if (pendingQueue_.size() > 0) {
            taskId = pendingQueue_.front();
            pendingQueue_.pop();
            if (taskMap_.find(taskId) != taskMap_.end()) {
                return taskMap_[taskId];
            }
        }
        return nullptr;
    };

    auto execTask = [this, &taskId](std::shared_ptr<DownloadServiceTask> task) -> bool {
        if (task == nullptr) {
            return false;
        }
        bool result = task->Run();
        this->MoveTaskToQueue(taskId, task);
        return result;
    };
    return execTask(pickupTask());
}

bool DownloadServiceManager::Pause(uint32_t taskId)
{
    if (!initialized_) {
        return false;
    }
    DOWNLOAD_HILOGD("Pause Task[%{public}d]", taskId);
    auto it = taskMap_.find(taskId);
    if (it == taskMap_.end()) {
        return false;
    }

    if (it->second->Pause()) {
        MoveTaskToQueue(taskId, it->second);
        return true;
    }
    return false;
}

bool DownloadServiceManager::Resume(uint32_t taskId)
{
    if (!initialized_) {
        return false;
    }
    DOWNLOAD_HILOGD("Resume Task[%{public}d]", taskId);
    auto it = taskMap_.find(taskId);
    if (it == taskMap_.end()) {
        return false;
    }

    if (it->second->Resume()) {
        MoveTaskToQueue(taskId, it->second);
        return true;
    }
    return false;
}

bool DownloadServiceManager::Remove(uint32_t taskId)
{
    if (!initialized_) {
        return false;
    }
    DOWNLOAD_HILOGD("Remove Task[%{public}d]", taskId);
    auto it = taskMap_.find(taskId);
    if (it == taskMap_.end()) {
        return false;
    }

    bool result = it->second->Remove();
    if (result) {
        std::lock_guard<std::recursive_mutex> autoLock(mutex_);
        taskMap_.erase(it);
        RemoveFromQueue(pendingQueue_, taskId);
        RemoveFromQueue(pausedQueue_, taskId);
    }
    return result;
}

bool DownloadServiceManager::Query(uint32_t taskId, DownloadInfo &info)
{
    if (!initialized_) {
        return false;
    }
    auto it = taskMap_.find(taskId);
    if (it == taskMap_.end()) {
        return false;
    }
    return it->second->Query(info);
}

bool DownloadServiceManager::QueryMimeType(uint32_t taskId, std::string &mimeType)
{
    if (!initialized_) {
        return false;
    }
    auto it = taskMap_.find(taskId);
    if (it == taskMap_.end()) {
        return false;
    }
    return it->second->QueryMimeType(mimeType);
}

void DownloadServiceManager::SetStartId(uint32_t startId)
{
    std::lock_guard<std::recursive_mutex> autoLock(mutex_);
    taskId_ = startId;
}

uint32_t DownloadServiceManager::GetStartId() const
{
    return taskId_;
}

uint32_t DownloadServiceManager::GetCurrentTaskId()
{
    std::lock_guard<std::recursive_mutex> autoLock(mutex_);
    return taskId_++;
}

DownloadServiceManager::QueueType DownloadServiceManager::DecideQueueType(DownloadStatus status)
{
    switch (status) {
        case SESSION_PAUSED:
            return QueueType::PAUSED_QUEUE;

        case SESSION_UNKNOWN:
            return QueueType::PENDING_QUEUE;
    
        case SESSION_PENDING:
        case SESSION_RUNNING:
        case SESSION_SUCCESS:
        case SESSION_FAILED:
        default:
            return QueueType::NONE_QUEUE;
    }
    return QueueType::NONE_QUEUE;
}

void DownloadServiceManager::MoveTaskToQueue(uint32_t taskId, std::shared_ptr<DownloadServiceTask> task)
{
    DownloadStatus status;
    ErrorCode code;
    PausedReason reason;
    task->GetRunResult(status, code, reason);
    DOWNLOAD_HILOGD("Status [%{public}d], Code [%{public}d], Reason [%{public}d]", status, code, reason);
    switch (DecideQueueType(status)) {
        case QueueType::PENDING_QUEUE: {
            std::lock_guard<std::recursive_mutex> autoLock(mutex_);
            RemoveFromQueue(pausedQueue_, taskId);
            PushQueue(pendingQueue_, taskId);
            break;
        }
        case QueueType::PAUSED_QUEUE: {
            std::lock_guard<std::recursive_mutex> autoLock(mutex_);
            RemoveFromQueue(pendingQueue_, taskId);
            PushQueue(pausedQueue_, taskId);
            break;
        }
        case QueueType::NONE_QUEUE:
        default:
            break;
    }
}

void DownloadServiceManager::PushQueue(std::queue<uint32_t> &queue, uint32_t taskId)
{
    std::lock_guard<std::recursive_mutex> autoLock(mutex_);
    if (taskMap_.find(taskId) == taskMap_.end()) {
        DOWNLOAD_HILOGD("invalid task id [%{public}d]", taskId);
        return;
    }

    if (queue.empty()) {
        queue.push(taskId);
        return;
    }

    auto headElement = queue.front();
    if (headElement == taskId) {
        return;
    }

    bool foundIt = false;
    uint32_t indicatorId = headElement;
    do {
        if (queue.front() == taskId) {
            foundIt = true;
        }
        queue.push(headElement);
        queue.pop();
        headElement = queue.front();
    } while (headElement != indicatorId);

    if (!foundIt) {
        queue.push(taskId);
    }
}

void DownloadServiceManager::RemoveFromQueue(std::queue<uint32_t> &queue, uint32_t taskId)
{
    std::lock_guard<std::recursive_mutex> autoLock(mutex_);
    if (queue.empty()) {
        return;
    }

    auto headElement = queue.front();
    if (headElement == taskId) {
        queue.pop();
        return;
    }

    auto indicatorId = headElement;
    do {
        if (headElement != taskId) {
            queue.push(queue.front());
        }
        queue.pop();
        headElement = queue.front();
    } while (headElement != indicatorId);
}

void DownloadServiceManager::SetInterval(uint32_t interval)
{
    interval_ = interval;
}
uint32_t DownloadServiceManager::GetInterval() const
{
    return interval_;
}

void DownloadServiceManager::ResumeTaskByNetwork()
{
    int taskCount = 0;
    std::lock_guard<std::recursive_mutex> autoLock(mutex_);
    size_t size = pausedQueue_.size();
    while (size-- > 0) {
        uint32_t taskId = pausedQueue_.front();
        if (taskMap_.find(taskId) != taskMap_.end()) {
            pausedQueue_.pop();
            auto task = taskMap_[taskId];
            DownloadStatus status;
            ErrorCode code;
            PausedReason reason;
            task->GetRunResult(status, code, reason);
            if (reason != PAUSED_BY_USER) {
                task->Resume();
                PushQueue(pendingQueue_, taskId);
                taskCount++;
            } else {
                pausedQueue_.push(taskId);
            }
        }
    }
    DOWNLOAD_HILOGD("[%{public}d] task has been resumed by network status changed", taskCount);
}

int32_t DownloadServiceManager::MonitorNetwork()
{
    int nRet = NetworkAdapter::GetInstance().RegOnNetworkChange([this]() {
        this->ResumeTaskByNetwork();
        this->UpdateNetworkType();
    });
    DOWNLOAD_HILOGD("RegisterNetConnCallback retcode= %{public}d", nRet);
    return nRet;
}

void DownloadServiceManager::UpdateNetworkType()
{
    DOWNLOAD_HILOGD("UpdateNetworkType start\n");
    std::lock_guard<std::recursive_mutex> autoLock(mutex_);
    DownloadStatus status;
    ErrorCode code;
    PausedReason reason;
    for (const auto &it : taskMap_) {
        it.second->GetRunResult(status, code, reason);
        bool bRet = status == SESSION_RUNNING || status == SESSION_PENDING
                    || status == SESSION_PAUSED;
        if (bRet) {
            if (!it.second->IsSatisfiedConfiguration()) {
                RemoveFromQueue(pendingQueue_, it.first);
                PushQueue(pausedQueue_, it.first);
            }
        }
    }
}

bool DownloadServiceManager::QueryAllTask(std::vector<DownloadInfo> &taskVector) const
{
    for (const auto &it : taskMap_) {
        DownloadInfo downloadInfo;
        it.second->Query(downloadInfo);
        taskVector.push_back(downloadInfo);
    }
    return true;
}
} // namespace OHOS::Request::Download