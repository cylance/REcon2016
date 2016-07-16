////////////////////////////////////////////////////////////////
// ModemSwitchboard/PipeChannel.cs
//==============================================================
//
//  BBS-Era Exploitation for Fun and Anachronism
//  REcon 2016
//
//  Derek Soeder and Paul Mehta
//  Cylance, Inc.
//______________________________________________________________
//
// Modem emulator and switchboard for connecting RIPterm and
// Wildcat over VMware virtual serial ports.
//
// July 1, 2016
//
////////////////////////////////////////////////////////////////

using System;
using System.IO;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Security;
using System.Security.Principal;
using System.Threading;

namespace Cylance.Research.ModemSwitchboard
{

    internal sealed class PipeChannel : ChannelHandler
    {

        private const int ConnectTimeout = 250;  // milliseconds
        private const int BreakPulseDelay = 25;  // milliseconds

        private readonly string _PipeName;
        private readonly bool _AutoReconnect;

        private NamedPipeClientStream _PipeStream;
        private readonly object _PipeLock = new object();  // held when connecting, disconnecting, and obtaining a pipe stream reference

        public PipeChannel(string pipe, bool isWildcat)
        {
            this._PipeName = pipe;
            this._AutoReconnect = !isWildcat;
        }

        public override void Start()
        {
            new Thread(BlockingReceiveThread).Start();
        }

        private void BlockingReceiveThread()
        {
            byte[] buffer = new byte[0x8000];

            for (; ; )
            {
                NamedPipeClientStream pipestream;

                lock (this._PipeLock)
                {
                    pipestream = this._PipeStream;
                }

                if (pipestream == null)
                {
                    Thread.Sleep(BreakPulseDelay);

                    if (this._AutoReconnect && this.ConnectPipe())
                        this.OnOOBReceived(OOBSignal.Ready);

                    continue;
                }

                try
                {
                    for (; ; )
                    {
                        int len = pipestream.Read(buffer, 0, buffer.Length);
                        if (len <= 0) break;

                        this.OnDataReceived(buffer, 0, len);
                    }
                }
                catch (IOException) { }
                catch (InvalidOperationException) { }

                this.ClosePipe();

                this.OnOOBReceived(OOBSignal.Break);
            } //for(;;)
        }

        private bool ConnectPipe()
        {
            lock (this._PipeLock)
            {
                try
                {
                    NamedPipeClientStream pipestream = this._PipeStream;

                    if (pipestream != null)
                    {
                        if (pipestream.IsConnected)
                            return false;

                        this._PipeStream = null;

                        pipestream.Close();
                    }

                    pipestream =
                        new NamedPipeClientStream(
                            ".",
                            this._PipeName,
                            PipeDirection.InOut,
                            PipeOptions.Asynchronous,
                            TokenImpersonationLevel.Identification,
                            HandleInheritability.None);

                    pipestream.Connect(ConnectTimeout);

                    this._PipeStream = pipestream;

                    return pipestream.IsConnected;
                }
                catch (TimeoutException) { }
                catch (IOException) { }
                catch (SecurityException) { }
            } //lock

            return false;
        }

        private static void DrainPipe(NamedPipeClientStream pipeStream)
        {
            if (pipeStream == null || !pipeStream.IsConnected || !HasData(pipeStream))
                return;

            byte[] buffer = new byte[0x8000];

            do
            {
                if (pipeStream.Read(buffer, 0, buffer.Length) <= 0)
                    break;
            }
            while (HasData(pipeStream));
        }

        private void ClosePipe()
        {
            lock (this._PipeLock)
            {
                NamedPipeClientStream pipestream = this._PipeStream;

                if (pipestream == null)
                    return;

                this._PipeStream = null;

                try
                {
                    if (pipestream.IsConnected)
                    {
                        DrainPipe(pipestream);
                        pipestream.Close();
                    }
                }
                catch (IOException) { }
                catch (InvalidOperationException) { }

                try
                {
                    pipestream.Dispose();
                }
                catch (IOException) { }
                catch (InvalidOperationException) { }
            }
        }

        public override bool Write(byte[] bytes, int offset, int count)
        {
            NamedPipeClientStream pipestream;

            lock (this._PipeLock)
            {
                pipestream = this._PipeStream;
            }

            if (pipestream == null)
                return false;

            try
            {
                pipestream.Write(bytes, offset, count);
                return true;
            }
            catch (IOException) { }
            catch (InvalidOperationException) { }

            return false;
        }

        public override void Break()
        {
            this.ClosePipe();

            Thread.Sleep(BreakPulseDelay);

            this.ConnectPipe();  // make sure we're connected before returning (even though BlockingReceiveThread will also try to reconnect)
        }

        [DllImport("kernel32.dll", CallingConvention = CallingConvention.Winapi, ExactSpelling = true, SetLastError = true)]
        private static extern bool PeekNamedPipe(
            IntPtr hNamedPipe,
            IntPtr lpBuffer,
            int nBufferSize,
            IntPtr lpBytesRead,
            out int lpTotalBytesAvail,
            IntPtr lpBytesLeftThisMessage);

        private static bool HasData(NamedPipeClientStream pipeStream)
        {
            bool success = false;
            pipeStream.SafePipeHandle.DangerousAddRef(ref success);

            if (!success)
                throw new InvalidOperationException();

            try
            {
                IntPtr hpipe = pipeStream.SafePipeHandle.DangerousGetHandle();

                int avail;
                if (!PeekNamedPipe(hpipe, IntPtr.Zero, 0, IntPtr.Zero, out avail, IntPtr.Zero))
                    throw new InvalidOperationException();

                return (avail > 0);
            }
            finally
            {
                pipeStream.SafePipeHandle.DangerousRelease();
            }
        }

    } //class PipeChannel

}
