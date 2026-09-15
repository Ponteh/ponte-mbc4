using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;

// Independent test inputs only. No production DSP or third-party recordings.
public static class PonteMeterStimuli
{
    const int Rate = 48000, Ramp = 96;
    public sealed class Event
    {
        public string Label, Signal;
        public long StartSample, EndSample;
        public double Frequency, LevelDb;
    }
    public sealed class Report
    {
        public string File, Sha256;
        public long Frames, Bytes;
        public double DurationSeconds, PeakDbfs;
        public List<Event> Events = new List<Event>();
    }
    static void Add(Report r, string label, double seconds, double frequency, double db, bool harmonic)
    {
        long end = r.Frames + (long)Math.Round(seconds * Rate);
        r.Events.Add(new Event { Label=label, StartSample=r.Frames, EndSample=end,
            Frequency=frequency, LevelDb=db, Signal=harmonic ? "harmonic_probe" : "sine" });
        r.Frames = end;
    }
    static void Quiet(Report r, double seconds) { Add(r, "silence", seconds, 0, -120, false); }
    static double Harmonic(double phase)
    {
        double v = Math.Sin(phase);
        for (int k=2; k<=12; ++k) v += (0.2/k) * Math.Sin(k*phase);
        return v;
    }
    public static List<Report> Generate(string folder)
    {
        var files = new List<Report>();
        var levels = new Report { File="01_IN_OUT_LEVELS.wav" };
        Quiet(levels, 2);
        foreach (double hz in new double[] {315, 2000})
        {
            foreach (double db in new double[] {-36,-24,-12,-6,-12,-24,-36})
                Add(levels, "level_plateau", 2.5, hz, db, false);
            Quiet(levels, 2);
        }
        Quiet(levels, 3); files.Add(levels);

        var bursts = new Report { File="02_IN_OUT_BURSTS.wav" };
        Quiet(bursts, 2);
        foreach (double hz in new double[] {315, 2000})
        {
            foreach (double duration in new double[] {.010,.030,.100,.300,1.000})
                foreach (double gap in new double[] {1.113,1.271,1.487})
                {
                    Add(bursts, "burst", duration, hz, -6, false);
                    Quiet(bursts, gap);
                }
            Quiet(bursts, 2);
        }
        Quiet(bursts, 3); files.Add(bursts);

        foreach (double hz in new double[] {315, 2000})
        {
            var gr = new Report { File=hz==315 ? "03_GR_BAND2_315Hz.wav" : "04_GR_BAND3_2000Hz.wav" };
            Quiet(gr, 2);
            Add(gr, "residual_baseline", 3, hz, -42, false);
            Add(gr, "long_compression", 3, hz, -6, false);
            Add(gr, "release_after_long", 6, hz, -42, false);
            Add(gr, "short_compression", .15, hz, -6, false);
            Add(gr, "release_after_short", 4, hz, -42, false);
            Add(gr, "medium_compression", .5, hz, -6, false);
            Add(gr, "release_after_medium", 4, hz, -42, false);
            Quiet(gr, 3); files.Add(gr);
        }

        var voice = new Report { File="05_FUNDAMENTAL_PROBE_90_120_180Hz.wav" };
        Quiet(voice, 2);
        foreach (double f0 in new double[] {90,120,180})
        {
            double[] durations = {1,.6,.25,.35,.35,.9,.5,.5,2.55};
            double[] dbs = {-42,-9,-42,-15,-42,-6,-42,-12,-42};
            for (int i=0; i<durations.Length; ++i)
                Add(voice, "fundamental_probe", durations[i], f0, dbs[i], true);
        }
        Quiet(voice, 3); files.Add(voice);

        double harmonicPeak = 0;
        for (int i=0; i<65536; ++i) harmonicPeak = Math.Max(harmonicPeak, Math.Abs(Harmonic(2*Math.PI*i/65536)));
        foreach (Report r in files) WriteAndVerify(folder, r, harmonicPeak);
        return files;
    }
    static void WriteAndVerify(string folder, Report r, double harmonicPeak)
    {
        string path = Path.Combine(folder, r.File);
        using (var w = new BinaryWriter(System.IO.File.Create(path)))
        {
            w.Write(Encoding.ASCII.GetBytes("RIFF")); w.Write((int)(36+r.Frames*6));
            w.Write(Encoding.ASCII.GetBytes("WAVEfmt ")); w.Write(16); w.Write((short)1); w.Write((short)2);
            w.Write(Rate); w.Write(Rate*6); w.Write((short)6); w.Write((short)24);
            w.Write(Encoding.ASCII.GetBytes("data")); w.Write((int)(r.Frames*6));
            double previousAmp=0, previousHz=315;
            bool previousHarmonic=false;
            foreach (Event e in r.Events)
            {
                double amp=e.Frequency==0 ? 0 : Math.Pow(10, e.LevelDb/20);
                double hz=e.Frequency==0 ? previousHz : e.Frequency;
                bool harmonic=e.Frequency==0 ? previousHarmonic : e.Signal=="harmonic_probe";
                for (long frame=e.StartSample; frame<e.EndSample; ++frame)
                {
                    long local=frame-e.StartSample;
                    double blend=local>=Ramp ? 1 : .5-.5*Math.Cos(Math.PI*local/Ramp);
                    double level=previousAmp+(amp-previousAmp)*blend;
                    double phase=2*Math.PI*hz*frame/Rate;
                    double sample=level*(harmonic ? Harmonic(phase)/harmonicPeak : Math.Sin(phase));
                    if (Math.Abs(sample)>=1 || double.IsNaN(sample)) throw new Exception("Invalid sample");
                    int pcm=(int)Math.Round(sample*8388607);
                    for (int ch=0; ch<2; ++ch)
                    { w.Write((byte)(pcm & 255)); w.Write((byte)((pcm>>8)&255)); w.Write((byte)((pcm>>16)&255)); }
                }
                previousAmp=amp; previousHz=hz; previousHarmonic=harmonic;
            }
        }
        // Re-read the encoded artifact; check header, length, stereo equality,
        // digital silence after fades and actual plateau peaks against the plan.
        byte[] data=System.IO.File.ReadAllBytes(path);
        if (data.Length!=44+r.Frames*6 || BitConverter.ToInt32(data,24)!=Rate
            || BitConverter.ToInt16(data,22)!=2 || BitConverter.ToInt16(data,34)!=24)
            throw new Exception("WAV header/length mismatch: "+path);
        double peak=0;
        foreach (Event e in r.Events)
        {
            double eventPeak=0;
            for (long frame=e.StartSample; frame<e.EndSample; ++frame)
            {
                int p=(int)(44+frame*6);
                for(int b=0;b<3;++b) if(data[p+b]!=data[p+3+b]) throw new Exception("Stereo mismatch");
                int value=data[p] | (data[p+1]<<8) | (data[p+2]<<16);
                if ((value & 0x800000)!=0) value-=0x1000000;
                double a=Math.Abs(value/8388607.0);
                peak=Math.Max(peak,a);
                if(frame>=e.StartSample+Ramp) eventPeak=Math.Max(eventPeak,a);
            }
            if(e.Frequency==0 && eventPeak!=0) throw new Exception("Silence mismatch");
            if(e.Frequency>0 && e.Signal=="sine" && e.EndSample-e.StartSample>=Rate/10
                && Math.Abs(20*Math.Log10(eventPeak)-e.LevelDb)>.02) throw new Exception("Plateau mismatch");
        }
        r.Bytes=data.Length; r.DurationSeconds=r.Frames/(double)Rate; r.PeakDbfs=20*Math.Log10(peak);
        using(var sha=SHA256.Create()) r.Sha256=BitConverter.ToString(sha.ComputeHash(data)).Replace("-", "");
    }
}
