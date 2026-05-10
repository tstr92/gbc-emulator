
package dev.tstr92.cgbemu;
import anywheresoftware.b4a.BA;
import anywheresoftware.b4a.BA.ShortName;
import android.media.AudioTrack;
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
    private static Thread emulatorCoreThread;

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

        emulatorCoreThread = new Thread(() -> EmulatorRun());
    }
    
    /*---------------------------------------------------------------------*
     *  Native Functions to be called from Java                            *
     *---------------------------------------------------------------------*/
    private static native void EmulatorRun();
    private static native void EmulatorStop();
    
    /*---------------------------------------------------------------------*
     *  Native Functions to be called from B4A                             *
     *---------------------------------------------------------------------*/
    public static native int LoadGame(byte[] rom, byte[] ram);
    public static native int[] GetScreen();

    /*---------------------------------------------------------------------*
     *  Java Functions to be called from C                                 *
     *---------------------------------------------------------------------*/
    public static void log(String msg)
    {
        // BA.Log("c: "+msg); // this works well, but below is a wotking example i might need to look up later.
        ba.raiseEventFromDifferentThread(ba.activity, null, 0, "c_log", false, new Object[]{ msg });
    }

    public static byte get_buttons()
    {
        return buttons;
    }

    public static void push_audio(byte[] audio_l, byte[] audio_r)
    {
        int samples = audio_l.length;
        byte[] interleaved = new byte[samples * 2];
        for (int i = 0; i < samples; i++)
        {
            interleaved[i*2 + 0] = audio_l[i];
            interleaved[i*2 + 1] = audio_r[i];
        }
        at.write(interleaved, 0, interleaved.length);
        ba.raiseEventFromDifferentThread(inst.ba.activity, null, 0, "draw", false, null);
    }

    /*---------------------------------------------------------------------*
     *  Java Functions to be called from B4A                               *
     *---------------------------------------------------------------------*/
    public void startEmulatorThread()
    {
        emulatorCoreThread.start();
        at.play();
    }
    public void EmulatorPause()
    {
        at.pause();
    }
    public void EmulatorResume()
    {
        at.play();
    }
    public void EmulatorDestroy()
    {
        at.stop();
        EmulatorStop();
    }
    public void setButtons(byte b)
    {
        buttons = b;
    }
}
