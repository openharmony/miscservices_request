/*
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

#include "header_receive_callback.h"
#include "upload_task.h"
#include "upload_task_napi.h"
using namespace OHOS::Request::UploadNapi;

namespace OHOS::Request::Upload {
HeaderReceiveCallback::HeaderReceiveCallback(ICallbackAbleJudger *judger, napi_env env, napi_value callback)
    :judger_(judger),
    env_(env)
{
    napi_create_reference(env, callback, 1, &callback_);
    napi_get_uv_event_loop(env, &loop_);
}

HeaderReceiveCallback::~HeaderReceiveCallback()
{
    napi_delete_reference(env_, callback_);
}

void HeaderReceiveCallback::HeaderReceive(const std::string &header)
{
    UPLOAD_HILOGD(UPLOAD_MODULE_JS_NAPI, "HeaderReceive. header : %{public}s", header.c_str());
    HeaderReceiveWorker *headerReceiveWorker = new (std::nothrow)HeaderReceiveWorker(judger_, this, header);
    if (headerReceiveWorker == nullptr) {
        UPLOAD_HILOGD(UPLOAD_MODULE_JS_NAPI, "Failed to create headerReceiveWorker");
        return;
    }
    uv_work_t *work = new (std::nothrow)uv_work_t();
    if (work == nullptr) {
        UPLOAD_HILOGD(UPLOAD_MODULE_JS_NAPI, "Failed to create uv work");
        return;
    }
    work->data = headerReceiveWorker;
    int ret = uv_queue_work(loop_, work,
        [](uv_work_t *work) {},
        [](uv_work_t *work, int status) {
            UPLOAD_HILOGD(UPLOAD_MODULE_JS_NAPI, "HeaderReceive. uv_queue_work start");
            std::shared_ptr<HeaderReceiveWorker> headerReceiveWorkerInner(
                reinterpret_cast<HeaderReceiveWorker *>(work->data));
            std::shared_ptr<uv_work_t> sharedWork(work);
            if (!headerReceiveWorkerInner->judger_->JudgeHeaderReceive(headerReceiveWorkerInner->callback)) {
                UPLOAD_HILOGD(UPLOAD_MODULE_JS_NAPI, "HeaderReceive. uv_queue_work callback removed!!");
                return;
            }
            napi_value jsHeader = nullptr;
            napi_value callback = nullptr;
            napi_value args[1];
            napi_value global = nullptr;
            napi_value result;
            napi_status callStatus = napi_generic_failure;

            jsHeader = UploadNapi::JSUtil::Convert2JSString(headerReceiveWorkerInner->callback->env_,
                headerReceiveWorkerInner->header);
            args[0] = { jsHeader };

            napi_get_reference_value(headerReceiveWorkerInner->callback->env_,
                headerReceiveWorkerInner->callback->callback_, &callback);
            napi_get_global(headerReceiveWorkerInner->callback->env_, &global);
            callStatus = napi_call_function(
                headerReceiveWorkerInner->callback->env_, global, callback, 1, args, &result);
            if (callStatus != napi_ok) {
                UPLOAD_HILOGD(UPLOAD_MODULE_JS_NAPI,
                    "HeaderReceive callback failed callStatus:%{public}d callback:%{public}p", callStatus, callback);
            }
        });
    if (ret != 0) {
        UPLOAD_HILOGE(UPLOAD_MODULE_JS_NAPI, "HeaderReceive. uv_queue_work Failed");
        delete headerReceiveWorker;
        delete work;
    }
}
} // end of OHOS::Request::Upload