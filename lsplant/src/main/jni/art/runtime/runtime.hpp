/*
 * This file is part of LSPosed.
 *
 * LSPosed is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LSPosed is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LSPosed.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2022 LSPosed Contributors
 */

#pragma once

#include "utils/hook_helper.hpp"

namespace lsplant::art {

class Runtime {
private:
    inline static Runtime *instance_;

    CREATE_MEM_FUNC_SYMBOL_ENTRY(void, SetJavaDebuggable, void *thiz, bool value) {
        if (SetJavaDebuggableSym) [[likely]] {
            SetJavaDebuggableSym(thiz, value);
        }
    }

public:
    inline static Runtime *Current() { return instance_; }

    void SetJavaDebuggable(bool value) { SetJavaDebuggable(this, value); }

    static bool Init(const HookHandler &handler) {
        int sdk_int = GetAndroidApiLevel();
        if (void **instance; !RETRIEVE_FIELD_SYMBOL(instance, "_ZN3art7Runtime9instance_E")) {
            return false;
        } else if (instance_ = reinterpret_cast<Runtime *>(*instance); !instance_) {
            return false;
        }
        LOGD("runtime instance = %p", instance_);
        if (sdk_int >= __ANDROID_API_O__) {
            if (!RETRIEVE_MEM_FUNC_SYMBOL(SetJavaDebuggable,
                                          "_ZN3art7Runtime17SetJavaDebuggableEb")) {
                return false;
            }
        }
        return true;
    }
};
}  // namespace lsplant::art
