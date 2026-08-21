package com.iediot.gbemu;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class GBImport {

    public static final int REQUEST_CODE = 0x6742;
    private static String sDestDir;

    public static void present(final Activity activity, final String destDir) {
        sDestDir = destDir;
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType("*/*");
                intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                activity.startActivityForResult(intent, REQUEST_CODE);
            }
        });
    }

    public static void onResult(Activity activity, Intent data) {
        if (data == null) {
            return;
        }
        if (data.getClipData() != null) {
            int count = data.getClipData().getItemCount();
            for (int i = 0; i < count; i++) {
                copy(activity, data.getClipData().getItemAt(i).getUri());
            }
        } else if (data.getData() != null) {
            copy(activity, data.getData());
        }
        nativeImportDone();
    }

    private static void copy(Activity activity, Uri uri) {
        if (uri == null || sDestDir == null) {
            return;
        }
        String name = displayName(activity, uri);
        if (name == null) {
            return;
        }
        File dest = new File(sDestDir, name);
        InputStream in = null;
        OutputStream out = null;
        try {
            in = activity.getContentResolver().openInputStream(uri);
            if (in == null) {
                return;
            }
            out = new FileOutputStream(dest);
            byte[] buffer = new byte[64 * 1024];
            int read;
            while ((read = in.read(buffer)) > 0) {
                out.write(buffer, 0, read);
            }
        } catch (Exception e) {
            Log.e("gbemu", "import failed for " + name, e);
            dest.delete();
        } finally {
            close(in);
            close(out);
        }
    }

    // the picker hands back a content uri, the real file name lives in its metadata
    private static String displayName(Activity activity, Uri uri) {
        Cursor cursor = null;
        try {
            cursor = activity.getContentResolver().query(uri, null, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (column >= 0) {
                    return new File(cursor.getString(column)).getName();
                }
            }
        } catch (Exception e) {
            Log.e("gbemu", "could not resolve a name for " + uri, e);
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
        return uri.getLastPathSegment() == null
                ? null
                : new File(uri.getLastPathSegment()).getName();
    }

    private static void close(java.io.Closeable c) {
        if (c != null) {
            try {
                c.close();
            } catch (Exception ignored) {
            }
        }
    }

    public static native void nativeImportDone();
}
