// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/target_handler_android.h"

#include "chrome/browser/android/devtools_manager_delegate_android.h"
#include "chrome/browser/devtools/devtools_browser_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

Profile* GetProfileForBrowserContext(
    const std::optional<std::string>& browser_context_id) {
  if (browser_context_id.has_value()) {
    return DevToolsBrowserContextManager::GetInstance().GetProfileById(
        *browser_context_id);
  }

  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
  return profile ? profile->GetOriginalProfile() : nullptr;
}

TabModel* FindTabModelForProfile(Profile* profile) {
  if (!profile) {
    return nullptr;
  }

  for (TabModel* model : TabModelList::models()) {
    if (model->GetProfile() == profile &&
        model->GetTabModelType() == TabModel::TabModelType::kStandard) {
      return model;
    }
  }

  return nullptr;
}

TabModel* FindAnyStandardTabModel() {
  for (TabModel* model : TabModelList::models()) {
    if (model->GetTabModelType() == TabModel::TabModelType::kStandard) {
      return model;
    }
  }

  return nullptr;
}

WebContents* CreateTabInProfile(TabModel* tab_model,
                                Profile* profile,
                                const GURL& url) {
  if (!tab_model || !profile) {
    return nullptr;
  }

  content::WebContents::CreateParams create_params(profile);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);
  if (!web_contents) {
    return nullptr;
  }

  tabs::TabInterface* new_tab = tab_model->CreateTab(
      /*parent=*/nullptr, std::move(web_contents), /*index=*/-1,
      TabModel::TabLaunchType::FROM_CHROME_UI, /*should_pin=*/false);
  if (!new_tab || !new_tab->GetContents()) {
    return nullptr;
  }

  WebContents* created_web_contents = new_tab->GetContents();
  content::NavigationController::LoadURLParams load_url_params(url);
  created_web_contents->GetController().LoadURLWithParams(load_url_params);
  return created_web_contents;
}

}  // namespace

TargetHandlerAndroid::TargetHandlerAndroid(protocol::UberDispatcher* dispatcher,
                                           bool is_trusted,
                                           bool may_read_local_files) {
  protocol::Target::Dispatcher::wire(dispatcher, this);
}

TargetHandlerAndroid::~TargetHandlerAndroid() = default;

protocol::Response TargetHandlerAndroid::SetRemoteLocations(
    std::unique_ptr<protocol::Array<protocol::Target::RemoteLocation>>
        locations) {
  remote_locations_.clear();
  if (!locations) {
    return protocol::Response::Success();
  }

  for (const auto& location : *locations) {
    remote_locations_.insert(
        net::HostPortPair(location->GetHost(), location->GetPort()));
  }

  return protocol::Response::Success();
}

protocol::Response TargetHandlerAndroid::CreateTarget(
    const std::string& url,
    std::optional<int> left,
    std::optional<int> top,
    std::optional<int> width,
    std::optional<int> height,
    std::optional<std::string> window_state,
    std::optional<std::string> browser_context_id,
    std::optional<bool> enable_begin_frame_control,
    std::optional<bool> new_window,
    std::optional<bool> background,
    std::optional<bool> for_tab,
    std::optional<bool> hidden,
    std::optional<bool> focus,
    std::string* out_target_id) {
  Profile* profile = GetProfileForBrowserContext(browser_context_id);
  if (!profile) {
    if (browser_context_id.has_value()) {
      return protocol::Response::ServerError(
          "Failed to find browser context with id " + *browser_context_id);
    }
    return protocol::Response::ServerError(
        "Could not find default browser context");
  }

  TabModel* tab_model = FindTabModelForProfile(profile);
  if (!tab_model) {
    tab_model = FindAnyStandardTabModel();
  }

  WebContents* web_contents = nullptr;
  if (tab_model && !browser_context_id.has_value()) {
    web_contents = tab_model->CreateNewTabForDevTools(
        GURL(url), new_window.value_or(false));
  } else {
    web_contents = CreateTabInProfile(tab_model, profile, GURL(url));
  }
  if (!web_contents) {
    return protocol::Response::ServerError("Could not create a Tab");
  }

  DevToolsManagerDelegateAndroid::MarkCreatedByDevTools(*web_contents);

  if (for_tab.value_or(false)) {
    *out_target_id =
        content::DevToolsAgentHost::GetOrCreateForTab(web_contents)->GetId();
  } else {
    *out_target_id =
        content::DevToolsAgentHost::GetOrCreateFor(web_contents)->GetId();
  }

  return protocol::Response::Success();
}
