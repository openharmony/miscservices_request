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

#ifndef DOWNLOAD_NOTIFY_INTERFACE_H
#define DOWNLOAD_NOTIFY_INTERFACE_H

#include "iremote_broker.h"
#include "iremote_object.h"

namespace OHOS::Request::Download {
class DownloadNotifyInterface : public IRemoteBroker {
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"OHOS.Download.DownloadNotifyInterface");
    virtual void OnCallBack(MessageParcel &data) = 0;
};

enum {
    DOWNLOAD_NOTIFY,
};
} // namespace OHOS::Request::Download
#endif // DOWNLOAD_NOTIFY_INTERFACE_H
