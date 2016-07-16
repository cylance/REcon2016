////////////////////////////////////////////////////////////////
// ModemSwitchboard/Modem.cs
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
using System.Collections.Generic;
using System.Text;
using System.Threading;

namespace Cylance.Research.ModemSwitchboard
{

    internal delegate ConnectResult DialCallback(Modem sender, string phoneNumber, ref Modem peer);
    internal delegate bool AnswerCallback(Modem sender, ref Modem peer);
    internal delegate void HangupCallback(Modem sender);

    internal sealed class Modem
    {

        public readonly string PhoneNumber;

        public event DialCallback Dial;
        public event AnswerCallback Answer;
        public event HangupCallback Hangup;

        private readonly ChannelHandler _Channel;

        private enum ModemState
        {
            Data = 0,
            DataPause = Data + 1,
            DataP = DataPause + 1,
            DataPP = DataP + 1,
            DataPPP = DataPP + 2,
            Command,
            CommandA = Command + 1,
            CommandAT = CommandA + 1
        }

        #region Guarded by _StateLock

        private readonly object _StateLock = new object();

        private ModemState _State;
        private StringBuilder _CommandBuffer;

        private Modem _Peer = null;

        private int _LastDataTime;
        private bool _CommandEcho;
        private bool _Ringing;

        #endregion

        private void Reset()
        {
            this._State = ModemState.Command;

            this._LastDataTime = Environment.TickCount;
            this._CommandEcho = false;
            this._Ringing = false;
        }

        public Modem(string phoneNumber, ChannelHandler channel)
        {
            if (phoneNumber == null || channel == null)
                throw new ArgumentNullException();

            this.PhoneNumber = phoneNumber;
            this._Channel = channel;

            this.Reset();

            channel.DataReceived += this.OnDataReceived;
            channel.OOBReceived += this.OnOOBReceived;
        }

        private const int EscapePauseInterval = 1000;

        private void OnDataReceived(byte[] bytes, int offset, int count)
        {
            ModemState state;
            bool commandecho;
            StringBuilder commandsb;

            lock (this._StateLock)
            {
                commandsb = this._CommandBuffer;
                this._CommandBuffer = null;

                if (commandsb == null)
                    commandsb = new StringBuilder();

                int now = Environment.TickCount;

                state = this._State;
                commandecho = this._CommandEcho;

                switch (state)
                {
                    case ModemState.Data:
                        if ((now - this._LastDataTime) >= EscapePauseInterval)
                            state = ModemState.DataPause;
                        break;

                    case ModemState.DataPPP:
                        if ((now - this._LastDataTime) >= EscapePauseInterval)
                            state = ModemState.Command;
                        break;
                }
                if (state == ModemState.Data && (now - this._LastDataTime) >= EscapePauseInterval)
                {
                    state = ModemState.DataPause;
                }

                this._LastDataTime = now;
            }

            int x, limit;
            for (x = offset, limit = offset + count; x < limit; x++)
            {
                byte b = bytes[x];

                switch (state)
                {
                    case ModemState.Data:
                        break;

                    case ModemState.DataPause:
                    case ModemState.DataP:
                        if (b == (byte)'+') state++;
                        else state = ModemState.Data;
                        break;

                    case ModemState.DataPP:
                    case ModemState.DataPPP:
                        if (b == (byte)'+') state = ModemState.DataPPP;
                        else state = ModemState.Data;
                        break;

                    case ModemState.Command:
                        if (b == (byte)'A') state = ModemState.CommandA;
                        break;

                    case ModemState.CommandA:
                        switch (b)
                        {
                            case (byte)'A': break;
                            case (byte)'T': state = ModemState.CommandAT; break;
                            default: state = ModemState.Command; break;
                        }
                        break;

                    case ModemState.CommandAT:
                        if (b == (byte)'\r' || b == (byte)'\n')
                        {
                            // echo command buffer before processing command, in case it generates its own output
                            //   (mode can only switch from command to data, and only once during this loop)
                            if (commandecho && offset <= x && state >= ModemState.Command && state <= ModemState.CommandAT)
                            {
                                this.ChannelWrite(bytes, offset, (x + 1) - offset);
                            }

                            string command = commandsb.ToString();
                            commandsb.Length = 0;
                            state = ModemState.Command;

                            lock (this._StateLock)  // commit state before DoCommand
                            {
                                this._State = state;
                                this._CommandBuffer = commandsb;
                            }

                            this.DoCommand(command);

                            lock (this._StateLock)  // reload state after DoCommand
                            {
                                state = this._State;
                                commandecho = this._CommandEcho;

                                commandsb = this._CommandBuffer;
                                this._CommandBuffer = null;

                                if (commandsb == null)
                                    commandsb = new StringBuilder();
                            }
                        }
                        else if (b != (byte)' ')
                        {
                            commandsb.Append((char)b);
                        }

                        offset = x + 1;
                        break;
                } //switch(state)
            } //for(x<limit)

            // can't switch from data mode to command mode during loop (because [pause] +++ [pause] is only detected before loop),
            //   but it could switch from command mode to data mode (ATO command)
            if (offset < limit)
            {
                if (state >= ModemState.Data && state <= ModemState.DataPPP)
                {
                    this.TransmitToPeer(bytes, offset, limit - offset);  // send data to remote modem
                }
                else if (commandecho && state >= ModemState.Command && state <= ModemState.CommandAT)
                {
                    this.ChannelWrite(bytes, offset, limit - offset);  // echo command
                }
            }

            lock (this._StateLock)  // commit state
            {
                this._State = state;
                this._CommandBuffer = commandsb;

                this._LastDataTime = Environment.TickCount;
            }
        }

        private void DoCommand(string command)
        {
            Console.WriteLine("[{0}] {1}", this.PhoneNumber, command);

            for (int x = 0; x < command.Length; )
            {
                switch (command[x++])
                {
                    case 'A':
                        lock (this._StateLock)
                        {
                            if (this._Ringing)
                            {
                                this.DoAnswer();
                            }
                        }
                        break;

                    case 'D':
                        if ((x + 2) <= command.Length && command[x++] == 'T')
                        {
                            this.DoDial(command.Substring(x));
                        }
                        return;

                    case 'E':
                        if (x < command.Length)
                        {
                            lock (this._StateLock)
                            {
                                switch (command[x++])
                                {
                                    case '0': this._CommandEcho = false; break;
                                    case '1': this._CommandEcho = true; break;
                                }
                            }
                        }
                        break;

                    case 'H':
                        if (x < command.Length && command[x++] == '0')
                        {
                            this.DoHangup();
                        }
                        break;

                    case 'O':
                        lock (this._StateLock)
                        {
                            if (this._Peer != null)
                                this._State = ModemState.Data;
                        }
                        break;

                    case 'Z':
                        lock (this._StateLock)
                        {
                            this.DoHangup();
                            this.Reset();
                        }
                        break;
                }
            }
        }

        private static readonly byte[] RingMessage = Encoding.ASCII.GetBytes("RING\r");
        private static readonly byte[] ResultCode19200 = Encoding.ASCII.GetBytes("14\r");

        private void OnOOBReceived(OOBSignal signal)
        {
            switch (signal)
            {
                case OOBSignal.Break:
                    lock (this._StateLock)
                    {
                        if (this._State >= ModemState.Data && this._State <= ModemState.DataPPP)
                            this._State = ModemState.Command;

                        this._Ringing = false;
                    }

                    this.DoHangup();  // let switchboard know call was disconnected
                    break;

                case OOBSignal.Ring:
                    this._Channel.Break();  // Wildcat hack

                    lock (this._StateLock)
                    {
                        this._Ringing = true;
                    }

                    //this.ChannelWrite(RingMessage);

                    // Wildcat hack:
                    this.ChannelWrite(ResultCode19200);

                    lock (this._StateLock)
                    {
                        this._State = ModemState.Data;
                    }

                    this.DoAnswer();
                    break;
            }
        }

        private void TransmitToPeer(byte[] bytes, int offset, int count)
        {
            Modem peer;

            lock (this._StateLock)
            {
                peer = this._Peer;
            }

            if (peer != null)
            {
                peer.ChannelWriteData(bytes, offset, count);
            }
        }

        private void ChannelWrite(byte[] bytes)
        {
            this.ChannelWrite(bytes, 0, bytes.Length);
        }

        private void ChannelWrite(byte[] bytes, int offset, int count)
        {
            this._Channel.Write(bytes, offset, count);
        }

        private void ChannelWriteData(byte[] bytes, int offset, int count)
        {
            lock (this._StateLock)
            {
                if (this._State < ModemState.Data || this._State > ModemState.DataPPP)
                    return;
            }

            this.ChannelWrite(bytes, offset, count);
        }

        private void ChannelWriteData(byte[] bytes)
        {
            this._Channel.Write(bytes, 0, bytes.Length);
        }

        private void ChannelBreak()
        {
            this._Channel.Break();
        }

        private static readonly byte[] ConnectMessage = Encoding.ASCII.GetBytes("CONNECT 115200\r");
        private static readonly byte[] NoDialtoneMessage = Encoding.ASCII.GetBytes("NO DIALTONE\r");
        private static readonly byte[] NoAnswerMessage = Encoding.ASCII.GetBytes("NO ANSWER\r");
        private static readonly byte[] BusyMessage = Encoding.ASCII.GetBytes("BUSY\r");

        private void DoDial(string phoneNumber)
        {
            Modem peer = null;
            ConnectResult result = this.Dial(this, phoneNumber, ref peer);

            if (result == ConnectResult.Connect && peer != null)
            {
                peer.OnOOBReceived(OOBSignal.Ring);
            }

            Thread.Sleep(1000);

            switch (result)
            {
                case ConnectResult.Connect:
                    this.ChannelWrite(ConnectMessage);

                    lock (this._StateLock)
                    {
                        this._Peer = peer;
                        this._State = ModemState.Data;
                    }
                    break;

                case ConnectResult.NoDialtone:
                    this.ChannelWrite(NoDialtoneMessage);
                    break;

                case ConnectResult.NoAnswer:
                    this.ChannelWrite(NoAnswerMessage);
                    break;

                case ConnectResult.Busy:
                    this.ChannelWrite(BusyMessage);
                    break;
            }
        }

        private void DoAnswer()
        {
            Modem peer = null;

            if (this.Answer(this, ref peer))
            {
                lock (this._StateLock)
                {
                    this._Peer = peer;

                    this._State = ModemState.Data;
                    this._Ringing = false;
                }
            }
        }

        private void DoHangup()
        {
            Modem peer;
            lock (this._StateLock)
            {
                peer = this._Peer;

                this._Peer = null;
                this._State = ModemState.Command;
            }

            if (peer != null)
            {
                peer.ChannelBreak();
                this.Hangup(this);
            }
        }

    } //class Modem

}
