////////////////////////////////////////////////////////////////
// Escaper.cs
//==============================================================
//
//  BBS-Era Exploitation for Fun and Anachronism
//  REcon 2016
//
//  Derek Soeder and Paul Mehta
//  Cylance, Inc.
//______________________________________________________________
//
// Generates periodic Escape keystrokes to dismiss dialog boxes
// that appear when fuzzing RIPterm.
//
// July 1, 2016
//
////////////////////////////////////////////////////////////////

using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace Escaper
{
    public static class Program
    {
        [DllImport("user32.dll", ExactSpelling = true, SetLastError = true)]
        private static extern void keybd_event(byte bvk, byte bScan, uint dwFlags, IntPtr dwExtraInfo);

        public static void Main(string[] args)
        {
            Thread.Sleep(60 * 1000);  // give me a minute to connect RIPterm to RIPpy

            for ( ; ; )
            {
                Thread.Sleep(10 * 1000);
                keybd_event(0x1B, 1, 0x00, IntPtr.Zero);  // press Escape
                Thread.Sleep(50);
                keybd_event(0x1B, 1, 0x02, IntPtr.Zero);  // release
            }
        }
    }
}
