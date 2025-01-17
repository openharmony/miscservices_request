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

#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "legacy/download_manager.h"
#include "upload_task_napi.h"
#include "js_util.h"
#include "download_task_napi.h"
#include "constant.h"

using namespace OHOS::Request::UploadNapi;
using namespace OHOS::Request::Upload;
using namespace OHOS::Request::Download;

// fix code rule issue
static napi_value network_mobile = nullptr;
static napi_value network_wifi = nullptr;
static napi_value err_cannot_resume = nullptr;
static napi_value err_dev_not_found = nullptr;
static napi_value err_file_exist = nullptr;
static napi_value err_file_error = nullptr;
static napi_value err_http_data = nullptr;
static napi_value err_no_space = nullptr;
static napi_value err_many_redirect = nullptr;
static napi_value err_http_code = nullptr;
static napi_value err_unknown = nullptr;
static napi_value err_network_fail = nullptr;
static napi_value paused_queue_wifi = nullptr;
static napi_value paused_for_network = nullptr;
static napi_value paused_to_retry = nullptr;
static napi_value paused_by_user = nullptr;
static napi_value paused_unknown = nullptr;
static napi_value session_success = nullptr;
static napi_value session_running = nullptr;
static napi_value session_pending = nullptr;
static napi_value session_paused = nullptr;
static napi_value session_failed = nullptr;

static void NapiCreateInt32(napi_env env)
{
    /* Create Network Type Const */
    napi_create_int32(env, static_cast<int32_t>(NETWORK_MOBILE), &network_mobile);
    napi_create_int32(env, static_cast<int32_t>(NETWORK_WIFI), &network_wifi);

    /* Create error cause const */
    napi_create_int32(env, static_cast<int32_t>(ERROR_CANNOT_RESUME), &err_cannot_resume);
    napi_create_int32(env, static_cast<int32_t>(ERROR_DEVICE_NOT_FOUND), &err_dev_not_found);
    napi_create_int32(env, static_cast<int32_t>(ERROR_FILE_ALREADY_EXISTS), &err_file_exist);
    napi_create_int32(env, static_cast<int32_t>(ERROR_FILE_ERROR), &err_file_error);
    napi_create_int32(env, static_cast<int32_t>(ERROR_HTTP_DATA_ERROR), &err_http_data);
    napi_create_int32(env, static_cast<int32_t>(ERROR_INSUFFICIENT_SPACE), &err_no_space);
    napi_create_int32(env, static_cast<int32_t>(ERROR_TOO_MANY_REDIRECTS), &err_many_redirect);
    napi_create_int32(env, static_cast<int32_t>(ERROR_UNHANDLED_HTTP_CODE), &err_http_code);
    napi_create_int32(env, static_cast<int32_t>(ERROR_UNKNOWN), &err_unknown);
    napi_create_int32(env, static_cast<int32_t>(ERROR_NETWORK_FAIL), &err_network_fail);

    /* Create paused reason Const */
    napi_create_int32(env, static_cast<int32_t>(PAUSED_QUEUED_FOR_WIFI), &paused_queue_wifi);
    napi_create_int32(env, static_cast<int32_t>(PAUSED_WAITING_FOR_NETWORK), &paused_for_network);
    napi_create_int32(env, static_cast<int32_t>(PAUSED_WAITING_TO_RETRY), &paused_to_retry);
    napi_create_int32(env, static_cast<int32_t>(PAUSED_BY_USER), &paused_by_user);
    napi_create_int32(env, static_cast<int32_t>(PAUSED_UNKNOWN), &paused_unknown);

    /* Create session status Const */
    napi_create_int32(env, static_cast<int32_t>(SESSION_SUCCESS), &session_success);
    napi_create_int32(env, static_cast<int32_t>(SESSION_RUNNING), &session_running);
    napi_create_int32(env, static_cast<int32_t>(SESSION_PENDING), &session_pending);
    napi_create_int32(env, static_cast<int32_t>(SESSION_PAUSED), &session_paused);
    napi_create_int32(env, static_cast<int32_t>(SESSION_FAILED), &session_failed);
}

static napi_value Init(napi_env env, napi_value exports)
{
    NapiCreateInt32(env);

    napi_property_descriptor desc[] = {
        DECLARE_NAPI_STATIC_PROPERTY("NETWORK_MOBILE", network_mobile),
        DECLARE_NAPI_STATIC_PROPERTY("NETWORK_WIFI", network_wifi),

        DECLARE_NAPI_STATIC_PROPERTY("ERROR_CANNOT_RESUME", err_cannot_resume),
        DECLARE_NAPI_STATIC_PROPERTY("ERROR_DEVICE_NOT_FOUND", err_dev_not_found),
        DECLARE_NAPI_STATIC_PROPERTY("ERROR_FILE_ALREADY_EXISTS", err_file_exist),
        DECLARE_NAPI_STATIC_PROPERTY("ERROR_FILE_ERROR", err_file_error),
        DECLARE_NAPI_STATIC_PROPERTY("ERROR_HTTP_DATA_ERROR", err_http_data),
        DECLARE_NAPI_STATIC_PROPERTY("ERROR_INSUFFICIENT_SPACE", err_no_space),
        DECLARE_NAPI_STATIC_PROPERTY("ERROR_TOO_MANY_REDIRECTS", err_many_redirect),
        DECLARE_NAPI_STATIC_PROPERTY("ERROR_UNHANDLED_HTTP_CODE", err_http_code),
        DECLARE_NAPI_STATIC_PROPERTY("ERROR_UNKNOWN", err_unknown),
        DECLARE_NAPI_STATIC_PROPERTY("ERROR_NETWORK_FAIL", err_network_fail),

        DECLARE_NAPI_STATIC_PROPERTY("PAUSED_QUEUED_FOR_WIFI", paused_queue_wifi),
        DECLARE_NAPI_STATIC_PROPERTY("PAUSED_WAITING_FOR_NETWORK", paused_for_network),
        DECLARE_NAPI_STATIC_PROPERTY("PAUSED_WAITING_TO_RETRY", paused_to_retry),
        DECLARE_NAPI_STATIC_PROPERTY("PAUSED_BY_USER", paused_by_user),
        DECLARE_NAPI_STATIC_PROPERTY("PAUSED_UNKNOWN", paused_unknown),

        DECLARE_NAPI_STATIC_PROPERTY("SESSION_SUCCESSFUL", session_success),
        DECLARE_NAPI_STATIC_PROPERTY("SESSION_RUNNING", session_running),
        DECLARE_NAPI_STATIC_PROPERTY("SESSION_PENDING", session_pending),
        DECLARE_NAPI_STATIC_PROPERTY("SESSION_PAUSED", session_paused),
        DECLARE_NAPI_STATIC_PROPERTY("SESSION_FAILED", session_failed),

        DECLARE_NAPI_METHOD("download", DownloadTaskNapi::JsMain),
        DECLARE_NAPI_METHOD("upload", UploadTaskNapi::JsUpload),
        DECLARE_NAPI_METHOD("onDownloadComplete", Legacy::DownloadManager::OnDownloadComplete),
    };

    napi_status status = napi_define_properties(env, exports, sizeof(desc) / sizeof(napi_property_descriptor), desc);
    UPLOAD_HILOGD(UPLOAD_MODULE_JS_NAPI, "init upload %{public}d", status);
    return exports;
}

static __attribute__((constructor)) void RegisterModule()
{
    static napi_module module = {
        .nm_version = 1,
        .nm_flags = 0,
        .nm_filename = nullptr,
        .nm_register_func = Init,
        .nm_modname = "request",
        .nm_priv = ((void *)0),
        .reserved = { 0 }
    };
    napi_module_register(&module);
    UPLOAD_HILOGD(UPLOAD_MODULE_JS_NAPI, "module register request");
}
