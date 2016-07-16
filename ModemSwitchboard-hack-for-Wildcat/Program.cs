////////////////////////////////////////////////////////////////
// ModemSwitchboard/Program.cs
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
using System.IO;
using System.Threading;

namespace Cylance.Research.ModemSwitchboard
{

    public sealed class Program
    {

        private static void UsageExit()
        {
            Console.Error.WriteLine(
@"Usage:  {0} endpoint_for_ripterm phonenumber_for_ripterm endpoint_for_wildcat phonenumber_for_wildcat

Where:  endpoint*  is an address in one of the following forms:
                     \\.\PIPE\namedpipename
                     ipv4address:port
                     [ipv6address]:port
     phonenumber*  is any unique, dialable string",
                AppDomain.CurrentDomain.FriendlyName);

            Environment.Exit(1);
            return;
        }

        public static void Main(string[] args)
        {
            if (args.Length != 4)
            {
                UsageExit();
                return;
            }

            Switchboard switchboard = new Switchboard();

            List<ChannelHandler> channels = new List<ChannelHandler>();

            for (int i = 0; (i + 2) <= args.Length; i += 2)
            {
                try
                {
                    ChannelHandler channel = ChannelHandler.Create(args[i + 0], (i == 2));
                    string phonenumber = args[i + 1];

                    switchboard.Add(new Modem(phonenumber, channel), phonenumber);

                    channels.Add(channel);
                }
                catch (ArgumentException)
                {
                    UsageExit();
                    return;
                }
            }

            foreach (ChannelHandler channel in channels)
            {
                channel.Start();
            }

            Console.WriteLine("Running...");
            Thread.Sleep(Timeout.Infinite);
        }

    } //class Program

}
