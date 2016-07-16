////////////////////////////////////////////////////////////////
// ModemSwitchboard/SocketChannel.cs
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
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace Cylance.Research.ModemSwitchboard
{

    internal sealed class SocketChannel : ChannelHandler
    {

        private const int BreakPulseDelay = 25;  // milliseconds

        private readonly IPEndPoint _Endpoint;
        private readonly bool _AutoReconnect;

        private TcpClient _Connection;
        private readonly object _ConnectionLock = new object();  // held when connecting, disconnecting, and obtaining a TCP client reference

        public SocketChannel(IPEndPoint endpoint, bool isWildcat)
        {
            this._Endpoint = endpoint;
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
                TcpClient connection;

                lock (this._ConnectionLock)
                {
                    connection = this._Connection;
                }

                if (connection == null)
                {
                    Thread.Sleep(BreakPulseDelay);

                    if (this._AutoReconnect && this.Connect())
                        this.OnOOBReceived(OOBSignal.Ready);

                    continue;
                }

                try
                {
                    Socket socket = connection.Client;

                    if (socket != null)
                    {
                        int len = socket.Receive(buffer, 0, buffer.Length, SocketFlags.None);

                        if (len > 0)
                        {
                            this.OnDataReceived(buffer, 0, len);
                            continue;
                        }
                    }
                }
                catch (SocketException) { }
                catch (InvalidOperationException) { }

                this.CloseConnection();

                this.OnOOBReceived(OOBSignal.Break);
            } //for(;;)
        }

        private bool Connect()
        {
            lock (this._ConnectionLock)
            {
                try
                {
                    TcpClient connection = this._Connection;

                    if (connection != null)
                    {
                        Socket socket = connection.Client;

                        if (socket != null && socket.Connected)
                            return false;

                        this._Connection = null;

                        connection.Close();
                    }

                    connection = new TcpClient();

                    connection.Connect(this._Endpoint);

                    this._Connection = connection;

                    {
                        Socket socket = connection.Client;
                        return (socket != null && socket.Connected);
                    }
                }
                catch (SocketException) { }
            } //lock

            return false;
        }

        private static void DrainSocket(Socket socket)
        {
            if (socket == null || !socket.Connected || !HasData(socket))
                return;

            byte[] buffer = new byte[0x8000];

            do
            {
                if (socket.Receive(buffer, 0, buffer.Length, SocketFlags.None) <= 0)
                    break;
            }
            while (HasData(socket));
        }

        private void CloseConnection()
        {
            lock (this._ConnectionLock)
            {
                TcpClient connection = this._Connection;

                if (connection == null)
                    return;

                this._Connection = null;

                try
                {
                    Socket socket = connection.Client;

                    if (socket != null && socket.Connected)
                    {
                        DrainSocket(socket);
                    }
                }
                catch (SocketException) { }
                catch (InvalidOperationException) { }

                try
                {
                    connection.Close();
                }
                catch (SocketException) { }
                catch (InvalidOperationException) { }
            } //lock
        }

        public override bool Write(byte[] bytes, int offset, int count)
        {
            TcpClient connection;

            lock (this._ConnectionLock)
            {
                connection = this._Connection;
            }

            if (connection == null)
                return false;

            try
            {
                Socket socket = connection.Client;

                if (socket != null)
                {
                    socket.Send(bytes, offset, count, SocketFlags.None);
                    return true;
                }
            }
            catch (SocketException) { }
            catch (InvalidOperationException) { }

            return false;
        }

        public override void Break()
        {
            this.CloseConnection();

            Thread.Sleep(BreakPulseDelay);

            this.Connect();  // make sure we're connected before returning (even though BlockingReceiveThread will also try to reconnect)
        }

        private static bool HasData(Socket socket)
        {
            try
            {
                if (socket == null || !socket.Connected)
                    return false;

                return (socket.Available > 0);
            }
            catch (SocketException) { }
            catch (InvalidOperationException) { }

            return false;
        }

    } //class SocketChannel

}
