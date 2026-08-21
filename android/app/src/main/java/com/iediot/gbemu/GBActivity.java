package com.iediot.gbemu;

import android.app.Activity;
import android.content.Intent;

import org.libsdl.app.SDLActivity;

public class GBActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "SDL2_image", "main" };
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == GBImport.REQUEST_CODE && resultCode == Activity.RESULT_OK) {
            GBImport.onResult(this, data);
        }
    }
}
