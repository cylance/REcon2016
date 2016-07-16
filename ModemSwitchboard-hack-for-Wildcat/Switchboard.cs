////////////////////////////////////////////////////////////////
// ModemSwitchboard/Switchboard.cs
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

namespace Cylance.Research.ModemSwitchboard
{

    internal enum ConnectResult
    {
        Connect = 0,
        NoDialtone = 1,
        NoAnswer = 2,
        Busy = 3
    }

    internal sealed class Switchboard
    {

        private sealed class Subscriber
        {
            public readonly Modem Modem;
            public Subscriber Peer;

            public bool OffHook
            {
                get
                {
                    return (this.Peer != null);
                }

                set
                {
                    if (value)
                    {
                        if (this.Peer != null)
                            throw new InvalidOperationException();
                        this.Peer = this;  // "connected" to self until a call is connected
                    }
                    else
                    {
                        this.Peer = null;
                    }
                }
            } //OffHook

            public Subscriber(Modem modem)
            {
                this.Modem = modem;
                this.Peer = null;
            }
        } //Switchboard.Subscriber

        private readonly object _DirectoryLock = new object();
        private readonly Dictionary<string, Subscriber> _Directory = new Dictionary<string, Subscriber>();

        public Switchboard()
        {
        }

        public static bool IsDialable(string phoneNumber)
        {
            if (String.IsNullOrEmpty(phoneNumber))
                return false;

            foreach (char ch in phoneNumber)
            {
                if (ch <= 0x0020 || ch > 0x007E)  // non-space, printable ASCII
                    return false;
            }

            return true;
        }

        public void Add(Modem modem, string phoneNumber)
        {
            if (modem == null || phoneNumber == null)
                throw new ArgumentNullException();
            else if (!IsDialable(phoneNumber))
                throw new ArgumentException();

            lock (this._DirectoryLock)
            {
                this._Directory.Add(phoneNumber, new Subscriber(modem));

                modem.Dial += OnDial;
                modem.Answer += OnAnswer;
                modem.Hangup += OnHangup;
            }
        }

        public ConnectResult Connect(string callerPhoneNumber, string dstPhoneNumber, out Modem modem)
        {
            lock (this._DirectoryLock)
            {
                Subscriber caller;
                Subscriber dst;

                if (!this._Directory.TryGetValue(callerPhoneNumber, out caller))
                    throw new ArgumentException();

                if (caller.OffHook)
                {
                    modem = null;
                    return ConnectResult.NoDialtone;  // "NO DIALTONE"
                }

                caller.OffHook = true;

                if (!this._Directory.TryGetValue(dstPhoneNumber, out dst))
                {
                    caller.OffHook = false;
                    modem = null;
                    return ConnectResult.NoAnswer;  // "NO ANSWER"
                }
                else if (dst.OffHook)
                {
                    caller.OffHook = false;
                    modem = null;
                    return ConnectResult.Busy;  // "BUSY"
                }

                dst.OffHook = true;

                caller.Peer = dst;
                dst.Peer = caller;

                modem = dst.Modem;
                return ConnectResult.Connect;  // "CONNECT"
            } //lock
        }

        public void Disconnect(string phoneNumber)
        {
            lock (this._DirectoryLock)
            {
                Subscriber subscriber;

                if (!this._Directory.TryGetValue(phoneNumber, out subscriber))
                    throw new ArgumentException();

                subscriber.OffHook = false;

                Subscriber peer = subscriber.Peer;
                if (peer != null)
                    peer.OffHook = false;
            } //lock
        }

        private ConnectResult OnDial(Modem sender, string phoneNumber, ref Modem peer)
        {
            return this.Connect(sender.PhoneNumber, phoneNumber, out peer);
        }

        private bool OnAnswer(Modem sender, ref Modem peer)
        {
            lock (this._DirectoryLock)
            {
                Subscriber receiver;

                if (!this._Directory.TryGetValue(sender.PhoneNumber, out receiver))
                    throw new ArgumentException();

                if (receiver == null || receiver.Peer == null)
                    return false;

                peer = receiver.Peer.Modem;
            }

            return (peer != null);
        }

        private void OnHangup(Modem sender)
        {
            this.Disconnect(sender.PhoneNumber);
        }

    } //class Switchboard

}
