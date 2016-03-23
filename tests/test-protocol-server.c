#include "protocol-server.h"

#include <stdio.h>
#include <string.h>

// we don't have to, but printing stuff is nice...
void on_open(server_pt server, int sockfd)
{
    printf("A connection was accepted on socket #%d.\n", sockfd);
}

// server_pt is just a nicely typed pointer,
void on_close(server_pt server, int sockfd)
{
    printf("Socket #%d is now disconnected.\n", sockfd);
}

// a simple echo... this is the main callback
void on_data(server_pt server, int sockfd)
{
    char buff[1024]; // We'll assign a reading buffer on the stack
    ssize_t incoming = 0;
    // Read everything, this is edge triggered, `on_data` won't be called
    // again until all the data was read.
    while ((incoming = Server.read(server, sockfd, buff, 1024)) > 0) {
        // since the data is stack allocated, we'll write a copy
        // optionally, we could avoid a copy using Server.write_move
        Server.write(server, sockfd, buff, incoming);
        if (!memcmp(buff, "bye", 3)) {
            ; // closes the connection automatically AFTER the buffer was sent.
            Server.close(server, sockfd);
        }
    }
}

int main(void)
{
    // We'll create the echo protocol object as the server's default
    struct Protocol protocol = {
        .on_open = on_open,
        .on_close = on_close,
        .on_data = on_data,
        .service = "echo"
    };
    // We'll use the macro start_server, because our settings are simple.
    // (this will call Server.listen(settings) with our settings)
    start_server(.protocol = &protocol, .timeout = 10, .threads = 8);
}
