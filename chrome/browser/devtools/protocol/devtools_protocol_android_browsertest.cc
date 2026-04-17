// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"

using DevToolsProtocolTest = DevToolsProtocolTestBase;

namespace {

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       BrowserSetDownloadBehaviorInRequestedBrowserContext) {
  AttachToBrowserTarget();

  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        if (request.relative_url == "/download-page.html") {
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/html");
          response->set_content(
              "<a id='download' href='/download.txt' download>"
              "download</a>");
          return response;
        }

        if (request.relative_url == "/download.txt") {
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/plain");
          response->AddCustomHeader("Content-Disposition",
                                    "attachment; filename=test.txt");
          response->set_content("downloaded");
          return response;
        }

        return nullptr;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::DictValue* result =
      SendCommandSync("Target.createBrowserContext");
  ASSERT_TRUE(result);
  const std::string context_id = *result->FindString("browserContextId");

  base::DictValue params;
  params.Set("url",
             embedded_test_server()->GetURL("/download-page.html").spec());
  params.Set("browserContextId", context_id);
  result = SendCommandSync("Target.createTarget", std::move(params));
  ASSERT_TRUE(result);
  const std::string target_id = *result->FindString("targetId");

  params = base::DictValue();
  params.Set("behavior", "allow");
  params.Set("browserContextId", context_id);
  params.Set("downloadPath", temp_dir.GetPath().AsUTF8Unsafe());
  params.Set("eventsEnabled", true);
  ASSERT_TRUE(
      SendCommandSync("Browser.setDownloadBehavior", std::move(params)));

  auto agent_host = content::DevToolsAgentHost::GetForId(target_id);
  ASSERT_TRUE(agent_host);
  content::WebContents* web_contents = agent_host->GetWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  ASSERT_TRUE(content::ExecJs(
      web_contents, "document.getElementById('download').click();"));

  base::DictValue download_will_begin =
      WaitForNotification("Browser.downloadWillBegin", true);
  const std::string* frame_id = download_will_begin.FindString("frameId");
  ASSERT_TRUE(frame_id);
  EXPECT_EQ(
      web_contents->GetPrimaryMainFrame()->GetDevToolsFrameToken().ToString(),
      *frame_id);

  base::DictValue download_progress;
  std::string state;
  do {
    download_progress = WaitForNotification("Browser.downloadProgress", true);
    const std::string* maybe_state = download_progress.FindString("state");
    ASSERT_TRUE(maybe_state);
    state = *maybe_state;
  } while (state == "inProgress");

  EXPECT_EQ("completed", state);
  const std::string* file_path = download_progress.FindString("filePath");
  ASSERT_TRUE(file_path);
  EXPECT_TRUE(base::StartsWith(*file_path, temp_dir.GetPath().AsUTF8Unsafe()));
}

}  // namespace
