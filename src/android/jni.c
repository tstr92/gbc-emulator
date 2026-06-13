
/*---------------------------------------------------------------------*
 *                                                                     *
 *                           GBC Emulator                              *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: jni.c                                                *
 *        author: tstr92                                               *
 *          date: 2025-04-08                                           *
 *                                                                     *
 *  java package: dev.tstr92.cgbemu                                    *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdio.h>

#include "jni.h"
#include "emulator.h"
#include "bus.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
static uint8_t speed = 10;


/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/
JavaVM *gJavaVM;

/*---------------------------------------------------------------------*
 *  Helper to get JNIEnv for current thread                            *
 *---------------------------------------------------------------------*/
static JNIEnv* getJNIEnv(void)
{
    JNIEnv *env = NULL;
    if ((*gJavaVM)->GetEnv(gJavaVM, (void**)&env, JNI_VERSION_1_6) != JNI_OK)
    {
        // Thread not attached, attach it
        if ((*gJavaVM)->AttachCurrentThread(gJavaVM, &env, NULL) != 0)
        {
            return NULL; // failed to attach
        }
        // Note: if you attach manually, remember to detach when thread exits
    }
    return env;
}

/*---------------------------------------------------------------------*
 *  Wrappers for C-Functions to be called from Java                    *
 *---------------------------------------------------------------------*/
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved)
{
    (void)reserved;
    gJavaVM = vm; // save JVM pointer for later
    return JNI_VERSION_1_6;
}

JNIEXPORT jint JNICALL
Java_dev_tstr92_cgbemu_CgbCore_LoadGame(JNIEnv *env, jclass clazz, jbyteArray rom, jbyteArray ram, jbyteArray rtc)
{
    (void) env;
    (void) clazz;

    jsize romLength = (*env)->GetArrayLength(env, rom);
    jbyte *romBytes = (*env)->GetByteArrayElements(env, rom, NULL);
    jsize ramLength = (*env)->GetArrayLength(env, ram);
    jbyte *ramBytes = (*env)->GetByteArrayElements(env, ram, NULL);
    jsize rtcLength = (*env)->GetArrayLength(env, rtc);
    jbyte *rtcBytes = (*env)->GetByteArrayElements(env, rtc, NULL);

    if ((!romBytes) || (0 == romLength))
    {
        return (jint) !0;
    }

    uint8_t * _ram = (0 < ramLength) ? (uint8_t *) ramBytes : NULL;
    rtc_t* _rtc = (sizeof(rtc_t) == rtcLength) ? (rtc_t *) rtcBytes : NULL;
    int result = emulator_load_game((uint8_t *)romBytes, (size_t)romLength, _ram, (size_t)ramLength, _rtc);

    (*env)->ReleaseByteArrayElements(env, rom, romBytes, 0);
    (*env)->ReleaseByteArrayElements(env, ram, ramBytes, 0);
    (*env)->ReleaseByteArrayElements(env, rtc, rtcBytes, 0);

    return (jint) result;
}

JNIEXPORT void JNICALL
Java_dev_tstr92_cgbemu_CgbCore_EmulatorSetSpeed(JNIEnv *env, jclass clazz, jbyte _speed)
{
    (void) env;
    (void) clazz;
    speed = (uint8_t) _speed;
}

JNIEXPORT void JNICALL
Java_dev_tstr92_cgbemu_CgbCore_EmulatorRun(JNIEnv *env, jclass clazz)
{
    (void) env;
    (void) clazz;
    emulator_run();
}

JNIEXPORT void JNICALL
Java_dev_tstr92_cgbemu_CgbCore_BusTick(JNIEnv *env, jclass clazz)
{
    (void) env;
    (void) clazz;
    bus_tick();
}

JNIEXPORT void JNICALL
Java_dev_tstr92_cgbemu_CgbCore_EmulatorStop(JNIEnv *env, jclass clazz)
{
    (void) env;
    (void) clazz;
    emulator_stop();
}

JNIEXPORT jstring JNICALL
Java_dev_tstr92_cgbemu_CgbCore_GetCartridgeTitle(JNIEnv *env, jclass clazz)
{
    (void) clazz;
    char buffer[20];
    bus_get_title(buffer, sizeof(buffer));
    return (*env)->NewStringUTF(env, buffer);
}

// JNIEXPORT jobject JNICALL
// Java_dev_tstr92_cgbemu_CgbCore_GetAudioData(JNIEnv *env, jclass clazz)
// {
//     uint8_t audio_r[NUM_AUDIO_SAMPLES_PER_FRAME], audio_l[NUM_AUDIO_SAMPLES_PER_FRAME];
//     size_t num_samples;

//     emulator_get_audio_data(audio_r, audio_l, &num_samples);

//     jbyteArray arr_r = (*env)->NewByteArray(env, num_samples);
//     jbyteArray arr_l = (*env)->NewByteArray(env, num_samples);
//     if (!arr_r || !arr_l)
//     {
//         return NULL;
//     }

//     (*env)->SetByteArrayRegion(env, arr_r, 0, num_samples, (jbyte*)audio_r);
//     (*env)->SetByteArrayRegion(env, arr_l, 0, num_samples, (jbyte*)audio_l);

//     jclass resultCls = (*env)->FindClass(env, "dev/tstr92/cgbemu/StereoAudio");
//     if (!resultCls)
//     {
//         return NULL;
//     }

//     jmethodID ctor = (*env)->GetMethodID(env, resultCls, "<init>", "([B[B)V");

//     jobject result = (*env)->NewObject(env, resultCls, ctor, arr_r, arr_l);

//     // local refs cleaned automatically at end of JNI call
//     return result;
// }

// JNIEXPORT jintArray JNICALL
// Java_dev_tstr92_cgbemu_CgbCore_GetScreen(JNIEnv *env, jclass clazz)
// {
//     uint32_t screen[CGB_SCREEN_HEIGTH][CGB_SCREEN_WIDTH];

//     emulator_get_video_data((uint32_t*) screen);

//     jintArray jScreen = (*env)->NewIntArray(env, CGB_SCREEN_WIDTH * CGB_SCREEN_HEIGTH);
//     if (!jScreen)
//     {
//         return NULL;
//     }

//     (*env)->SetIntArrayRegion(env, jScreen, 0, CGB_SCREEN_WIDTH * CGB_SCREEN_HEIGTH, (jint*)screen);
//     return jScreen;
// }

JNIEXPORT jbyteArray JNICALL
Java_dev_tstr92_cgbemu_CgbCore_BusGetSram(JNIEnv *env, jclass clazz)
{
    sram_t *p_sram = bus_get_sram();
    if (!p_sram)
    {
        return NULL;
    }
    jbyteArray sram = (*env)->NewByteArray(env, sizeof(sram_t));
    if (!sram)
    {
        return NULL;
    }
    (*env)->SetByteArrayRegion(env, sram, 0, sizeof(sram_t), (jbyte*)p_sram);
    return sram;
}

JNIEXPORT jbyteArray JNICALL
Java_dev_tstr92_cgbemu_CgbCore_BusGetRtc(JNIEnv *env, jclass clazz)
{
    jbyteArray rtc;

    rtc_t *p_rtc = bus_get_rtc();
    
    if (p_rtc)
    {
        rtc = (*env)->NewByteArray(env, sizeof(rtc_t));
        if (rtc)
        {
            (*env)->SetByteArrayRegion(env, rtc, 0, sizeof(rtc_t), (jbyte*)p_rtc);
        }
    }
    else
    {
        rtc = (*env)->NewByteArray(env, 0);
    }

    return rtc;
}

JNIEXPORT void JNICALL
Java_dev_tstr92_cgbemu_CgbCore_EmulatorSaveInternalState(JNIEnv *env, jclass clazz)
{
    (void) env;
    (void) clazz;
	emulator_write_save_file();
}

JNIEXPORT jint JNICALL
Java_dev_tstr92_cgbemu_CgbCore_EmulatorInternalStateSize(JNIEnv *env, jclass clazz)
{
    return (jint) emulator_get_save_file_size();
}

JNIEXPORT jint JNICALL
Java_dev_tstr92_cgbemu_CgbCore_EmulatorLoadInternalState(JNIEnv *env, jclass clazz)
{
    return (jint) emulator_load_save_file();
}

JNIEXPORT void JNICALL
Java_dev_tstr92_cgbemu_CgbCore_AddSecondsToRtc(JNIEnv *env, jclass clazz, jint seconds)
{
    bus_add_rtc_seconds((uint32_t) seconds);
}

/*---------------------------------------------------------------------*
 *  Wrappers for Java-Functions to be called from C                    *
 *---------------------------------------------------------------------*/
int android_printf(const char *fmt, ...)
{
    static char msg[4096];
    int ret;

    JNIEnv *env = getJNIEnv();
    if (!env)
    {
        return 0;
    }

    jclass cls = (*env)->FindClass(env, "dev/tstr92/cgbemu/CgbCore");
    if (!cls)
    {
        return 0;
    }

    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "log", "(Ljava/lang/String;)V");
    if (!mid)
    {
        (*env)->DeleteLocalRef(env, cls);
        return 0;
    }
    
    va_list args;
    va_start(args, fmt);
    ret = vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    jstring jmsg = (*env)->NewStringUTF(env, msg);
    (*env)->CallStaticVoidMethod(env, cls, mid, jmsg);

    (*env)->DeleteLocalRef(env, jmsg);
    (*env)->DeleteLocalRef(env, cls);

    return ret;
}

void emulator_cb_audio_ready(void)
{
    uint8_t audio_r[NUM_AUDIO_SAMPLES_PER_FRAME], audio_l[NUM_AUDIO_SAMPLES_PER_FRAME];
    size_t num_samples;
    emulator_get_audio_data(audio_r, audio_l, &num_samples);

    JNIEnv *env = getJNIEnv();
    if (!env)
    {
        return;
    }

    jclass cls = (*env)->FindClass(env, "dev/tstr92/cgbemu/CgbCore");
    if (!cls)
    {
        return;
    }

    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "push_audio", "([B[B)V");
    if (!mid)
    {
        (*env)->DeleteLocalRef(env, cls);
        return;
    }

    jbyteArray arrL = (*env)->NewByteArray(env, num_samples);
    jbyteArray arrR = (*env)->NewByteArray(env, num_samples);

    (*env)->SetByteArrayRegion(env, arrL, 0, num_samples, (jbyte*) audio_l);
    (*env)->SetByteArrayRegion(env, arrR, 0, num_samples, (jbyte*) audio_r);

    (*env)->CallStaticVoidMethod(env, cls, mid, arrL, arrR);

    (*env)->DeleteLocalRef(env, arrL);
    (*env)->DeleteLocalRef(env, arrR);
    (*env)->DeleteLocalRef(env, cls);

    return;
}

void emulator_cb_push_video(uint32_t framebuffer[144][160])
{
    JNIEnv *env = getJNIEnv();
    if (!env)
    {
        return;
    }

    jclass cls = (*env)->FindClass(env, "dev/tstr92/cgbemu/CgbCore");
    if (!cls)
    {
        return;
    }

    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "push_video", "([I)V");
    if (!mid)
    {
        (*env)->DeleteLocalRef(env, cls);
        return;
    }

    jintArray jFramebuffer = (*env)->NewIntArray(env, 144 * 160);
    (*env)->SetIntArrayRegion(env, jFramebuffer, 0, 144 * 160, (const jint*) framebuffer);

    (*env)->CallStaticVoidMethod(env, cls, mid, jFramebuffer);

    (*env)->DeleteLocalRef(env, jFramebuffer);
    (*env)->DeleteLocalRef(env, cls);

    return;
}

uint32_t platform_getSysTick_ms(void)
{
    return 0;
}

uint8_t gbc_joypad_buttons_cb(void)
{
    JNIEnv *env = getJNIEnv();
    if (!env)
    {
        return 0;
    }

    jclass cls = (*env)->FindClass(env, "dev/tstr92/cgbemu/CgbCore");
    if (!cls)
    {
        return 0;
    }

    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "get_buttons", "()B");
    if (!mid)
    {
        (*env)->DeleteLocalRef(env, cls);
        return 0;
    }

    // Call method
    jbyte result = (*env)->CallStaticByteMethod(env, cls, mid);

    // Cleanup
    (*env)->DeleteLocalRef(env, cls);

    return (uint8_t) result;
}


uint8_t emulator_get_speed(void)
{
    return speed;
}


void emulator_cb_write_to_save_file(const uint8_t *data, size_t size, char *name)
{
    (void) name;
    JNIEnv *env = getJNIEnv();
    if (!env)
    {
        return;
    }

    jclass cls = (*env)->FindClass(env, "dev/tstr92/cgbemu/CgbCore");
    if (!cls)
    {
        return;
    }

    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "save_internal_state_cb", "([B)V");
    if (!mid)
    {
        (*env)->DeleteLocalRef(env, cls);
        return;
    }

    jbyteArray jData = (*env)->NewByteArray(env, size);
    (*env)->SetByteArrayRegion(env, jData, 0, size, (const jbyte*) data);

    (*env)->CallStaticVoidMethod(env, cls, mid, jData);

    (*env)->DeleteLocalRef(env, jData);
    (*env)->DeleteLocalRef(env, cls);
    
    return;
}

int emulator_cb_read_from_save_file(uint8_t *data, size_t size)
{
    JNIEnv *env = getJNIEnv();
    if (!env)
    {
        return 1;
    }

    jclass cls = (*env)->FindClass(env, "dev/tstr92/cgbemu/CgbCore");
    if (!cls)
    {
        return 1;
    }

    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "read_internal_state_cb", "(Ljava/nio/ByteBuffer;I)V");
    if (!mid)
    {
        (*env)->DeleteLocalRef(env, cls);
        return 1;
    }

    jobject byteBuffer = (*env)->NewDirectByteBuffer(env, data, size);
    (*env)->CallStaticVoidMethod(env, cls, mid, byteBuffer, (jint)size);

    (*env)->DeleteLocalRef(env, byteBuffer);
    (*env)->DeleteLocalRef(env, cls);

    return 0;
}

void emulator_tick_cb(void)
{
}

void emulator_debug_pixel_draw_event(void)
{
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
