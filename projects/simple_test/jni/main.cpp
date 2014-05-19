/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

//BEGIN_INCLUDE(all)
#include <jni.h>
#include <errno.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/asset_manager.h>

#include "utils/file_io.hpp"
#include "img_io/png_io.hpp"
#include "gl_fw/glmobj.hpp"
#include "utils/file_info.hpp"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "million-aa", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "million-aa", __VA_ARGS__))

/**
 * Our saved state data.
 */
struct saved_state {
    float angle;
    int32_t x;
    int32_t y;
};

/**
 * Shared state for our app.
 */
struct engine_t {
    android_app* app;

    ASensorManager* sensorManager;
    const ASensor* accelerometerSensor;
    ASensorEventQueue* sensorEventQueue;

    int animating;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;

    int32_t width;
    int32_t height;
    saved_state state;

	img::png_io	png_;
	gl::mobj	mobj_;
	bool		setup_;
	gl::mobj::handle	hnd_;

	engine_t() : app(0),
		sensorManager(0), accelerometerSensor(0), sensorEventQueue(0),
		animating(0), width(0), height(0), setup_(false), hnd_(0) { }
};


static bool get_png_file(android_app* app, const std::string& name, img::png_io& png)
{
	AAssetManager* amgr = app->activity->assetManager;
	if(amgr == 0) return false;

	AAsset* asset = AAssetManager_open(amgr, name.c_str(), AASSET_MODE_UNKNOWN);
	if(asset == 0) return false; 
	size_t sz = static_cast<size_t>(AAsset_getLength(asset));
	char* bf = new char[sz];
	utils::file_io fio;
	fio.open(bf, sz);
	bool f = false;
	if(AAsset_read(asset, bf, sz) == sz) {
		f = png.load(fio);
	}
	fio.close();
	delete[] bf;
	AAsset_close(asset);
	return f;
}

#if 0

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.FontMetrics;

public class Font {
	static Paint paint_ = new Paint(); // 描画情報 
	static FontMetrics font_ = paint_.getFontMetrics(); // フォントメトリクス 
	static Bitmap bitmap_;
	
	/** 
	 * 描画 
	 * @param text
	 */
	public static int get_bitmap(String text) {
		// 文字サイズの取得  
		int w = (int) (paint_.measureText(text) + 0.5f);
		int h = (int) (Math.abs(font_.top) + Math.abs(font_.bottom) + 0.5f);

		// キャンバスとビットマップを共有化  
//		bitmap_ = Bitmap.createBitmap(w, h, Config.ARGB_8888);
//		Canvas canvas = new Canvas(bitmap_);

		// 文字列をキャンバスに描画 
//		paint_.setAntiAlias(true);
//		canvas.drawText(text, 0, Math.abs(font_.ascent), paint_);
		
		// return bitmap_;
		return w;
	}
}



	// フォントのビットマップ取得関数の登録
	std::string cl = r + "/GL2JNILib";
	jclass jc = env->FindClass(cl.c_str());
	if(jc == 0) {
		LOGI("Can't find class: '%s'\n", cl.c_str());
	} else {
		jmethodID sid;
		sid = env->GetStaticMethodID(jc, "get_text_bitmap", "(Ljava/lang/String;)Landroid/graphics/Bitmap;");
		jstring text = env->NewStringUTF("漢");
		jobject bmp = env->CallObjectMethod(jc, sid, text);
		if(bmp == 0) {
			LOGI("Can't load bitmap:\n");
		} else {
			AndroidBitmapInfo info;
			AndroidBitmap_getInfo(env, bmp, &info);
///			LOGI("Font: %d, %d\n", info.width, info.height);
			img::img_gray8 img;
			img.create(vtx::spos(info.width, info.height));
			void* pix = 0;
			AndroidBitmap_lockPixels(env, bmp, &pix);
			if(pix) {
				const uint8_t* line = static_cast<const uint8_t*>(pix);
				vtx::spos pos;
				for(pos.y = 0; pos.y < static_cast<short>(info.height); ++pos.y) {
					const uint8_t* p = line;
					for(pos.x = 0; pos.x < static_cast<short>(info.width); ++pos.x) {
						img::gray8 g;
						g.g = *p++;
						img.put_pixel(pos, g);
					}
					line += info.stride;
				}
			}
			AndroidBitmap_unlockPixels(env, bmp);
		}
	}
#endif


/**
 * Initialize an EGL context for the current display.
 */
static int engine_init_display(engine_t& eg) {
    // initialize OpenGL ES and EGL

    /*
     * Here specify the attributes of the desired configuration.
     * Below, we select an EGLConfig with at least 8 bits per color
     * component compatible with on-screen windows
     */
    const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_NONE
    };
    EGLint w, h, dummy, format;
    EGLint numConfigs;
    EGLConfig config;

    eg.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(eg.display, 0, 0);

    /* Here, the application chooses the configuration it desires. In this
     * sample, we have a very simplified selection process, where we pick
     * the first EGLConfig that matches our criteria */
    eglChooseConfig(eg.display, attribs, &config, 1, &numConfigs);

    /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
     * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
     * As soon as we picked a EGLConfig, we can safely reconfigure the
     * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
    eglGetConfigAttrib(eg.display, config, EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry(eg.app->window, 0, 0, format);

    eg.surface = eglCreateWindowSurface(eg.display, config, eg.app->window, 0);
    eg.context = eglCreateContext(eg.display, config, 0, 0);

    if (eglMakeCurrent(eg.display, eg.surface, eg.surface, eg.context) == EGL_FALSE) {
        LOGW("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(eg.display, eg.surface, EGL_WIDTH, &w);
    eglQuerySurface(eg.display, eg.surface, EGL_HEIGHT, &h);

    eg.width = w;
    eg.height = h;
    eg.state.angle = 0;

    // Initialize GL state.
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_DEPTH_TEST);

    return 0;
}

/**
 * Just the current frame in the display.
 */
static void engine_draw_frame(engine_t& eg) {
    if (eg.display == 0) {
        // No display.
        return;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrthof((float)0, (float)eg.width,
			(float)eg.height, (float)0, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if(!eg.setup_) {
		eg.mobj_.initialize();

		if(get_png_file(eg.app, "test.png", eg.png_)) {
			const img::i_img* img = eg.png_.get_image_if();
			if(img) {
				LOGI("Image: %d, %d\n",
					static_cast<int>(img->get_size().x), static_cast<int>(img->get_size().y));
				eg.hnd_ = eg.mobj_.install(img);
			}
		}
	 	eg.setup_ = true;
	}

	if(eg.hnd_) {
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		eg.mobj_.setup_matrix();
		eg.mobj_.draw(eg.hnd_, gl::mobj::attribute::normal, vtx::spos(0, 0));
		eg.mobj_.restore_matrix();
	}

    eglSwapBuffers(eg.display, eg.surface);
}

/**
 * Tear down the EGL context currently associated with the display.
 */
static void engine_term_display(engine_t& eg)
{
	eg.mobj_.destroy();
	eg.png_.destroy();

    if (eg.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(eg.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eg.context != EGL_NO_CONTEXT) {
            eglDestroyContext(eg.display, eg.context);
        }
        if (eg.surface != EGL_NO_SURFACE) {
            eglDestroySurface(eg.display, eg.surface);
        }
        eglTerminate(eg.display);
    }
	eg.setup_ = false;
    eg.animating = 0;
    eg.display = EGL_NO_DISPLAY;
    eg.context = EGL_NO_CONTEXT;
    eg.surface = EGL_NO_SURFACE;
}

/**
 * Process the next input event.
 */
static int engine_handle_input(struct android_app* app, AInputEvent* event) {
    engine_t* engine = (engine_t*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        engine->animating = 1;
        engine->state.x = AMotionEvent_getX(event, 0);
        engine->state.y = AMotionEvent_getY(event, 0);
        return 1;
    }
    return 0;
}

/**
 * Process the next main command.
 */
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    engine_t* engine = (engine_t*)app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            // The system has asked us to save our current state.  Do so.
            engine->app->savedState = malloc(sizeof(struct saved_state));
            *((struct saved_state*)engine->app->savedState) = engine->state;
            engine->app->savedStateSize = sizeof(struct saved_state);
            break;
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            if (engine->app->window != 0) {
                engine_init_display(*engine);
                engine_draw_frame(*engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            engine_term_display(*engine);
            break;
        case APP_CMD_GAINED_FOCUS:
            // When our app gains focus, we start monitoring the accelerometer.
            if (engine->accelerometerSensor != 0) {
                ASensorEventQueue_enableSensor(engine->sensorEventQueue,
                        engine->accelerometerSensor);
                // We'd like to get 60 events per second (in us).
                ASensorEventQueue_setEventRate(engine->sensorEventQueue,
                        engine->accelerometerSensor, (1000L/60)*1000);
            }
            break;
        case APP_CMD_LOST_FOCUS:
            // When our app loses focus, we stop monitoring the accelerometer.
            // This is to avoid consuming battery while not being used.
            if (engine->accelerometerSensor != 0) {
                ASensorEventQueue_disableSensor(engine->sensorEventQueue,
                        engine->accelerometerSensor);
            }
            // Also stop animating.
			engine->setup_ = false;
            engine->animating = 0;
            engine_draw_frame(*engine);
            break;
    }
}

extern "C" {
	void android_main(struct android_app* state);
};

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* state) {
    engine_t eg;

    // Make sure glue isn't stripped.
    app_dummy();

    state->userData = &eg;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    eg.app = state;

    // Prepare to monitor accelerometer
    eg.sensorManager = ASensorManager_getInstance();
    eg.accelerometerSensor = ASensorManager_getDefaultSensor(eg.sensorManager,
            ASENSOR_TYPE_ACCELEROMETER);
    eg.sensorEventQueue = ASensorManager_createEventQueue(eg.sensorManager,
            state->looper, LOOPER_ID_USER, 0, 0);

	// アセット・ディレクトリー・リスト
	if(0) {
		LOGI("Start assert...\n");
		AAssetManager* amgr = eg.app->activity->assetManager;
		AAssetDir* d = AAssetManager_openDir(amgr, "");
		while(d != 0) {
			const char* fn = AAssetDir_getNextFileName(d);
			if(fn) {
				LOGI("%s\n", fn);
			} else {
				AAssetDir_close(d);
				break;
			}
		}
	}

    if (state->savedState != 0) {
        // We are starting with a previous saved state; restore from it.
        eg.state = *(struct saved_state*)state->savedState;
    }

    // loop waiting for stuff to do.

    while (1) {
        // Read all pending events.
        int ident;
        int events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while ((ident=ALooper_pollAll(eg.animating ? 0 : -1, 0, &events,
                (void**)&source)) >= 0) {

            // Process this event.
            if (source != 0) {
                source->process(state, source);
            }

            // If a sensor has data, process it now.
            if (ident == LOOPER_ID_USER) {
                if (eg.accelerometerSensor != 0) {
                    ASensorEvent event;
                    while (ASensorEventQueue_getEvents(eg.sensorEventQueue,
                            &event, 1) > 0) {
//                        LOGI("accelerometer: x=%f y=%f z=%f",
//                                event.acceleration.x, event.acceleration.y,
//                                event.acceleration.z);
                    }
                }
            }

            // Check if we are exiting.
            if (state->destroyRequested != 0) {
                engine_term_display(eg);
                return;
            }
        }

        if (eg.animating) {
            // Done with events; draw next animation frame.
            eg.state.angle += .01f;
            if (eg.state.angle > 1) {
                eg.state.angle = 0;
            }

            // Drawing is throttled to the screen update rate, so there
            // is no need to do timing here.
            engine_draw_frame(eg);
        }
    }
}
