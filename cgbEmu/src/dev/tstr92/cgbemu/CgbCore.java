
package dev.tstr92.cgbemu;
import anywheresoftware.b4a.BA;
import anywheresoftware.b4a.BA.ShortName;
import android.media.AudioTrack;

import java.nio.ByteBuffer;

import android.media.AudioFormat;
import android.media.AudioManager;

@ShortName("CgbCore")
public class CgbCore
{
    /*---------------------------------------------------------------------*
     *  Data                                                               *
     *---------------------------------------------------------------------*/
    private static BA ba;
    private static CgbCore inst;
    private static AudioTrack at;
    private static final int SAMPLE_RATE = 32768;
    private static byte buttons;
    // private static Thread emulatorCoreThread;
    private static Boolean run;
    private static Boolean running;
    private static int SaveStateOffset;
    private static int ReadDataOffset;
    public static int[] framebuffer;
    public static byte[] EmulatorSaveState;
    public static byte[] EmulatorLoadState;

    /*---------------------------------------------------------------------*
     *  Init                                                               *
     *---------------------------------------------------------------------*/
    public CgbCore()
    {
    }
    public void Initialize(BA _ba)
    {
        ba = _ba;
        inst = this;
        System.loadLibrary("CgbEmulator");

        int bufferSize = AudioTrack.getMinBufferSize(
            SAMPLE_RATE,
            AudioFormat.CHANNEL_OUT_STEREO,
            AudioFormat.ENCODING_PCM_8BIT
        );

        this.at = new AudioTrack(
            AudioManager.STREAM_MUSIC,
            SAMPLE_RATE,
            AudioFormat.CHANNEL_OUT_STEREO,
            AudioFormat.ENCODING_PCM_8BIT,
            bufferSize,
            AudioTrack.MODE_STREAM
        );

        framebuffer = new int[160 * 144];

        // emulatorCoreThread = new Thread(() -> EmulatorThread());
        run = false;
        running = false;
    }

    /*---------------------------------------------------------------------*
     *  Java Functions                                                     *
     *---------------------------------------------------------------------*/
    private static void EmulatorThread()
    {
        running = true;
        while (run)
        {
            BusTick();
        }
        at.pause();
        running = false;
        log("exit thread");
    }

    /*---------------------------------------------------------------------*
     *  Native Functions to be called from Java                            *
     *---------------------------------------------------------------------*/
    private static native void BusTick();
    private static native void EmulatorSetSpeed(byte speed);
    public native String GetCartridgeTitle();
    public native byte[] BusGetSram();
    public native byte[] BusGetRtc();
    private native void EmulatorSaveInternalState();
    private native int EmulatorInternalStateSize();
    private native int EmulatorLoadInternalState();
    public native void AddSecondsToRtc(int seconds);
    // private static native void EmulatorRun();
    // private static native void EmulatorStop();
    
    /*---------------------------------------------------------------------*
     *  Native Functions to be called from B4A                             *
     *---------------------------------------------------------------------*/
    public static native int LoadGame(byte[] rom, byte[] ram, byte[] rtc);
    // public static native int[] GetScreen();

    /*---------------------------------------------------------------------*
     *  Java Functions to be called from C                                 *
     *---------------------------------------------------------------------*/
    private static void log(String msg)
    {
        // BA.Log("c: "+msg); // this works well, but below is a wotking example i might need to look up later.
        ba.raiseEventFromDifferentThread(ba.activity, null, 0, "c_log", false, new Object[]{ msg });
    }

    private static byte get_buttons()
    {
        return buttons;
    }

    private static void push_audio(byte[] audio_l, byte[] audio_r)
    {
        
        while (at.getPlayState() == AudioTrack.PLAYSTATE_PAUSED)
        {
            try
            {
                Thread.sleep(10);
            }
            catch (InterruptedException e)
            {

            }
        }
        int samples = audio_l.length;
        byte[] interleaved = new byte[samples * 2];
        for (int i = 0; i < samples; i++)
        {
            interleaved[i*2 + 0] = audio_l[i];
            interleaved[i*2 + 1] = audio_r[i];
        }
        at.write(interleaved, 0, interleaved.length);
    }

    private static void push_video(int[] fb)
    {
        framebuffer = fb;
        ba.raiseEventFromDifferentThread(inst.ba.activity, null, 0, "draw", false, null);
    }

    private static void save_internal_state_cb(byte[] data)
    {
        System.arraycopy(data, 0, EmulatorSaveState, SaveStateOffset, data.length);
        SaveStateOffset += data.length;
    }

    private static void read_internal_state_cb(ByteBuffer buffer, int size)
    {
        for (int i = 0; i < size; i++)
        {
            buffer.put(i, EmulatorLoadState[ReadDataOffset + i]); // or your logic
        }
        ReadDataOffset += size;
    }

    /*---------------------------------------------------------------------*
     *  Java Functions to be called from B4A                               *
     *---------------------------------------------------------------------*/
    public void startEmulatorThread()
    {
        if (!running)
        {
            run = true;
            new Thread(() -> EmulatorThread()).start();
            at.play();
        }
        // emulatorCoreThread.start();
        // at.play();
    }
    public void stopEmulatorThread()
    {
        if (running)
        {
            run = false;
            while(running)
            {
                try
                {
                    Thread.sleep(10);
                }
                catch (InterruptedException e)
                {

                }
            }
        }
        // at.stop();
        // EmulatorStop();
    }
    public void EmulatorPause()
    {
        if (running)
        {
            at.pause();
        }
    }
    public void EmulatorResume()
    {
        if (running)
        {
            at.play();
        }
    }
    public void setButtons(byte b)
    {
        buttons = b;
    }
    public void setSpeed(int s)
    {
        if (10 > s)
        {
            EmulatorSetSpeed((byte) 10);
        }
        else if (20 < s)
        {
            EmulatorSetSpeed((byte) 20);
        }
        else
        {
            EmulatorSetSpeed((byte) s);
        }
    }
    public Boolean SaveGame()
    {
        EmulatorSaveState = new byte[EmulatorInternalStateSize()];
        SaveStateOffset = 0;
        EmulatorSaveInternalState();
        return SaveStateOffset == EmulatorSaveState.length;
    }
    public Boolean LoadGameState(byte[] loadState)
    {
        EmulatorLoadState = loadState;
        ReadDataOffset = 0;
        return 0 == EmulatorLoadInternalState();
    }
}
