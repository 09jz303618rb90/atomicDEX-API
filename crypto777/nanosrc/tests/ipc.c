/*
    Copyright (c) 2012 Martin Sustrik  All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#include "../nn.h"
#include "../pair.h"
#include "../pubsub.h"
#include "../ipc.h"

#include "testutil.h"

/*  Tests IPC transport. */

#define SOCKET_ADDRESS "ipc://test.ipc"

int testipc()
{
    int sb;
    int sc;
    int i;
    int s1, s2;

	size_t size;
	char * buf;
    printf("test ipc\n");
    if ( 1 )
    {
        /*  Try closing a IPC socket while it not connected. */
        sc = test_socket (AF_SP, NN_PAIR);
        test_connect (sc, SOCKET_ADDRESS);
        test_close (sc);
        
        /*  Open the socket anew. */
        sc = test_socket (AF_SP, NN_PAIR);
        test_connect (sc, SOCKET_ADDRESS);
        
        /*  Leave enough time for at least one re-connect attempt. */
        nn_sleep (200);
        
        sb = test_socket (AF_SP, NN_PAIR);
        test_bind (sb, SOCKET_ADDRESS);
        
        /*  Ping-pong test. */
        for (i = 0; i != 1; ++i) {
            test_send (sc, "0123456789012345678901234567890123456789");
            test_recv (sb, "0123456789012345678901234567890123456789");
            test_send (sb, "0123456789012345678901234567890123456789");
            test_recv (sc, "0123456789012345678901234567890123456789");
        }
        
        /*  Batch transfer test. */
        for (i = 0; i != 100; ++i) {
            test_send (sc, "XYZ");
        }
        for (i = 0; i != 100; ++i) {
            test_recv (sb, "XYZ");
        }
        
        /*  Send something large enough to trigger overlapped I/O on Windows. */
        size = 10000;
        buf = malloc( size );
        for (i =0; i != size - 1; ++i) {
            buf[i] = 48 + i % 10;
        }
        buf[size-1] = '\0';
        test_send (sc, buf);
        test_recv (sb, buf);
        free( buf );
        
        test_close (sc);
        test_close (sb);
    }
    if ( 1 )
    {
        /*  Test whether connection rejection is handled decently. */
        sb = test_socket (AF_SP, NN_PAIR);
        test_bind (sb, SOCKET_ADDRESS);
        s1 = test_socket (AF_SP, NN_PAIR);
        test_connect (s1, SOCKET_ADDRESS);
        s2 = test_socket (AF_SP, NN_PAIR);
        test_connect (s2, SOCKET_ADDRESS);
        nn_sleep (100);
        test_close (s2);
        test_close (s1);
        test_close (sb);
    }
    if ( 1 )
    {
        /*  Test two sockets binding to the same address. */
        sb = test_socket (AF_SP, NN_PAIR);
        test_bind (sb, SOCKET_ADDRESS);
        s1 = test_socket (AF_SP, NN_PAIR);
        test_bind (s1, SOCKET_ADDRESS);
        sc = test_socket (AF_SP, NN_PAIR);
        test_connect (sc, SOCKET_ADDRESS);
        //printf("sb.%d s1.%d sc.%d\n",sb,s1,sc);
        nn_sleep (100);
        //printf("send.(ABC) to sb\n");
        test_send (sb, "ABC");
        //printf("check recv.(ABC) via sc\n");
        test_recv (sc, "ABC");
        //printf("close sb\n");
        test_close (sb);
        //printf("send.(DEF) to s1 getchar()\n"), getchar();
        test_send (s1, "DEF");
        //printf("check recv.(DEF) via sc, getchar()\n"); getchar();
        //nn_sleep(1000);
        test_recv (sc, "DEF");
        //printf("close sc getchar()\n"); getchar();
        test_close (sc);
        //printf("close s1\n");
        test_close (s1);
    }
    //printf("finished ipc test\n");

    return 0;
}

