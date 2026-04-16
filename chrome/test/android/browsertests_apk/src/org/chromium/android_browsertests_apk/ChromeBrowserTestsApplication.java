// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_browsertests_apk;

import android.content.Context;

import org.chromium.base.PathUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.browser.app.notifications.ContextualNotificationPermissionRequesterImpl;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.native_test.NativeBrowserTestApplication;
import org.chromium.ui.base.ResourceBundle;

/** A basic chrome.browser.tests {@link android.app.Application}. */
public class ChromeBrowserTestsApplication extends NativeBrowserTestApplication {
    static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "android_browsertests";
    // Matches the packaged Chrome locale paks used by Android browser tests.
    private static final String[] AVAILABLE_LOCALES = {
        "af", "am", "ar", "ar-XB", "as", "az", "be", "bg", "bn", "bs", "ca", "cs", "da", "de",
        "el", "en-GB", "en-US", "en-XA", "es", "es-419", "et", "eu", "fa", "fi", "fil", "fr",
        "fr-CA", "gl", "gu", "he", "hi", "hr", "hu", "hy", "id", "is", "it", "ja", "ka", "kk",
        "km", "kn", "ko", "ky", "lo", "lt", "lv", "mk", "ml", "mn", "mr", "ms", "my", "nb",
        "ne", "nl", "or", "pa", "pl", "pt-BR", "pt-PT", "ro", "ru", "si", "sk", "sl", "sq",
        "sr", "sr-Latn", "sv", "sw", "ta", "te", "th", "tr", "uk", "ur", "uz", "vi", "zh-CN",
        "zh-HK", "zh-TW", "zu"
    };

    @Override
    protected void attachBaseContext(Context base) {
        boolean isBrowserProcess = isBrowserProcess();

        if (isBrowserProcess) UmaUtils.recordMainEntryPointTime();

        super.attachBaseContext(base);
        LibraryLoader.getInstance()
                .setLibraryProcessType(
                        isBrowserProcess
                                ? LibraryProcessType.PROCESS_BROWSER
                                : LibraryProcessType.PROCESS_CHILD);
        if (isBrowserProcess) {
            // Test-only stuff, see also NativeUnitTest.java.
            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
            ResourceBundle.setAvailablePakLocales(AVAILABLE_LOCALES);
            // Some browser tests trigger access to ContextualNotificationPermissionRequester. It
            // is normally initialized as part of Chrome startup.
            // TODO(crbug.com/454692653): This class should share more code with the production
            // ChromeApplicationImpl so test startup would be more similar to production.
            ContextualNotificationPermissionRequesterImpl.initialize();
        }
    }
}
