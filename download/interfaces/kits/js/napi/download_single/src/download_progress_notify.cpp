/*
 * Copyright (C) 2021-2022 Huawei Device Co., Ltd.
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

#include "download_progress_notify.h"

#include <uv.h>

#include "log.h"
#include "napi_utils.h"

namespace OHOS::Request::Download {
DownloadProgressNotify::DownloadProgressNotify(
    napi_env env, const std::string &type, const DownloadTask *task, napi_ref ref)
    : DownloadBaseNotify(env, type, task, ref)
{
}

DownloadProgressNotify::~DownloadProgressNotify()
{
    DOWNLOAD_HILOGD("");
}

void DownloadProgressNotify::OnCallBack(MessageParcel &data)
{
    DOWNLOAD_HILOGD("Progress callback in");
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        DOWNLOAD_HILOGE("Failed to get uv event loop");
        return;
    }
    uv_work_t *work = new (std::nothrow) uv_work_t;
    if (work == nullptr) {
        DOWNLOAD_HILOGE("Failed to create uv work");
        return;
    }

    NotifyData *notifyData = GetNotifyData();
    notifyData->env = env_;
    notifyData->ref = ref_;
    notifyData->type = type_;
    notifyData->task = task_;
    notifyData->firstArgv = data.ReadUint32();
    notifyData->secondArgv = data.ReadUint32();
    DOWNLOAD_HILOGD("recv progress notification's arg: [%{public}d, %{public}d]",
                    notifyData->firstArgv, notifyData->secondArgv);
    work->data = notifyData;
    
    uv_queue_work(
        loop, work, [](uv_work_t *work) {},
        [](uv_work_t *work, int statusInt) {
            NotifyData *notifyData = static_cast<NotifyData*>(work->data);
            if (notifyData != nullptr) {
                napi_value undefined = 0;
                napi_get_undefined(notifyData->env, &undefined);
                napi_value callbackFunc = nullptr;
                napi_get_reference_value(notifyData->env, notifyData->ref, &callbackFunc);
                napi_value result = nullptr;
                napi_value callbackVal[NapiUtils::TWO_ARG] = {0};
                napi_create_uint32(notifyData->env, notifyData->firstArgv, &callbackVal[NapiUtils::FIRST_ARGV]);
                napi_create_uint32(notifyData->env, notifyData->secondArgv, &callbackVal[NapiUtils::SECOND_ARGV]);
                napi_call_function(notifyData->env, nullptr, callbackFunc, NapiUtils::TWO_ARG, callbackVal, &result);
                if (work != nullptr) {
                    delete work;
                    work = nullptr;
                }
                delete notifyData;
                notifyData = nullptr;
            }
        });
}
} // namespace OHOS::Request::Download