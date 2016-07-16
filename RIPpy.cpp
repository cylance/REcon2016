////////////////////////////////////////////////////////////////
// RIPpy.cpp
//==============================================================
//
//  BBS-Era Exploitation for Fun and Anachronism
//  REcon 2016
//
//  Derek Soeder and Paul Mehta
//  Cylance, Inc.
//______________________________________________________________
//
// Crude RIPscrip command fuzzer for RIPterm.
//
// July 1, 2016
//
////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32")



////////////////////////////////////////////////////////////////
// Communication
////////////////////////////////////////////////////////////////

////////////////////////////////
// drain_pipe
////////////////////////////////
bool drain_pipe(
        HANDLE                  hPipe )
{
        DWORD                   cb, cbavail;
        BYTE                    rgbbuf[2048];

        //----------------
        for ( ; ; )
        {
                cbavail = 0;
                if (!PeekNamedPipe( hPipe, NULL, 0, NULL, &cbavail, NULL ))
                        return false;

                if (cbavail == 0) break;

                cb = 0;
                if (!ReadFile( hPipe, rgbbuf, cbavail, &cb, NULL ) || cb == 0)
                        return false;
                printf( "%.*s", cb, rgbbuf );
        }

        return true;
} //drain_pipe


////////////////////////////////
// read_byte
////////////////////////////////
int read_byte(
        HANDLE                  hPipe )
{
        BYTE                    b;
        DWORD                   cb;

        //----------------
        if (!ReadFile( hPipe, &b, 1, &cb, NULL ) || cb != 1)
                return -1;

        return b;
} //read_byte


////////////////////////////////
// write_byte
////////////////////////////////
bool write_byte(
        HANDLE                  hPipe,
        BYTE                    b )
{
        DWORD                   cb;

        //----------------
        return (WriteFile( hPipe, &b, 1, &cb, NULL ) && cb == 1);
} //write_byte


////////////////////////////////
// write_raw
////////////////////////////////
bool write_raw(
        HANDLE                  hPipe,
        const void              * pData,
        int                     cbData )
{
        DWORD                   cb;

        //----------------
        return (WriteFile( hPipe, pData, (DWORD)cbData, &cb, NULL ) && cb == (DWORD)cbData);
} //write_raw


////////////////////////////////
// write_string
////////////////////////////////
bool write_string(
        HANDLE                  hPipe,
        const char              * szString )
{
        return write_raw( hPipe, szString, (DWORD) strlen( szString ) );
} //write_string



////////////////////////////////////////////////////////////////
// Attack code
////////////////////////////////////////////////////////////////

const char * const VARIABLE_NAMES[] =
{
        ">C:\\RIPTERM\\RIPTERM.DB",
        ">RIPTERM.DB",
        "ADOW",
        "ALARM",
        "AMPM",
        "APP0",
        "APP1",
        "APP2",
        "APP3",
        "APP4",
        "APP5",
        "APP6",
        "APP7",
        "APP8",
        "APP9",
        "ATW",
        "AVP",
        "BACKSTAT",
        "BASEMATH",
        "BAUDEMUL",
        "BEEP",
        "BLIP",
        "CLS",
        "COFF",
        "COLORMODE",
        "COLORS",
        "COMPAT",
        "CON",
        "COORDSIZE",
        "COPY",
        "CTRL",
        "CUR",
        "CURSOR",
        "CURX",
        "CURY",
        "D",
        "DATE",
        "DATETIME",
        "DAY",
        "DOW",
        "DOY",
        "DTW",
        "DVP",
        "DWAYOFF",
        "DWAYON",
        "EGW",
        "ETW",
        "FIELDID",
        "FILEDEL",
        "FYEAR",
        "HKEYOFF",
        "HKEYON",
        "HOSTDIR",
        "HOUR",
        "IFS",
        "IMGSTYLE",
        "INUSE",
        "ISEXTWIN",
        "ISPALETTE",
        "ISPROT",
        "M",
        "MCURSOR",
        "MHOUR",
        "MIN",
        "MKILL",
        "MONTH",
        "MONTHNUM",
        "MSTAT",
        "MTW",
        "MUSIC",
        "MVP",
        "NOREFRESH",
        "NULL",
        "OFFSCREEN",
        "OPTION",
        "PALENTRY",
        "PCB",
        "PCB",
        "PHASER",
        "PORTH",
        "PORTW",
        "PORTX0",
        "PORTX1",
        "PORTY0",
        "PORTY1",
        "PROT",
        "RBS",
        "RCB",
        "RCP",
        "REFRESH",
        "RENV",
        "RESET",
        "RESTALL",
        "RESTORE",
        "RESTORE0",
        "RESTORE1",
        "RESTORE2",
        "RESTORE3",
        "RESTORE4",
        "RESTORE5",
        "RESTORE6",
        "RESTORE7",
        "RESTORE8",
        "RESTORE9",
        "RESTOREALL",
        "RESX",
        "RESY",
        "REVPHASER",
        "RGS",
        "RIPVER",
        "RMF",
        "RTW",
        "SAVE",
        "SAVE0",
        "SAVE1",
        "SAVE2",
        "SAVE3",
        "SAVE4",
        "SAVE5",
        "SAVE6",
        "SAVE7",
        "SAVE8",
        "SAVE9",
        "SAVEALL",
        "SBAROFF",
        "SBARON",
        "SBS",
        "SCB",
        "SCP",
        "SEC",
        "SENV",
        "SGS",
        "SHIFT",
        "SMF",
        "STATBAR",
        "STW",
        "T",
        "TABOFF",
        "TABON",
        "TERMINFO",
        "TEXTXY",
        "TIME",
        "TIMEZONE",
        "TWERASEEOL",
        "TWFONT",
        "TWFONTS",
        "TWGOTO",
        "TWH",
        "TWHOME",
        "TWIN",
        "TWW",
        "TWX0",
        "TWX1",
        "TWY0",
        "TWY1",
        "UNPROT",
        "VT102OFF",
        "VT102ON",
        "WDAY",
        "WORLD",
        "WORLDH",
        "WORLDW",
        "WOY",
        "WOYM",
        "X",
        "XFERXY",
        "XY",
        "XYM",
        "Y",
        "YEAR"
};


////////////////////////////////
// random_buffer
////////////////////////////////
void random_buffer(
        char                    * szBuffer,
        int                     nLength)
{
        int                     i, n;

        //----------------
        while (nLength > 0)
        {
                switch (rand() & 15)
                {
                case 0:
                        *(szBuffer++) = '0' + (rand() % 10);
                        nLength--;
                        break;
                case 1:
                        *(szBuffer++) = 'A' + (rand() % 26);
                        nLength--;
                        break;
                case 2:
                        *(szBuffer++) = 'a' + (rand() % 26);
                        nLength--;
                        break;
                case 3:
                        *(szBuffer++) = "\x1B" "$0123456789?ABCDEFGHIJKLMNOPQRSTUVWXYZ\\abcdefghijklmnopqrstuvwxyz"[rand() % 66];
                        nLength--;
                        break;
                case 4:
                        *(szBuffer++) = "\1\2\x1B !\"#$%&'()*+,-./0123456789:;<=>@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F"[rand() % 98];
                        nLength--;
                        break;
                case 5:
                        *(szBuffer++) = (char)(rand());
                        nLength--;
                        break;
                case 6:
                        n = 1 + (rand() % min(nLength, 8));
                        nLength -= n;
                        while (n-- > 0) *(szBuffer++) = '0' + (rand() % 10);
                        break;
                case 7:
                        if (nLength < 4) break;
                        n = (4 + (rand() % (nLength - 3))) >> (rand() & 3);
                        nLength -= n;
                        while (n-- > 0) *(szBuffer++) = '0' + (rand() % 10);
                        break;
                case 8:
                        if (nLength < 4) break;
                        n = (4 + (rand() % (nLength - 3))) >> (rand() & 3);
                        nLength -= n;
                        while (n-- > 0) *(szBuffer++) = 'A' + (rand() % 26);
                        break;
                case 9:
                        if (nLength < 4) break;
                        n = (4 + (rand() % (nLength - 3))) >> (rand() & 3);
                        nLength -= n;
                        while (n-- > 0) *(szBuffer++) = 'a' + (rand() % 26);
                        break;
                case 10:
                        if (nLength < 4) break;
                        n = (4 + (rand() % (nLength - 3))) >> (rand() & 3);
                        nLength -= n;
                        while (n-- > 0) *(szBuffer++) = "\x1B" "$0123456789?ABCDEFGHIJKLMNOPQRSTUVWXYZ\\abcdefghijklmnopqrstuvwxyz"[rand() % 66];
                        break;
                case 11:
                        if (nLength < 4) break;
                        n = (4 + (rand() % (nLength - 3))) >> (rand() & 3);
                        nLength -= n;
                        while (n-- > 0)
                                *(szBuffer++) = "\1\2\x1B !\"#$%&'()*+,-./0123456789:;<=>@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                "[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F"[rand() % 98];
                        break;
                case 12:
                        if (nLength <= 4) break;
                        nLength -= 4;
                        *(szBuffer++) = '(';
                        *(szBuffer++) = '(';
                        n = 1 + (rand() % (1 + (nLength >> (rand() & 3))));
                        random_buffer(szBuffer, n);
                        szBuffer += n;
                        nLength -= n;
                        *(szBuffer++) = ')';
                        *(szBuffer++) = ')';
                        break;
                case 13:
                        if (nLength <= 3) break;
                        nLength -= 2;
                        *(szBuffer++) = '$';
                        n = 1 + (rand() % (1 + ((nLength - 1) >> (rand() & 3))));
                        random_buffer(szBuffer, n);
                        szBuffer += n;
                        nLength -= n;
                        *(szBuffer++) = '$';
                        break;
                case 14:
                        if (nLength < 3) break;
                        nLength -= 2;
                        *(szBuffer++) = '$';
                        for (;;)
                        {
                                i = rand() % (sizeof(VARIABLE_NAMES) / sizeof(VARIABLE_NAMES[0]));
                                n = (int)strlen(VARIABLE_NAMES[i]);
                                if (n <= nLength)
                                {
                                        memcpy(szBuffer, VARIABLE_NAMES[i], n);
                                        szBuffer += n;
                                        nLength -= n;
                                        break;
                                }
                        }
                        *(szBuffer++) = '$';
                        break;
                default:
                        if (nLength < 2) break;
                        nLength -= 2;
                        *(szBuffer++) = '\x1B';
                        *(szBuffer++) = '[';
                        break;
                } //switch
        } //while(nLength)
} //random_buffer


////////////////////////////////
// do_attack
////////////////////////////////
bool do_attack(
        HANDLE                  hPipe )
{
        char                    szbuf[4096];
        int                     i, n, nattacks;
        DWORD                   cb;

        //----------------
        for (nattacks = 0; ; nattacks++)
        {
                if (!write_string( hPipe, "!|" )) return false;

                n = (rand() & 31);
                switch (n)
                {
                        case 0:
                        case 1:
                        case 2:
                        case 3:
                        case 4:
                        case 5:
                        case 6:
                        case 7:
                                n = '0' + (rand() % 10);
                                write_byte( hPipe, (BYTE)n );
                                break;
                        case 8:
                        case 9:
                        case 10:
                        case 11:
                                for (n = rand() & 15, i = 0; i < n; i++)
                                        szbuf[i] = '0' + (rand() % 10);
                                if (!write_raw( hPipe, szbuf, n )) return false;
                                break;
                        case 12:
                        case 13:
                                for (n = rand() & 31, i = 0; i < n; i++)
                                        szbuf[i] = "\1\2\x1B !\"#$%&'()*+,-./0123456789:;<=>@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                   "[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F"[rand() % 98];
                                if (!write_raw( hPipe, szbuf, n )) return false;
                                break;
                        case 14:
                                for (n = rand() & 31, i = 0; i < n; i++)
                                        szbuf[i] = (char)(rand());
                                if (!write_raw( hPipe, szbuf, n )) return false;
                                break;
                } //switch(level)

                n = (rand() & 15);
                switch (n)
                {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                        n = "\x1B#*=>@ABCDEFGHIKLMOPQRSTUVWXYZacegimopstvw"[rand() % 42];
                        if (!write_byte( hPipe, (BYTE)n )) return false;
                        break;
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                        n = "\1\2\x1B !\"#$%&'()*+,-./0123456789:;<=>@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F"[rand() % 98];
                        if (!write_byte( hPipe, (BYTE)n )) return false;
                        break;
                case 15:
                        n = (rand() & 0xFF);
                        if (!write_byte( hPipe, (BYTE)n )) return false;
                        break;
                } //switch(command)

                n = (4 + (rand() % (sizeof(szbuf) - 3))) >> (rand() & 3);

                random_buffer( szbuf, n );

                if (!write_raw( hPipe, szbuf, n ) || !write_string( hPipe, "\r\n")) return false;

                if (!write_string( hPipe, "\x1B[!" ) || !ReadFile( hPipe, szbuf, sizeof(szbuf), &cb, NULL ) || cb == 0)
                        return false;

                putchar( '.' );
        } //for(;;)

        return true;
} //do_attack


////////////////////////////////
// modem_accept_connect
////////////////////////////////
bool modem_accept_connect(
        HANDLE                  hPipe )
{
        int                     n;

        //----------------
        if (read_byte( hPipe ) != 'A') return false;
        if (read_byte( hPipe ) != 'T') return false;
        if (read_byte( hPipe ) != 'D') return false;
        if (read_byte( hPipe ) != 'T') return false;

        for ( ; ; )
        {
                int ch = read_byte( hPipe );
                if (ch < 0) return false;
                if (ch < ' ' || ch == '<' || ch == '>' || ch > '~') printf( "<%02X>", ch ); else putchar( ch );
                if (ch == '\r' || ch == '\n') break;
        }
        printf( "\n" );

        Sleep( 1000 );
        write_string( hPipe, "CONNECT 115200\r" );

        Sleep( 500 );
        write_string( hPipe, "\x1B[!" );
        Sleep( 500 );

        for (n = 0; n < 14; n++)
        {
                int ch = read_byte( hPipe );
                if (ch < 0) return false;
                if (ch < ' ' || ch == '<' || ch == '>' || ch > '~') printf( "<%02X>", ch ); else putchar( ch );
                if (ch == '\r' || ch == '\n') break;
        }
        printf( "\n" );

        return true;
} //modem_accept_connect



////////////////////////////////////////////////////////////////
// Main
////////////////////////////////////////////////////////////////

////////////////////////////////
// parse_endpoint
////////////////////////////////
bool parse_endpoint(
        const char              * szEndpoint,
        const char *            * pszPipeName,
        sockaddr_in             * psinPeer )
{
        char                    szip[16];
        const char              * pch;
        int                     nport;

        //----------------
        *pszPipeName = 0;
        memset( psinPeer, 0, sizeof(psinPeer) );

        if (memcmp( szEndpoint, "\\\\.\\", 4 ) == 0)
        {
                *pszPipeName = szEndpoint;
                return true;
        }

        pch = strchr( szEndpoint, ':' );
        if (!pch || (pch - szEndpoint) >= sizeof(szip))
                return false;

        memcpy( szip, szEndpoint, (pch - szEndpoint) );
        szip[pch - szEndpoint] = '\0';

        if ( (psinPeer->sin_addr.S_un.S_addr = inet_addr( szip )) == INADDR_NONE ||
             sscanf_s( pch + 1, "%d", &nport ) != 1 || nport <= 0 || nport > 65535 )
        {
                return false;
        }

        psinPeer->sin_family = AF_INET;
        psinPeer->sin_port   = htons( (u_short)nport );
        return true;
} //parse_endpoint


////////////////////////////////
// main
////////////////////////////////
int main(
        int                     argc,
        const char *            argv[] )
{
        WSADATA                 wsa;
        sockaddr_in             sin;
        const char              * szpipe;
        HANDLE                  hconn;
        char                    szbuf[32];

        //---------------- parse command line and initialize
        if (WSAStartup( MAKEWORD(2,0), &wsa ) != 0)
        {
                fprintf( stderr, "%s: WSAStartup failed with error %u\n", argv[0], GetLastError() );
                return 1;
        }

        if (argc != 2 || strchr( argv[1], '?' ) || !parse_endpoint( argv[1], &szpipe, &sin ))
        {
                fprintf( stderr, "Usage: %s { \\\\.\\PIPE\\namedpipe | ipv4address:tcpport }\n", argv[0] );
                WSACleanup();
                return 1;
        }

        srand(1);  // constant seed for reproducible results; change as desired

        //---------------- connect to endpoint
        if (szpipe)
        {
                hconn = CreateFileA(
                        argv[1],
                        GENERIC_READ|GENERIC_WRITE,
                        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,
                        NULL );

                if (hconn == INVALID_HANDLE_VALUE || hconn == NULL)
                {
                        fprintf( stderr, "%s: CreateFile failed with error %u\n", argv[0], GetLastError() );
                        WSACleanup();
                        return 1;
                }

                modem_accept_connect( hconn );  // we're emulating modem and waiting for RIPterm to dial out
        }
        else
        {
                hconn = (HANDLE) WSASocketA( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0 );
                if (hconn == (HANDLE)INVALID_SOCKET || hconn == INVALID_HANDLE_VALUE || hconn == NULL)
                {
                        fprintf( stderr, "%s: WSASocket failed with error %d\n", argv[0], WSAGetLastError() );
                        WSACleanup();
                        return 1;
                }

                if (connect( (SOCKET)hconn, (sockaddr *)&sin, sizeof(sin) ) != 0)
                {
                        fprintf( stderr, "%s: connect failed with error %d\n", argv[0], WSAGetLastError() );
                        closesocket( (SOCKET)hconn );
                        WSACleanup();
                        return 1;
                }

                // DOSBox is emulating modem, and we're calling RIPterm; receive anything so we know it has answered
                if (recv( (SOCKET)hconn, szbuf, sizeof(szbuf), 0 ) <= 0)
                {
                        fprintf( stderr, "%s: recv failed with error %d\n", argv[0], WSAGetLastError() );
                        closesocket( (SOCKET)hconn );
                        WSACleanup();
                        return 1;
                }
        } //if-else(ip:port)

        //---------------- attack
        do_attack( hconn );

        //---------------- clean up and exit
        if (szpipe)
        {
                drain_pipe( hconn );  // always drain pipe before closing
                CloseHandle( hconn );
        }
        else
        {
                shutdown( (SOCKET)hconn, SD_BOTH );
                closesocket( (SOCKET)hconn );
        }

        WSACleanup();
        return 0;
} //main
