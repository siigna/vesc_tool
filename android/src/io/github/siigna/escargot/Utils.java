/*
    Copyright 2019 - 2025 Benjamin Vedder	benjamin@vedder.se

    This file is part of VESC Tool.

    VESC Tool is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VESC Tool is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

package io.github.siigna.escargot;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.provider.Settings;
import android.os.Build;
import android.text.TextUtils;
import android.provider.Settings.SettingNotFoundException;
import android.location.LocationManager;
import android.view.WindowInsets;
import android.app.Activity;
import android.view.Window;
import android.graphics.Rect;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;

public class Utils
{
    public static void startVForegroundService(Context ctx) {
        Intent intent = new Intent(ctx, VForegroundService.class);
        intent.setAction(VForegroundService.ACTION_START_FOREGROUND_SERVICE);
        ctx.startService(intent);
    }

    public static void stopVForegroundService(Context ctx) {
        Intent intent = new Intent(ctx, VForegroundService.class);
        intent.setAction(VForegroundService.ACTION_STOP_FOREGROUND_SERVICE);
        ctx.startService(intent);
    }

    public static boolean checkLocationEnabled(Context ctx) {
        int locationMode = 0;
        String locationProviders;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            LocationManager lm = (LocationManager) ctx.getSystemService(Context.LOCATION_SERVICE);
            return lm.isLocationEnabled();
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT){
            try {
                locationMode = Settings.Secure.getInt(ctx.getContentResolver(), Settings.Secure.LOCATION_MODE);
            } catch (SettingNotFoundException e) {
                e.printStackTrace();
                return false;
            }

            return locationMode != Settings.Secure.LOCATION_MODE_OFF;
        } else {
            locationProviders = Settings.Secure.getString(ctx.getContentResolver(), Settings.Secure.LOCATION_PROVIDERS_ALLOWED);
            return !TextUtils.isEmpty(locationProviders);
        }
    }

    public static Activity getActivity(Context context) {
        if (context == null) return null;
        if (context instanceof Activity) return (Activity) context;
        if (context instanceof ContextWrapper) return getActivity(((ContextWrapper)context).getBaseContext());
        return null;
    }

    public static int topBarHeight(Context ctx) {
        try {
            if (Build.VERSION.SDK_INT >= 35) {
                WindowInsets windowInsets = getActivity(ctx).getWindow().getDecorView().getRootWindowInsets();
                return windowInsets.getInsets(WindowInsets.Type.systemBars()).top;
            } else {
                return 0;
            }
        } catch (Exception e) {
            return 0;
        }
    }

    public static int bottomBarHeight(Context ctx) {
        try {
            if (Build.VERSION.SDK_INT >= 35) {
                WindowInsets windowInsets = getActivity(ctx).getWindow().getDecorView().getRootWindowInsets();

                int hKeyboard = windowInsets.getInsets(WindowInsets.Type.ime()).bottom;
                int hBars = windowInsets.getInsets(WindowInsets.Type.systemBars()).bottom;

                if (hKeyboard > 0) {
                    return hKeyboard;
                } else {
                    return hBars;
                }
            } else {
                return 0;
            }
        } catch (Exception e) {
            return 0;
        }
    }

    public static int rightBarHeight(Context ctx) {
        try {
            if (Build.VERSION.SDK_INT >= 35) {
                WindowInsets windowInsets = getActivity(ctx).getWindow().getDecorView().getRootWindowInsets();
                return windowInsets.getInsets(WindowInsets.Type.systemBars()).right;
            } else {
                return 0;
            }
        } catch (Exception e) {
            return 0;
        }
    }

    public static int leftBarHeight(Context ctx) {
        try {
            if (Build.VERSION.SDK_INT >= 35) {
                WindowInsets windowInsets = getActivity(ctx).getWindow().getDecorView().getRootWindowInsets();
                return windowInsets.getInsets(WindowInsets.Type.systemBars()).left;
            } else {
                return 0;
            }
        } catch (Exception e) {
            return 0;
        }
    }

    /*
     * Storage access framework helpers, for the ride log.
     *
     * Logs used to be written straight into Documents/logs with
     * WRITE_EXTERNAL_STORAGE. That has not worked since API 30, where legacy
     * external storage is ignored; the permission request in
     * Utility::requestFilePermission was a stub returning true, so the write
     * simply failed and the comment there said as much.
     *
     * Instead the user grants one directory, once, and Android remembers it.
     * No storage permission is involved at all, the log survives uninstall,
     * and a file manager can reach it.
     *
     * The picker itself is launched from C++ (Utility::pickLogDirectory),
     * because QtAndroid::startActivity already carries a result callback and
     * doing it here would need onActivityResult plumbing through QtActivity.
     */

    /* Makes a granted tree URI survive a reboot. Without this the grant
     * lasts only as long as the process. */
    public static boolean takeTreePermission(Context ctx, String treeUri) {
        try {
            ContentResolver cr = ctx.getContentResolver();
            cr.takePersistableUriPermission(
                    Uri.parse(treeUri),
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                            | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    /* True while the grant is still held. A user can revoke it in settings,
     * or the directory can be on a volume that is no longer mounted, and
     * either way the next log would fail with no explanation. */
    public static boolean hasTreePermission(Context ctx, String treeUri) {
        try {
            Uri uri = Uri.parse(treeUri);
            for (android.content.UriPermission p :
                    ctx.getContentResolver().getPersistedUriPermissions()) {
                if (p.getUri().equals(uri) && p.isWritePermission()) {
                    return true;
                }
            }
            return false;
        } catch (Exception e) {
            return false;
        }
    }

    /* Something to show the user. Falls back to the raw tree id rather than
     * to an empty string, so the settings page never looks unset when it is
     * actually set. */
    public static String treeDisplayName(Context ctx, String treeUri) {
        try {
            Uri uri = Uri.parse(treeUri);
            Uri doc = DocumentsContract.buildDocumentUriUsingTree(
                    uri, DocumentsContract.getTreeDocumentId(uri));

            Cursor c = ctx.getContentResolver().query(
                    doc,
                    new String[] { DocumentsContract.Document.COLUMN_DISPLAY_NAME },
                    null, null, null);

            if (c != null) {
                try {
                    if (c.moveToFirst() && !c.isNull(0)) {
                        String name = c.getString(0);
                        if (name != null && name.length() > 0) {
                            return name;
                        }
                    }
                } finally {
                    c.close();
                }
            }

            return DocumentsContract.getTreeDocumentId(uri);
        } catch (Exception e) {
            return "";
        }
    }

    /*
     * Creates one CSV in the granted tree and returns a writable file
     * descriptor, detached so the caller owns it.
     *
     * Returns -1 on any failure. The caller cannot usefully distinguish the
     * reasons -- revoked grant, unmounted volume, no space -- and all of them
     * mean the same thing to it: do not start logging.
     *
     * text/csv rather than a wildcard, so the provider does not append its
     * own extension to a name that already has one.
     */
    public static int createLogFile(Context ctx, String treeUri, String displayName) {
        ParcelFileDescriptor pfd = null;

        try {
            Uri tree = Uri.parse(treeUri);
            Uri parent = DocumentsContract.buildDocumentUriUsingTree(
                    tree, DocumentsContract.getTreeDocumentId(tree));

            Uri file = DocumentsContract.createDocument(
                    ctx.getContentResolver(), parent, "text/csv", displayName);

            if (file == null) {
                return -1;
            }

            pfd = ctx.getContentResolver().openFileDescriptor(file, "w");

            if (pfd == null) {
                return -1;
            }

            return pfd.detachFd();
        } catch (Exception e) {
            if (pfd != null) {
                try {
                    pfd.close();
                } catch (Exception ignored) {
                }
            }
            return -1;
        }
    }
}
