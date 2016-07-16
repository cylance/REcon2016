////////////////////////////////////////////////////////////////
// ModemSwitchboard/ChannelHandler.cs
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
using System.Net;
using System.Threading;

namespace Cylance.Research.ModemSwitchboard
{

    internal enum OOBSignal
    {
        Disconnected = -1,
        None = 0,
        Ready,
        Break,
        Ring
    }

    internal delegate void DataReceivedCallback(byte[] bytes, int offset, int count);

    internal delegate void OOBReceivedCallback(OOBSignal signal);

    internal abstract class ChannelHandler
    {

        protected ChannelHandler()
        {
        }

        public abstract void Start();

        public event DataReceivedCallback DataReceived;  // called asynchronously
        public event OOBReceivedCallback OOBReceived;  // called asynchronously

        protected void OnDataReceived(byte[] bytes, int offset, int count)
        {
            if (this.DataReceived != null)
            {
                this.DataReceived(bytes, offset, count);
            }
        }

        protected void OnOOBReceived(OOBSignal signal)
        {
            if (this.OOBReceived != null)
            {
                this.OOBReceived(signal);
            }
        }

        public abstract bool Write(byte[] bytes, int offset, int count);

        public abstract void Break();

        public static ChannelHandler Create(string endpoint, bool isWildcat)
        {
            if (endpoint == null)
                throw new ArgumentNullException();

            // named pipe
            if (endpoint.StartsWith(@"\\.\pipe\", StringComparison.OrdinalIgnoreCase) ||
                endpoint.StartsWith(@"\\?\pipe\", StringComparison.OrdinalIgnoreCase))
            {
                if (endpoint.Length <= 9)
                    throw new ArgumentException();

                return new PipeChannel(endpoint.Substring(9), isWildcat);
            }
            else if (endpoint.StartsWith(@"\pipe\", StringComparison.OrdinalIgnoreCase))
            {
                if (endpoint.Length <= 6)
                    throw new ArgumentException();

                return new PipeChannel(endpoint.Substring(6), isWildcat);
            }

            // IPv4/IPv6
            IPAddress ipaddr;
            int port;

            int x = endpoint.LastIndexOf(':');
            if (x >= 0 && IPAddress.TryParse(endpoint.Substring(0, x), out ipaddr) && int.TryParse(endpoint.Substring(x + 1), out port))
            {
                IPEndPoint ipep = new IPEndPoint(ipaddr, port);

                return new SocketChannel(ipep, isWildcat);
            }

            throw new ArgumentException();
        }

    } //class ChannelHandler

}
